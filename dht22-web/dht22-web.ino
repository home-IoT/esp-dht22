/*
 * esp-dht22
 *
 * A HTTP server for ESP8266 boards, responding to read request of
 * temperature and humidity of an attached DHT22 sensor.
 *
 * (c) 2018 Roozbeh Farahbod
 * Distributed under the MIT License.
 *
 */

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <FS.h>
#include <ArduinoJson.h>
#include <DHT_U.h>

#define MAX_CONFIG_SIZE (2 * 1024)
#define WIFI_STATION_TIME_TO_LIVE 30 // seconds
#define WIFI_AP_TIME_TO_LIVE (5 * 60 * 1000) // milliseconds
#define MILLISECONDS_IN_A_DAY (24 * 3600 * 1000)
#define CONFIG_FILE "/config.json"

// --- Change these settings as needed
#define DHTTYPE DHT22
#define DHTPIN 14       // what digital pin the DHT22 is conected to
const char* deviceName = "dht22-04";
const char* def_ssid = "ESPConfigurator";
const char* def_password = "";
// ---

char ssid[64];
char password[128];

ESP8266WebServer server(80);

// Response construct to keep the last reading in case of error
struct Response {
  float temp, humidity, heatIndex = 0.0;
  unsigned long timeStamp = 0;
};
Response* lastResponse = new(Response);


void setup(void){
  Serial.begin(115200);
  Serial.setTimeout(2000);

  // wait for serial to initialize.
  while(!Serial) { }

  String message = (String)"Device " + deviceName + " started.";
  Serial.println(message);

  Serial.println("Mounting FS...");
  if (!SPIFFS.begin()) {
    Serial.println("Cannot mount file system.");
    return;
  }

  // if configuration cannot be loaded, use default and save it
  if (!loadConfig()) {
    error("Failed to load config file.");
    setDefaultConfig();
    saveConfig();
  }

  // Try connecting with the loaded configuration
  if (!setupWiFi(WIFI_STATION_TIME_TO_LIVE)) {
    // if failed, fall back to default and try again.
    setDefaultConfig();
    setupWiFi(0);
  } 
  
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());


  server.on("/", handleRoot);

  server.on("/config", handleConfig);

  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started.");
}

void loop(void){
  server.handleClient();
}

bool setupWiFi(int timeout) {
  WiFi.mode(WIFI_STA);

  if (strlen(password) == 0) {
    Serial.println((String)"\nConnecting to WiFi with SSID " + ssid + "...\n");
    WiFi.begin(ssid);
  } else {
    Serial.println((String)"\nConnecting to WiFi with SSDI " + ssid + " and password " + password + "...\n");
    WiFi.begin(ssid, password);
  }
 
  int i = 0;
  while (WiFi.status() != WL_CONNECTED && (timeout == 0 || i < timeout)) {
    delay(1000);
    i++;
    Serial.print('.');
  }

  return WiFi.status() == WL_CONNECTED;
}


// --- HTTP Endpoint Handlers
void handleRoot() {
  DHT dht(DHTPIN, DHTTYPE);

  boolean stale = false;

  float h = dht.readHumidity();
  // read temperature as Celsius
  float t = dht.readTemperature();

  if (isnan(h) || isnan(t)) {
        Serial.println("Failed to read from DHT sensor!");
        stale = true;
  } else {
    // compute heat index in Celsius
    float hi = dht.computeHeatIndex(t, h, false);

    lastResponse->temp = t;
    lastResponse->humidity = h;
    lastResponse->heatIndex = hi;
    lastResponse->timeStamp = millis();
  }

  StaticJsonBuffer<500> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();

  root["device"] = deviceName;
  root["stale"] = stale;
  long readingTime = millis() - lastResponse->timeStamp;
  if (readingTime < 0 || readingTime > MILLISECONDS_IN_A_DAY) {
    readingTime = MILLISECONDS_IN_A_DAY;
  }
  root["readingTime"] = readingTime;

  JsonObject& dht22 = root.createNestedObject("dht22");
  dht22["temperature"] = lastResponse->temp;
  dht22["humidity"] = lastResponse->humidity;
  dht22["heatIndex"] = lastResponse->heatIndex;

  String json;
  root.printTo(json);

  server.send(200, "application/json", json);
}

// GET /config?ssid=<ssid>&password=<password>
void handleConfig() {
  if (server.arg("ssid") == "") {
    serverSendError(400, "SSID is required.");
    return;
  }

  int size = server.arg("ssid").length() + 1;
  server.arg("ssid").toCharArray(ssid, size);

  size = server.arg("password").length() + 1;
  server.arg("password").toCharArray(password, size);

  Serial.println(ssid);
  Serial.println(password);

  saveConfig();

  server.send(200, "application/json", "{}");

  Serial.println("Restarting...");

  delay(5000);

  ESP.restart();
}

void handleNotFound(){
  server.send(404, "text/plain", "Not found.");
}

// --- Configuration File

// setDefaultConfig sets the default configuration
void setDefaultConfig() {
  strcpy(ssid, def_ssid);
  strcpy(password, def_password);
}

// loadConfig loads the stored configuration from the config file
bool loadConfig(void) {
  Serial.println("Loading configuration file...");

  File configFile = SPIFFS.open(CONFIG_FILE, "r");
  if (!configFile) {
    error("Cannot open config file.");
    return false;
  }

  size_t size = configFile.size();
  if (size > MAX_CONFIG_SIZE) {
    error("Config file size is too large.");
    return false;
  }

  std::unique_ptr<char[]> buf(new char[size]);
  configFile.readBytes(buf.get(), size);

  StaticJsonBuffer<256> jsonBuffer;
  JsonObject& json = jsonBuffer.parseObject(buf.get());

  if (!json.success()) {
    error("Failed to parse config file.");
    return false;
  }

  const char* readSSID = json["ssid"];
  const char* readPass = json["password"];

  strcpy(ssid, readSSID);
  strcpy(password, readPass);

  Serial.println(ssid);
  Serial.println(password);

  configFile.close();
  Serial.println("Config loaded successfully.");

  return true;
}

// saveConfig saves the current WiFi configuration
bool saveConfig() {
  StaticJsonBuffer<256> jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();

  json["ssid"] = ssid;
  json["password"] = password;

  File configFile = SPIFFS.open(CONFIG_FILE, "w");
  if (!configFile) {
    error("Failed to open config file for writing.");
    return false;
  }

  json.printTo(configFile);

  configFile.close();
  Serial.println("Config file saved.");

  return true;
}


// --- Helper Functions
void error(char* msg) {
  Serial.print("ERR: ");
  Serial.println(msg);
}

// serverSendError sends an HTTP error response with the given code
void serverSendError(int code, char* msg) {
    server.send(code, "application/json", (String)"{\"error\": \"" + msg + "\"}");
}

