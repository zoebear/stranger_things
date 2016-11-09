#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <Wire.h>
#include <map>

#include "Adafruit_GFX.h"
#include "Adafruit_SSD1306.h"
#include "WebSocketsClient.h"
#include "Hash.h"
#include "ArduinoJson.h"
#include "Adafruit_NeoPixel.h"

#define WIFI_SSID       "" //SSID goes here
#define WIFI_PASSWORD   "" //password goes here
#define SLACK_TOKEN     "" // Follow https://my.slack.com/services/new/bot to create a new bot
#define SLACK_SSL_FINGERPRINT "" // If Slack changes their SSL fingerprint, you would need to update this

ESP8266WiFiMulti WiFiMulti;
WebSocketsClient webSocket;

// Which pin on the Arduino is connected to the NeoPixels?
// On a Trinket or Gemma we suggest changing this to 1
#define PIN            13

// How many NeoPixels are attached to the Arduino?
#define NUMPIXELS      30

// When we setup the NeoPixel library, we tell it how many pixels, and which pin to use to send signals.
// Note that for older NeoPixel strips you might need to change the third parameter--see the strandtest
// example for more information on possible values.
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIXELS, PIN, NEO_RGB + NEO_KHZ800);

#define OLED_RESET      16
Adafruit_SSD1306 display(OLED_RESET);

int delayval = 100; // delay for half a second
long nextCmdId = 1;
bool connected = false;

#if (SSD1306_LCDHEIGHT != 64)
#error("Height incorrect, please fix Adafruit_SSD1306.h!");
#endif

std::map<char, int> pixelmap;
char slackmsg[201];
bool updated = false;

void oled_println(const char *msg) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.println(msg);
  display.display();
}

void oled_print(const char *msg) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.println(msg);
  display.display();
}

/**
  Sends a ping message to Slack. Call this function immediately after establishing
  the WebSocket connection, and then every 5 seconds to keep the connection alive.
*/
void sendPing() {
  DynamicJsonBuffer jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["type"] = "ping";
  root["id"] = nextCmdId++;
  String json;
  root.printTo(json);
  webSocket.sendTXT(json);
}

void processSlackMessage(char *payload) {
  Serial.printf("Payload: %s\n", payload);

  StaticJsonBuffer<600> JSONBuffer;
  JsonObject& rootJSON = JSONBuffer.parseObject(payload);

  //Serial.printf("Type of message: %s\n", msgType);
  const char* msgType = rootJSON["type"];

  if (strcmp(msgType,"message") == 0) {
    strncpy(slackmsg, rootJSON["text"], 200);
    updated = true;
  }
}

/**
  Called on each web socket event. Handles disconnection, and also
  incoming messages from slack.
*/
void webSocketEvent(WStype_t type, uint8_t *payload, size_t len) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.printf("[WebSocket] Disconnected :-( \n");
      connected = false;
      break;

    case WStype_CONNECTED:
      Serial.printf("[WebSocket] Connected to: %s\n", payload);
      sendPing();
      break;

    case WStype_TEXT:
      Serial.printf("[WebSocket] Message: %s\n", payload);
      processSlackMessage((char*)payload);
      break;
  }
}

/**
  Establishes a bot connection to Slack:
  1. Performs a REST call to get the WebSocket URL
  2. Connects the WebSocket
  Returns true if the connection was established successfully.
*/
bool connectToSlack() {
  // Step 1: Find WebSocket address via RTM API (https://api.slack.com/methods/rtm.start)
  HTTPClient http;

  Serial.println("Start request");
  http.begin("https://slack.com/api/rtm.start?token=" SLACK_TOKEN, SLACK_SSL_FINGERPRINT);
  int httpCode = http.GET();

  if (httpCode != HTTP_CODE_OK) {
   Serial.printf("HTTP GET failed with code %d\n", httpCode);
   return false;
  }

  // Grab the URL to websocket
  WiFiClient *client = http.getStreamPtr();
  client->find("wss:\\/\\/");
  String host = client->readStringUntil('\\');
  String path = client->readStringUntil('"');
  path.replace("\\/", "/");

  http.end();

  // Step 2: Open WebSocket connection and register event handler
  Serial.println("WebSocket Host=" + host + " Path= " + path);
  webSocket.beginSSL(host, 443, path, "", "");
  webSocket.onEvent(webSocketEvent);

  return true;
}

