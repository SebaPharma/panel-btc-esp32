#include <Arduino.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <AnimatedGIF.h>
#include <ArduinoOTA.h>

#include "secrets.h"
#include "gif_data.h"

// Valores por defecto si tu secrets.h no los define (ver include/secrets.example.h)
#ifndef CITY_1_LABEL
#define CITY_1_LABEL "Santiago"
#define CITY_1_QUERY "Santiago"
#endif
#ifndef CITY_2_LABEL
#define CITY_2_LABEL "Valpo"
#define CITY_2_QUERY "Valparaiso"
#endif
#ifndef SHOW_WIFI_NAME
#define SHOW_WIFI_NAME 0
#endif

constexpr unsigned long kBTC_RefreshMs = 5000;
constexpr unsigned long kCLP_RefreshMs = 600000;
constexpr unsigned long kWeather_RefreshMs = 900000;
constexpr unsigned long kGIF_ChangeMs = 10000;
constexpr const char* kBleDeviceName = "BTC-Auto";
constexpr char kServiceUuid[] = "4fafc201-1fb5-459e-8fcc-c5c9c331914b";
constexpr char kPriceCharUuid[] = "beb5483e-36e1-4688-b7f5-ea07361b26a8";

TFT_eSPI tft;
WiFiMulti wifiMulti;
BLEServer* bleServer = nullptr;
AnimatedGIF gif;

String btcPrice = "";
String usdPrice = "";
String eurPrice = "";
String ufPrice = "";
String city1Weather = "";
String city2Weather = "";

unsigned long lastBTC_FetchMs = 0;
unsigned long lastCLP_FetchMs = 0;
unsigned long lastWeather_FetchMs = 0;
unsigned long lastGIF_ChangeMs = 0;
bool bleConnected = false;

bool btcPulseActive = false;
unsigned long btcPulseStartMs = 0;
unsigned long lastAnimMs = 0;
int hudBrightness = 0;
bool gifIsOpen = false;

#define GIF_X 176
#define GIF_Y 5
#define GIF_WIDTH 64
#define GIF_HEIGHT 64

// La lista de GIF sale de data/ (tools/convert_gif.py genera gif_data.h)
int currentGIF = 0;

const char* kaomojiFaces[] = {
  "=^_^=", "Q('.'Q)", "<('.'<)", "(>-'.'-)>", "^( 'O' )^",
  "<(o.O)>", "(^._.^)", "(@_@)", "(-_-)", "*\\ (^_^)",
  "(//_^)", "c[_]c", "(^-^)", "(>_<)", "(^o^)",
  "(T_T)", "(¬_¬)", "(^_^)9", "=^..^=", "(*^_^*)"
};

const int kaomojiCount = sizeof(kaomojiFaces) / sizeof(kaomojiFaces[0]);
uint16_t kaomojiColors[] = {TFT_CYAN, TFT_RED, TFT_MAGENTA}; 

int currentKaomojiFace = 0;
int currentKaomojiColor = 0;
unsigned long lastFaceChangeMs = 0;
unsigned long lastColorChangeMs = 0;

String formatCLP(const String& raw) {
  double value = raw.toDouble();
  if (value <= 0.0) return raw;
  char buffer[24];
  if (value >= 1000.0) {
    snprintf(buffer, sizeof(buffer), "$%.0f", value);
  } else {
    snprintf(buffer, sizeof(buffer), "$%.2f", value);
  }
  return String(buffer);
}

void drawCentered(const String& text, int y, uint16_t color, uint8_t font) {
  tft.setTextColor(color, TFT_BLACK);
  tft.setTextFont(font);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(text, tft.width() / 2, y);
}

void drawLeft(const String& text, int x, int y, uint16_t color, uint8_t font) {
  tft.setTextColor(color, TFT_BLACK);
  tft.setTextFont(font);
  tft.setTextDatum(TL_DATUM);
  tft.drawString(text, x, y);
}

