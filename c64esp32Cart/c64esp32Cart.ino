/*
 * Inspired on  chat64 cart by Bart Venneker and Theo van den Beld, same hardware with some midifications:
 * - SD card
 * - 27C512 EEPROM
 * - add jumpers to select 8k memory inside EEPROM
 * - add transistor to control EXROM line
 * - standart or ultiMax mode jumper
 * 
 * About the skech:
 * The sketck is a working progress, there are many bugs to fix, expecialy SD directory code.
 * Not all SIDs file are playable, same as PRG file.
 * Feaures:
 * - Run PRG file
 * - play SID files and show SID info
 * - show koala images
 * - show current time
 * - show current weather
 * - show news by using 2x2 charset
 * 
 * Any help is appreciate,
 * Have fun
 * 
 * M.Angelini
 *
*/
#include <WiFi.h>
#include <NTPClient.h>
#include <HTTPClient.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>

#include <SPI.h>
#include <SD.h>
//#include "BluetoothSerial.h"
#include "supermon.h"
#include "chars.h"
//#include "SID.h"


const char *ssid     = "xxxx";
const char *password = "xxxx";

//forecast API http://api.openweathermap.org

String URL = "http://api.openweathermap.org/data/2.5/weather?";
String openweatherApiKey = "xxxx";
// Replace with your location Credentials
String lat = "43.06071239857755";
String lon = "10.614730891178944";

//news API: https://newsapi.org

String newsApiKey = "xxxxx";

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

File directory;
File myFile;
File root;

//BluetoothSerial SerialBT;

// ********************************
// **     Global Variables       **
// ********************************

bool invert_Reset = true;  


String weekDays[7]={"Sunday", "monday", "tuesday", "wednesday", "thursday", "friday", "saturday"};
String months[12]={"january", "february", "march", "april", "may", "june", "july", "august", "september", "october", "november", "december"};
              

volatile bool dataFromC64 = false;
volatile bool io2 = false;
unsigned long outbuffersize = 0;

volatile byte ch = 0;
int num;

char inbuffer[250];  // a character buffer for incomming data
int inbuffersize = 0;
char *outbuffer;

String currentDate;
String weatherMain = "";
String weatherDescription = "";
String weatherLocation = "";
String country;
int humidity;
int pressure;
float temp;
float tempMin, tempMax;
int clouds;
float windSpeed;
String weatherString = "";

uint32_t SID_data_size = 0;
uint16_t MagicID = 0;
uint16_t VERSION = 0;
bool PLAYABLE_SID = false;

uint32_t IRQ_TYPE_PER_TUNE = 0; // subtune speed info
uint32_t VIDEO_TYPE = 0; // pal or ntsc
uint32_t MODEL_TYPE = 0; //MOS6581 or MOS8580
uint32_t FLAGS76 = 0;
uint8_t ComputeMUSplayer = 0;
uint8_t C64Compatible = 0;
uint16_t LOAD_ADDRESS = 0;
uint32_t SUBTUNE_SPEED = 0;
char info_string_SID [300];

uint16_t SID_load_start = 0; //SID_data[0x7c] + (SID_data[0x7d] * 256); // get start address from .sid file
uint16_t SID_load_end = 0; //SID_start + SID_data_size - 0x7e ; // end address of music routine , not included "busy blocks", aka, ram needed after end of actual sid file.

uint16_t SID_play = 0;//SID_data[13] + (SID_data[12] * 256); // sid play address
uint16_t SID_init = 0;//SID_data[11] + (SID_data[10] * 256); // sid init address

uint8_t SID_default_tune = 0;//SID_data[17] + (SID_data[16] * 256); // default song to be played first
uint8_t SID_number_of_tunes = 0;//SID_data[15] + (SID_data[14] * 256); // number of tunes in sid
uint8_t SID_current_tune = 0;//SID_default_tune;

char SID_filename[128] ; // reserve 128 bytes for file name
char SID_DIR_name [128] ; // reserve 128 bytes for directory path name
char SID_path_filename [256] ; // reserve 256 bytes for full path of file
bool IS_SID_FILE = false; //  for extension check

char  SIDinfo_filetype [8] ;
char  SIDinfo_name [33] ;
char  SIDinfo_author [33] ;
char  SIDinfo_released [33] ;
char  SIDinfo_VIDEO [14] ;
char  SIDinfo_CLOCK [7] ;
char  SIDinfo_PLAYABLE [6];
char  SIDinfo_MODEL [20];
char  SIDinfo_PLAYLIST [10];
char  SIDinfo_RANDOM [4];
char  SIDheader [0x7e];

