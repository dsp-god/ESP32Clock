#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7789.h> // Hardware-specific library for ST7789
#include <SPI.h>

#include <WiFi.h>
#include <esp_wifi.h>
//#include "driver/adc.h"

#include <TimeLib.h>
#include <ArduinoJson.h>

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <BLE2901.h>

#include <EEPROM.h>


#define WiFiNameAddress         0x000000
#define WiFiPassAddress         0x000032
#define TimeZoneAddress         0x000064
#define FGColorAddress          0x000096
#define BGColorAddress          0x00009A
#define LatitudeAddress         0x00009E
#define LongtitudeAddress       0x0000A2

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define SERVICE_UUID_WiFi "a2ca39b4-9575-42fd-94be-6ddf82fb0c10"
#define SERVICE_UUID_LatLong "e782b8a5-2e0f-4f4f-9fa3-ed98bf7d20ae"
#define SERVICE_UUID_Colors "e353a191-9ad8-4d34-88c8-8248e35fab9c"

#define WIFI_NAME_UUID "9f41898d-0ccf-4327-8e9f-f414b1ef2cbe"
#define WIFI_PASS_UUID "7732ce76-c10f-47c4-b33c-d05f44ad9ce9"
#define LATITUDE_UUID "06956f61-3abd-49be-9f2f-834ef371301f"
#define LONGTITUDE_UUID "61e8086a-b892-4a34-95b6-9e3ba13d2a38"
#define TIMEZONE_UUID "d3e2f8aa-adbb-4b05-951b-f7b5bed54904"
#define FG_COLOR_UUID "deb7fa76-2e32-43dc-89be-8933968cb586"
#define BG_COLOR_UUID "2c8130f4-d127-48ed-9ce9-2a4c498d8614"



// #ifdef ARDUINO_ESP32_DEV
//   #define TFT_CS         7
//   #define TFT_RST        22 // Or set to -1 and connect to Arduino RESET pin
//   #define TFT_DC         19
// #else
  #define TFT_CS         7
  #define TFT_RST        0 // Or set to -1 and connect to Arduino RESET pin
  #define TFT_DC         10
//#endif

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

/* ----- Set from BT ----- */
String wifiSSID = "WiFiName";
String wifiPass = "WiFiPassword";
String timeZone = "Warsaw";
float latitude = 52.237049;
float longtitude = 21.017532;
uint16_t foregroundColor = 2047; //0xffe0; //0xfb56;
uint16_t backgroundColor = 12295; //0x30c6; //0x20a5;
/* ----------------------- */

bool bDrawTimeColumn = true;

std::string weatherStrings[3];

unsigned long weatherCheckMillis = 0;
unsigned long weatherCheckIntervalMillis = 600000; // 10 min



void connectToWiFi() {
  //adc_power_on();
  WiFi.disconnect(false);  // Reconnect the network
  WiFi.mode(WIFI_STA);    // Switch WiFi on

  WiFi.begin(wifiSSID, wifiPass);

  int to = 0;
  while(WiFi.status() != WL_CONNECTED) {
    Serial.print(F("."));
    delay(100);

   to++;
   if(to > 300) {
    Serial.println(F("\nFailed to connect to WiFi!"));
    break;
   }
  }

  Serial.println(F("\nWiFi connected!"));
}

void disconnectFromWiFi() {
    //adc_power_off();
    WiFi.disconnect(true);  // Disconnect from the network
    WiFi.mode(WIFI_OFF);    // Switch WiFi off

  Serial.println(F("\nWiFi disconnected!"));
}

std::string dateTimeHelper(int val) {
  if(val >= 10) {
    return std::to_string(val);
  }
  else {
    std::string result("0");
    result += std::to_string(val);
    return result;
  }
}

void getTimeData() {
  WiFiClient timeClient;
  if(timeClient.connect("worldtimeapi.org", 80)) {
    Serial.println("Connected to time server");
    // Make a HTTP request:
    timeClient.print("GET ");
    timeClient.print("/api/timezone/Europe/");
    timeClient.print(timeZone);
    timeClient.print(".json");
    timeClient.println(" HTTP/1.0");
    timeClient.println("Connection: close");
    timeClient.println();

    // Check HTTP status
    char status[32] = {0};
    timeClient.readBytesUntil('\r', status, sizeof(status));
    // It should be "HTTP/1.0 200 OK" or "HTTP/1.1 200 OK"
    if (strcmp(status + 9, "200 OK") != 0) {
      Serial.print(F("Unexpected response: "));
      Serial.println(status);
      timeClient.stop();

      delay(500);
      getTimeData();
      return;
    }

    // Skip HTTP headers
    if (!timeClient.find("\r\n\r\n")) {
      Serial.println(F("Invalid response"));
      timeClient.stop();

      delay(500);
      getTimeData();
      return;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, timeClient);
    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
    }
    else {
      tm currentTime;
      strptime(doc["datetime"], "%FT%TZ", &currentTime);
      setTime(mktime(&currentTime));
    }

    timeClient.stop();
  }
}