void drawHUDCorners() {
  uint16_t color = (hudBrightness == 0) ? TFT_DARKCYAN : TFT_CYAN;
  int len = 20;
  tft.drawLine(5, 5, 5 + len, 5, color);
  tft.drawLine(5, 5, 5, 5 + len, color);
  tft.drawLine(235 - len, 5, 235, 5, color);
  tft.drawLine(235, 5, 235, 5 + len, color);
  tft.drawLine(5, 235, 5 + len, 235, color);
  tft.drawLine(5, 235 - len, 5, 235, color);
  tft.drawLine(235 - len, 235, 235, 235, color);
  tft.drawLine(235, 235 - len, 235, 235, color);
}

void drawKaomoji() {
  tft.fillRect(125, 15, 110, 25, TFT_BLACK);
  uint16_t color = kaomojiColors[currentKaomojiColor];
  tft.setTextColor(color, TFT_BLACK);
  tft.setTextFont(2); 
  tft.setTextDatum(TL_DATUM);
  tft.drawString(kaomojiFaces[currentKaomojiFace], 125, 15);
}

void drawBTCPulse() {
  if (!btcPulseActive) return;
  unsigned long elapsed = millis() - btcPulseStartMs;
  if (elapsed > 500) {
    btcPulseActive = false;
    return;
  }
  uint16_t pulseColor = (elapsed < 250) ? TFT_CYAN : TFT_GREEN;
  tft.setTextColor(pulseColor, TFT_BLACK);
  tft.setTextFont(6); 
  tft.setTextDatum(MC_DATUM);
  tft.drawString(formatCLP(btcPrice), tft.width() / 2, 85);
}

void GIFDraw(GIFDRAW *pDraw) {
  uint8_t *s = pDraw->pPixels;
  uint16_t *d = (uint16_t *)malloc(pDraw->iWidth * 2);
  if (d == nullptr) return;
  
  uint16_t *pPalette = (uint16_t *)pDraw->pPalette;
  for (int i = 0; i < pDraw->iWidth; i++) {
    d[i] = pPalette[s[i]];
  }
  tft.pushImage(GIF_X + pDraw->iX, GIF_Y + pDraw->y, pDraw->iWidth, 1, d);
  free(d);
}

void updateGIF() {
  if (!gifIsOpen) {
    if (gif.open((uint8_t*)GIFS[currentGIF].data, GIFS[currentGIF].len, GIFDraw)) {
      gifIsOpen = true;
    }
  } else {
    if (!gif.playFrame(true, NULL)) {
      gif.close();
      gifIsOpen = false;
    }
  }
}

void updateScreen() {
  tft.fillScreen(TFT_BLACK);
  drawHUDCorners();
  drawLeft("DASHBOARD", 15, 25, TFT_CYAN, 2);
  drawKaomoji();
  
  tft.drawLine(10, 50, 230, 50, TFT_DARKCYAN);
  drawCentered("BTC / USDT", 65, TFT_WHITE, 2);
  if (btcPrice == "") {
    drawCentered("Cargando...", 90, TFT_YELLOW, 4);
  } else {
    drawCentered(formatCLP(btcPrice), 90, TFT_GREEN, 6);
  }
  
  tft.drawLine(10, 120, 230, 120, TFT_DARKCYAN);
  drawLeft("USD:", 15, 130, TFT_WHITE, 2);
  drawLeft(usdPrice == "" ? "..." : formatCLP(usdPrice), 60, 130, TFT_GREEN, 2);
  drawLeft("EUR:", 130, 130, TFT_WHITE, 2);
  drawLeft(eurPrice == "" ? "..." : formatCLP(eurPrice), 175, 130, TFT_GREEN, 2);
  drawLeft("UF:", 15, 155, TFT_WHITE, 2);
  drawLeft(ufPrice == "" ? "..." : formatCLP(ufPrice), 60, 155, TFT_GREEN, 2);
  
  tft.drawLine(10, 180, 230, 180, TFT_DARKCYAN);
  drawLeft(String(CITY_1_LABEL) + ":", 15, 190, TFT_WHITE, 2);
  drawLeft(city1Weather == "" ? "..." : city1Weather, 90, 190, TFT_CYAN, 2);
  drawLeft(String(CITY_2_LABEL) + ":", 15, 215, TFT_WHITE, 2);
  drawLeft(city2Weather == "" ? "..." : city2Weather, 90, 215, TFT_CYAN, 2);
  
  tft.setTextFont(1);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.setTextDatum(BC_DATUM);
  
  if (bleConnected) {
    tft.drawString("BLE ACTIVO", tft.width() / 2, 235);
  } else if (WiFi.status() == WL_CONNECTED) {
#if SHOW_WIFI_NAME
    tft.drawString("WiFi: " + WiFi.SSID(), tft.width() / 2, 235);
#else
    tft.drawString("WiFi OK", tft.width() / 2, 235);
#endif
  } else {
    tft.drawString("SIN WIFI", tft.width() / 2, 235);
  }
}