String hex_value = "";

String response = "";
char title [2048];
String currentDir = "";
String parentDir = "";

// ********************************
// **        OUTPUTS             **
// ********************************
// see http://www.bartvenneker.nl/index.php?art=0030
// for usable io pins!
#define oC64D0 GPIO_NUM_4   // data bit 0 for data from the ESP32 to the C64
#define oC64D1 GPIO_NUM_33  // data bit 1 for data from the ESP32 to the C64
#define oC64D2 GPIO_NUM_14  // data bit 2 for data from the ESP32 to the C64
#define oC64D3 GPIO_NUM_27  // data bit 3 for data from the ESP32 to the C64
#define oC64D4 GPIO_NUM_13  // data bit 4 for data from the ESP32 to the C64
#define oC64D5 GPIO_NUM_22  // data bit 5 for data from the ESP32 to the C64
#define oC64D6 GPIO_NUM_17  // data bit 6 for data from the ESP32 to the C64
#define oC64D7 GPIO_NUM_26  // data bit 7 for data from the ESP32 to the C64

#define oC64RST GPIO_NUM_21  // reset signal to C64
#define oC64NMI GPIO_NUM_32  // non-maskable interrupt signal to C64
 #define CLED GPIO_NUM_2      // led on ESP32 module
#define sclk GPIO_NUM_25     // serial clock signal to the shift register
#define pload GPIO_NUM_16    // parallel load signal to the shift register

// ********************************
// **        INPUTS             **
// ********************************
#define resetSwitch GPIO_NUM_15  // this pin outputs PWM signal at boot
#define C64IO1 GPIO_NUM_35
#define sdata GPIO_NUM_34
#define C64IO2 GPIO_NUM_36

// ********************************
// **        SD connections      **
// ********************************
/*
 
  MOSI - pin 23
  MISO - pin 19
  SCK - pin 18
  CS - GPIO_NUM_5 
*/

#define CS GPIO_NUM_5

// *************************************************
// Interrupt routine for IO1
// *************************************************
void IRAM_ATTR isr_io1() {

  // This signal goes LOW when the commodore writes to (or reads from) the IO1 address space
  // In our case the Commodore 64 only WRITES the IO1 address space, so ESP32 can read the data.
  digitalWrite(oC64D7, LOW);  // this pin is used for flow controll,
                              // make it low so the C64 will not send the next byte
                              // until we are ready for it
  ch = 0;
  digitalWrite(pload, HIGH);  // stop loading parallel data and enable shifting serial data
  ch = shiftIn(sdata, sclk, MSBFIRST);
  dataFromC64 = true;
  digitalWrite(pload, LOW);
}

// *************************************************
// Interrupt routine for IO2
// *************************************************
void IRAM_ATTR isr_io2() {
  // This signal goes LOW when the commodore reads from (or write to) the IO2 address space
  // In this case the commodore only uses the IO2 address space to read from, so ESP32 can send data.
  io2 = true;
}

// *************************************************
// Interrupt routine, to restart the esp32
// *************************************************
void IRAM_ATTR isr_reset() {
  ESP.restart();
}

// ******************************************************************************
// send a single byte to the C64
// ******************************************************************************
void sendByte(byte b) {
  outByte(b);
  io2 = false;
  triggerNMI();
  // wait for io2 interupt
  while (io2 == false) {
    delayMicroseconds(10); //default 2
  }
  io2 = false;
}

// ******************************************************************************
// void to set a byte in the 74ls244 buffer
// ******************************************************************************
void outByte(byte c) {
  gpio_set_level(oC64D0, bool(c & B00000001));
  gpio_set_level(oC64D1, bool(c & B00000010));
  gpio_set_level(oC64D2, bool(c & B00000100));
  gpio_set_level(oC64D3, bool(c & B00001000));
  gpio_set_level(oC64D4, bool(c & B00010000));
  gpio_set_level(oC64D5, bool(c & B00100000));
  gpio_set_level(oC64D6, bool(c & B01000000));
  gpio_set_level(oC64D7, bool(c & B10000000));
}

// ******************************************************************************
// pull the NMI line low for a few microseconds
// ******************************************************************************
void triggerNMI() {
  digitalWrite(oC64NMI, HIGH);
  delayMicroseconds(150);  // minimal 100 microseconds delay default 125
  digitalWrite(oC64NMI, LOW);
}


