
// Bee-Finder Mrk1

// LED Rows and SCAN select are on PORTC
// -------------------------------------
// 
// PC0  PC1  PC2  PC3  PC4  PC5  PC6  PC7
// ADC0 ADC1 ADC2 ADC3 ADC4 ADC5 RST  xx
// |    |    |    |    |    |    |    |
// |    |    |    |    |    |    |    Unused
// |    |    |    |    |    |    Unused
// |    |    |    |    |    scan - Used for button input
// |<====== LEDs =====>| 
// 
// LED columns are on PORTD
// ------------------------
// PD7 PD6 PD5 PD4 PD3 PD2 PD1 PD0
// D07 D06 D05 D04 D03 D02 D01 D00
// |   |   |   |   |   |   |   |
// |   |   |   |   |   |   |   Rx 
// |   |   |   |   |   |   Tx
// |   |   |   |   |   Not used     
// |<=== LEDs ====>|   
//
// RFM69HW - Uses SPI plus an interrupt pin
// ----------------------------------------
// RFM69HW    ARDUINO Pins
// SCK  <---> D13 (PB5/SCK)
// MISO <---> D12 (PB4/MISO) 
// MOSI <---> D11 (PB3/MOSI)
// NSS  <---> D10 (PB2/SS - Slave Select)
// DIO  <---> D2 (PD2/INT0)
// 

#include "font.h"
#include <TimerOne.h>
#include "config.h"

// RFM69 Settings - using lowpowerlab library (see below)
// **********************************************************************************
#include <RFM69.h>         //get it here: https://www.github.com/lowpowerlab/rfm69
#include <RFM69_ATC.h>     //get it here: https://github.com/lowpowerlab/RFM69
#include <SPI.h>           //included with Arduino IDE (www.arduino.cc)
#include <LowPower.h>      //get library from: https://github.com/lowpowerlab/lowpower
#include <TimerOne.h>

//****************************************************************************************************************
//**** IMPORTANT RADIO SETTINGS - YOU MUST CHANGE/CONFIGURE TO MATCH YOUR HARDWARE TRANSCEIVER CONFIGURATION! ****
//****************************************************************************************************************
#define NETWORKID     19  //the same on all nodes that talk to each other.  Using 19 (from 19 Rathbone PLace)
//Match frequency to the hardware version of the radio on your Moteino (uncomment one):
//#define FREQUENCY     RF69_433MHZ
#define FREQUENCY     RF69_868MHZ
//#define FREQUENCY     RF69_915MHZ
// ENCRYPT key should be defined in config.h which is NOT in github
//#define ENCRYPTKEY    "sampleEncryptKey" //exactly the same 16 characters/bytes on all nodes!

#define IS_RFM69HW_HCW  //uncomment only for RFM69HW/HCW! Leave out if you have RFM69W/CW!
//*********************************************************************************************
//#define ENABLE_ATC      //comment out this line to disable AUTO TRANSMISSION CONTROL
#define ATC_RSSI        -75
//*********************************************************************************************

#ifdef ENABLE_ATC
  RFM69_ATC radio;
#else
  RFM69 radio;
#endif

// ***********************************************************************************************
// **** SETTINGS HERE ARE CUSTIMISED FOR EACH DEVICE  ****
// **********************************************************************************************
const byte KUNAL_UID  = 0;   //unique IDs.  Allocated in order of start dates.
const byte KEITH_UID  = 1;
const byte DERYA_UID  = 2;
const byte MERRY_UID  = 3;
const byte NAJEEB_UID = 4;
const byte NODEID     = KEITH_UID;  //change to one of the above UIDs to personalise this board

// Name string to be displayed and it's length. Rememeber to add a space on the end to get
// it to scroll off the screen nicely.
const int STR_LEN = 6;
const char MY_STRING[] = "KEITH ";
// **********************************************************************************************

// **********************************************************************************************
// ***** Name Display Settings ****
// **********************************************************************************************

const int DIM = 5;
int COL_COUNT = 0;
int COL_IDX = 0;
unsigned long LAST_COL_INC = 0;
byte CURRENT_FRAME[5];


// *************************
// **** Common settings ****
// *************************

const long SERIAL_BAUD = 115200;
volatile bool UPDATE_DISPLAY = false;

// Needs to be volatile as ISR is modifying this
volatile byte ROW = 0;
volatile byte BITMASK = 0x01;

