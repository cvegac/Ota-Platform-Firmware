#ifndef IOT_APP_H
#define IOT_APP_H

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Update.h>

#define VERSION 0

#define WIFI_SSID "My_RED"
#define WIFI_PASSWORD "u2t4n6810"

#define AWS_IOT_ENDPOINT "a27kd2bcc63jdi-ats.iot.us-east-2.amazonaws.com"

#define CA_PATH "/root_cert_auth.crt"

class IoTApp
{
public:
  IoTApp();
  void setup();
  void loop();

private:
  WiFiClientSecure secureClient;
  PubSubClient mqttClient;
  String THING_NAME;
  String AWS_TEST_TOPIC;
  String AWS_JOB_TOPIC;
  String AWS_JOB_NEXT_TOPIC;
  String AWS_JOB_GET_TOPIC;
  String AWS_JOB_GET_ACEPTED_TOPIC;
  String CERT_PATH;
  String KEY_PATH;

  void fillConstants();
  void connectToWiFi();
  void loadCertificates();
  void listFiles();
  void connectToAWS();
  void mqttCallback(char *topic, byte *payload, unsigned int length);
  void handleAceptedJobMessage(const String &message);
  void handleJobMessage(const String &message);
  void sendMessage(const String &topic, const String &message);
  bool downloadAndUpdate(const String &url, const String &jobId);
  bool downloadFirmware(const String &url, const char *filename);
  bool updateFirmware(const char *firmwareFile);
  void notifyJobStatus(const String &jobId, const String &status);
  void requestFullJobDocument(const String &jobId);
  String stringToJsonString(const String &message, const String &fieldName);
};

#endif