void writeDirectoryFile (File folder) {

   
  directory = SD.open("/directory.txt", FILE_WRITE);

  if (folder) {
    directory.println("/..");
    
  while (true) {
    File entry =  folder.openNextFile();
    if (! entry) {
      // no more files
      entry.close();
      break;
    }

      
    if (entry.isDirectory()) {
        directory.print("/");
        directory.println(entry.name());
        
        
      }
    else {  
		directory.println(entry.name());
    }		
    }
    
 
  directory.close();
  
  }
  else {
    Serial.println("error opening folder");  
}

myFile = SD.open("/directory.txt");

  if (myFile) {

    Serial.println("directory:");
    while (myFile.available()) {
    Serial.write(myFile.read());
    }
    myFile.close();

  } else {
    Serial.println("error opening directory.txt file");
  }
}

void getTime(){
  timeClient.begin();
  // Set offset time in seconds to adjust for your timezone, for example:
  // GMT +1 = 3600
  // GMT +8 = 28800
  // GMT -1 = -3600
  // GMT 0 = 0
  timeClient.setTimeOffset(3600);

  timeClient.update();

  time_t epochTime = timeClient.getEpochTime();
  String formattedTime = timeClient.getFormattedTime();
 
  int currentHour = timeClient.getHours();
  int currentMinute = timeClient.getMinutes();
  int currentSecond = timeClient.getSeconds();
  String weekDay = weekDays[timeClient.getDay()];
     
  //Get a time structure
  struct tm *ptm = gmtime ((time_t *)&epochTime); 
  int monthDay = ptm->tm_mday;
  int currentMonth = ptm->tm_mon+1;
  String currentMonthName = months[currentMonth-1];
  int currentYear = ptm->tm_year+1900;
  
  //Print complete date:
  currentDate = "Time: " + formattedTime + "\r\n" + "Date: " + weekDay + " " + String(monthDay) + " " + currentMonthName + " " + String(currentYear)+ "\r\n";
  }


void getWeatherData(){
    HTTPClient http;

    //Set HTTP Request Final URL with Location and API key information
    http.begin(URL + "lat=" + lat + "&lon=" + lon + "&units=metric&appid=" + openweatherApiKey);

    // start connection and send HTTP Request
    int httpCode = http.GET();

    // httpCode will be negative on error
    if (httpCode > 0) {

      //Read Data as a JSON string
      String JSON_Data = http.getString();
      //Serial.println(JSON_Data);

      //Retrieve some information about the weather from the JSON format
      DynamicJsonDocument doc(2048);
      deserializeJson(doc, JSON_Data);
      JsonObject obj = doc.as<JsonObject>();
/*
      //Display the Current Weather Info
      const char* description = obj["weather"][0]["description"].as<const char*>();
      const float temp = obj["main"]["temp"].as<float>();
      const float humidity = obj["main"]["humidity"].as<float>();
*/
      //weatherMain = obj["weather"]["main"].as<String>();
      weatherDescription = obj["weather"][0]["description"].as<String>();
      //weatherDescription.toLowerCase();
      weatherLocation = obj["name"].as<String>();
      //weatherLocation.toLowerCase();
      country = obj["sys"]["country"].as<String>();
      //country.toLowerCase();
      temp = obj["main"]["temp"];
      humidity = obj["main"]["humidity"];
      pressure = obj["main"]["pressure"];
      tempMin = obj["main"]["temp_min"];
      tempMax = obj["main"]["temp_max"];
      windSpeed = obj["wind"]["speed"];
      clouds = obj["Clouds"]["all"];
      weatherString = "Location: " + weatherLocation + "," + country + "\r\n";
      weatherString += "Description: " + weatherDescription + "\r\n";
      weatherString += "Temp: " + String(temp,1) + "c (" + String(tempMin,1) + "-" + String(tempMax,1) + ")  "+ "\r\n" ;
      weatherString += "Humidity: " + String(humidity) + "%  "+ "\r\n";
      weatherString += "Pressure: " + String(pressure) + " hpa  "+ "\r\n";
      weatherString += "Clouds: " + String(clouds) + "%  "+ "\r\n";
      weatherString += "Wind: " + String(windSpeed,1) + " m/s"+ "\r\n";
      
      //Serial.print(weatherString);

    } else {
      Serial.println("Error!");
      Serial.print("Can't Get DATA!");
    }

    http.end();
}

