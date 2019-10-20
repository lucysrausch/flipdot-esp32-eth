#include <SPI.h>
#include <ETH.h>
#include <ESPmDNS.h>


#define PANEL_WIDTH  112
#define PANEL_HEIGHT 16

#define OSCDEBUG    (0)

const int pixelCount = PANEL_WIDTH * PANEL_HEIGHT;

unsigned char screenBuffer[PANEL_WIDTH][PANEL_HEIGHT] PROGMEM;
unsigned char screenState[PANEL_WIDTH][PANEL_HEIGHT] PROGMEM;

unsigned char index_to_bitpattern_map[28];

static const int spiClk = 16000000; // 16 MHz

MDNSResponder mdns;
// Actual name will be "flipopc.local"
const char myDNSName[] = "flipopc";

// Create an instance of the server
// specify the port to listen on as an argument
WiFiServer server(7890);

static bool eth_connected = false;

//uninitalised pointers to SPI objects
SPIClass * hspi = NULL;

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

  for(int y = 0; y < PANEL_HEIGHT; y++) {
    for(int x = 0; x < PANEL_WIDTH; x++) {
      screenBuffer[x][y] = 1;
      screenState[x][y] = 0;
      setDot(x,y, 0);
    }
  }

  WiFi.onEvent(WiFiEvent);
  ETH.begin(1);

  digitalWrite(BL, HIGH);

  delay(100);
  flip();
}
  

void loop() {
  clientEvent();
}


#define minsize(x,y) (((x)<(y))?(x):(y))
WiFiClient client;

void clientEvent()
{
  static int packetParse = 0;
  static uint8_t pktChannel, pktCommand;
  static uint16_t pktLength, pktLengthAdjusted, bytesIn;
  static uint8_t pktData[pixelCount*3];
  uint16_t bytesRead;
  size_t frame_count = 0, frame_discard = 0;

  if (!client) {
    // Check if a client has connected
    client = server.available();
    if (!client) {
      return;
    }
    Serial.println("new OPC client");
  }

  if (!client.connected()) {
    Serial.println("OPC client disconnected");
    client = server.available();
    if (!client) {
      return;
    }
  }


  while (client.available()) {
    switch (packetParse) {
      case 0: // Get pktChannel
        pktChannel = client.read();
        packetParse++;
        #if OSCDEBUG
          Serial.printf("pktChannel %u\r\n", pktChannel);
        #endif
        break;
      case 1: // Get pktCommand
        pktCommand = client.read();
        packetParse++;
        #if OSCDEBUG
          Serial.printf("pktCommand %u\r\n", pktCommand);
        #endif
        break;
      case 2: // Get pktLength (high byte)
        pktLength = client.read() << 8;
        packetParse++;
        #if OSCDEBUG
          Serial.printf("pktLength high byte %u\r\n", pktLength);
        #endif
        break;
      case 3: // Get pktLength (low byte)
        pktLength = pktLength | client.read();
        packetParse++;
        bytesIn = 0;
        #if OSCDEBUG
          Serial.printf("pktLength %u\r\n", pktLength);
        #endif
        if (pktLength > sizeof(pktData)) {
          Serial.println("Packet length exceeds size of buffer! Data discarded");
          pktLengthAdjusted = sizeof(pktData);
        }
        else {
          pktLengthAdjusted = pktLength;
        }
        break;
      case 4: // Read pktLengthAdjusted bytes into pktData
        bytesRead = client.read(&pktData[bytesIn],
            minsize(sizeof(pktData), pktLengthAdjusted) - bytesIn);
        bytesIn += bytesRead;
        if (bytesIn >= pktLengthAdjusted) {
          if ((pktCommand == 0) && (pktChannel <= 1)) {
            int i;
            uint8_t *pixrgb;
            pixrgb = pktData;
            for (i = 0; i < minsize((pktLengthAdjusted / 3), pixelCount); i++) {
              uint8_t color = *pixrgb++ < 127;
              *pixrgb++;
              *pixrgb++;
              putPixel(i % PANEL_WIDTH, i / PANEL_WIDTH, color);
              //strip.SetPixelColor(i,
              //    RgbColor(GammaLUT[*pixrgb++],
              //             GammaLUT[*pixrgb++],
              //             GammaLUT[*pixrgb++]));
            }
            // Display only the first frame in this cycle. Buffered frames
            // are discarded.
            if (frame_count == 0) {
              #if OSCDEBUG
                Serial.print("=");
                unsigned long startMicros = micros();
              #endif

              flip();
    
              #if OSCDEBUG
                Serial.printf("%lu\r\n", micros() - startMicros);
              #endif
            }
            else {
              frame_discard++;
            }
            frame_count++;
          }
          if (pktLength == pktLengthAdjusted)
            packetParse = 0;
          else
            packetParse++;
        }
        break;
      default:  // Discard data that does not fit in pktData
        bytesRead = client.read(pktData, pktLength - bytesIn);
        bytesIn += bytesRead;
        if (bytesIn >= pktLength) {
          packetParse = 0;
        }
        break;
    }
    Serial.print("#");
  }
  #if OSCDEBUG
    if (frame_discard) {
      Serial.printf("discard %u\r\n", frame_discard);
    }
  #endif
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

      server.begin();
      Serial.println("Server listening on port 7890");
      
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
