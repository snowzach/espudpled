#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>
#include <Adafruit_GFX.h>
#include <Adafruit_NeoMatrix.h>
#include <Adafruit_NeoPixel.h>
#include <EEPROM.h>

#define PIN 3

#define MATRIX_ROWS 16
#define MATRIX_COLS 16
#define NUM_PIXELS (MATRIX_ROWS * MATRIX_COLS)
#define TPM2NET_LISTENING_PORT 65506
#define UDP_PACKET_SIZE 1500
uint8_t packetBuffer[UDP_PACKET_SIZE];
int led_index = 0;

#define CONFIG_START 32
#define BUFFER_SIZE 33
struct Settings {
  char ssid[BUFFER_SIZE];
  char password[BUFFER_SIZE];
  char eeprom[4];
} settings = {
  "None",
  "None",
  "eul"
};

WiFiUDP Client;

// Setup the Matrix
Adafruit_NeoMatrix matrix = Adafruit_NeoMatrix(MATRIX_ROWS, MATRIX_COLS, PIN,
  NEO_MATRIX_TOP  + NEO_MATRIX_LEFT +
  NEO_MATRIX_ROWS + NEO_MATRIX_PROGRESSIVE,
  NEO_GRB         + NEO_KHZ800);

void loadConfig() {
  // To make sure there are settings, and they are YOURS!
  // If nothing is found it will use the default settings.
  if (//EEPROM.read(CONFIG_START + sizeof(settings) - 1) == settings.eeprom[3] // this is '\0'
      EEPROM.read(CONFIG_START + sizeof(settings) - 2) == settings.eeprom[2] &&
      EEPROM.read(CONFIG_START + sizeof(settings) - 3) == settings.eeprom[1] &&
      EEPROM.read(CONFIG_START + sizeof(settings) - 4) == settings.eeprom[0]) { 
        // reads settings from EEPROM
        for (unsigned int t=0; t<sizeof(settings); t++) {
          *((char*)&settings + t) = EEPROM.read(CONFIG_START + t);
        }
  } else {
    // settings aren't valid! will overwrite with default settings
    Serial.println("Initializing EEPROM");
    saveConfig();
  }
}

void saveConfig() {
  for (unsigned int t=0; t<sizeof(settings); t++) { 
    // writes to EEPROM
    EEPROM.write(CONFIG_START + t, *((char*)&settings + t));
    // and verifies the data
    if (EEPROM.read(CONFIG_START + t) != *((char*)&settings + t))
    {
      // error writing to EEPROM
      Serial.println("Error verifying EEPROM settings.");
      while(true);
    }
  }
}

void setup(void) {
  Serial.begin(115200);
  EEPROM.begin(512);
  loadConfig();

  String input;

  while(true) {
    
    Serial.printf("\nPress any key within 5 seconds to change SSID(%s):", settings.ssid);
    delay(5000);
    if(Serial.available()) {
      // Clear the buffer
      Serial.readString();

      // Read SSID
      Serial.setTimeout(30000);
      Serial.println();
      Serial.println("Input New SSID:");
      input = Serial.readStringUntil('\n');
      if(input.length()) {
        input.toCharArray(settings.ssid, BUFFER_SIZE);
        Serial.printf("\nWiFi SSID:'%s'\n", settings.ssid);
        Serial.println();
        // Read Password
        Serial.println("Input New PW:");
        input = Serial.readStringUntil('\n');
        if(input.length()) {
          input.toCharArray(settings.password, BUFFER_SIZE);
          Serial.printf("\nWiFi PW:'%s'\n", settings.password);
          Serial.println();
          Serial.println("New SSID PW:");
        } else {
          continue;
        }
      } else {
        continue;
      }
    }
  
    //WIFI INIT
    Serial.printf("\nConnecting to %s\n", settings.ssid);
    if (String(WiFi.SSID()) != String(settings.ssid)) {
      WiFi.begin(settings.ssid, settings.password);
    }

    unsigned long start_time = millis();
    while (WiFi.status() != WL_CONNECTED) {
      delay(1000);
      Serial.print(".");
      if(millis() - start_time > 25000) {
        break;
      }
    }

    // Breakout, ready to go
    if(WiFi.status() == WL_CONNECTED) {
      saveConfig();
      EEPROM.end();
      break;
    }
  }

    
  Serial.println("");
  Serial.print("Connected! IP address: ");
  Serial.println(WiFi.localIP());

  // Listen to Port
  Client.begin(TPM2NET_LISTENING_PORT);
  // Init Matrix
  matrix.begin();
  matrix.fillScreen(0);
  matrix.show();
}

void loop(void) {
  int packetSize = Client.parsePacket();
  if (packetSize) {
    // Read into the buffer
    Client.read(packetBuffer, UDP_PACKET_SIZE);
    // Parse Header
    byte blocktype = packetBuffer[1]; // block type (0xDA)
    unsigned int framelength = ((unsigned int)packetBuffer[2] << 8) | (unsigned int)packetBuffer[3]; // frame length (0x0069) = 105 leds
    byte packetnum = packetBuffer[4];        // packet number 0-255 0x00 = no frame split (0x01)
    byte numpackets = packetBuffer[5];       // total packets 1-255 (0x01)
//    Serial.print("L:");
//    Serial.println(framelength); // chan/block

    // If this is a datablock
    if (blocktype == 0xDA) {
      int packetindex;
      if (packetSize >= framelength + 7 && packetBuffer[6 + framelength] == 0x36) { 
        // header end (packet stop)
        //Serial.println("s:");
        int i = 0;
        packetindex = 6; 
        if(packetnum == 1) {
          led_index = 0;
        }
        while(packetindex < (framelength + 6)) {
          matrix.setPixelColor(led_index, packetBuffer[packetindex], packetBuffer[packetindex+1], packetBuffer[packetindex+2]);
//          Serial.printf("L:%i C(%i,%i,%i)\n", led_index, packetBuffer[packetindex], packetBuffer[packetindex+1], packetBuffer[packetindex+2]);
          led_index++;
          packetindex +=3;
        }
      }
//      Serial.printf("P:%i/%i\n", packetnum, numpackets);
    }

    // If it's the last packet, show it on the matrix
    if((packetnum == numpackets) && (led_index == NUM_PIXELS)) {
      matrix.show();
      led_index==0;
    }  
  }
}