// ******************************************************************************
// translate screen codes to ascii
// ******************************************************************************
char screenCode_to_Ascii(byte screenCode) {

  byte screentoascii[] = { 64, 97, 98, 99, 100, 101, 102, 103, 104, 105,
                           106, 107, 108, 109, 110, 111, 112, 113, 114, 115,
                           116, 117, 118, 119, 120, 121, 122, 91, 92, 93,
                           94, 95, 32, 33, 34, 125, 36, 37, 38, 39,
                           40, 41, 42, 43, 44, 45, 46, 47, 48, 49,
                           50, 51, 52, 53, 54, 55, 56, 57, 58, 59,
                           60, 61, 62, 63, 95, 65, 66, 67, 68, 69,
                           70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
                           80, 81, 82, 83, 84, 85, 86, 87, 88, 89,
                           90, 43, 32, 124, 32, 32, 32, 32, 32, 32,
                           95, 32, 32, 32, 32, 32, 32, 32, 32, 32,
                           32, 95, 32, 32, 32, 32, 32, 32, 32, 32,
                           32, 32, 32, 32, 32, 32, 32, 32, 32 };

  char result = char(screenCode);
  if (screenCode < 129) result = char(screentoascii[screenCode]);
  return result;
}

// ******************************************************************************
// translate ascii to c64 screen codes
// ******************************************************************************
byte Ascii_to_screenCode(char ascii) {

  byte asciitoscreen[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
                           11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
                           22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32,
                           33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43,
                           44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54,
                           55, 56, 57, 58, 59, 60, 61, 62, 63, 0, 65,
                           66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76,
                           77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87,
                           88, 89, 90, 27, 92, 29, 30, 100, 39, 1, 2,
                           3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13,
                           14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
                           25, 26, 27, 93, 35, 64, 32, 32 };
  byte result = ascii;
  if (int(ascii) < 129) result = byte(asciitoscreen[int(ascii)]);
  return result;
}


// ******************************************************************************
// translate ascii to PETascii
// ******************************************************************************
char asciiToPET (char ascii) {
  if (ascii > 64 && ascii < 91) ascii += 128;
  if (ascii > 96 && ascii < 123) ascii -= 32;
  return ascii;
}

bool Compatibility_check() {
  
  //SID_data_size = sizeof(SIDheader);

  MagicID = SIDheader[0] ;
  VERSION =  SIDheader[0x05];
 

  LOAD_ADDRESS = 0 ;
  SID_load_start = (SIDheader[0x08] * 256) + SIDheader[0x09];

  if (SID_load_start == 0) {
    SID_load_start = SIDheader[0x7c] + SIDheader[0x7d] * 256;
  }
  if ( SID_load_start >= 0x07E8)  {  
    LOAD_ADDRESS = SID_load_start;
  }

  SID_load_end = SID_load_start + SID_data_size - 0x7e ;
  

  SID_play =  SIDheader[0x0d] + SIDheader[0x0c] * 256;
  SID_init = SIDheader[0x0b] + SIDheader[0x0a] * 256;
  SID_number_of_tunes =  SIDheader[0x0f] + SIDheader[0x0e] * 256;
  SID_default_tune =  SIDheader[0x11] + SIDheader[0x10] * 256;
  SID_current_tune =  SID_default_tune;



  // $12-$15 - 32bit big endian bits
  // 0 - VBI , 1 - CIA
  // for tunes > 32 , it's tune&0x1f
  // it is just indication what type of interrupts is used: VIC or CIA, and only default values are used.
  // Multispeed tune's  code set it's own VIC/CIA values. Must emulate VIC and CIA to be able to play(detect) multi speed tunes.
  IRQ_TYPE_PER_TUNE = (      ( SIDheader[0x15] )
                             |      ( SIDheader[0x14] << 8 )
                             |                          ( SIDheader[0x13] << 16 )
                             |                          ( SIDheader[0x12] << 24 )
                      );



  FLAGS76 = ( ( SIDheader[0x76] << 8 ) | SIDheader[0x77] ); // 16bit big endian number

  ComputeMUSplayer = FLAGS76 & 0x01;                        // bit0 - if set, not playable

  C64Compatible = (FLAGS76 >> 1) & 0x01;                   // bit1 - is PlaySID specific, e.g. uses PlaySID samples (psidSpecific):
  //                                                          0 = C64 compatible,                            // playable
  //                                                          1 = PlaySID specific (PSID v2NG, v3, v4)       // not playable
  //                                                          1 = C64 BASIC flag (RSID)                      // not playable

  VIDEO_TYPE =  (FLAGS76 >> 2) & 0x03 ;                       // 2bit value
  //  00 = Unknown,
  //  01 = PAL,
  //  10 = NTSC,
  //  11 = PAL and NTSC.
  // used in combination with SUBTUNE_SPEED to set speed per tune


 

  MODEL_TYPE =  (FLAGS76 >> 4) & 0x03 ;                       // SID Model
  // 00 = Unknown,
  // 01 = MOS6581,
  // 10 = MOS8580,
  // 11 = MOS6581 and MOS8580.

  
  if (
    (MagicID != 0x50)  
    | (VERSION < 2)
    | (SID_play == 0)
    | (ComputeMUSplayer)
    | (C64Compatible) )
  { // play tune if no errors
    PLAYABLE_SID = false;

  }
  else {
    PLAYABLE_SID = true;

  }
  return PLAYABLE_SID;
}

