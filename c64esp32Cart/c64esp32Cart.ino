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
 * Scheda: MH ET LIVE ESP32MiniKit (ESP32-WROOM-32, 4MB flash)
 * FQBN:   esp32:esp32:mhetesp32minikit
 *
*/
#include <WiFi.h>
#include <NTPClient.h>
#include <HTTPClient.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <esp_system.h>
#include <vector>
#include <algorithm>

#include <SPI.h>
#include <SD.h>
//#include "BluetoothSerial.h"
#include "supermon.h"
#include "chars.h"
//#include "SID.h"


#include "secrets.h"   // copiare secrets.example.h in secrets.h

//forecast API http://api.openweathermap.org

String URL = "http://api.openweathermap.org/data/2.5/weather?";
// Coordinate della localita': stanno in secrets.h (openweatherLat/Lon) per
// non finire nel repository pubblico.

// true se lat/lon sono state configurate in secrets.h
static bool weatherLocationConfigured() {
  if (openweatherLat.length() == 0 || openweatherLon.length() == 0) return false;
  if (openweatherLat == "xxxxx" || openweatherLon == "xxxxx") return false;
  return true;
}

//news API: https://newsapi.org


WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

String directoryListing;
uint16_t prgEntry = 0;
uint8_t sid_conflicts = 0;
int slideshowIndex = 0;
int koalaCurrent = 0;
std::vector<String> koalaList;
String koalaListDir = "";          // directory a cui si riferisce koalaList
std::vector<String> sidList;
String currentSidName = "";

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
String currentDateOnly;
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
bool IS_KOALA_FILE = false;

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
  Scheda: MH ET LIVE ESP32MiniKit / ESP32-WROOM-32
  SPI della microSD (VSPI):
  MOSI - pin 23
  MISO - pin 19
  SCK  - pin 18
  CS   - GPIO_NUM_5
*/

#define CS      GPIO_NUM_5
#define SD_MOSI GPIO_NUM_23
#define SD_MISO GPIO_NUM_19
#define SD_SCK  GPIO_NUM_18

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


// normalizza un percorso assoluto
String normalizePath(String path) {
  if (path.length() == 0) return "/";
  if (path[0] != '/') path = "/" + path;
  while (path.length() > 1 && path.endsWith("/")) {
    path.remove(path.length() - 1);
  }
  return path;
}

// unisce directory corrente e nome (/nome, nome, /..)
String makePath(const String &dir, const String &name) {
  if (name.length() == 0) return normalizePath(dir);
  if (name[0] == '/') {
    if (dir.length() == 0 || dir == "/") return normalizePath(name);
    return normalizePath(dir + name);
  }
  if (dir.length() == 0 || dir == "/") return normalizePath("/" + name);
  return normalizePath(dir + "/" + name);
}

String parentPath(const String &dir) {
  if (dir.length() <= 1) return "/";
  int pos = dir.lastIndexOf('/');
  if (pos <= 0) return "/";
  return dir.substring(0, pos);
}

String buildDirectoryListing(File folder) {
  String listing;
  if (!folder) {
    Serial.println("error opening folder");
    return listing;
  }

  // Lista costruita in RAM e inviata direttamente al C64: niente file
  // temporaneo sulla SD, quindi nessuna scrittura/lettura inutile.
  listing.reserve(1024);
  listing = "/..\r";

  // Approfittiamo della stessa scansione della directory per preparare la
  // lista dei Koala: cosi' il comando 62 (avvio slideshow) non deve piu'
  // rileggere tutta la SD e l'immagine parte subito.
  koalaList.clear();
  sidList.clear();

  while (true) {
    File entry = folder.openNextFile();
    if (!entry) {
      break;
    }

    const char *entryName = entry.name();
    if (entryName == nullptr || entryName[0] == 0) {
      entry.close();
      continue;
    }

    String name(entryName);
    if (!entry.isDirectory() && isKoalaName(name)) {
      koalaList.push_back(name);
    }
    if (!entry.isDirectory() && isSidName(name)) {
      sidList.push_back(name);
    }

    if (!entry.isDirectory() &&
        (name.equalsIgnoreCase("directory.txt") || name.equalsIgnoreCase("koalist.txt"))) {
      entry.close();
      continue;
    }

    String line;
    if (entry.isDirectory()) {
      line = "/";
    }
    line += name;
    line += '\r';
    entry.close();

    // Il testo viene copiato in RAM C64 a partire da $1000.
    // Tengo la lista sotto $4000, per non sovrascrivere i SID rilocati
    // a $4000-$5FFF (e per non arrivare al codice a $C000).
    // Appena la lista e' piena mi fermo: scorrere tutto il root di una SD
    // grande puo' richiedere troppo tempo e far scadere wait_rx_long.
    if (listing.length() + line.length() > 12032) {
      break;
    }
    listing += line;
  }

  folder.close();
  return listing;
}

void buildDirectoryList() {
  File dir = SD.open(currentDir);
  if (!dir) {
    Serial.print("error opening ");
    Serial.println(currentDir);
    directoryListing = "";
    return;
  }

  directoryListing = buildDirectoryListing(dir);
  dir.close();
  koalaListDir = currentDir;          // lista Koala gia' pronta per lo slideshow

  Serial.println("directory:");
  Serial.print(directoryListing);
}

void enterDirectory(const String &path) {
  String normalized = normalizePath(path);
  File dir = SD.open(normalized);
  if (!dir) {
    Serial.print("error opening dir ");
    Serial.println(normalized);
    return;
  }
  dir.close();

  parentDir = currentDir;
  currentDir = normalized;
}

void enterParentDirectory() {
  currentDir = normalizePath(parentDir);
  parentDir = parentPath(currentDir);
}

// Cerca un comando SYS nella prima riga BASIC di un PRG caricato a $0801.
// Ritorna l'indirizzo decimale, oppure 0 se non lo trova.
uint16_t parseBasicSys(File &f, uint32_t dataLen) {
  uint8_t buf[160];
  size_t n = 0;

  f.seek(2);                        // salta load address
  while (n < sizeof(buf) && n < dataLen && f.available()) {
    int c = f.read();
    if (c < 0) break;
    buf[n++] = (uint8_t)c;
  }
  f.seek(2);

  // BASIC: link (2 byte), numero riga (2 byte), poi i token
  for (size_t i = 4; i < n; i++) {
    if (buf[i] == 0) break;         // fine della prima riga
    if (buf[i] == 0x9E) {           // token SYS
      size_t j = i + 1;
      while (j < n && buf[j] == ' ') j++;
      uint16_t addr = 0;
      bool any = false;
      while (j < n && buf[j] >= '0' && buf[j] <= '9') {
        addr = addr * 10 + (buf[j] - '0');
        any = true;
        j++;
      }
      if (any) return addr;
    }
  }
  return 0;
}

// invia la lista corrente al C64 (header: lunghezza + destinazione $1000)
void sendDirectory() {
  outbuffersize = directoryListing.length();
  if (outbuffersize > 12032) {
    outbuffersize = 12032;
  }

  sendByte((byte)(outbuffersize));
  sendByte((byte)(outbuffersize >> 8));
  sendByte(0x00);
  sendByte(0x10);

  for (unsigned long x = 0; x < outbuffersize; x++) {
    sendByte(asciiToPET(directoryListing[x]));
  }
}

void getTime(){
  currentDate = "";
  if (WiFi.status() != WL_CONNECTED) {
    currentDate = "WiFi not connected\r\n";
    currentDateOnly = "NO DATE";
    return;
  }

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
  currentDateOnly = weekDay + " " + String(monthDay) + " " + currentMonthName + " " + String(currentYear);
  currentDate = "Time: " + formattedTime + "\r\n" + "Date: " + currentDateOnly + "\r\n";
  }


void getWeatherData(){
    weatherString = "";
    if (WiFi.status() != WL_CONNECTED) {
      return;
    }
    if (!weatherLocationConfigured()) {
      Serial.println("Meteo: lat/lon non configurate in secrets.h");
      return;
    }

    HTTPClient http;

    //Set HTTP Request Final URL with Location and API key information
    http.begin(URL + "lat=" + openweatherLat + "&lon=" + openweatherLon +
               "&units=metric&appid=" + openweatherApiKey);

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
      clouds = obj["clouds"]["all"];
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

  int c = (uint8_t)ascii;

  // Il font 2x2 usa come base solo i codici 0..63.
  // Le maiuscole vanno portate in minuscolo; i byte UTF-8 (>127) o i
  // caratteri non supportati diventano spazio, mai indici fuori array.
  if (c >= 'A' && c <= 'Z') c += 'a' - 'A';

  if (c >= 0 && c < 128) {
    byte v = asciitoscreen[c];
    if (v < 64) return v;
  }

  return 32;  // spazio
}