void fetchBTC() {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  if (http.begin(client, "https://api.binance.com/api/v3/ticker/price?symbol=BTCUSDT")) {
    if (http.GET() == HTTP_CODE_OK) {
      JsonDocument doc;
      if (deserializeJson(doc, http.getString()) == DeserializationError::Ok) {
        String newPrice = doc["price"].as<String>();
        if (newPrice != btcPrice) {
          btcPrice = newPrice;
          btcPulseActive = true;
          btcPulseStartMs = millis();
        }
      }
    }
    http.end();
  }
}

void fetchCLP() {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  if (http.begin(client, "https://mindicador.cl/api")) {
    if (http.GET() == HTTP_CODE_OK) {
      JsonDocument doc;
      if (deserializeJson(doc, http.getString()) == DeserializationError::Ok) {
        usdPrice = doc["dolar"]["valor"].as<String>();
        eurPrice = doc["euro"]["valor"].as<String>();
        ufPrice = doc["uf"]["valor"].as<String>();
      }
    }
    http.end();
  }
}

void fetchWeather(const String& city, String& weatherOut) {
  WiFiClient client;
  HTTPClient http;
  String url = "http://wttr.in/" + city + "?format=%t|%C&m";
  if (http.begin(client, url)) {
    if (http.GET() == HTTP_CODE_OK) {
      String response = http.getString();
      response.trim();
      int separator = response.indexOf('|');
      if (separator > 0) {
        String temp = response.substring(0, separator);
        String desc = response.substring(separator + 1);
        String descShort = "Variable";
        desc.toLowerCase();
        if (desc.indexOf("sunny") >= 0 || desc.indexOf("clear") >= 0) descShort = "Despejado";
        else if (desc.indexOf("partly") >= 0 || desc.indexOf("cloudy") >= 0) descShort = "Nublado";
        else if (desc.indexOf("rain") >= 0 || desc.indexOf("drizzle") >= 0) descShort = "Lluvia";
        else if (desc.indexOf("snow") >= 0 || desc.indexOf("fog") >= 0) descShort = "Nieve/Niebla";
        else if (desc.indexOf("overcast") >= 0) descShort = "Cubierto";
        weatherOut = temp + " " + descShort;
      } else {
        weatherOut = "Error";
      }
    } else {
      weatherOut = "Error";
    }
    http.end();
  } else {
    weatherOut = "Error";
  }
}

class PriceCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* characteristic) override {
    String value = characteristic->getValue().c_str();
    value.trim();
    value.replace("$", "");
    value.replace(",", "");
    if (value.length() > 0 && value.toDouble() > 0) {
      if (value != btcPrice) {
        btcPrice = value;
        btcPulseActive = true;
        btcPulseStartMs = millis();
        updateScreen();
      }
    }
  }
};

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* server) override {
    bleConnected = true;
    updateScreen();
  }
  void onDisconnect(BLEServer* server) override {
    bleConnected = false;
    updateScreen();
    BLEDevice::startAdvertising();
  }
};