void SerialPrintHEX (const int32_t output) {

  Serial.print("$");
  Serial.print(output, HEX);

}

void  infoSID() {

  // - PSID (0x50534944)
  // - RSID (0x52534944)

  strcpy (SIDinfo_filetype, "UNKNOWN"); // if PSID/RSID check fail

  if ( (SIDheader[0] == 0x50) & (SIDheader[1]== 0x53) & (SIDheader[2] == 0x49) & (SIDheader[3] == 0x44) ) {
    strcpy (SIDinfo_filetype, "PSID");
  }

  if ( (SIDheader[0] == 0x52) & (SIDheader[1] == 0x53) & (SIDheader[2] == 0x49) & (SIDheader[3] == 0x44) ) {
    strcpy (SIDinfo_filetype, "RSID");
  }

  strcpy (SIDinfo_name, "");
  for (int cc = 0; cc < 0x20; cc++) {
    SIDinfo_name[cc] = (SIDheader[0x16 + cc]);
    if (cc == 0x1f) {
      SIDinfo_name[0x20] = 0; // null terminating string
    }
  }
  strcpy (SIDinfo_author, "");
  for (int cc = 0; cc < 0x20; cc++) {
    SIDinfo_author[cc] = (SIDheader[0x36 + cc]);
    if (cc == 0x1f) {
      SIDinfo_author[0x20] = 0; // null terminating string
    }
  }
  strcpy (SIDinfo_released, "");
  for (int cc = 0; cc < 0x20; cc++) {
    SIDinfo_released[cc] = (SIDheader[0x56 + cc]);
    if (cc == 0x1f) {
      SIDinfo_released[0x20] = 0; // null terminating string
    }
  }

  switch (VIDEO_TYPE) {
    case 0:
      strcpy (SIDinfo_VIDEO, "UNKNOWN");
      break;
    case 1:
      strcpy (SIDinfo_VIDEO, "PAL");
      break;
    case 2:
      strcpy (SIDinfo_VIDEO, "NTSC");
      break;
    case 3:
      strcpy (SIDinfo_VIDEO, "PAL and NTSC");
      break;

  }
  switch (SUBTUNE_SPEED) {
    case 0:
      strcpy (SIDinfo_CLOCK, "VIC II");
      break;
    case 1:
      strcpy (SIDinfo_CLOCK, "CIA");
      break;
  }

  if (PLAYABLE_SID) {
    strcpy (SIDinfo_PLAYABLE, "OK");
  } else {
    strcpy (SIDinfo_PLAYABLE, "ERROR");
  }

  switch (MODEL_TYPE) {
    case 0:
      strcpy (SIDinfo_MODEL, "UNKNOWN");
      break;
    case 1:
      strcpy (SIDinfo_MODEL, "MOS6581");
      break;
    case 2:
      strcpy (SIDinfo_MODEL, "MOS8580");
      break;
    case 3:
      strcpy (SIDinfo_MODEL, "MOS6581 and MOS8580");
      break;
  }


  Serial.println ("");
  Serial.println ("--------------------------------------------------------------------");
  Serial.print   ("Name:      "); Serial.print (SIDinfo_name); Serial.println ("");
  Serial.print   ("Author:    "); Serial.print (SIDinfo_author); Serial.println ("");
  Serial.print   ("Released:  "); Serial.print (SIDinfo_released); Serial.println ("");
  Serial.println ("--------------------------------------------------------------------");
  Serial.print   ("Size:      "); Serial.print(SID_data_size); Serial.print(" bytes"); Serial.println (" ");
  Serial.print   ("Type:      "); Serial.print (SIDinfo_filetype); Serial.println ("");
  Serial.print   ("Range:     "); SerialPrintHEX(SID_load_start); Serial.print(" - "); SerialPrintHEX(SID_load_end); Serial.println ("");
  Serial.print   ("Init:      "); SerialPrintHEX(SID_init); Serial.println ("");
  Serial.print   ("Play:      "); SerialPrintHEX(SID_play); Serial.println ("");
  Serial.print   ("Video:     "); Serial.print   (SIDinfo_VIDEO); Serial.println ("");
  Serial.print   ("Clock:     "); Serial.print   (SIDinfo_CLOCK);Serial.println ("");
  Serial.print   ("SID model: "); Serial.print   (SIDinfo_MODEL); Serial.println ("");
  Serial.println ("--------------------------------------------------------------------");



  strcpy(info_string_SID,"");
  strcat(info_string_SID, "Name: ");
  strcat(info_string_SID, SIDinfo_name);
  strcat(info_string_SID, "\r\n");
  strcat(info_string_SID, "Author: ");
  strcat(info_string_SID, SIDinfo_author);
  strcat(info_string_SID, "\r\n");
  strcat(info_string_SID, "Released: ");
  strcat(info_string_SID, SIDinfo_released);
  strcat(info_string_SID, "\r\n");
  strcat(info_string_SID, "Size: ");
  hex_value = String(SID_data_size);
  strcat(info_string_SID,hex_value.c_str());
  strcat(info_string_SID, " bytes");
  strcat(info_string_SID, "\r\n");
  strcat(info_string_SID, "Type: ");
  strcat(info_string_SID, SIDinfo_filetype);
  strcat(info_string_SID, "\r\n");
  strcat(info_string_SID, "Range: ");
  hex_value = String(SID_load_start, HEX);
  strcat(info_string_SID,hex_value.c_str());
  strcat(info_string_SID, " - ");
  hex_value = String(SID_load_end, HEX);
  strcat(info_string_SID,hex_value.c_str());
  strcat(info_string_SID, "\r\n");
  strcat(info_string_SID,"Init: ");
  hex_value = String(SID_init, HEX);
  strcat(info_string_SID,hex_value.c_str());
  strcat(info_string_SID, "\r\n");
  strcat(info_string_SID, "Play: ");
  hex_value = String(SID_play, HEX);
  strcat(info_string_SID,hex_value.c_str());
  strcat(info_string_SID, "\r\n");
  strcat(info_string_SID, "Video: ");
  strcat(info_string_SID, SIDinfo_VIDEO);
  strcat(info_string_SID, "\r\n");
  strcat(info_string_SID, "Clock: ");
  strcat(info_string_SID, SIDinfo_CLOCK);
  strcat(info_string_SID, "\r\n");
  strcat(info_string_SID, "SID model: ");
  strcat(info_string_SID, SIDinfo_MODEL);
  strcat(info_string_SID, "\0");

}