void setup() {
  Serial.begin(115200);
  //Serial.setDebugOutput(true);

  // top row
  pixelmap['a'] = 29;
  pixelmap['b'] = 28;
  pixelmap['c'] = 27;
  pixelmap['d'] = 26;
  pixelmap['e'] = 25;
  pixelmap['f'] = 24;
  pixelmap['g'] = 23;
  pixelmap['h'] = 22;
  pixelmap['i'] = 21;
  pixelmap['j'] = 20;

  // second row
  pixelmap['t'] = 19;
  pixelmap['s'] = 18;
  pixelmap['r'] = 17;
  pixelmap['q'] = 16;
  pixelmap['p'] = 15;
  pixelmap['o'] = 14;
  pixelmap['n'] = 13;
  pixelmap['m'] = 12;
  pixelmap['l'] = 11;
  pixelmap['k'] = 10;

  // bottom row
  pixelmap['u'] = 9;
  pixelmap['v'] = 8;
  pixelmap['w'] = 7;
  pixelmap['x'] = 6;
  pixelmap['y'] = 5;
  pixelmap['z'] = 4;
  pixelmap[' '] = 3;
  pixelmap['?'] = 2;
  pixelmap[','] = 1;
  pixelmap['.'] = 0;

  pixels.begin(); // This initializes the NeoPixel library.
  pixels.clear();
  pixels.show();

  // by default, we'll generate the high voltage from the 3.3v line internally! (neat!)
  display.begin(SSD1306_SWITCHCAPVCC, 0x3D);  // initialize with the I2C addr 0x3D (for the 128x64)
  // init done
  oled_println("Starting...");

  WiFiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);
  while (WiFiMulti.run() != WL_CONNECTED) {
    oled_println("Waiting for Wifi");
    delay(500);
  }

  oled_println("Syncing time");
  configTime(-8 * 3600, 0, "pool.ntp.org", "time.nist.gov");

  oled_println("Ready");
}

unsigned long lastPing = 0;
int n = 0;
bool start_neopixel = false;

void loop() {
  webSocket.loop();

  if (connected) {
    // Send ping every 5 seconds, to keep the connection alive
    if (millis() - lastPing > 5000) {
      sendPing();
      lastPing = millis();
    }
  } else {
    // Try to connect / reconnect to slack
    connected = connectToSlack();
    if (!connected) {
      delay(500);
    }
  }

  // Display message on OLED
  if (updated) {
    Serial.printf("Message updated: %s\n", slackmsg);
    updated = false;

    // Display message on OLED
    oled_print(slackmsg);

    // Signal start of neopixel loop
    start_neopixel = true;
    n = 0;
  }

  if (start_neopixel) {
    if (n > strlen(slackmsg) - 1) {
      delay(1000);
      start_neopixel = false;
      n = 0;

    } else {

      char c = tolower(slackmsg[n]);
      int pixel = 0;

      Serial.printf("Next character is: %c\n", c);

      if (pixelmap.find(c) != pixelmap.end()) {
        pixel = pixelmap[c];
        Serial.printf("Lighting pixel: %d\n", pixel);
        pixels.clear();
        pixels.setPixelColor(pixel, pixels.Color(255,255,255)); // Moderately bright green color.
        pixels.show(); // This sends the updated pixel color to the hardware.
        delay(500); // Delay for a period of time (in milliseconds).

      } else {
        Serial.println("Non-printable character found, skipping");
      }

      pixels.clear();
      pixels.show();
      delay(200);

      n++;
    }
  }
}