// ******************************************************************************
// UTF-8 -> ASCII compatibile con il charset 2x2 dello scroller
// ******************************************************************************
String utf8ToScrollerAscii(const String &in) {
  String out;
  out.reserve(in.length());

  for (unsigned int i = 0; i < in.length(); ) {
    uint8_t c = (uint8_t)in[i];

    if (c < 0x80) {
      if (c >= 32 && c != 127) out += (char)c;
      else out += ' ';
      i++;
      continue;
    }

    uint32_t cp = 0;
    int extra = 0;

    if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; extra = 1; }
    else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; extra = 2; }
    else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; extra = 3; }
    else { out += ' '; i++; continue; }

    if (i + extra >= in.length()) { out += ' '; break; }

    bool valid = true;
    for (int k = 1; k <= extra; k++) {
      uint8_t cc = (uint8_t)in[i + k];
      if ((cc & 0xC0) != 0x80) { valid = false; break; }
      cp = (cp << 6) | (cc & 0x3F);
    }

    if (!valid) { out += ' '; i++; continue; }
    i += extra + 1;

    char m = '?';

    if ((cp >= 0xC0 && cp <= 0xC5) || (cp >= 0xE0 && cp <= 0xE5)) m = 'a';
    else if (cp == 0xC7 || cp == 0xE7) m = 'c';
    else if ((cp >= 0xC8 && cp <= 0xCB) || (cp >= 0xE8 && cp <= 0xEB)) m = 'e';
    else if ((cp >= 0xCC && cp <= 0xCF) || (cp >= 0xEC && cp <= 0xEF)) m = 'i';
    else if (cp == 0xD1 || cp == 0xF1) m = 'n';
    else if ((cp >= 0xD2 && cp <= 0xD6) || (cp >= 0xF2 && cp <= 0xF6)) m = 'o';
    else if ((cp >= 0xD9 && cp <= 0xDC) || (cp >= 0xF9 && cp <= 0xFC)) m = 'u';
    else if (cp == 0xDF) m = 's';
    else if (cp == 0x2018 || cp == 0x2019 || cp == 0x02BC) m = '\'';
    else if (cp == 0x201C || cp == 0x201D || cp == 0x00AB || cp == 0x00BB) m = '"';
    else if ((cp >= 0x2010 && cp <= 0x2015) || cp == 0x2212) m = '-';
    else if (cp == 0x2026) { out += "..."; continue; }
    else if (cp == 0x00A0 || cp == 0x2009 || cp == 0x200A) m = ' ';
    else if (cp >= 0x1F000 && cp <= 0x1FAFF) m = ' ';

    out += m;
  }

  out.toLowerCase();
  return out;
}


// ******************************************************************************
// translate ascii to PETascii
// ******************************************************************************
// ******************************************************************************
// NEWS: fonti selezionabili dal menu C64
// ******************************************************************************
struct NewsSource {
  const char *name;
  const char *url;
  bool useNewsApi;
};

static const NewsSource NEWS_SOURCES[] = {
  { "ANSA",        "https://www.ansa.it/sito/notizie/topnews/topnews_rss.xml", false },
  { "Rai News",    "https://www.rainews.it/rss/tutti",                        false },
  { "Repubblica",  "https://www.repubblica.it/rss/homepage/rss2.0.xml",       false },
  { "Il Sole 24 Ore", "https://www.ilsole24ore.com/rss/italia.xml",           false },
  { "Google News", "https://news.google.com/rss?hl=it&gl=IT&ceid=IT:it",      false },
  { "NewsAPI",     "",                                                        true  },
  { "BBC World",   "https://feeds.bbci.co.uk/news/world/rss.xml",            false },
  { "Al Jazeera",  "https://www.aljazeera.com/xml/rss/all.xml",              false },
  { "SCMP",        "https://www.scmp.com/rss/91/feed",                       false },
  { "CGTN World",  "https://www.cgtn.com/subscribe/rss/section/world.xml",   false },
};

const int NEWS_SOURCE_COUNT = sizeof(NEWS_SOURCES) / sizeof(NEWS_SOURCES[0]);
int newsSourceIndex = 0;

static void appendUtf8CodePoint(String &out, uint32_t cp) {
  if (cp < 0x80) {
    out += (char)cp;
  } else if (cp < 0x800) {
    out += (char)(0xC0 | (cp >> 6));
    out += (char)(0x80 | (cp & 0x3F));
  } else if (cp < 0x10000) {
    out += (char)(0xE0 | (cp >> 12));
    out += (char)(0x80 | ((cp >> 6) & 0x3F));
    out += (char)(0x80 | (cp & 0x3F));
  } else {
    out += (char)(0xF0 | (cp >> 18));
    out += (char)(0x80 | ((cp >> 12) & 0x3F));
    out += (char)(0x80 | ((cp >> 6) & 0x3F));
    out += (char)(0x80 | (cp & 0x3F));
  }
}

// Converte le entita' HTML/XML piu' comuni (&amp; &#x27; ecc.) in UTF-8.
static String decodeHtmlEntities(const char *s, unsigned int len) {
  String out;
  out.reserve(len);

  for (unsigned int i = 0; i < len; i++) {
    if (s[i] != '&') {
      out += s[i];
      continue;
    }

    unsigned int j = i + 1;
    while (j < len && j <= i + 10 && s[j] != ';') j++;
    if (j >= len || s[j] != ';') {
      out += '&';
      continue;
    }

    String ent(s + i + 1, j - i - 1);
    bool recognized = true;

    if (ent == "amp") out += '&';
    else if (ent == "lt") out += '<';
    else if (ent == "gt") out += '>';
    else if (ent == "quot") out += '"';
    else if (ent == "apos") out += '\'';
    else if (ent == "nbsp") out += ' ';
    else if (ent.length() > 1 && ent[0] == '#') {
      uint32_t cp = 0;
      bool ok = true;

      if (ent[1] == 'x' || ent[1] == 'X') {
        if (ent.length() == 2) ok = false;
        for (unsigned int k = 2; ok && k < ent.length(); k++) {
          char h = ent[k];
          if (h >= '0' && h <= '9') cp = cp * 16 + (h - '0');
          else if (h >= 'a' && h <= 'f') cp = cp * 16 + (h - 'a' + 10);
          else if (h >= 'A' && h <= 'F') cp = cp * 16 + (h - 'A' + 10);
          else ok = false;
        }
      } else {
        for (unsigned int k = 1; ok && k < ent.length(); k++) {
          char d = ent[k];
          if (d >= '0' && d <= '9') cp = cp * 10 + (d - '0');
          else ok = false;
        }
      }

      if (ok && cp > 0 && cp <= 0x10FFFF) appendUtf8CodePoint(out, cp);
      else recognized = false;
    } else {
      recognized = false;
    }

    if (recognized) i = j;
    else out += '&';   // entita' sconosciuta: lascia il testo com'e'
  }

  return out;
}

// Legge il feed RSS e raccoglie i <title> dentro gli <item>.
// Il sink viene alimentato da HTTPClient::writeToStream(), che decodifica
// anche il Transfer-Encoding chunked (ANSA, Rai, Google lo usano).
// Non tiene in RAM l'intero XML (Google News puo' superare i 100 KB).
class RssTitleSink : public Stream {
public:
  RssTitleSink(String &out, unsigned int maxLen, int maxTitles)
    : out(out), maxLen(maxLen), maxTitles(maxTitles) {}

  size_t write(uint8_t c) override {
    feed((char)c);
    return 1;
  }

  size_t write(const uint8_t *buffer, size_t size) override {
    for (size_t i = 0; i < size; i++) feed((char)buffer[i]);
    return size;
  }

