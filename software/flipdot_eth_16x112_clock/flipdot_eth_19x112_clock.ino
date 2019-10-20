#include <SPI.h>
#include <ETH.h>
#include <ESPmDNS.h>
#include <NTPClient.h>
#include "font.h"
#include "terminus12b.h"
#include "tomthumb4x6.h"

#define PANEL_WIDTH  112
#define PANEL_HEIGHT 16

#define OSCDEBUG    (1)

#define BLACK 1
#define WHITE 0

const int pixelCount = PANEL_WIDTH * PANEL_HEIGHT;

unsigned char screenBuffer[PANEL_WIDTH][PANEL_HEIGHT] PROGMEM;
unsigned char screenState[PANEL_WIDTH][PANEL_HEIGHT] PROGMEM;

unsigned char index_to_bitpattern_map[28];

static const int spiClk = 16000000; // 16 MHz

Font* font;
Font* font1;
Font* font2;

MDNSResponder mdns;
// Actual name will be "flipopc.local"
const char myDNSName[] = "flipopc";

// Create an instance of the server
// specify the port to listen on as an argument
WiFiServer server(7890);

static bool eth_connected = false;

//void putChar(unsigned int x, unsigned int y, char c, unsigned char inverted = false, unsigned char* screenBuffer = NULL);
//void writeDots(const char* str, int x = -1, int y = -1, unsigned char inverted = false, unsigned char* screenBuffer = NULL);
    

//uninitalised pointers to SPI objects
SPIClass * hspi = NULL;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

#define LATCH   15
#define FIRE    32
#define BL      33

void setup() {
  //initialise two instances of the SPIClass attached to VSPI and HSPI respectively
  hspi = new SPIClass(HSPI);
  hspi->begin();
  
  pinMode(LATCH, OUTPUT);
  pinMode(FIRE, OUTPUT);
  pinMode(BL, OUTPUT);

  Serial.begin(115200);
  Serial.println("Start");

  for(int i = 0; i < 28; i++) {
    index_to_bitpattern_map[i] = index_to_bitpattern(i);
  }

  hspi->beginTransaction(SPISettings(spiClk, MSBFIRST, SPI_MODE0));
  Serial.println("Start");


  WiFi.onEvent(WiFiEvent);
  ETH.begin(1);

  digitalWrite(BL, HIGH);

  delay(100);
  flip();

  font1 = new Font(10,18, (const uint16_t*)font_terminus_10x12);
  font2 = new Font(  4, 6, (const uint16_t*)font_tom_thumb_4x6);
  font  = font1;
}

int lastMinute = 0;

void clearDots(unsigned char color /*= BLACK*/ ) {
  for(int y = 0; y < PANEL_HEIGHT; y++) {
    for(int x = 0; x < PANEL_WIDTH; x++) {
      putPixel(x,y, color);
    }
  }
}

void loop() {
  //clientEvent();

  /*for(int y = 0; y < PANEL_HEIGHT; y++) {
    for(int x = 0; x < PANEL_WIDTH; x++) {
      screenBuffer[x][y] = 1;
      //screenState[x][y] = 0;
    }
  }
  flip();

  for(int y = 0; y < PANEL_HEIGHT; y++) {
    for(int x = 0; x < PANEL_WIDTH; x++) {
      screenBuffer[x][y] = 0;
      //screenState[x][y] = 0;
    }
  }
  flip();*/

  while(!timeClient.update()) {
    timeClient.forceUpdate();
  }

  int currentHour = timeClient.getHours();
  int currentMin = timeClient.getMinutes();
  int currentSec = timeClient.getSeconds();
  if (currentMin != lastMinute) {
    lastMinute = currentMin;
    #define ARRAYSIZE 60
    int hour = 3;
    //minute ++;
    const String time[ARRAYSIZE] = {" ", "eins", "zwei", "drei", "vier", "fuenf", "sechs", "sieben", "acht", "neun", "zehn", "elf", "zwoelf", "dreizehn", "vierzehn", "fuenfzehn", "sechszehn", "siebzehn", "achtzehn", "neunzehn", "zwanzig", "einundzwanzig", "zweiundzwanzig", "dreiundzwanzig", "vierundzwanzig", "fuenfundzwanzig", "sechsundzwanzig", "siebenundzwanzig", "achtundzwanzig", "neunundzwanzig", "dreissig", "einunddreissig", "zweiunddreissig", "dreiunddreissig", "vierunddreissig", "fuenfunddreissig", "sechunddreissig", "siebenunddreissig", "achtunddreissig", "neununddreissig", "vierzig", "einundvierzig", "zweiundvierzig", "dreiundvierzig", "vierundvierzig", "fuenfundvierzig", "sechsundvierzig", "siebenundvierzig", "achtundvierzig", "neunundvierzig", "fuenfzig", "einundfuenfzig", "zweiundfuenfzig", "dreiundfuenfzig", "vierundfuenfzig", "fuenfundfuenfzig", "sechsundfuenfzig", "siebenundfuenfzig", "achtundfuenfzig", "neunundfuenfzig"};
    String speaktime = "Es ist " + time[currentHour] + " Uhr " + time[currentMin];
    char __dataFileName[100];
    speaktime.toCharArray(__dataFileName, 100);
    setFont(1);
    for (int i = 28; i > -400; i--) {
      writeDots(__dataFileName, i, -2, 0);
      flip();
      if ((-i+28) > 250) {
        delay((-i+28-250)/10);
      }
    }

    char logString[8];
    sprintf(logString,"%2d:%02d",currentHour,currentMin);

    setFont(1);

    clearDots(BLACK);

    writeDots(logString, 30, -1, 0);
    flip();
    redraw();
    redraw();
  }
  delay(1000);
  redraw();
}