void getWeatherData() {
  WiFiClient weatherClient;
  if(weatherClient.connect("api.open-meteo.com", 80)) {
    char buff[50];
    Serial.println(F("Connected to weather server"));
    // Make a HTTP request:
    weatherClient.print("GET ");
    weatherClient.print("/v1/forecast?latitude=");
    snprintf(buff, sizeof(buff), "%f", latitude);
    weatherClient.print(buff);
    weatherClient.print("&longitude=");
    snprintf(buff, sizeof(buff), "%f", longtitude);
    weatherClient.print(buff);
    weatherClient.print("&current=temperature_2m,rain,showers,snowfall&daily=temperature_2m_max,temperature_2m_min,sunrise,sunset&timezone=auto&forecast_days=2");
    weatherClient.println(" HTTP/1.1");
    weatherClient.println("Host: api.open-meteo.com");
    weatherClient.println("Accept: application/json");
    weatherClient.println("Connection: close");
    weatherClient.println();

    // Check HTTP status
    char status[32] = {0};
    weatherClient.readBytesUntil('\r', status, sizeof(status));
    //It should be "HTTP/1.0 200 OK" or "HTTP/1.1 200 OK"
    if (strcmp(status + 9, "200 OK") != 0) {
      Serial.print(F("Unexpected response: "));
      Serial.println(status);
      weatherClient.stop();
      return;
    }

    //Skip HTTP headers
    if (!weatherClient.find("\r\n\r\n")) {
      Serial.println(F("Invalid response"));
      weatherClient.stop();
      return;
    }

    const int chunksize = strtol(weatherClient.readStringUntil('\r\n').c_str(), nullptr, 16);
    Serial.print(F("Chunk size: "));
    Serial.println(chunksize);

    uint8_t* buf = new uint8_t[chunksize];
    weatherClient.readBytes(buf, chunksize);

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, buf);

    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
    }
    else {
      char buf[50];

      // Current, high and low temperature

      weatherStrings[0].reserve(50);

      memset(buf, 0, sizeof(buf));
      sprintf(buf, "Temp: %.0f", round(doc["current"]["temperature_2m"].as<float>()));
      weatherStrings[0] = buf;

      memset(buf, 0, sizeof(buf));
      sprintf(buf, ", %.0f", round(doc["daily"]["temperature_2m_min"][0].as<float>()));
      weatherStrings[0] += buf;

      memset(buf, 0, sizeof(buf));
      sprintf(buf, " - %.0f", round(doc["daily"]["temperature_2m_max"][0].as<float>()));
      weatherStrings[0] += buf;

      // Sunrise and sunset

      const time_t currentTime = now();
      tm todaySunrise, todaySunset, tomorrowSunrise;
      todaySunrise.tm_sec = 0;
      todaySunset.tm_sec = 0;
      tomorrowSunrise.tm_sec = 0;

      strptime(doc["daily"]["sunrise"][0].as<const char*>(), "%Y-%m-%dT%H:%M", &todaySunrise);
      strptime(doc["daily"]["sunset"][0].as<const char*>(), "%Y-%m-%dT%H:%M", &todaySunset);
      strptime(doc["daily"]["sunrise"][1].as<const char*>(), "%Y-%m-%dT%H:%M", &tomorrowSunrise);
      
      memset(buf, 0, sizeof(buf));
      if(difftime(currentTime, mktime(&todaySunrise)) < 0) {
        // Before today's sunrise
        strftime(buf, 50, "Sunrise: %H:%M", &todaySunrise);
      }
      else if(difftime(currentTime, mktime(&todaySunset)) < 0) {
        // After today's sunrise, but before today's sunset
        strftime(buf, 50, "Sunset: %H:%M", &todaySunset);
      }
      else {
        // After today's sunset, before tomorrow's sunrise
        strftime(buf, 50, "Sunrise: %H:%M", &tomorrowSunrise);
      }
      weatherStrings[1] = buf;

      // Precipitation

      const float snow = doc["current"]["snowfall"];
      const float rain = doc["current"]["showers"].as<float>() + doc["current"]["rain"].as<float>();

      memset(buf, 0, sizeof(buf));
      if(snow > 0.0) {
        sprintf(buf, "Snow: %.1f", snow);
      } else if(rain > 0.0) {
        sprintf(buf, "Rain: %.1f", rain);
      } else {
        sprintf(buf, "No precipitation");
      }
      weatherStrings[2] = buf;
    }

    delete[] buf;
    weatherClient.stop();

    weatherCheckMillis = millis();
  }
}

