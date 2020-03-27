// Program to read clock and data from a MLX90109 RFID chip
// and write tag data out
//
// It needs to handle both positive-true and negative-true data
// and set a pin to indicate whether an RFID transponder is in
// range.
//
// The fastest data will arrive is on the order of 250uS
//

#include <SendOnlySoftwareSerial.h>
#include <avr/interrupt.h>

#define IN_CLK_PIN   3
#define IN_CLK_MASK  0b00001000
#define IN_DATA_PIN  4
#define IN_DATA_MASK 0b00010000

#define PRESENT_PIN 0
#define OUT_DATA_TX_PIN 1

#define GOOD_READ_COUNT 3
#define READ_TIMEOUT 100000

#define SOFTS

volatile bool edge_found = false;
volatile byte indata = 0;

byte valid_count = 0;
bool valid_read = false;
unsigned long valid_data = 0;;
long timeout_count = 0;

bool pt_header_found = false;
byte pt_header_count = 0;
byte pt_bit_count = 0;
byte pt_nibble_count = 0;
byte pt_nibble_parity = 0;
byte pt_column_parity = 0;
byte pt_nibble = 0;
byte pt_version = 0;
unsigned long pt_data = 0;
bool pt_bad = false;

bool nt_header_found = false;
byte nt_header_count = 0;
byte nt_bit_count = 0;
byte nt_nibble_count = 0;
byte nt_nibble_parity = 0;
byte nt_column_parity = 0;
byte nt_nibble = 0;
byte nt_version = 0;
unsigned long nt_data = 0;
bool nt_bad = false;

byte RFID_version = 0;
unsigned long RFID_data = 0;

#ifdef SOFTS
SendOnlySoftwareSerial SSerial(OUT_DATA_TX_PIN);
#endif

void setup() {
  // put your setup code here, to run once:
  pinMode (IN_CLK_PIN, INPUT);
  pinMode (IN_DATA_PIN, INPUT);
  pinMode (PRESENT_PIN, OUTPUT);
  pinMode (OUT_DATA_TX_PIN, OUTPUT);

  digitalWrite (PRESENT_PIN, 0);
  digitalWrite (OUT_DATA_TX_PIN, 0);

  edge_found = false;
  indata = 0;

  valid_count = 0;
  valid_read = false;
  valid_data = 0;
  timeout_count = 0;

  GIMSK = 0b00100000; // turn on pin change interrupts
  PCMSK = IN_CLK_MASK; // turn on interrupts on clock pin
  sei(); // enable interrupts

#ifdef SOFTS
  SSerial.begin(9600);
#endif
    
}

// ISR to sample data pin on rising clock
//
ISR(PCINT0_vect) {
  // make sure it is a rising clock
  if ((PINB & IN_CLK_MASK) != 0) {
    // sample data pin
    indata = ((PINB & IN_DATA_MASK) != 0);
    edge_found = true;
    //digitalWrite(PRESENT_PIN, indata & 0x1);
  }
}

void send_data (byte ver, unsigned long dat) {
#ifdef SOFTS
  SSerial.write(0x2); // start byte (STX)
  int i;
  byte crc = ver;
  SSerial.print (((ver >> 4) & 0xf), HEX);
  SSerial.print ((ver & 0xf), HEX);
  for (i=3; i>=0; i--) {
    byte b = (dat >> i*8) & 0xff;
    crc = crc ^ b;
    SSerial.print (((b >> 4) & 0xf), HEX);
    SSerial.print ((b & 0xf), HEX);
  }
  SSerial.print (((crc >> 4) & 0xf), HEX);
  SSerial.print ((crc & 0xf), HEX);
  SSerial.write (0xd); // CR
  SSerial.write (0xa); // LF
  SSerial.write (0x3); // end byte (ETX)
#endif
}