// *************************************************
//  SETUP
// *************************************************
void setup() {
 
  
  // define inputs
  pinMode(sdata, INPUT);
  pinMode(C64IO1, INPUT_PULLDOWN);
  pinMode(C64IO2, INPUT_PULLUP);
  pinMode(resetSwitch, INPUT_PULLUP);

  // define interrupts
  attachInterrupt(C64IO1, isr_io1, RISING);          // interrupt for io1, C64 writes data to io1 address space
  attachInterrupt(C64IO2, isr_io2, FALLING);         // interrupt for io2, c64 reads
  attachInterrupt(resetSwitch, isr_reset, FALLING);  // interrupt for reset button

  // define outputs
  pinMode(CLED, OUTPUT);
  digitalWrite(CLED, HIGH);
  //digitalWrite(CLED, LOW);
  pinMode(oC64D0, OUTPUT);
  pinMode(oC64D1, OUTPUT);
  pinMode(oC64D2, OUTPUT);
  pinMode(oC64D3, OUTPUT);
  pinMode(oC64D4, OUTPUT);
  pinMode(oC64D5, OUTPUT);
  pinMode(oC64D6, OUTPUT);
  pinMode(oC64D7, OUTPUT);
  digitalWrite(oC64D7, LOW);
  pinMode(oC64RST, OUTPUT);
  digitalWrite(oC64RST, invert_Reset);
  pinMode(oC64NMI, OUTPUT);
  digitalWrite(oC64NMI, LOW);
  pinMode(pload, OUTPUT);
  digitalWrite(pload, LOW);  // must be low to load parallel data
  pinMode(sclk, OUTPUT);
  digitalWrite(sclk, LOW);  //data shifts to serial data output on the transition from low to high.


  // Reset the C64
  digitalWrite(oC64RST, !invert_Reset);
  delay(250);
  digitalWrite(oC64RST, invert_Reset);

  Serial.begin(115200);
  delay(500);
  
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    //Serial.print(".");
  }

  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  //if (SerialBT.begin("ESP32Cart"))digitalWrite(CLED, HIGH);         // start serial bluetooth

  if (!SD.begin(CS)) {

    Serial.println("SD begin failed!");
    
  }

  currentDir = parentDir = "/";
  root = SD.open(currentDir);
  writeDirectoryFile (root);
  root.close();
  delay(500);
  digitalWrite(CLED, LOW);
  Serial.println("");
  Serial.println("done!");

}  // end of setup