// These are the pins we use for the rows and columns
const uint8_t ROWS[] = {A4, A3, A2, A1, A0};
const uint8_t COLS[] = {PD3, PD4, PD5, PD6, PD7};
const uint8_t SCAN_PIN = A5;

unsigned long LAST_TX = 0;
int TX_INTERVAL = 5000;
int RX_TIMEOUT = 10000;
unsigned long RX_TIMES[5] = {10000, 10000, 10000, 10000, 10000};
int RSSI_LEVELS[5] = {-999, -999 , -999 ,-999, -999};
byte RSSI_DATA[5] = {B00001,B00000,B00000,B00000,B00000};

void setup() {
  Serial.begin(SERIAL_BAUD);
  radio.initialize(FREQUENCY, NODEID, NETWORKID);
#ifdef IS_RFM69HW_HCW
  radio.setHighPower(); //must include this only for RFM69HW/HCW!
#endif
  radio.encrypt(ENCRYPTKEY);
  
#ifdef ENABLE_ATC
  radio.enableAutoPower(ATC_RSSI);
#endif

  char buff[50];
  sprintf(buff, "\nListening on %d Mhz...", FREQUENCY==RF69_433MHZ ? 433 : FREQUENCY==RF69_868MHZ ? 868 : 915);
  Serial.println(buff);
  Serial.flush();

  // set the LED pins as outputs
  for (int i=0; i<DIM; i++){
    pinMode(COLS[i], OUTPUT);
    pinMode(ROWS[i], OUTPUT);
  }

  // Set all the rows HIGH (Turn the row off)
  for (int i=0; i<DIM; i++){
    digitalWrite(ROWS[i], HIGH);
  }

  // Illumintate each LED in turn 
  for (int r=0; r<5; r++){
    digitalWrite(ROWS[r], LOW);
    for (int c=0; c<5; c++){
      digitalWrite(COLS[c], HIGH);
      delay(40);
      digitalWrite(COLS[c], LOW);
    }
    digitalWrite(ROWS[r], HIGH);
  }

  // Setup the scan pin
  pinMode(SCAN_PIN, INPUT_PULLUP);

  // Setup the display interrupt
  // We illuminate one LED at a time.  Timer1 defines the on period.
  Timer1.initialize(200);  // microseconds between pixels
  Timer1.attachInterrupt(isr_display_update);

  // Work out how many columns we have in our message by adding up the char size
  // for each char in the string. Chars can be 4,5 or 6 columns wide
  for (int i=0; i<STR_LEN; i++){
    COL_COUNT += get_char_length(MY_STRING[i]);
  }

  buildFrame();
}

// ********************************
// **** Name Display Functions ****
// ********************************

void incrementColumn(){
  // From scrolling text we define each column of pixels as a single step
  // and we increment the column index to walk through the message
  if (++COL_IDX > COL_COUNT - 1){
    COL_IDX = 0;
  }
}

int get_char_idx(){ 
  // For any given column index we need to be able to find the character
  // in the string that the column is part of.  This is because we are
  // using column_idx as our overall index.
  int col_sum = 0;
  for (int c=0; c<STR_LEN; c++){
    col_sum += get_char_length(MY_STRING[c]);
    if (COL_IDX < col_sum){
      return c;
    }
  }
}

int get_char_start(int str_idx){
  // Return the column index for the start of the current character
  int c_start = 0;
  for (int i=0; i<str_idx; i++){
    c_start += get_char_length(MY_STRING[i]);
  }
  return c_start;
}

int get_char_length(int my_char){
  if (my_char == ' '){
    return 5;
  }
  return  pgm_read_byte(letter_len + my_char - 65);
}

byte get_row(int my_char, int row){
  // Get the wanted character for the LED pixel LUT
  // Get the row data for the given row
  // We must use pgm_read_byte to read a byte from an address in program memory
  // The data is stored as an array of arrays so we have to use this macro twice
  if (my_char == ' '){
    return B00000000;
  }
  return pgm_read_byte( pgm_read_byte(&(letters[(int)my_char - 65])) + row );
}