// deal with next incoming bit
void process_bit (byte data) {
  byte pbit = data & 0x1;
  byte nbit = (~data) & 0x1;

  //digitalWrite(PRESENT_PIN, pbit);
  // positive-true logic
  //
  if (!pt_header_found) {
    // either looking for header, or in header
    //
    if (pbit == 0x1) {
      // so far, so good => wait for 9 header bits
      pt_header_count++;
      if (pt_header_count == 9) {
        pt_header_found = true;
        pt_bit_count = 0;
        pt_nibble_count = 0;
        pt_nibble_parity = 0;
        pt_column_parity = 0;
        pt_nibble = 0;
        pt_version = 0;
        pt_data = 0;
        pt_bad = false;

        //digitalWrite(PRESENT_PIN, 1);
      }
    } else {
      pt_header_found = false;
      pt_header_count = 0;
    }
  } else { // outside of header
    //digitalWrite(PRESENT_PIN, pbit);
    //if ((pt_nibble_count == 8) && (pt_bit_count == 0))
    //  digitalWrite(PRESENT_PIN, pbit);
    // reading version, data and parity bits
    //
    if (pt_bit_count < 4) {
      // First 4 bits of group => version, data or column parity
      //
      if (pt_nibble_count <  2) {
        pt_version = (pt_version << 1) | pbit;
      } else if (pt_nibble_count < 10)  {
        pt_data = (pt_data << 1) | pbit;
      }
      pt_nibble = (pt_nibble << 1) | pbit;
      pt_nibble_parity = pt_nibble_parity ^ pbit;
      pt_column_parity = pt_column_parity ^ (pbit << pt_bit_count);
      pt_bit_count++;
    } else { // 5th bit
      // 5th bit in group => nibble parity or stop bit
      //
      if (pt_nibble_count < 10) {
        pt_nibble_parity = pt_nibble_parity ^ pbit;
        if (pt_nibble_parity != 0) {
          pt_bad = true;
        }
        pt_bit_count = 0;
        pt_nibble = 0;
        pt_nibble_parity = 0;
        pt_nibble_count++;
      } else {
        //digitalWrite(PRESENT_PIN, pt_bad);
        // Last bit!
        //
        // need to see stop bit and good parity
        //
        if ((pbit == 0) && (!pt_bad) && (pt_column_parity == 0)) {
          // winner, winner, chicken dinner!! We have a valid data packet!
          //
          RFID_version = pt_version;
          RFID_data = pt_data;
          
          nt_header_found = false;
          nt_header_count = 0;
          //
          // serial write of data
          //
          if (!valid_read) {
            if (valid_data != RFID_data) {
              valid_data = RFID_data;
              valid_count = 0;
            } else {
              valid_count++;
              if (valid_count == GOOD_READ_COUNT) {
                valid_read = true;
                timeout_count = READ_TIMEOUT;
                valid_count = 0;
                digitalWrite (PRESENT_PIN, 1);
                send_data (RFID_version, RFID_data);
              }
            }
          } else {
            timeout_count = READ_TIMEOUT;
          }
        }

        // now look for the next header....
        //
        pt_header_found = false;
        pt_header_count = 0;
      }
    }
  }

  
  // negative-true logic
  //
  if (!nt_header_found) {
    // either looking for header, or in header
    //
    if (nbit == 0x1) {
      // so far, so good => wait for 9 header bits
      nt_header_count++;
      if (nt_header_count == 9) {
        nt_header_found = true;
        nt_bit_count = 0;
        nt_nibble_count = 0;
        nt_nibble_parity = 0;
        nt_column_parity = 0;
        nt_nibble = 0;
        nt_version = 0;
        nt_data = 0;
        nt_bad = false;

      }
    } else {
      nt_header_found = false;
      nt_header_count = 0;
    }
  } else { // outside of header
    // reading version, data and parity bits
    //
    if (nt_bit_count < 4) {
      // First 4 bits of group => version, data or column parity
      //
      if (nt_nibble_count <  2) {
        nt_version = (nt_version << 1) | nbit;
      } else if (nt_nibble_count < 10)  {
        nt_data = (nt_data << 1) | nbit;
      }
      nt_nibble = (nt_nibble << 1) | nbit;
      nt_nibble_parity = nt_nibble_parity ^ nbit;
      nt_column_parity = nt_column_parity ^ (nbit << nt_bit_count);
      nt_bit_count++;
    } else { // 5th bit
      // 5th bit in group => nibble parity or stop bit
      //
      if (nt_nibble_count < 10) {
        nt_nibble_parity = nt_nibble_parity ^ nbit;
        if (nt_nibble_parity != 0) {
          nt_bad = true;
        }
        nt_bit_count = 0;
        nt_nibble = 0;
        nt_nibble_parity = 0;
        nt_nibble_count++;
      } else {
        // Last bit!
        //
        // need to see stop bit and good parity
        //
        if ((nbit == 0) && (!nt_bad) && (nt_column_parity == 0)) {
          // winner, winner, chicken dinner!! We have a valid data packet!
          //
          RFID_version = nt_version;
          RFID_data = nt_data;
          
          pt_header_found = false;
          pt_header_count = 0;
          //
          // serial write of data
          //
          if (!valid_read) {
            if (valid_data != RFID_data) {
              valid_data = RFID_data;
              valid_count = 0;
            } else {
              valid_count++;
              if (valid_count == GOOD_READ_COUNT) {
                valid_read = true;
                timeout_count = READ_TIMEOUT;
                valid_count = 0;
                digitalWrite (PRESENT_PIN, 1);
                send_data (RFID_version, RFID_data);
              }
            }
          } else {
            timeout_count = READ_TIMEOUT;
          }
        }

        // now look for the next header....
        //
        nt_header_found = false;
        nt_header_count = 0;
      }
    }
  }
}

void loop() {
  if (edge_found) {
    edge_found = false;
    process_bit (indata);
    //digitalWrite(PRESENT_PIN, indata);
  }

  // valid_read flag (and PRESENT output)
  // will time out after TIMEOUT*10 microseconds
  //
  if (valid_read) {
    if (timeout_count != 0) {
      timeout_count--;
    } else {
      valid_read = false;
      digitalWrite (PRESENT_PIN, 0);
    }
  }
  
  delayMicroseconds (10);
}
