// **********************************************************************************
#include <RFM69.h>         //get it here: https://www.github.com/lowpowerlab/rfm69
#include <RFM69_ATC.h>     //get it here: https://github.com/lowpowerlab/RFM69
#include <SPI.h>           //included with Arduino IDE (www.arduino.cc)
#include <LowPower.h>      //get library from: https://github.com/lowpowerlab/lowpower

//****************************************************************************************************************
//**** IMPORTANT RADIO SETTINGS - YOU MUST CHANGE/CONFIGURE TO MATCH YOUR HARDWARE TRANSCEIVER CONFIGURATION! ****
//****************************************************************************************************************
#define NETWORKID     19  //the same on all nodes that talk to each other.  Using 19 (from 19 Rathbone PLace)
#define KUNAL_UID     1   //unique ID of the device.  Allocated in order of start dates.
#define KEITH_UID     2
#define DERYA_UID     3
#define MERRY_UID     4
#define NAJIB_UID     5
#define NODEID        KUNAL_UID  //change to one of the above UIDs to personalise this board

//Match frequency to the hardware version of the radio on your Moteino (uncomment one):
//#define FREQUENCY     RF69_433MHZ
#define FREQUENCY     RF69_868MHZ
//#define FREQUENCY     RF69_915MHZ
// ENCRYPT key should be defined in config.h which is NOT in github
//#define ENCRYPTKEY    "sampleEncryptKey" //exactly the same 16 characters/bytes on all nodes!
#define IS_RFM69HW_HCW  //uncomment only for RFM69HW/HCW! Leave out if you have RFM69W/CW!
//*****************************************************************************************************************************
//#define ENABLE_ATC      //comment out this line to disable AUTO TRANSMISSION CONTROL
#define ATC_RSSI        -75
//*********************************************************************************************

#ifdef ENABLE_ATC
  RFM69_ATC radio;
#else
  RFM69 radio;
#endif

#define SERIAL_BAUD   115200
#define LED_COL       3
#define LED_ROW       A4


void setup() {
  Serial.begin(SERIAL_BAUD);
  radio.initialize(FREQUENCY,NODEID,NETWORKID);
#ifdef IS_RFM69HW_HCW
  radio.setHighPower(); //must include this only for RFM69HW/HCW!
#endif
  radio.encrypt(ENCRYPTKEY);
  
#ifdef ENABLE_ATC
  radio.enableAutoPower(ATC_RSSI);
#endif

  char buff[50];
  sprintf(buff, "\nListening at %d Mhz...", FREQUENCY==RF69_433MHZ ? 433 : FREQUENCY==RF69_868MHZ ? 868 : 915);
  Serial.println(buff);
  Serial.flush();

  pinMode(LED_ROW, OUTPUT);
  pinMode(LED_COL, OUTPUT);
  
  digitalWrite(LED_ROW, LOW);
  digitalWrite(LED_COL, LOW);
}

char text[5];
void loop() {
  // put your main code here, to run repeatedly:

 for (int i=0; i<32; i++){
  radio.setPowerLevel(i);
  Serial.println(radio.getPowerLevel());
  sprintf(text, "L=%d", i);
  radio.send(RF69_BROADCAST_ADDR, text, 5);
  Blink(LED_COL, 40, 3);
  delay(1000);
 }

}

void Blink(byte PIN, byte DELAY_MS, byte loops)
{
  for (byte i=0; i<loops; i++)
  {
    digitalWrite(PIN,HIGH);
    delay(DELAY_MS);
    digitalWrite(PIN,LOW);
    delay(DELAY_MS);
  }
}
