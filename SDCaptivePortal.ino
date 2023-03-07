#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <SPI.h>
#include <SD.h>
#include <ArduinoJson.h>

#define SDCARD_CS_PIN SS
#define DNS_PORT 53
#define HTTP_PORT 80
#define SSID_NAME "SOIL🌱"
#define SIGNAL 32

DNSServer dnsServer;
IPAddress gateway(192,168,0,1);
IPAddress local_ip(192,168,0,2);
IPAddress subnet(255,255,255,0);
WebServer server(HTTP_PORT);

static bool hasSD = false;
unsigned long prevTime = millis();
int sensorTiming = 60000;
int val;

String getDataType(String path) {
  String dataType = "text/plain";

  if (path.endsWith(".htm") || path.endsWith(".html")) {
    dataType = "text/html";
  } else if (path.endsWith(".css")) {
    dataType = "text/css";
  } else if (path.endsWith(".js")) {
    dataType = "application/javascript";
  } else if (path.endsWith(".png")) {
    dataType = "image/png";
  } else if (path.endsWith(".gif")) {
    dataType = "image/gif";
  } else if (path.endsWith(".jpg")) {
    dataType = "image/jpeg";
  } else if (path.endsWith(".ico")) {
    dataType = "image/x-icon";
  } else if (path.endsWith(".xml")) {
    dataType = "text/xml";
  } else if (path.endsWith(".pdf")) {
    dataType = "application/pdf";
  } else if (path.endsWith(".zip")) {
    dataType = "application/zip";
  }

  return dataType;
}

void loadFileFromSdCard() {
  String path = server.uri();
  String dataType = getDataType(path);
  File dataFile;

  if (!SD.exists(path)) {
    dataFile = SD.open("/index.htm", "r");
  } else {
    dataFile = SD.open(path.c_str(), "r");
  }

  server.streamFile(dataFile, dataType);
  dataFile.close();

  return;
}

/**
 * Writes a integer value to a json file
**/
void writeData(String filename, int value) {
  if (!SD.exists(filename)) {
    File file = SD.open(filename, FILE_WRITE, true);
    // Create temporary JsonDocument
    DynamicJsonDocument doc(512);
    // Set values to the document
    doc["sensor"] = "Soil Moisture Sensor";
    // Serialize JSON to file
    if (serializeJson(doc, file) == 0) {
      Serial.println("[JSON] Failed to write to file");
    }
    file.close();
  }

  DynamicJsonDocument doc(ESP.getMaxAllocHeap());
  JsonObject obj;
  File file = SD.open(filename);

  if (!file) return;

  DeserializationError error = deserializeJson(doc, file);
  if (error) {
    Serial.println("[JSON] Error parsing JSON");
    Serial.println(error.c_str());
    return;
  } else {
    obj = doc.as<JsonObject>();
  }

  file.close();

  JsonArray data;
  if (!obj.containsKey("data")) {
    Serial.print("[JSON] Creating data array...");
    data = obj.createNestedArray("data");
    Serial.println("OK");
  } else {
    Serial.println("[JSON] Found data array");
    data = obj["data"];
  }

  // Create object to add to the array
  JsonObject objArrayData = data.createNestedObject();
  objArrayData["time"] = millis();
  objArrayData["value"] = value;

  SD.remove("/data.json");
  
  // Open file for writing
  file = SD.open("/data.json", FILE_WRITE);

  // Serialize JSON to file
  if (serializeJson(doc, file) == 0) {
    Serial.println("[JSON] Failed to write to file");
  } else {
    Serial.print("[JSON] Writing to file...");
  }
  file.close();
  Serial.println("OK");
}


/**
 * Sends the 'data.json' file from the SD card to the client
**/
void listData() {
  if (server.uri() != "/data") return;
  if (!SD.exists("/data.json")) return;

  DynamicJsonDocument doc(ESP.getMaxAllocHeap()); // HACK
  File file = SD.open("/data.json", "r");
  DeserializationError err = deserializeJson(doc, file);
  file.close();
  
  if (err) {
    Serial.print("[JSON] Desirialization failed: ");
    Serial.println(err.c_str());
    return;
  }

  String buf;
  serializeJson(doc, buf);

  Serial.print("[JSON] Sending response...");
  server.send(200, "application/json", buf);
  Serial.println("OK");
}

void setup() {
  Serial.begin(115200);

  if (SD.begin(SDCARD_CS_PIN)) {
    hasSD = true;
  } else {
    while (true)
      ;
  }

  WiFi.mode(WIFI_AP);
  WiFi.softAP(SSID_NAME);
  WiFi.softAPConfig(gateway, gateway, subnet);

  dnsServer.start(DNS_PORT, "*", gateway);

  server.enableCORS(); // this?
  server.enableCrossOrigin(); // or this one
  server.on("/data", HTTP_GET, listData);
  server.onNotFound(loadFileFromSdCard);
  server.begin();
}

void loop() {
  unsigned long currentTime = millis();

  dnsServer.processNextRequest();
  server.handleClient();
  delay(2);

  if (currentTime - prevTime > sensorTiming) {
    val = analogRead(SIGNAL);
    Serial.print("[SENSOR] Value: ");
    Serial.println(val);
    writeData("/data.json", val);

    prevTime = currentTime;
  }
}
