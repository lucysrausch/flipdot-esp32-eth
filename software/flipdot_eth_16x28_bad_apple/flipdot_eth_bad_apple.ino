#include <SPI.h>
#include "anime.h"


#define PANEL_WIDTH 28
#define PANEL_HEIGHT 16

const int PixelCount = PANEL_WIDTH * PANEL_HEIGHT;

unsigned char screenBuffer[PANEL_WIDTH][PANEL_HEIGHT] PROGMEM;
unsigned char screenState[PANEL_WIDTH][PANEL_HEIGHT] PROGMEM;

unsigned char index_to_bitpattern_map[28];

static const int spiClk = 16000000; // 16 MHz

//uninitalised pointers to SPI objects
SPIClass * hspi = NULL;

#define LATCH   15
#define FIRE    32

void setup() {
  //initialise two instances of the SPIClass attached to VSPI and HSPI respectively
  hspi = new SPIClass(HSPI);
  hspi->begin();
  
  pinMode(LATCH, OUTPUT);
  pinMode(FIRE, OUTPUT);

  Serial.begin(115200);
  Serial.println("Start");

  for(int i = 0; i < 28; i++) {
    index_to_bitpattern_map[i] = index_to_bitpattern(i);
  }

  hspi->beginTransaction(SPISettings(spiClk, MSBFIRST, SPI_MODE0));
  Serial.println("Start");

  for(int y = 0; y < PANEL_HEIGHT; y++) {
    for(int x = 0; x < PANEL_WIDTH; x++) {
      screenBuffer[x][y] = 1;
      screenState[x][y] = 0;
      setDot(x,y, 0);
    }
  }
  
  delay(100);
}

int framecounter = 0;

void loop() {
  //clientEvent();

  while(framecounter < 6500) {
    for (int y = 0; y < 16; y++) {
      for (int x = 0; x < 22; x++) {
        putPixel(x+3,y, !(badapple[((x*16+y)/8)+44*framecounter]>>((x*16+y)%8) & 1));
      }
    }
    framecounter++;
    flip();
    delay(20);
  }

  framecounter = 0;

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
    bitWrite(byte1, i, index_to_bitpattern_map[x]&(1<<i));
  }
  bitWrite(byte1, 7, color);

  //hspi->beginTransaction(SPISettings(spiClk, MSBFIRST, SPI_MODE0));
  hspi->transfer(byte1);
  hspi->transfer(byte0);
  //hspi->endTransaction();

  digitalWrite(LATCH, HIGH);
  digitalWrite(FIRE, HIGH);
  delayMicroseconds(200);
  digitalWrite(LATCH, LOW);
  digitalWrite(FIRE, LOW);
  //delayMicroseconds(7);
}

void putPixel(unsigned int x, unsigned int y, unsigned char color) {
  if(x < 0 || y < 0 || x >= PANEL_WIDTH || y >= PANEL_HEIGHT) return;
  screenBuffer[x][y] = color;
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