void setupEEPROM() {
  const int eeprom_size = 50 + 50 + 50 + 4 + 4 + 4 + 4;
  EEPROM.begin(eeprom_size);

  wifiSSID = EEPROM.readString(WiFiNameAddress);
  wifiPass = EEPROM.readString(WiFiPassAddress);
  latitude = EEPROM.readFloat(LatitudeAddress);
  longtitude = EEPROM.readFloat(LongtitudeAddress);
  timeZone = EEPROM.readString(TimeZoneAddress);
  foregroundColor = EEPROM.readInt(FGColorAddress);
  backgroundColor = EEPROM.readInt(BGColorAddress);

  Serial.print(F("WiFiName: "));
  Serial.println(wifiSSID);

  Serial.print(F("WiFiPass: "));
  Serial.println(wifiPass);

  Serial.print(F("Latitude: "));
  Serial.println(latitude);

  Serial.print(F("Longtitude: "));
  Serial.println(longtitude);

  Serial.print(F("FGColor: "));
  Serial.println(foregroundColor);

  Serial.print(F("BGColor: "));
  Serial.println(backgroundColor);
}


class MyCharactericsticCallback : public BLECharacteristicCallbacks {

  public:

  virtual void onWrite(BLECharacteristic *pCharacteristic, esp_ble_gatts_cb_param_t *param) {
    Serial.println(pCharacteristic->toString());
    Serial.println(pCharacteristic->getValue());

    if(pCharacteristic->getUUID().toString() == WIFI_NAME_UUID) {
        EEPROM.writeString(WiFiNameAddress, pCharacteristic->getValue());
    } 
    else if(pCharacteristic->getUUID().toString() == WIFI_PASS_UUID) {
        EEPROM.writeString(WiFiPassAddress, pCharacteristic->getValue());
    }
    else if(pCharacteristic->getUUID().toString() == LATITUDE_UUID) {
        EEPROM.writeFloat(LatitudeAddress, pCharacteristic->getValue().toFloat());
    } 
    else if(pCharacteristic->getUUID().toString() == LONGTITUDE_UUID) {
        EEPROM.writeFloat(LongtitudeAddress, pCharacteristic->getValue().toFloat());
    }  
    else if(pCharacteristic->getUUID().toString() == TIMEZONE_UUID) {
        EEPROM.writeString(TimeZoneAddress, pCharacteristic->getValue());
    } 
    else if(pCharacteristic->getUUID().toString() == FG_COLOR_UUID) {
        EEPROM.writeInt(FGColorAddress, pCharacteristic->getValue().toInt());
        foregroundColor = pCharacteristic->getValue().toInt();
        initTFT();
    }
    else if(pCharacteristic->getUUID().toString() == BG_COLOR_UUID) {
        EEPROM.writeInt(BGColorAddress, pCharacteristic->getValue().toInt());
        backgroundColor = pCharacteristic->getValue().toInt();
        initTFT();
    }

    EEPROM.commit();
  }
};

BLECharacteristic* newCharacteristic(BLEService* service, const char* uuid, String description) {
  BLECharacteristic* pCharacteristic = service->createCharacteristic(uuid, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
  pCharacteristic->addDescriptor(new BLE2902());
  BLE2901* descriptor = new BLE2901();
  descriptor->setDescription(description);
  descriptor->setAccessPermissions(ESP_GATT_PERM_READ);  // enforce read only - default is Read|Write
  pCharacteristic->addDescriptor(descriptor);
  pCharacteristic->setCallbacks(new MyCharactericsticCallback());

  return pCharacteristic;
}

void setupBT() {
  BLEDevice::init("My Sweet Clock");
  BLEServer *pServer = BLEDevice::createServer();
  BLEService *pServiceWiFi = pServer->createService(SERVICE_UUID_WiFi);
  BLEService *pServiceLatLong = pServer->createService(SERVICE_UUID_LatLong);
  BLEService *pServiceColors = pServer->createService(SERVICE_UUID_Colors);

  BLECharacteristic* pCharacteristic_WiFiName = newCharacteristic(pServiceWiFi, WIFI_NAME_UUID, "WiFi Name.");
  pCharacteristic_WiFiName->setValue(wifiSSID);

  BLECharacteristic* pCharacteristic_WiFiPassword = newCharacteristic(pServiceWiFi, WIFI_PASS_UUID, "WiFi Password.");
  pCharacteristic_WiFiPassword->setValue(wifiPass);

  BLECharacteristic* pCharacteristic_Latitude = newCharacteristic(pServiceLatLong, LATITUDE_UUID, "Latitude.");
  pCharacteristic_Latitude->setValue(latitude);

  BLECharacteristic* pCharacteristic_Longtitude = newCharacteristic(pServiceLatLong, LONGTITUDE_UUID, "Longtitude.");
  pCharacteristic_Longtitude->setValue(longtitude);

  BLECharacteristic* pCharacteristic_TimeZone = newCharacteristic(pServiceLatLong, TIMEZONE_UUID, "Time zone.");
  pCharacteristic_TimeZone->setValue(timeZone);

  BLECharacteristic* pCharacteristic_FGColor = newCharacteristic(pServiceColors, FG_COLOR_UUID, "Foreground Color.");
  pCharacteristic_FGColor->setValue(foregroundColor);

  BLECharacteristic* pCharacteristic_BGColor = newCharacteristic(pServiceColors, BG_COLOR_UUID, "Background Color.");
  pCharacteristic_BGColor->setValue(backgroundColor);

  pServiceWiFi->start();
  pServiceLatLong->start();
  pServiceColors->start();

  // BLEAdvertising *pAdvertising = pServer->getAdvertising();  // this still is working for backward compatibility
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID_WiFi);
  pAdvertising->addServiceUUID(SERVICE_UUID_LatLong);
  pAdvertising->addServiceUUID(SERVICE_UUID_Colors);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);  // functions that help with iPhone connections issue
  pAdvertising->setMinPreferred(0x12);

  BLEDevice::startAdvertising();
  Serial.println(F("Characteristic defined! Now you can read it in your phone!"));
}