  int available() override { return 0; }
  int read() override { return -1; }
  int peek() override { return -1; }
  void flush() override {}

private:
  void feed(char c) {
    if (titles >= maxTitles || out.length() >= maxLen) return;

    if (inCdata) {
      if (c == ']') {
        cdataBracket++;
        return;
      }
      if (c == '>' && cdataBracket >= 2) {
        inCdata = false;
        cdataBracket = 0;
        return;
      }
      while (cdataBracket > 0) {
        if (inTitle && titleLen < (int)sizeof(title) - 1) title[titleLen++] = ']';
        cdataBracket--;
      }
      if (inTitle && titleLen < (int)sizeof(title) - 1) title[titleLen++] = c;
      return;
    }

    if (!haveTag) {
      if (c == '<') {
        haveTag = true;
        closing = false;
        tagLen = 0;
      } else if (inTitle && titleLen < (int)sizeof(title) - 1) {
        title[titleLen++] = c;
      }
      return;
    }

    if (c != '>') {
      if (tagLen < (int)sizeof(tag) - 1) tag[tagLen++] = c;
      // <![CDATA[ ... ]]> : il contenuto va preso alla lettera
      if (tagLen == 8 && memcmp(tag, "![CDATA[", 8) == 0) {
        inCdata = true;
        haveTag = false;
        cdataBracket = 0;
      }
      return;
    }

    tag[tagLen] = '\0';
    haveTag = false;

    char *p = tag;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    closing = (*p == '/');
    if (closing) p++;

    char name[16];
    int k = 0;
    while (*p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n' &&
           *p != '/' && k < (int)sizeof(name) - 1) {
      name[k++] = tolower((unsigned char)*p++);
    }
    name[k] = '\0';

    if (inTitle) {
      if (closing && strcmp(name, "title") == 0) {
        String t = decodeHtmlEntities(title, titleLen);
        t.trim();
        if (t.length() > 0) {
          if (out.length() > 0) out += ' ';
          out += t;
          titles++;
        }
        inTitle = false;
        titleLen = 0;
      }
    } else if (!closing && strcmp(name, "item") == 0) {
      inItem = true;
    } else if (closing && strcmp(name, "item") == 0) {
      inItem = false;
    } else if (inItem && !closing && strcmp(name, "title") == 0) {
      inTitle = true;
      titleLen = 0;
    }
  }

  String &out;
  unsigned int maxLen;
  int maxTitles;
  int titles = 0;
  bool inItem = false;
  bool inTitle = false;
  bool haveTag = false;
  bool closing = false;
  bool inCdata = false;
  int cdataBracket = 0;
  char tag[32];
  int tagLen = 0;
  char title[512];
  int titleLen = 0;
};

static String fetchRssHeadlines(const char *url, unsigned int maxLen, int maxTitles) {
  HTTPClient http;
  http.setTimeout(15000);
  if (!http.begin(url)) {
    Serial.println("NEWS: begin() fallito");
    return "";
  }

  int httpCode = http.GET();
  Serial.printf("NEWS: HTTP %d (%s)\n", httpCode, url);
  if (httpCode != HTTP_CODE_OK) {
    http.end();
    return "";
  }

  String headlines;
  headlines.reserve(maxLen + 256);
  RssTitleSink sink(headlines, maxLen, maxTitles);
  int written = http.writeToStream(&sink);
  http.end();
  Serial.printf("NEWS: RSS %d byte, %u titoli\n", written, (unsigned)headlines.length());
  return headlines;
}

static String fetchNewsApiHeadlines() {
  HTTPClient http;
  http.setTimeout(15000);
  http.begin("https://newsapi.org/v2/everything?q=italia&language=it&sortBy=publishedAt&pageSize=10&apiKey=" + newsApiKey);
  int httpCode = http.GET();
  Serial.printf("NEWS: HTTP %d (NewsAPI)\n", httpCode);
  if (httpCode != HTTP_CODE_OK) {
    http.end();
    return "";
  }

  response = http.getString();
  http.end();
  Serial.printf("NEWS: risposta %u byte\n", (unsigned)response.length());

  DynamicJsonDocument doc(32768);
  DeserializationError error = deserializeJson(doc, response);
  if (error) {
    Serial.printf("NEWS: JSON error %s\n", error.c_str());
    return "";
  }

  JsonArray articles = doc["articles"];
  String headlines;
  headlines.reserve(2048);
  for (size_t i = 0; i < articles.size(); i++) {
    const char *t = articles[i]["title"] | "";
    if (headlines.length() > 0) headlines += ' ';
    headlines += t;
    if (headlines.length() >= 2000) break;
  }
  return headlines;
}

// ******************************************************************************
// NEWS: grafica PETSCII generata per le 23 righe sopra lo scroller
// ******************************************************************************
static const int ART_ROWS = 23;          // righe disegnate
static const int ART_COLS = 40;
static const int ART_SIZE = 25 * ART_COLS; // 1000: include le 2 righe scroller

static uint8_t newsArt[ART_SIZE];
static uint8_t newsArtColor[ART_SIZE];

static void artPut(int r, int c, uint8_t code, uint8_t color) {
  if (r < 0 || r >= ART_ROWS || c < 0 || c >= ART_COLS) return;
  int i = r * ART_COLS + c;
  newsArt[i] = code;
  newsArtColor[i] = color;
}

static void artText(int r, int c, const char *s, uint8_t color) {
  for (; *s; s++, c++) {
    uint8_t ch = (uint8_t)*s;
    if (ch >= 'A' && ch <= 'Z') ch = ch - 'A' + 1;        // screen code A-Z
    else if (ch >= 'a' && ch <= 'z') ch = ch - 'a' + 1;
    else if (ch >= '0' && ch <= '9') ch = ch - '0' + 48;  // screen code 0-9
    artPut(r, c, ch, color);
  }
}

static int lastArtPattern = -1;

static void artFrameAndHeader() {
  const uint8_t colBorder = 14;  // azzurro
  const uint8_t colLine   = 3;   // ciano

  for (int r = 1; r < ART_ROWS - 1; r++) {
    artPut(r, 0, 93, colBorder);                 // |
    artPut(r, ART_COLS - 1, 93, colBorder);
  }
  for (int c = 0; c < ART_COLS; c++) {
    artPut(0, c, 64, colBorder);                 // linea orizzontale piena
    artPut(ART_ROWS - 1, c, 64, colBorder);
  }

  artText(2, 16, "C64 NEWS", 7);
  for (int c = 10; c <= 29; c++) artPut(4, c, 64, colLine);

  // nome della fonte sotto il titolo
  const char *name = NEWS_SOURCES[newsSourceIndex].name;
  int len = strlen(name);
  int c = (ART_COLS - len) / 2;
  if (c < 2) c = 2;
  artText(6, c, name, 15);
}

static void artPatternCity() {
  static const uint8_t palette[] = { 1, 3, 5, 7, 10, 13, 14, 15 };
  static const uint8_t caps[] = { 81, 86, 87, 90 };  // pallini/diamanti
  const int base = ART_ROWS - 2;                     // riga 21

  for (int c = 2; c < ART_COLS - 2; c++) {
    if (random(100) < 28) continue;
    int h = 1 + random(9);
    uint8_t col = palette[random(sizeof(palette))];
    for (int r = base - h + 1; r <= base; r++) artPut(r, c, 93, col);
    artPut(base - h, c, caps[random(sizeof(caps))], col);
  }

  for (int i = 0; i < 36; i++) {
    int r = 8 + random(4);
    int c = 2 + random(ART_COLS - 4);
    artPut(r, c, (random(3) == 0) ? 42 : 46,
           (random(2) == 0) ? 1 : 15);
  }

  for (int i = 0; i < 3; i++) {
    int r = 8 + random(4);
    int c = 3 + random(ART_COLS - 8);
    int len = 4 + random(8);
    uint8_t ch = (random(2) == 0) ? 47 : 92;
    uint8_t col = (random(2) == 0) ? 3 : 14;
    for (int k = 0; k < len; k++) {
      int cc = (ch == 47) ? c + k : c - k;
      if (cc <= 0 || cc >= ART_COLS - 1) break;
      artPut(r + k, cc, ch, col);
    }
  }
}

static void artPatternStars() {
  for (int i = 0; i < 80; i++) {
    int r = 8 + random(14);
    int c = 2 + random(ART_COLS - 4);
    uint8_t ch = (random(4) == 0) ? 42 : 46;
    uint8_t col = (random(3) == 0) ? 15 : 1;
    artPut(r, c, ch, col);
  }

  const int mr = 9 + random(6);
  const int mc = 6 + random(ART_COLS - 12);
  artPut(mr, mc, 87, 7);                       // luna
  artPut(mr + 1, mc, 81, 7);

  static const uint8_t big[] = { 81, 86, 87, 90 };
  for (int i = 0; i < 8; i++) {
    int r = 9 + random(12);
    int c = 3 + random(ART_COLS - 6);
    artPut(r, c, big[random(sizeof(big))], (random(2) == 0) ? 3 : 14);
  }
}