#define minsize(x,y) (((x)<(y))?(x):(y))
WiFiClient client;


char* ip2CharArray(IPAddress ip) {
  static char a[16];
  sprintf(a, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
  return a;
}


void WiFiEvent(WiFiEvent_t event)
{
  switch (event) {
    case SYSTEM_EVENT_ETH_START:
      Serial.println("ETH Started");
      //set eth hostname here
      ETH.setHostname("esp32-ethernet");
      break;
    case SYSTEM_EVENT_ETH_CONNECTED:
      Serial.println("ETH Connected");
      break;
    case SYSTEM_EVENT_ETH_GOT_IP:
      Serial.print("ETH MAC: ");
      Serial.print(ETH.macAddress());
      Serial.print(", IPv4: ");
      Serial.print(ETH.localIP());
      writeDots(ip2CharArray(ETH.localIP()), 0, -1, 0);
      flip();

      delay(2000);
      if (ETH.fullDuplex()) {
        Serial.print(", FULL_DUPLEX");
      }
      Serial.print(", ");
      Serial.print(ETH.linkSpeed());
      Serial.println("Mbps");
      if (!MDNS.begin(myDNSName)) {
        Serial.println("Error setting up MDNS responder!");
      }
      else {
        Serial.println("mDNS responder started");
        Serial.printf("My name is [%s]\r\n", myDNSName);
      }

      //server.begin();
      //Serial.println("Server listening on port 7890");+

      timeClient.begin();
      // Set offset time in seconds to adjust for your timezone, for example:
      // GMT +1 = 3600
      // GMT +8 = 28800
      // GMT -1 = -3600
      // GMT 0 = 0
      timeClient.setTimeOffset(7200);
      
      eth_connected = true;
      break;
    case SYSTEM_EVENT_ETH_DISCONNECTED:
      Serial.println("ETH Disconnected");
      eth_connected = false;
      break;
    case SYSTEM_EVENT_ETH_STOP:
      Serial.println("ETH Stopped");
      eth_connected = false;
      break;
    default:
      break;
  }
}

void setDot(unsigned int x, unsigned int y, unsigned char color) {
  if(x < 0 || y < 0 || x >= PANEL_WIDTH || y >= PANEL_HEIGHT || color > 1 || screenState[x][y] == color) {
    //delayMicroseconds(50);
    return;
  }
  screenState[x][y] = color;


  uint8_t byte0 = 0, byte1 = 0;
  for(int i = 0; i < 5; i++) {
    bitWrite(byte0, i, index_to_bitpattern_map[y]&(1<<i));
    bitWrite(byte1, i, index_to_bitpattern_map[x%28]&(1<<i));
  }
  
  bitWrite(byte0, 5, ((x / 28) >> 0) & 1);
  bitWrite(byte0, 6, ((x / 28) >> 1) & 1);
  bitWrite(byte0, 7, ((x / 28) >> 2) & 1);
  
  bitWrite(byte1, 7, color);

  //hspi->beginTransaction(SPISettings(spiClk, MSBFIRST, SPI_MODE0));
  hspi->transfer(byte1);
  hspi->transfer(byte0);
  //hspi->endTransaction();

  digitalWrite(LATCH, HIGH);
  digitalWrite(FIRE, HIGH);
  delayMicroseconds(250);
  digitalWrite(LATCH, LOW);
  digitalWrite(FIRE, LOW);
  //delayMicroseconds(7);
}

void putPixel(unsigned int x, unsigned int y, unsigned char color) {
  if(x < 0 || y < 0 || x >= PANEL_WIDTH || y >= PANEL_HEIGHT) return;
  screenBuffer[x][y] = color;
}

void putCharDots(unsigned int x, unsigned int y, char c, unsigned char inverted) {
  for(int _x = 0; _x < font->getCharWidth(c); _x++) {
    for(int _y = 0; _y < font->getCharHeight(); _y++) {
      if(!inverted) {
        putPixel(x+_x, y+_y, (font->getCharBit(c,_x,_y)) ? WHITE : BLACK);
      }
      else {
        putPixel(x+_x, y+_y, (font->getCharBit(c,_x,_y)) ? BLACK : WHITE);
      }
    }
  }
}

void writeDots(const char* str, int x, int y, unsigned char inverted) {
  int strlen = 0;
  int strwidth = 0;
  unsigned int index = 0;
  while(str[strlen] != '\0') { strwidth += font->getCharWidth(str[strlen++]); };

  int _x = x;
  int _y = y;

  /*if(x < 0)
    _x = -1 + PANEL_WIDTH/2 - (strwidth/2);
  if(y < 0)
    _y = (PANEL_HEIGHT-font->getCharHeight())/2;*/

  char c;
  while((c = str[index++]) != '\0') {
    putCharDots(_x, _y, c, inverted);
    _x += font->getCharWidth(c);
  }
}


void setFont(int fontIndex) {
  switch(fontIndex) {
    case 1: font = font1; break;
    case 2: font = font2; break;
  }
}


void redraw() {
  for(int i = 0; i < PANEL_HEIGHT; i++) {
    for(int j = 0; j < PANEL_WIDTH; j++) {
      screenBuffer[j][i] = screenState[j][i];
      screenState[j][i] = -1;
    }
  }
  flip();
}

void flip() {
  for(int x = 0; x < PANEL_WIDTH; x++) {
    for(int y = 0; y < PANEL_HEIGHT; y++) {
      setDot(x,y,screenBuffer[x][y]);
    }
  }
}

int index_to_bitpattern(int index) {
  return 1+index + (index/7);
}