void initTFT() {
  tft.init(240, 280);           // Init ST7789 280x240
  tft.setRotation(3);
  tft.fillScreen(backgroundColor);
  tft.setTextColor(foregroundColor, backgroundColor);
}

void drawSplashScreen() {
  tft.setCursor(40, 30);
  tft.setTextSize(7);
  tft.println("Hello");

  tft.setCursor(40, 30+70);
  tft.setTextSize(7);
  tft.println("Lena!");

  tft.setCursor(20, 30+70+70);
  tft.setTextSize(7);
  tft.println("  <3");

  delay(1000);
}

void setup(void) {
  Serial.begin(9600);
  //while (!Serial);

  // delay(2000);

  // Serial.print("MISO:");
  // Serial.println(MISO);
  // Serial.print("MOSI:");
  // Serial.println(MOSI);
  // Serial.print("SCK:");
  // Serial.println(SCK);
  // Serial.print("CS:");
  // Serial.println(SS);

  // while(1);
  SPI.begin(SCK, MISO, MOSI, SS);

  setupEEPROM();
  initTFT();
  drawSplashScreen();
  setupBT();
  connectToWiFi();
  getTimeData();
  getWeatherData();
  disconnectFromWiFi();

  setCpuFrequencyMhz(80);

  tft.fillScreen(backgroundColor);

  Serial.println(F("Initialized"));
}

void loop() {
  if(millis() - weatherCheckMillis > weatherCheckIntervalMillis) {
    connectToWiFi();
    getWeatherData();
    disconnectFromWiFi();
  }

  tmElements_t currentTime;
  breakTime(now(), currentTime);

  // ----- DATE -----
  std::string dateStr =
    dateTimeHelper(currentTime.Day) +
    "." +
    dateTimeHelper(currentTime.Month) +
    "." +
    dateTimeHelper(tmYearToCalendar(currentTime.Year)) +
    " " +
    + dayShortStr(currentTime.Wday);

  tft.setCursor(14, 25);
  tft.setTextSize(3);
  tft.println(dateStr.c_str());
  // ----------------


  // ----- TIME -----
  std::string timeStr =
    dateTimeHelper(currentTime.Hour) +
    (bDrawTimeColumn ? ":" : " ") +
    dateTimeHelper(currentTime.Minute);
  
  tft.setCursor(20, 75);
  tft.setTextSize(8);
  tft.println(timeStr.c_str());
  // ----------------


  // ----- WEATHER -----
  tft.setCursor(20, 160);
  tft.setTextSize(2);
  tft.println(weatherStrings[0].c_str());

  tft.setCursor(20, 185);
  tft.println(weatherStrings[1].c_str());

  tft.setCursor(20, 210);
  tft.println(weatherStrings[2].c_str());
  // --------------------


  bDrawTimeColumn = !bDrawTimeColumn;

  //digitalWrite(LED_BUILTIN, (uint8_t)bDrawTimeColumn);
  //Serial.print(F("."));

  delay(500);

  if(Serial.available() > 5) {
    backgroundColor = Serial.parseInt();
    //Serial.readStringUntil('\n');
    foregroundColor = Serial.parseInt();

    tft.fillScreen(backgroundColor);
    tft.setTextColor(foregroundColor, backgroundColor);
  }
/*
  Serial.print (F("Hour: "));
  Serial.println (currentTime.Hour);
  Serial.print (F("Min: "));
  Serial.println (currentTime.Minute);
  Serial.print (F("Sec: "));
  Serial.println (currentTime.Second);
*/
  // for(int i = 0; i < 3; i++) {
  //   Serial.println(weatherStrings[i].c_str());
  // }
}