static void artPatternBricks() {
  static const uint8_t brickColors[] = { 2, 8, 9, 10, 7 };
  for (int r = 8; r < ART_ROWS - 2; r += 2) {
    uint8_t col = brickColors[random(sizeof(brickColors))];
    for (int c = 2; c < ART_COLS - 2; c++) artPut(r, c, 64, col);
    int off = ((r / 2) & 1) ? 2 : 0;
    for (int c = 4 + off; c < ART_COLS - 2; c += 5) {
      artPut(r + 1, c, 93, col);
    }
  }
}

static void artPatternMaze() {
  static const uint8_t mazeColors[] = { 3, 5, 13, 14, 7 };
  for (int r = 8; r < ART_ROWS - 2; r += 2) {
    for (int c = 2; c < ART_COLS - 2; c += 2) {
      uint8_t col = mazeColors[random(sizeof(mazeColors))];
      if (random(2) == 0) {
        artPut(r, c, 64, col);
        if (random(2) == 0) artPut(r, c + 1, 64, col);
      } else {
        artPut(r, c, 93, col);
        if (random(2) == 0) artPut(r + 1, c, 93, col);
      }
      if (random(8) == 0) artPut(r, c, 91, col);  // crocevia
    }
  }
}

static void buildNewsArt() {
  randomSeed(esp_random());
  memset(newsArt, 32, sizeof(newsArt));          // spazio
  memset(newsArtColor, 1, sizeof(newsArtColor)); // bianco

  artFrameAndHeader();

  const int count = 4;
  int pattern = random(count);
  if (pattern == lastArtPattern) pattern = (pattern + 1) % count;
  lastArtPattern = pattern;

  switch (pattern) {
    case 0: artPatternCity(); break;
    case 1: artPatternStars(); break;
    case 2: artPatternBricks(); break;
    default: artPatternMaze(); break;
  }
}

// ******************************************************************************
// PETSCII SEQ dentro file .d64 (slideshow durante le NEWS)
// ******************************************************************************
static const int D64_SEQ_MAX = 8000;
static uint8_t d64SeqData[D64_SEQ_MAX];
static int d64SeqLen = 0;
static String d64SeqName;

static std::vector<String> d64Files;
static int d64FileIndex = 0;
static bool d64ListReady = false;

struct D64SeqEntry {
  uint8_t track;
  uint8_t sector;
  char name[17];
};

static D64SeqEntry d64SeqEntries[128];
static int d64SeqCount = 0;

static uint8_t d64TrackSectors(uint8_t track) {
  if (track < 18) return 21;
  if (track < 25) return 19;
  if (track < 31) return 18;
  return 17;
}

static uint32_t d64Offset(uint8_t track, uint8_t sector) {
  uint32_t off = 0;
  for (uint8_t t = 1; t < track; t++) off += (uint32_t)d64TrackSectors(t) * 256;
  return off + (uint32_t)sector * 256;
}

static bool d64ReadSector(File &f, uint8_t track, uint8_t sector, uint8_t *buf) {
  if (track < 1 || track > 40 || sector >= d64TrackSectors(track)) return false;
  uint32_t off = d64Offset(track, sector);
  if ((uint32_t)f.size() < off + 256) return false;
  if (!f.seek(off)) return false;
  return f.read(buf, 256) == 256;
}

static bool d64ParseDir(File &f) {
  d64SeqCount = 0;
  uint8_t t = 18;
  uint8_t s = 1;
  int guard = 0;

  while (t != 0 && guard++ < 32) {
    uint8_t sec[256];
    if (!d64ReadSector(f, t, s, sec)) return false;
    uint8_t nt = sec[0];
    uint8_t ns = sec[1];

    for (int i = 0; i < 8; i++) {
      uint8_t *e = &sec[2 + i * 32];
      if ((e[0] & 0x0F) != 1) continue;          // solo SEQ

      uint8_t ft = e[1];
      uint8_t fs = e[2];
      if (ft < 1 || ft > 40 || fs >= d64TrackSectors(ft)) continue;

      if (d64SeqCount < (int)(sizeof(d64SeqEntries) / sizeof(d64SeqEntries[0]))) {
        D64SeqEntry &x = d64SeqEntries[d64SeqCount++];
        x.track = ft;
        x.sector = fs;
        for (int k = 0; k < 16; k++) {
          char c = (char)e[3 + k];
          if (c == (char)0xA0) c = ' ';
          x.name[k] = c;
        }
        x.name[16] = 0;
      }
    }

    t = nt;
    s = ns;
  }
  return d64SeqCount > 0;
}

static int d64ReadSeq(File &f, uint8_t t, uint8_t s, uint8_t *buf, int maxLen) {
  int out = 0;
  int guard = 0;

  while (t != 0 && guard++ < 1024) {
    uint8_t sec[256];
    if (!d64ReadSector(f, t, s, sec)) return -1;

    uint8_t nt = sec[0];
    uint8_t ns = sec[1];

    if (nt == 0) {
      int n = ns;
      if (n < 2) n = 2;
      if (n > 256) n = 256;
      for (int i = 2; i < n; i++) {
        if (out >= maxLen) return -1;
        buf[out++] = sec[i];
      }
      break;
    }

    for (int i = 2; i < 256; i++) {
      if (out >= maxLen) return -1;
      buf[out++] = sec[i];
    }

    t = nt;
    s = ns;
  }
  return out;
}

static void buildD64List() {
  if (d64ListReady) return;
  d64Files.clear();

  const char *dirs[] = { "/petscii-art", "/" };
  for (int d = 0; d < 2 && d64Files.empty(); d++) {
    File dir = SD.open(dirs[d]);
    if (!dir) continue;

    while (true) {
      File entry = dir.openNextFile();
      if (!entry) break;
      if (!entry.isDirectory()) {
        String name = entry.name();
        String low = name;
        low.toLowerCase();
        if (low.endsWith(".d64")) {
          String path = String(dirs[d]);
          if (!path.endsWith("/")) path += '/';
          path += name;
          d64Files.push_back(path);
        }
      }
      entry.close();
    }
    dir.close();
  }

  d64ListReady = true;
  Serial.printf("SEQ: %u file .d64 trovati\n", (unsigned)d64Files.size());
}

static bool loadNextSeqImage() {
  buildD64List();
  if (d64Files.empty()) return false;

  for (size_t attempt = 0; attempt < d64Files.size(); attempt++) {
    if (d64FileIndex >= (int)d64Files.size()) d64FileIndex = 0;
    String path = d64Files[d64FileIndex++];

    File f = SD.open(path);
    if (!f) continue;

    if (d64ParseDir(f)) {
      int idx = random(d64SeqCount);
      int n = d64ReadSeq(f, d64SeqEntries[idx].track, d64SeqEntries[idx].sector,
                         d64SeqData, D64_SEQ_MAX);
      if (n > 0) {
        d64SeqLen = n;
        d64SeqName = String(d64SeqEntries[idx].name);
        f.close();
        Serial.printf("SEQ: %s / %s, %d byte\n",
                      path.c_str(), d64SeqName.c_str(), d64SeqLen);
        return true;
      }
    }
    f.close();
  }
  return false;
}

// ******************************************************************************
// Slideshow standalone: file .seq estratti in /petscii-seq
// ******************************************************************************
static std::vector<String> seqFiles;
static int seqFileIndex = 0;
static bool seqListReady = false;

// Legge una lista gia' pronta (manifest.txt o seqlist.txt) senza scandire
// tutte le entry della directory. Ogni riga puo' essere:
//   nome.seq
//   nome.seq<TAB>dimensione<TAB>origine<TAB>nome CBM
//   /petscii-seq/nome.seq
static bool readSeqListFile(const char *path) {
  seqFiles.clear();
  File f = SD.open(path);
  if (!f) return false;

  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) continue;

    int tab = line.indexOf('\t');
    String name = (tab >= 0) ? line.substring(0, tab) : line;
    name.trim();

    String low = name;
    low.toLowerCase();
    if (!low.endsWith(".seq")) continue;

    if (!name.startsWith("/")) name = String("/petscii-seq/") + name;
    seqFiles.push_back(name);
  }
  f.close();
  return !seqFiles.empty();
}