// ******************************************************************************
// Main loop
// ******************************************************************************
void loop() {

    gpio_set_level(oC64D7, HIGH);
    
    if (dataFromC64){
    dataFromC64 = false;
    gpio_set_level(oC64D7, LOW);  // flow control

    // 49 = read directory from SD
    // 50 = load supermon+
    // 51 = receive string in inbuffer[i]
    // 52 = Get time & weather
    // 53 = file from ESP32 to c64
    

    switch (ch) { 
      //========================================================
      case 49:{           // 49 = read directory from SD

             File dataFile = SD.open("/directory.txt");
        
      if (dataFile) {
      outbuffersize = dataFile.size();
      sendByte((byte)(outbuffersize));
      sendByte((byte)(outbuffersize>>8)); 
      sendByte(0x00);
      sendByte(0x10);
 
     for (int x = 0; x < outbuffersize; x++) {
          sendByte(asciiToPET(dataFile.read()));}
    dataFile.close();}
    
     else {
     Serial.println("error opening directory.txt");}
     break;}
     
   //========================================================         
      case 50:{         // 50 = load supermon+
          

          outbuffersize = 2820; 
          sendByte((byte)(outbuffersize));
          sendByte((byte)(outbuffersize>>8));
          for (int x = 0; x < outbuffersize; x++) {
          sendByte(supermonA000[x]);}
          
      break;}
   
      
    //========================================================
      case 51:{         // 51 = receive string in inbuffer[i] 
   
  int i = 0;

  while (ch != 128) {
    gpio_set_level(oC64D7, HIGH);  // ready for next byte
    while (dataFromC64 == false) {
      delayMicroseconds(2);  // wait for next byte
    }
    gpio_set_level(oC64D7, LOW);  //
    dataFromC64 = false;
    inbuffer[i] = screenCode_to_Ascii(ch);
    i++;
  }
  i--;
  inbuffer[i] = 0;  // close the buffer
  inbuffersize = i;

  if (strcmp(&inbuffer[strlen(inbuffer) - 4], ".SID") == 0) {
    IS_SID_FILE = true;
  }
   if (strcmp(&inbuffer[strlen(inbuffer) - 4], ".sid") == 0) {
    IS_SID_FILE = true;
  }
  else IS_SID_FILE = false;
  if (IS_SID_FILE) Serial.println("SID file ok");
   else Serial.println("Not SID file");
  // char bns[inbuffersize];
  // strncpy ( bns, inbuffer, inbuffersize + 1);
  String text_lowcase = inbuffer;
  //text_lowcase.toLowerCase();
  Serial.println(text_lowcase);
  
     break;}
     
     //========================================================
     case 52:{   //52 = Get time & weather

    getTime();
    getWeatherData();
    currentDate += weatherString;

  Serial.println(currentDate);
   
    outbuffersize = currentDate.length();
  
    sendByte((byte)(outbuffersize));
    sendByte((byte)(outbuffersize>>8));
    sendByte(0x00);
    sendByte(0x10); 
    
    for (int x = 0; x < outbuffersize; x++) {
          sendByte(asciiToPET(currentDate[x]));}
    
     break;}

    //========================================================
      case 53:{          // 53 = file from ESP32 to c64
      
      String fileName = currentDir;
      bool result = strcmp(inbuffer,"/..");
      
    if (result == 0){
      currentDir = parentDir; 
      File dir;
       
     dir = SD.open(currentDir);
         writeDirectoryFile (dir);

         
         dir.close(); 
    }

    
	  else if (inbuffer[0] == '/'){

     currentDir = inbuffer;
     
		 File dir;
       
		 dir = SD.open(currentDir);
         writeDirectoryFile (dir);

         
         dir.close(); 
	  }
	  
	  else {
        fileName += "/";
        fileName += inbuffer;
		  
        File dataFile = SD.open(fileName);
  
  
          
  if (dataFile) {
    
  
    if (IS_SID_FILE == false){
    outbuffersize = dataFile.size();
    sendByte((byte)(outbuffersize));
    sendByte((byte)(outbuffersize>>8)); 
    
    for (int x = 0; x < outbuffersize; x++) {
          sendByte(dataFile.read());}
    dataFile.close();
    Serial.print("Load: ");
    Serial.println(inbuffer);}

    else{
    for (int i = 0; i < 0x7c; i++) {
              SIDheader[i] = dataFile.read();
          }

    SID_data_size = dataFile.size() - 0x7c;
    outbuffersize = SID_data_size;

    
    outbuffer = (char*)malloc(outbuffersize);
    
    if (outbuffer == NULL) {
      Serial.println("errore memoria!");
    }

    for (int i = 0; i < outbuffersize; i++) {
              outbuffer[i] = dataFile.read();
          }

 
            PLAYABLE_SID = Compatibility_check();
            infoSID();
            
          sendByte((byte)(5));
          sendByte((byte)(0)); 
          sendByte(0x3c);    //cassette_buffer    
          sendByte(0x03);

          sendByte((byte)(IS_SID_FILE));
          sendByte((byte)(SID_init));
          sendByte((byte)(SID_init>>8));
          sendByte((byte)(SID_play));
          sendByte((byte)(SID_play>>8));

          
          
          dataFile.close();       
  }
  }
  
    
  else {
    Serial.print("error opening ");
    Serial.println(fileName);}
  
	  }
      break;}

   //========================================================
     case 55:{   //55 = send SID file to c64
    
   
    
    sendByte((byte)(SID_data_size));
    sendByte((byte)(SID_data_size>>8)); 

    
    for (int x = 0; x < SID_data_size; x++) {
          sendByte(outbuffer[x]);}
          
    free(outbuffer);
   
     break;}
    //========================================================
     case 56:{   //56 = print SID info

    outbuffersize = strlen(info_string_SID);
  
    sendByte((byte)(outbuffersize));
    sendByte((byte)(outbuffersize>>8));
    sendByte(0x00);
    sendByte(0x10);

    String text_lowcase = info_string_SID;
    //text_lowcase.toLowerCase();
    
    for (int x = 0; x < outbuffersize; x++) {
          sendByte(asciiToPET(text_lowcase[x]));}
    
     break;}

     //========================================================         
      case 57:{         // 57 = load charset 1x2
          

          outbuffersize = 2048; 
          sendByte((byte)(outbuffersize));
          sendByte((byte)(outbuffersize>>8));
          sendByte(0x00);
          sendByte(0x38);
          for (int x = 0; x < outbuffersize; x++) {
          sendByte(chars[x]);}
          
      break;}

      //======================================================== 
      case 58:{       // 57 = get NEWS
        
      HTTPClient http;
      http.begin("https://newsapi.org/v2/top-headlines?country=it&apiKey=" + newsApiKey);
      int httpCode = http.GET();
      response = http.getString();

      // Parse the response to get the headlines
     DynamicJsonDocument doc(19508);
     //DeserializationError error = deserializeJson(doc, payload);
     DeserializationError error = deserializeJson(doc, response);
     if (error) {
     Serial.println("Error parsing news data");
     return;
     }

     // Extract the "title" field from the first article
    JsonArray articles = doc["articles"];

    //String title = articles[0]["title"];
    // Display the title of each article

    strcpy(title, "");
    for (int i = 0; i < articles.size(); i++) {
  
    strcat(title, articles[i]["title"]);
    strcat(title, " ");
 
   }

   Serial.println(title);
 
   String text_lowcase = title;
   text_lowcase.toLowerCase();
   outbuffersize = text_lowcase.length();
  
    sendByte((byte)(outbuffersize));
    sendByte((byte)(outbuffersize>>8));
    sendByte(0x00);
    sendByte(0x30); 
    
    for (int x = 0; x < outbuffersize; x++) {
          sendByte(Ascii_to_screenCode(text_lowcase[x]));}
    sendByte(0xff);

    http.end();
   }

    //========================================================
    //========================================================
    }
    }    
}  // end of main loop