void update_rssi_pixels(){
  // Update the RSSI pixel data based on the recorded RSSI levels
  for (int n=0; n<DIM; n++){
     (RSSI_LEVELS[n] > -40)  ? bitSet(RSSI_DATA[0], n) : bitClear(RSSI_DATA[0], n);
     (RSSI_LEVELS[n] > -60)  ? bitSet(RSSI_DATA[1], n) : bitClear(RSSI_DATA[1], n);
     (RSSI_LEVELS[n] > -80)  ? bitSet(RSSI_DATA[2], n) : bitClear(RSSI_DATA[2], n);
     (RSSI_LEVELS[n] > -100) ? bitSet(RSSI_DATA[3], n) : bitClear(RSSI_DATA[3], n);
     (RSSI_LEVELS[n] > -120) ? bitSet(RSSI_DATA[4], n) : bitClear(RSSI_DATA[4], n);
  }  
}

void buildFrame(){
  // For scrolling text we define each frame as the scrolling frames
  // We increment the frame counter to scroll the text one column to the left
  // In order to build the 5 led row data we use use bit manipulation to
  // 'add' characters together and then shuffle the bits to the correct position
  // for the current frame index.

  // The screen starts blank and we shuffle columns left onto  the screen
  // The index is 'pointing' at the RHS column so we need the previous 4
  // columns plus the current column to build the disaply data.

  int str_idx = get_char_idx();
  int char_len = get_char_length(MY_STRING[str_idx]);

  for (int r=0; r<DIM; r++){
    int col_data = 0;
    // Put previous char in upper bits
    if (str_idx != 0){
      col_data = get_row(MY_STRING[str_idx - 1], r);
      col_data <<= char_len;
    }
  
    // Put current char in lower bits
    col_data |= get_row(MY_STRING[str_idx], r);
  
    // Bit shift to get the screen data into the lower bits
    col_data >>= char_len - COL_IDX + get_char_start(str_idx);
    CURRENT_FRAME[r] = col_data &= 0b11111;
  }
}

void display_data(byte * row_data){
  // Turn the row off
  digitalWrite(ROWS[ROW], HIGH);

  // Build a bitmask for the column data
  // Move the column bitmask one bit left every iteration
  // We are turning on one pixel (row,col) every timer interrupt
  // If we have shown the whole row (all columns) then increment
  // to the next row.  Reset row if we have shown all rows.

  BITMASK = BITMASK << 1;
  if (BITMASK > (0b1 << DIM - 1)) {
    BITMASK = 0x01;
    ROW++;
    if (ROW >= DIM) {
      ROW = 0;
    }
  }

  // Turn off the LED bits and leave others as they are
  PORTD &= B00000111;
  // Use a bitmask to select a single bit then left shift the
  // data because we are using the upper 5 output bits.
  PORTD |= (row_data[ROW] & BITMASK) << 3;
  digitalWrite(ROWS[ROW], LOW); // Turn on the row
}

void loop() {

  // We are in SCAN mode so search for other beebadges and display their RSSI 
  if (!digitalRead(SCAN_PIN)){ 
    // Transmit our beacon
    // This sum handles the millis rollover
    // Difference of unsigned long works for this
    // We Sync the transmit with the update of the column corresponding to 
    // this nodeid - I'm expecting the 
    if (((millis() - LAST_TX) > TX_INTERVAL) ){ //&& (ROW == 0) && (BITMASK == NODEID)) {
      radio.send(RF69_BROADCAST_ADDR, "", 1);
      LAST_TX = millis();
    }
  
    // Reset the saved RSSI for any device that has not been heard recently
    for (int i=0; i<5; i++){
      if ((millis() - RX_TIMES[i]) > RX_TIMEOUT){
        (i == NODEID) ? RSSI_LEVELS[i] = -100 : RSSI_LEVELS[i] = -999;
      } 
    }
  
    if (radio.receiveDone())
    {
        RSSI_LEVELS[radio.SENDERID] = radio.RSSI;
        RX_TIMES[radio.SENDERID] = millis();
        //update_rssi_pixels();
    }
  
    if (UPDATE_DISPLAY){
      update_rssi_pixels();
      UPDATE_DISPLAY = false;
      display_data(RSSI_DATA);
    }
  }

  // We are in Name display mode
  else {
    if ((millis() - LAST_COL_INC) > 200) {
      LAST_COL_INC = millis();
      incrementColumn();
      buildFrame();
//      for (int i=0; i<DIM; i++){
//        Serial.println(CURRENT_FRAME[i],BIN);
//      }
    }
    if (UPDATE_DISPLAY){
      UPDATE_DISPLAY = false;
      display_data(CURRENT_FRAME);
    }
  }
  
}

void isr_display_update(){
  UPDATE_DISPLAY = true;
}