static void buildSeqList() {
  if (seqListReady) return;
  seqFiles.reserve(1024);

  // 1) manifest.txt creato da tools/extract_seq.py: un solo file da leggere.
  if (readSeqListFile("/petscii-seq/manifest.txt")) {
    seqListReady = true;
    Serial.printf("SEQ: %u file da manifest\n", (unsigned)seqFiles.size());
    return;
  }

  // 2) cache creata dal firmware al primo avvio.
  if (readSeqListFile("/petscii-seq/seqlist.txt")) {
    seqListReady = true;
    Serial.printf("SEQ: %u file da cache\n", (unsigned)seqFiles.size());
    return;
  }

  // 3) fallback: scansione della directory (lenta la prima volta).
  seqFiles.clear();
  const char *dirs[] = { "/petscii-seq", "/" };
  for (int d = 0; d < 2 && seqFiles.empty(); d++) {
    File dir = SD.open(dirs[d]);
    if (!dir) continue;

    while (true) {
      File entry = dir.openNextFile();
      if (!entry) break;
      if (!entry.isDirectory()) {
        String name = entry.name();
        String low = name;
        low.toLowerCase();
        if (low.endsWith(".seq")) {
          String path = String(dirs[d]);
          if (!path.endsWith("/")) path += '/';
          path += name;
          seqFiles.push_back(path);
        }
      }
      entry.close();
    }
    dir.close();
  }

  std::sort(seqFiles.begin(), seqFiles.end());

  // Salva la cache per i prossimi avvii.
  File cache = SD.open("/petscii-seq/seqlist.txt", FILE_WRITE);
  if (cache) {
    for (size_t i = 0; i < seqFiles.size(); i++) cache.println(seqFiles[i]);
    cache.close();
  }

  seqListReady = true;
  Serial.printf("SEQ: %u file .seq trovati (cache creata)\n", (unsigned)seqFiles.size());
}

static bool sendSeqFile(const String &path) {
  File f = SD.open(path);
  if (!f) return false;

  size_t sz = f.size();
  if (sz == 0 || sz > D64_SEQ_MAX) {
    f.close();
    return false;
  }

  int n = f.read(d64SeqData, sz);
  f.close();
  if (n <= 0) return false;

  d64SeqLen = n;
  d64SeqName = path;

  sendByte((byte)n);
  sendByte((byte)(n >> 8));
  sendByte(0x00);
  sendByte(0xA0);              // $A000: stream SEQ
  for (int i = 0; i < n; i++) sendByte(d64SeqData[i]);

  Serial.printf("SEQ standalone: %s, %d byte\n", path.c_str(), n);
  return true;
}

static bool sendSeqStep(int step) {
  buildSeqList();
  int n = (int)seqFiles.size();
  if (n == 0) return false;

  int dir = (step < 0) ? -1 : 1;
  if (step != 0) seqFileIndex = (seqFileIndex + step + n) % n;

  for (int tries = 0; tries < n; tries++) {
    if (seqFileIndex < 0) seqFileIndex = n - 1;
    if (seqFileIndex >= n) seqFileIndex = 0;
    if (sendSeqFile(seqFiles[seqFileIndex])) return true;
    seqFileIndex = (seqFileIndex + dir + n) % n;
  }
  return false;
}

static void sendGeneratedArtBlock() {
  buildNewsArt();
  unsigned int total = ART_SIZE * 2;
  sendByte((byte)(total));
  sendByte((byte)(total >> 8));
  sendByte(0x00);
  sendByte(0x90);              // $9000: screen + colori
  for (unsigned int i = 0; i < ART_SIZE; i++) sendByte(newsArt[i]);
  for (unsigned int i = 0; i < ART_SIZE; i++) sendByte(newsArtColor[i]);
  Serial.println("NEWS: nuovo pattern PETSCII");
}

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

  // SID_load_start e SID_data_size sono gia' stati calcolati dal file:
  // non ricavarli di nuovo dall'header (nel caso load address = 0).
  if ( SID_load_start >= 0x07E8)  {  
    LOAD_ADDRESS = SID_load_start;
  }

  SID_load_end = SID_load_start + SID_data_size - 1;
  

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
  // GPIO34/35/36 sono input-only: non hanno pull-up/pull-down interni.
  pinMode(C64IO1, INPUT);
  pinMode(C64IO2, INPUT);
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

  // Il WiFi serve solo per meteo/ora e NEWS. Se non e' configurato o non
  // e' disponibile, NON bloccare l'avvio: SD e menu devono funzionare lo stesso.
  if (strlen(ssid) > 0 && strcmp(ssid, "xxxx") != 0) {
    Serial.print("Connecting to ");
    Serial.println(ssid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    unsigned long wifiStart = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - wifiStart) < 15000) {
      delay(500);
      Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
    } else {
      Serial.println("WiFi: connessione fallita, continuo senza rete");
    }
  } else {
    Serial.println("WiFi non configurato (ssid/password): continuo senza rete");
  }
  //if (SerialBT.begin("ESP32Cart"))digitalWrite(CLED, HIGH);         // start serial bluetooth

  // Pin SPI espliciti: corrispondono alla scheda MH ET LIVE ESP32MiniKit
  // e allo schema elettrico della cartuccia (VSPI).
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, CS);
  if (!SD.begin(CS, SPI, 4000000)) {

    Serial.println("SD begin failed!");
    
  }

  SD.remove("/directory.txt");          // pulizia del vecchio file temporaneo
  currentDir = parentDir = "/";
  buildDirectoryList();
  // Aspetta che il C64 abbia copiato il bootstrap dalla EPROM prima di
  // alzare EXROM. Il loader puo' anche inviare prima il comando 112.
  delay(1500);
  digitalWrite(CLED, LOW);
  Serial.println("");
  Serial.println("done!");

}  // end of setup


// Invia un blocco di testo allo scroller NEWS ($3000) e termina con $FF.
void sendScrollerMessage(const String &text, bool withArt = false) {
  unsigned int textLen = text.length();
  if (textLen > 2047) textLen = 2047;
  unsigned int total = 2048 + (withArt ? ART_SIZE * 2 : 0);

  outbuffersize = total;
  sendByte((byte)(total));
  sendByte((byte)(total >> 8));
  sendByte(0x00);
  sendByte(0x88);          // testo a $8800, grafica accodata a $9000

  for (unsigned int x = 0; x < textLen; x++) {
    sendByte(Ascii_to_screenCode(text[x]));
  }
  sendByte(0xff);

  // Il testo occupa 2048 byte; l'eventuale grafica parte da $9000.
  for (unsigned int x = textLen + 1; x < 2048; x++) sendByte(0x00);

  if (withArt) {
    buildNewsArt();
    for (unsigned int x = 0; x < ART_SIZE; x++) sendByte(newsArt[x]);
    for (unsigned int x = 0; x < ART_SIZE; x++) sendByte(newsArtColor[x]);
    Serial.println("NEWS: grafica PETSCII accodata al testo");
  }
}

// ******************************************************************************
// Menu template: 1000 byte di screen RAM + 1000 byte di color RAM.
// Viene inviato direttamente al C64 a $9800 (comando 64) e mostrato da lì.
// Uso screen code: nel charset minuscolo (D018=23) i codici 65-90 sono le
// lettere maiuscole; 64 e 93 sono le linee orizzontale/verticale, 43 il '+'.
// ******************************************************************************
static uint8_t menuScreen[1000];
static uint8_t menuColors[1000];
static bool menuUpperCharset = false;

static uint8_t menuAsciiToScreen(char c) {
  if (c >= 'a' && c <= 'z') c -= 32;
  if (menuUpperCharset && c >= 'A' && c <= 'Z') return (uint8_t)(c - 'A' + 1);
  return (uint8_t)c;
}

static void menuSet(int x, int y, uint8_t code, uint8_t color) {
  if (x < 0 || x >= 40 || y < 0 || y >= 25) return;
  int i = y * 40 + x;
  menuScreen[i] = code;
  menuColors[i] = color;
}

static void menuCenter(int row, const char *text, uint8_t color) {
  int len = strlen(text);
  int x = (40 - len) / 2;
  if (x < 0) x = 0;
  for (int i = 0; i < len && x + i < 40; i++) {
    menuSet(x + i, row, menuAsciiToScreen(text[i]), color);
  }
}

static void menuLeft(int row, int col, const char *text, uint8_t color) {
  int len = strlen(text);
  for (int i = 0; i < len && col + i < 40; i++) {
    menuSet(col + i, row, menuAsciiToScreen(text[i]), color);
  }
}

static void menuClearScreen() {
  memset(menuScreen, 32, sizeof(menuScreen));
  memset(menuColors, 1, sizeof(menuColors));
}

static void drawFrame() {
  for (int x = 1; x < 39; x++) {
    menuSet(x, 0, 64, 14);
    menuSet(x, 24, 64, 14);
  }
  for (int y = 1; y < 24; y++) {
    menuSet(0, y, 93, 14);
    menuSet(39, y, 93, 14);
  }
  menuSet(0, 0, 43, 14);
  menuSet(39, 0, 43, 14);
  menuSet(0, 24, 43, 14);
  menuSet(39, 24, 43, 14);
}

