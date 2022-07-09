     /*
 * Test using interrupts to write to a 3x3 grid of LEDs
 * 
 * LED Rows are on PORTC
 * ---------------------
 * 
 * PC0  PC1  PC2  PC3  PC4  PC5  PC6  PC7
 * ADC0 ADC1 ADC2 ADC3 ADC4 ADC5 RST  xx
 * |    |    |    |    |    |    |    |
 * |    |    |    |    |    |    |    Unused
 * |    |    |    |    |    |    Unused
 * |    |    |    |    |    scan - Used for button input
 * |<====== LEDs =====>| 
 * 
 * LED columns are on PORTD
 * ------------------------
 * PD7 PD6 PD5 PD4 PD3 PD2 PD1 PD0
 * D07 D06 D05 D04 D03 D02 D01 D00
 * |   |   |   |   |   |   |   |
 * |   |   |   |   |   |   |   Rx 
 * |   |   |   |   |   |   Tx
 * |   |   |   |   |   Not used     
 * |<=== LEDs ====>|   
 *
 * RFM69HW - Uses SPI plus an interrupt pin
 * ----------------------------------------
 * RFM69HW    ARDUINO Pins
 * SCK  <---> D13 (PB5/SCK)
 * MISO <---> D12 (PB4/MISO) 
 * MOSI <---> D11 (PB3/MOSI)
 * NSS  <---> D10 (PB2/SS - Slave Select)
 * DIO  <---> D2 (PD2/INT0)
 * 
 */

#define __AVR_ATmega328P__
#include <Arduino.h>
#include "font.h"
#include <TimerOne.h>


// Number of rows/columns of LEDs
const int DIM = 5;

// Needs to be volatile as ISR is modifying this
volatile byte ROW = 0;

// These are the pins we use for the rows and columns
const uint8_t ROWS[] = {A4, A3, A2, A1, A0};
const uint8_t COLS[] = {PD3, PD4, PD5, PD6, PD7};

// Put the string here that you want to display.
// Add a space in the end to make sure text scrolls completely off LHS of screen
const int STR_LEN = 6;
const char MY_STRING[] = "KUNAL ";
int COL_COUNT = 0;
int COL_IDX = 0;


void setup() {

  Serial.begin(115200);
  while (!Serial);
  Serial.println("Starting...");
  
  // set the LED pins as outputs
  for (int i=0; i<DIM; i++){
    pinMode(COLS[i], OUTPUT);
    pinMode(ROWS[i], OUTPUT);
  }

  // Blank out all LEDs
  // Set the columns LOW
  for (int i=0; i<DIM; i++){
    digitalWrite(COLS[i], LOW);
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

  // Setupt the interrupt
  Timer1.initialize(600);  // microseconds
  Timer1.attachInterrupt(display);

  // Work out how many columns we have in our message by adding up the char size
  // for each char in the string. Chars can be 4,5 or 6 columns wide
  for (int i=0; i<STR_LEN; i++){
    COL_COUNT += get_char_length(MY_STRING[i]);
  }

}

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

int buildFrame_new(){
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
  int col_data = 0;

  // Put previous char in upper bits
  if (str_idx != 0){
    col_data = get_row(MY_STRING[str_idx - 1], ROW);
    col_data <<= char_len;
  }

  // Put current char in lower bits
  col_data |= get_row(MY_STRING[str_idx], ROW);

  // Bit shift to get the screen data into the lower bits
  col_data >>= char_len - COL_IDX + get_char_start(str_idx);

  return col_data & 0b11111;
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

void loop() {
  delay(200); // wait time between frames milliseconds
  incrementColumn();
}

// ISR Routine
byte bitmask = 0x01;
byte row_data;
void display(){
  // Turn the row off
  digitalWrite(ROWS[ROW], HIGH);

  // Build a bitmask for the column data
  // Move the column bitmask one bit left every iteration
  // We are turning on one pixel (row,col) every timer interrupt
  // If we have shown the whole row (all columns) then increment
  // to the next row.  Reset row if we have shown all rows.
  bitmask = bitmask << 1;
  int row_size = get_char_length(MY_STRING[get_char_idx()]);
  
  if (bitmask > (0b1 << row_size)) {
    bitmask = 0x01;
    ROW++;
    if (ROW >= DIM) {
      ROW=0;
    }
  }

  // Turn off the LED bits and leave others as they are
  PORTD &= B00000111;
  // Use a bitmask to select a single bit then left shift the
  // data because we are using the upper 5 output bits.
  row_data = buildFrame_new();
  PORTD |= (row_data & bitmask) << 3;
  digitalWrite(ROWS[ROW], LOW); // Turn on the row
}