void setupBLE() {
  BLEDevice::init(kBleDeviceName);
  bleServer = BLEDevice::createServer();
  bleServer->setCallbacks(new ServerCallbacks());
  BLEService* service = bleServer->createService(kServiceUuid);
  BLECharacteristic* priceChar = service->createCharacteristic(
      kPriceCharUuid,
      BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
  priceChar->setCallbacks(new PriceCallbacks());
  service->start();
  BLEAdvertising* advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(kServiceUuid);
  advertising->setScanResponse(true);
  BLEDevice::startAdvertising();
}

void setup() {
  Serial.begin(115200);
  delay(500);

  gif.begin(BIG_ENDIAN_PIXELS);

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  updateScreen();

  setupBLE();

  WiFi.mode(WIFI_STA);
  for (size_t i = 0; i < WIFI_NETWORK_COUNT; i++) {
    wifiMulti.addAP(WIFI_NETWORKS[i].ssid, WIFI_NETWORKS[i].password);
  }

  Serial.println("Conectando WiFi...");
  unsigned long startAttemptTime = millis();
  while (millis() - startAttemptTime < 10000) {
    if (wifiMulti.run() == WL_CONNECTED) {
      Serial.println("WiFi: " + WiFi.SSID());
      break;
    }
    delay(500);
  }
  
  // --- ACTUALIZACIÓN POR WIFI (OTA) ---
  // Solo se activa si definiste OTA_PASSWORD en secrets.h
#ifdef OTA_PASSWORD
  if (WiFi.status() == WL_CONNECTED) {
    ArduinoOTA.setHostname("btc-auto-esp32"); // Nombre para identificarlo en la red
    ArduinoOTA.setPassword(OTA_PASSWORD);
    
    ArduinoOTA.onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH) type = "sketch";
      else type = "filesystem";
      Serial.println("Iniciando OTA: " + type);
      tft.fillScreen(TFT_BLUE);
      tft.setTextColor(TFT_WHITE);
      tft.drawString("Actualizando...", 10, 10);
    });
    ArduinoOTA.onEnd([]() {
      Serial.println("\nOTA Finalizado. Reiniciando...");
      tft.fillScreen(TFT_GREEN);
      tft.drawString("Listo!", 10, 10);
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progreso: %u%%\r", (progress / (total / 100)));
    });
    ArduinoOTA.onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });
    
    ArduinoOTA.begin();
    Serial.println("OTA Listo. Hostname: btc-auto-esp32");
  }
#endif

  if (WiFi.status() == WL_CONNECTED) {
    fetchBTC();
    fetchCLP();
    fetchWeather(CITY_1_QUERY, city1Weather);
    fetchWeather(CITY_2_QUERY, city2Weather);
  }
  
  lastBTC_FetchMs = millis();
  lastCLP_FetchMs = millis();
  lastWeather_FetchMs = millis();
  lastGIF_ChangeMs = millis();
  updateScreen();
}

void loop() {
  unsigned long now = millis();
  
  // Manejo de OTA (debe ir en el loop)
#ifdef OTA_PASSWORD
  if (WiFi.status() == WL_CONNECTED) {
    ArduinoOTA.handle();
  }
#endif

  updateGIF();
  
  if (now - lastGIF_ChangeMs >= kGIF_ChangeMs) {
    currentGIF = (currentGIF + 1) % GIF_COUNT;
    lastGIF_ChangeMs = now;
    gifIsOpen = false;
    Serial.println("Cambiando a GIF: " + String(currentGIF));
  }
  
  if (now - lastAnimMs >= 1000) {
    hudBrightness = 1 - hudBrightness;
    drawHUDCorners();
    lastAnimMs = now;
  }
  
  if (now - lastColorChangeMs >= 1500) {
    currentKaomojiColor = (currentKaomojiColor + 1) % 3;
    drawKaomoji();
    lastColorChangeMs = now;
  }
  
  if (now - lastFaceChangeMs >= 3000) {
    currentKaomojiFace = (currentKaomojiFace + 1) % kaomojiCount;
    drawKaomoji();
    lastFaceChangeMs = now;
  }
  
  drawBTCPulse();
  
  if (wifiMulti.run() == WL_CONNECTED) {
    if (now - lastBTC_FetchMs >= kBTC_RefreshMs) {
      fetchBTC();
      lastBTC_FetchMs = now;
      updateScreen();
    }
    if (now - lastCLP_FetchMs >= kCLP_RefreshMs) {
      fetchCLP();
      lastCLP_FetchMs = now;
      updateScreen();
    }
    if (now - lastWeather_FetchMs >= kWeather_RefreshMs) {
      fetchWeather(CITY_1_QUERY, city1Weather);
      fetchWeather(CITY_2_QUERY, city2Weather);
      lastWeather_FetchMs = now;
      updateScreen();
    }
  }
  
  delay(10);
  yield();
}