static void buildMenuScreen() {
  menuClearScreen();
  drawFrame();

  menuCenter(2, "C64 ESP32 CARTRIDGE", 7);
  menuCenter(3, "BY M.ANGELINI", 15);
  menuLeft(5, 8, "MAIN MENU", 3);

  static const char *items[] = {
    "SD CARD", "SUPERMON", "HELP", "CLOCK / WEATHER", "BASIC",
    "KOALA", "NEWS", "KOALA SLIDESHOW", "PETSCII SLIDESHOW"
  };
  for (int i = 0; i < 9; i++) menuLeft(6 + i, 8, items[i], 1);

  menuLeft(16, 8, "UP/DOWN SELECT - RETURN ENTER", 15);
  menuLeft(19, 8, "LOAD PRG/GAMES, SID, KOALA", 3);
  menuLeft(20, 8, "NEWS + WEATHER FROM INTERNET", 3);
  menuLeft(21, 8, "PETSCII ART FROM SD CARD", 3);
}

static void buildFrameScreen() {
  menuClearScreen();
  drawFrame();
}

static void buildNewsSourceScreen() {
  menuClearScreen();
  drawFrame();
  menuCenter(2, "NEWS SOURCE", 7);
  menuCenter(4, "SELECT WITH UP/DOWN AND RETURN", 15);

  for (int i = 0; i < NEWS_SOURCE_COUNT && i < 10; i++) {
    menuLeft(6 + i, 8, NEWS_SOURCES[i].name, 1);
  }

  menuLeft(18, 8, "UP/DOWN SELECT - RETURN ENTER", 3);
}

static void buildNewsBackgroundScreen() {
  menuClearScreen();
  drawFrame();
  menuCenter(2, "NEWS BACKGROUND", 7);
  menuCenter(4, "SELECT WITH UP/DOWN AND RETURN", 15);

  static const char *items[] = {
    "PETSCII ART", "KOALA", "CLOCK / WEATHER"
  };
  for (int i = 0; i < 3; i++) menuLeft(6 + i, 8, items[i], 1);

  menuLeft(12, 8, "KOALA MUST BE LOADED FROM SD", 3);
  menuLeft(18, 8, "UP/DOWN SELECT - RETURN ENTER", 3);
}

static void buildWeatherScreen() {
  // Il template meteo viene mostrato con il charset ROM maiuscolo ($1000):
  // le lettere vanno quindi codificate come screen code 1..26.
  menuUpperCharset = true;

  menuClearScreen();
  drawFrame();

  bool online = (WiFi.status() == WL_CONNECTED);
  menuCenter(2, "CLOCK / WEATHER", 7);

  String timeStr = online ? timeClient.getFormattedTime() : "--:--:--";
  menuLeft(4, 16, timeStr.c_str(), 7);

  menuCenter(6, online ? currentDateOnly.c_str() : "NO DATE", 15);

  if (online && weatherLocation.length() > 0) {
    String loc = "LOCATION: " + weatherLocation + ", " + country;
    menuCenter(9, loc.c_str(), 3);
    menuCenter(10, weatherDescription.c_str(), 1);

    String t = "TEMP: " + String(temp, 1) + " C  MIN " + String(tempMin, 1) + "  MAX " + String(tempMax, 1);
    String h = "HUMIDITY: " + String(humidity) + " %";
    String p = "PRESSURE: " + String(pressure) + " HPA";
    String w = "WIND: " + String(windSpeed, 1) + " M/S";
    String c = "CLOUDS: " + String(clouds) + " %";

    menuCenter(12, t.c_str(), 1);
    menuCenter(13, h.c_str(), 1);
    menuCenter(14, p.c_str(), 1);
    menuCenter(15, w.c_str(), 1);
    menuCenter(16, c.c_str(), 1);
  } else {
    menuCenter(9, online ? "WEATHER DATA UNAVAILABLE" : "WIFI NOT CONNECTED", 10);
    menuCenter(10, online ? "CHECK SD / INTERNET" : "CHECK SECRETS.H", 1);
  }

  menuCenter(20, "SPACE = BACK TO MENU", 15);
  menuUpperCharset = false;
}

static void buildSidScreen() {
  menuUpperCharset = false;
  menuClearScreen();
  drawFrame();
  menuCenter(2, "SID PLAYER", 7);

  int row = 4;
  int i = 0;
  while (info_string_SID[i] && row < 18) {
    String line = "";
    while (info_string_SID[i] && info_string_SID[i] != '\r' && info_string_SID[i] != '\n') {
      line += info_string_SID[i++];
    }
    while (info_string_SID[i] == '\r' || info_string_SID[i] == '\n') i++;
    if (line.length() > 32) line = line.substring(0, 32);
    menuLeft(row++, 4, line.c_str(), 1);
  }

  menuLeft(19, 6, "F1 PREV - F7 NEXT - SPACE MENU", 3);
}

static void sendScreenTemplate(uint8_t destHigh) {
  const unsigned int len = 2000;
  sendByte((byte)len);
  sendByte((byte)(len >> 8));
  sendByte(0x00);
  sendByte(destHigh);
  for (unsigned int i = 0; i < 1000; i++) sendByte(menuScreen[i]);
  for (unsigned int i = 0; i < 1000; i++) sendByte(menuColors[i]);
}

// ******************************************************************************
// HELP page, sent to the C64 at $A000 and printed with CHROUT.
// PETSCII codes: 0x93 clear, 0x05 white, 0x9E yellow, 0x9F cyan, 13 = newline.
// ******************************************************************************
static const char helpText[] =
  "\x93\x9e"
  "+--------------------------------------+\r"
  "      C64 ESP32 CARTRIDGE - HELP\r"
  "+--------------------------------------+\r"
  "\x05"
  " SD CARD   LOAD PRG/GAMES, SID, KOALA\r"
  "           F1/F7 PAGE, RETURN OPEN\r"
  "           Q = QUIT DIRECTORY\r"
  " SUPERMON  RAM MONITOR AT $A000\r"
  " HELP      THIS PAGE\r"
  " CLOCK     TIME AND WEATHER\r"
  "           FROM OPENWEATHER\r"
  " BASIC     RETURN TO BASIC\r"
  " KOALA     SHOW LOADED KOALA\r"
  " NEWS      MULTIPLE SOURCES:\r"
  "           ANSA/RAI/BBC/ALJAZEERA...\r"
  " SLIDESHOW KOALA OR PETSCII ART\r"
  "           SPACE EXITS\r"
  "\x9e"
  " SID       LOAD A SID FROM SD FIRST\r"
  "           THEN NEWS/SLIDES CAN USE IT\r"
  " SD: /PETSCII-ART (.D64)\r"
  "     /PETSCII-SEQ (.SEQ)\r"
  "\x9f"
  " UP/DOWN SELECT - RETURN ENTER\r"
  " PRESS A KEY TO RETURN TO MENU\r";

void buildKoalaList() {
  // Se la directory e' gia' stata scandita da buildDirectoryList(), la lista
  // e' pronta: evitiamo una seconda lettura della SD (causava 10-20 s di
  // attesa prima della prima immagine dello slideshow).
  if (koalaListDir == currentDir) {
    Serial.printf("Koala list: %u file (gia' pronta)\n",
                  (unsigned)koalaList.size());
    return;
  }

  koalaList.clear();

  File dir = SD.open(currentDir);
  if (dir) {
    while (true) {
      File entry = dir.openNextFile();
      if (!entry) break;
      if (!entry.isDirectory()) {
        String n = entry.name();
        if (isKoalaName(n)) {
          koalaList.push_back(n);
        }
      }
      entry.close();
    }
    dir.close();
  }
  koalaListDir = currentDir;

  // Cache testuale nella directory corrente. Il C64 non la mostra.
  String cachePath = makePath(currentDir, "koalist.txt");
  File cache = SD.open(cachePath, FILE_WRITE);
  if (cache) {
    for (size_t i = 0; i < koalaList.size(); i++) {
      cache.println(koalaList[i]);
    }
    cache.close();
  }

  Serial.printf("Koala list: %u file, cache %s\n",
                (unsigned)koalaList.size(), cachePath.c_str());
}

bool isKoalaName(const String &name) {
  int len = name.length();
  if (len < 4) return false;
  String suffix = name.substring(len - 4);
  suffix.toLowerCase();
  return suffix == ".kla" || suffix == ".koa";
}

bool isSidName(const String &name) {
  int len = name.length();
  if (len < 4) return false;
  String suffix = name.substring(len - 4);
  suffix.toLowerCase();
  return suffix == ".sid";
}

void sendZeroBlock() {
  sendByte(0);
  sendByte(0);
  sendByte(0);
  sendByte(0);
}

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
        buildDirectoryList();
        sendDirectory();
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

  IS_SID_FILE = false;
  IS_KOALA_FILE = false;
  int len = strlen(inbuffer);
  if (len >= 4 && inbuffer[0] != '/') {
    String suffix = String(inbuffer + (len - 4));
    suffix.toLowerCase();
    IS_SID_FILE = (suffix == ".sid");
    IS_KOALA_FILE = (suffix == ".kla" || suffix == ".koa");
  }
  if (IS_SID_FILE) Serial.println("SID file ok");
  else if (IS_KOALA_FILE) Serial.println("Koala file ok");
  else Serial.println("Not SID/Koala file");
  // char bns[inbuffersize];
  // strncpy ( bns, inbuffer, inbuffersize + 1);
  String text_lowcase = inbuffer;
  //text_lowcase.toLowerCase();
  Serial.println(text_lowcase);

  // Selezione di una voce di directory: cambia la directory corrente.
  // Il C64 chiedera' subito dopo il comando 49 per ricevere la nuova lista.
  if (strcmp(inbuffer, "/..") == 0) {
    enterParentDirectory();
  } else if (inbuffer[0] == '/') {
    enterDirectory(makePath(currentDir, inbuffer));
  }
  
     break;}
     
     //========================================================
     case 52:{   // 52 = clock/weather screen template a $A000
        getTime();
        getWeatherData();
        Serial.println(currentDate);
        buildWeatherScreen();
        sendScreenTemplate(0xA0);
        break;}

    //========================================================
      case 64:{          // 64 = main menu template a $9800
        buildMenuScreen();
        sendScreenTemplate(0x98);
        break;}

    //========================================================
      case 65:{          // 65 = news source screen a $A000
        buildNewsSourceScreen();
        sendScreenTemplate(0xA0);
        break;}

    //========================================================
      case 66:{          // 66 = directory border screen a $A000
        buildFrameScreen();
        sendScreenTemplate(0xA0);
        break;}

    //========================================================
      case 67:{          // 67 = NEWS background screen a $A000
        buildNewsBackgroundScreen();
        sendScreenTemplate(0xA0);
        break;}

    //========================================================
      case 63:{          // 63 = pagina HELP (stream PETSCII a $A000)
        unsigned int helpLen = sizeof(helpText) - 1;
        sendByte((byte)helpLen);
        sendByte((byte)(helpLen >> 8));
        sendByte(0x00);
        sendByte(0xA0);
        for (unsigned int i = 0; i < helpLen; i++) sendByte(helpText[i]);
        break;}

    //========================================================
      case 62:{          // 62 = reset slideshow Koala
        buildKoalaList();
        slideshowIndex = 0;
        koalaCurrent = 0;
        sendZeroBlock();
        break;}

    //========================================================
      case 61:{          // 61 = prossimo Koala della directory corrente
        if (slideshowIndex < 0 || slideshowIndex >= (int)koalaList.size()) {
          sendZeroBlock();
          slideshowIndex = 0;
          break;
        }

        koalaCurrent = slideshowIndex;
        String targetName = koalaList[slideshowIndex];
        strncpy(inbuffer, targetName.c_str(), sizeof(inbuffer) - 1);
        inbuffer[sizeof(inbuffer) - 1] = 0;
        IS_SID_FILE = false;
        IS_KOALA_FILE = true;
        slideshowIndex++;
        // fall through: il case 53 invia il file Koala con l'header corretto
      }

    //========================================================
      case 53:{          // 53 = file from ESP32 to c64
      sidFile:
      sendFile:
      if (strcmp(inbuffer, "/..") == 0) {
        enterParentDirectory();
        buildDirectoryList();
        sendDirectory();
      } else if (inbuffer[0] == '/') {
        enterDirectory(makePath(currentDir, inbuffer));
        buildDirectoryList();
        sendDirectory();
      } else {
        String fileName = makePath(currentDir, inbuffer);
		  
        File dataFile = SD.open(fileName);
  
  
          
  if (dataFile) {
    
  
    if (IS_KOALA_FILE) {
      // Koala: 8000 bitmap + 1000 video + 1000 color + 1 background.
      // Alcuni file hanno 2 byte di load address $00 $60 davanti ai 10001
      // byte Koala: in quel caso vanno saltati e la destinazione e' sempre $6000.
      uint32_t fileSize = dataFile.size();

      Serial.printf("Koala: file %lu byte, primi byte:", (unsigned long)fileSize);
      for (int i = 0; i < 8 && dataFile.available(); i++) {
        int b = dataFile.read();
        Serial.printf(" %02X", b);
      }
      Serial.println();
      dataFile.seek(0);

      // Un file Koala standard e' 10001 byte. Se ce ne sono alcuni in piu'
      // all'inizio sono un header (tipicamente 2 byte di load address $00 $60).
      // Va saltato: la destinazione e' comunque sempre $6000.
      uint32_t headerLen = 0;
      if (fileSize > 10001 && fileSize <= 10009) {
        headerLen = fileSize - 10001;
      }

      for (uint32_t i = 0; i < headerLen; i++) {
        int b = dataFile.read();
        Serial.printf("Koala header byte %lu: %02X\n", (unsigned long)i, b);
      }

      uint32_t dataLen = fileSize - headerLen;

      sendByte((byte)(dataLen));
      sendByte((byte)(dataLen >> 8));
      sendByte(0x00);                    // destinazione $6000
      sendByte(0x60);

      for (uint32_t x = 0; x < dataLen; x++) {
        sendByte(dataFile.read());
      }

      dataFile.close();
      Serial.printf("Koala: %lu byte dati, header %lu byte\n",
                    (unsigned long)dataLen,
                    (unsigned long)headerLen);
    }
    else if (IS_SID_FILE == false){
      uint32_t fileSize = dataFile.size();

      if (fileSize < 3) {
        Serial.println("PRG troppo corto");
        prgEntry = 0;
        dataFile.close();
      } else {
        int lo = dataFile.read();
        int hi = dataFile.read();
        uint16_t loadAddr = (uint16_t)lo | ((uint16_t)hi << 8);
        uint32_t dataLen = fileSize - 2;

        prgEntry = loadAddr;
        if (loadAddr == 0x0801) {
          uint16_t sys = parseBasicSys(dataFile, dataLen);
          prgEntry = (sys != 0) ? sys : 0xE37B;  // fallback: BASIC warm start
        }

        sendByte((byte)(dataLen));
        sendByte((byte)(dataLen >> 8));
        sendByte((byte)loadAddr);
        sendByte((byte)(loadAddr >> 8));

        dataFile.seek(2);
        for (uint32_t x = 0; x < dataLen; x++) {
          sendByte(dataFile.read());
        }
        dataFile.close();

        Serial.printf("PRG: load $%04X, entry $%04X, %lu byte\n",
                      loadAddr, prgEntry, (unsigned long)dataLen);
      }
    }

    else{
    currentSidName = inbuffer;
    uint32_t sidFileSize = dataFile.size();

    for (int i = 0; i < 0x7c && dataFile.available(); i++) {
              SIDheader[i] = dataFile.read();
          }

    // Il data offset e' nei byte 6-7 (big endian). Per PSID v2 e' 0x7c,
    // per PSID v3 puo' essere 0x7e.
    uint16_t dataOffset = ((uint16_t)SIDheader[6] << 8) | SIDheader[7];
    if (dataOffset < 0x76 || dataOffset > sidFileSize) {
      dataOffset = 0x7c;
    }

    uint16_t loadStart = ((uint16_t)SIDheader[8] << 8) | SIDheader[9];
    uint32_t dataStart = dataOffset;

    // Se il campo load address e' 0, i primi 2 byte dei dati sono
    // il vero load address (little endian) e vanno esclusi dal payload.
    if (loadStart == 0) {
      dataFile.seek(dataOffset);
      int lo = dataFile.read();
      int hi = dataFile.read();
      loadStart = (uint16_t)lo | ((uint16_t)hi << 8);
      dataStart = dataOffset + 2;
    }

    if (dataStart > sidFileSize) {
      Serial.println("SID: offset dati non valido");
      dataFile.close();
      break;
    }

    SID_load_start = loadStart;
    SID_data_size = sidFileSize - dataStart;
    outbuffersize = SID_data_size;

    Serial.printf("SID header: offset=%u load=$%04X dataStart=%lu size=%lu\n",
                  dataOffset, SID_load_start,
                  (unsigned long)dataStart, (unsigned long)SID_data_size);

    outbuffer = (char*)malloc(outbuffersize);
    
    if (outbuffer == NULL) {
      Serial.println("errore memoria!");
      dataFile.close();
      break;
    }

    dataFile.seek(dataStart);
    for (unsigned long i = 0; i < outbuffersize; i++) {
              outbuffer[i] = dataFile.read();
          }

 
            PLAYABLE_SID = Compatibility_check();
            infoSID();

          // Calcola quali aree del layout NEWS/Koala sarebbero sovrascritte
          // dal SID. Il C64 usera' questa maschera per evitare crash/corruzione.
          uint32_t sidEnd = (uint32_t)SID_load_start + SID_data_size - 1;
          sid_conflicts = 0;
          if (SID_load_start <= 0x07E7 && sidEnd >= 0x0400) sid_conflicts |= 0x01; // screen
          if (SID_load_start <= 0x0FFF && sidEnd >= 0x0800) sid_conflicts |= 0x02; // charset
          if (SID_load_start <= 0x3F3F && sidEnd >= 0x2000) sid_conflicts |= 0x04; // bitmap
          if (SID_load_start <= 0x8710 && sidEnd >= 0x6000) sid_conflicts |= 0x08; // sorgente Koala
          if (SID_load_start <= 0xBFFF && sidEnd >= 0x8800) sid_conflicts |= 0x10; // NEWS + grafica + SEQ
          if (SID_load_start <= 0xCFFF && sidEnd >= 0xC000) sid_conflicts |= 0x20; // codice
          if (SID_load_start <= 0x9FCF && sidEnd >= 0x9800) sid_conflicts |= 0x40; // menu template
          Serial.printf("SID range: $%04X-$%04X, conflict mask: %02X\n",
                        SID_load_start, (unsigned)(sidEnd & 0xFFFF), sid_conflicts);

          sendByte((byte)(6));
          sendByte((byte)(0)); 
          sendByte(0x3c);    //cassette_buffer    
          sendByte(0x03);

          sendByte((byte)(IS_SID_FILE));
          sendByte((byte)(SID_init));
          sendByte((byte)(SID_init>>8));
          sendByte((byte)(SID_play));
          sendByte((byte)(SID_play>>8));
          sendByte((byte)(sid_conflicts));

          
          
          dataFile.close();       
  }
  }
  
    
  else {
    Serial.print("error opening ");
    Serial.println(fileName);}
  
	  }
      break;}

   //========================================================
    case 54: {          // 54 = Koala precedente
      if (koalaList.empty()) {
        sendZeroBlock();
        break;
      }
      int n = (int)koalaList.size();
      int cur = koalaCurrent;
      if (cur < 0 || cur >= n) cur = 0;
      cur = (cur <= 0) ? n - 1 : cur - 1;
      koalaCurrent = cur;
      slideshowIndex = (cur + 1) % n;
      String targetName = koalaList[cur];
      strncpy(inbuffer, targetName.c_str(), sizeof(inbuffer) - 1);
      inbuffer[sizeof(inbuffer) - 1] = 0;
      IS_SID_FILE = false;
      IS_KOALA_FILE = true;
      goto sendFile;
    }

   //========================================================
    case 68:
    case 69: {          // 68 = next SID, 69 = previous SID
      if (sidList.empty()) {
        sendZeroBlock();
        break;
      }
      int idx = -1;
      for (int i = 0; i < (int)sidList.size(); i++) {
        if (sidList[i].equalsIgnoreCase(currentSidName)) { idx = i; break; }
      }
      if (idx < 0) {
        idx = (ch == 68) ? 0 : (int)sidList.size() - 1;
      } else if (ch == 68) {
        idx = (idx + 1) % (int)sidList.size();
      } else {
        idx = (idx + (int)sidList.size() - 1) % (int)sidList.size();
      }
      currentSidName = sidList[idx];
      strncpy(inbuffer, currentSidName.c_str(), sizeof(inbuffer) - 1);
      inbuffer[sizeof(inbuffer) - 1] = 0;
      IS_SID_FILE = true;
      IS_KOALA_FILE = false;
      goto sidFile;
    }

   //========================================================
     case 55:{   //55 = send SID file to c64
    
   
    
    sendByte((byte)(SID_data_size));
    sendByte((byte)(SID_data_size>>8));
    sendByte((byte)(SID_load_start));
    sendByte((byte)(SID_load_start>>8));

    for (unsigned long x = 0; x < SID_data_size; x++) {
          sendByte(outbuffer[x]);}

    Serial.printf("SID: load $%04X, %lu byte\n", SID_load_start, (unsigned long)SID_data_size);
          
    free(outbuffer);
   
     break;}
    //========================================================
     case 56:{   //56 = schermata SID player a $A000

    buildSidScreen();
    sendScreenTemplate(0xA0);
    
     break;}

     //========================================================         
      case 57:{         // 57 = load charset 1x2
          

          outbuffersize = 2048; 
          sendByte((byte)(outbuffersize));
          sendByte((byte)(outbuffersize>>8));
          sendByte(0x00);
          sendByte(0x08);          // charset a $0800 (libero, fuori dal bitmap)
          for (int x = 0; x < outbuffersize; x++) {
          sendByte(chars[x]);}
          
      break;}

      //======================================================== 
      // 58 = get NEWS dalla fonte corrente
      // 70..75 = scegli la fonte e scarica le NEWS
      // 80..85 = come 70..75 ma invia anche la grafica PETSCII
      case 58:
      case 70 ... 85:
      case 90 ... 105: {
      bool withArt = (ch >= 90);
      if (withArt) newsSourceIndex = ch - 90;
      else if (ch >= 70) newsSourceIndex = ch - 70;
      if (newsSourceIndex < 0 || newsSourceIndex >= NEWS_SOURCE_COUNT) newsSourceIndex = 0;

      const NewsSource &src = NEWS_SOURCES[newsSourceIndex];
      Serial.printf("NEWS: fonte %d '%s'\n", newsSourceIndex, src.name);

      if (WiFi.status() != WL_CONNECTED) {
        Serial.println("NEWS: WiFi not connected");
        sendScrollerMessage("wifi not connected");
        break;
      }

      String headlines = src.useNewsApi
                           ? fetchNewsApiHeadlines()
                           : fetchRssHeadlines(src.url, 2000, 15);

      String newsText = utf8ToScrollerAscii(headlines);
      if (newsText.length() > 2000) newsText.remove(2000);
      if (newsText.length() == 0) newsText = "no news available";

      Serial.print("NEWS: testo: ");
      Serial.println(newsText);
      sendScrollerMessage(newsText, withArt);
      break;
   }

      //========================================================
      case 86:{         // 86 = nuovo art: SEQ da .d64 o pattern generato
      if (loadNextSeqImage()) {
        sendByte((byte)d64SeqLen);
        sendByte((byte)(d64SeqLen >> 8));
        sendByte(0x00);
        sendByte(0xA0);          // $A000: stream PETSCII SEQ
        for (int i = 0; i < d64SeqLen; i++) sendByte(d64SeqData[i]);
      } else {
        sendGeneratedArtBlock();
      }
      break;
   }

      //========================================================
      case 87:{         // 87 = prossima immagine SEQ (slideshow standalone)
      if (!sendSeqStep(1)) sendGeneratedArtBlock();
      break;
   }

      //========================================================
      case 88:{         // 88 = immagine SEQ precedente
      if (!sendSeqStep(-1)) sendGeneratedArtBlock();
      break;
   }

      //========================================================
      case 89:{         // 89 = prima immagine SEQ / avvio slideshow
      seqFileIndex = 0;
      if (!sendSeqStep(0)) sendGeneratedArtBlock();
      break;
   }

      //========================================================
      case 60:{         // 60 = indirizzo di ingresso del PRG appena caricato
        sendByte(2);
        sendByte(0);
        sendByte(0x39);
        sendByte(0x03);
        sendByte((byte)(prgEntry));
        sendByte((byte)(prgEntry >> 8));
        break;}

    //========================================================
      case 112:{        // 112 = bootstrap: EXROM alto (EPROM $8000 off)
        digitalWrite(CLED, LOW);
        sendZeroBlock();
        break;}

    //========================================================
    //========================================================
    }
    }    
}  // end of main loop
