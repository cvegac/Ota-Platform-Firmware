#include "IoTApp.h"

unsigned long lastJobCheck = 0;                    // Última vez que se consultó por jobs
const unsigned long jobCheckInterval = 60 * 60000; // ⏳ Intervalo de 1 hora

IoTApp::IoTApp() : mqttClient(secureClient)
{
  mqttClient.setBufferSize(4096);
  mqttClient.setServer(AWS_IOT_ENDPOINT, 8883);
  mqttClient.setCallback([this](char *t, byte *p, unsigned int l)
                         { this->mqttCallback(t, p, l); });
  mqttClient.setKeepAlive(60);
}

void IoTApp::setup()
{
  Serial.begin(115200);
  Serial.printf("🏗 Version: %d\n", VERSION);
  listFiles();
  fillConstants();
  connectToWiFi();
  loadCertificates();
  connectToAWS();
}

void IoTApp::loop()
{
  mqttClient.loop();

  if (millis() - lastJobCheck >= jobCheckInterval)
  {
    Serial.println("🔄 Consultando AWS IoT por jobs pendientes...");
    sendMessage(AWS_JOB_GET_TOPIC, "{}");
    lastJobCheck = millis();
  }
}

void IoTApp::fillConstants()
{
  Serial.println("🔧 Configurando constantes...");

  AWS_TEST_TOPIC = "test/" + THING_NAME;
  AWS_JOB_TOPIC = "$aws/things/" + THING_NAME + "/jobs";
  AWS_JOB_NEXT_TOPIC = AWS_JOB_TOPIC + "/notify-next";
  AWS_JOB_GET_TOPIC = AWS_JOB_TOPIC + "/get";
  AWS_JOB_GET_ACEPTED_TOPIC = AWS_JOB_GET_TOPIC + "/accepted";
  printf("🔹 AWS_TEST_TOPIC: %s\n", AWS_TEST_TOPIC.c_str());
  printf("🔹 AWS_JOB_TOPIC: %s\n", AWS_JOB_TOPIC.c_str());
  printf("🔹 AWS_JOB_NEXT_TOPIC: %s\n", AWS_JOB_NEXT_TOPIC.c_str());
  printf("🔹 AWS_JOB_GET_TOPIC: %s\n", AWS_JOB_GET_TOPIC.c_str());
  printf("🔹 AWS_JOB_GET_ACEPTED_TOPIC: %s\n", AWS_JOB_GET_ACEPTED_TOPIC.c_str());

  CERT_PATH = "/" + THING_NAME + ".cert.pem.crt";
  KEY_PATH = "/" + THING_NAME + ".private.pem.key";
  Serial.println("🔹 Certificado: " + CERT_PATH);
  Serial.println("🔹 Llave privada: " + KEY_PATH);

  Serial.println("✅ Constantes configuradas");
}

void IoTApp::connectToWiFi()
{
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("🛠  Conectando a WiFi");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n🌐 WiFi conectado");
}

void IoTApp::listFiles()
{
  if (!LittleFS.begin(true))
  {
    Serial.println("Error iniciando LittleFS");
    return;
  }

  File root = LittleFS.open("/");
  File file = root.openNextFile();
  Serial.println("Listado de arcihvos: ");
  while (file)
  {
    String fileName = file.name();
    String fileType = ".private.pem.key";
    if (fileName.substring(fileName.length() - fileType.length(), fileName.length()) == fileType)
    {
      THING_NAME = fileName.substring(0, fileName.length() - fileType.length());
    }
    Serial.print("- ");
    Serial.println(fileName);
    file = root.openNextFile();
  }
}

void IoTApp::loadCertificates()
{

  File certFile = LittleFS.open(CERT_PATH, "r");
  String cert = certFile.readString();

  File keyFile = LittleFS.open(KEY_PATH, "r");
  String key = keyFile.readString();

  File caFile = LittleFS.open(CA_PATH, "r");
  String ca = caFile.readString();

  if (!certFile || !keyFile || !caFile)
  {
    Serial.println("Error cargando certificados");
    return;
  }

  secureClient.setCertificate(cert.c_str());
  secureClient.setPrivateKey(key.c_str());
  secureClient.setCACert(ca.c_str());

  certFile.close();
  keyFile.close();
  caFile.close();
  Serial.println("Certificados cargados");
}

void IoTApp::connectToAWS()
{
  int attempts = 0;
  while (!mqttClient.connected())
  {
    Serial.println("🛠  Conectando a AWS IoT...");
    if (mqttClient.connect(THING_NAME.c_str()))
    {
      Serial.println("🌐 Conectado a AWS IoT");

      mqttClient.subscribe(AWS_JOB_NEXT_TOPIC.c_str());
      Serial.print("⚡ Suscrito a: ");
      Serial.println(AWS_JOB_NEXT_TOPIC);

      mqttClient.subscribe(AWS_TEST_TOPIC.c_str());
      Serial.print("⚡ Suscrito a: ");
      Serial.println(AWS_TEST_TOPIC);

      mqttClient.subscribe(THING_NAME.c_str());
      Serial.print("⚡ Suscrito a: ");
      Serial.println(THING_NAME);

      mqttClient.subscribe(AWS_JOB_GET_ACEPTED_TOPIC.c_str());
      Serial.print("⚡ Suscrito a: ");
      Serial.println(AWS_JOB_GET_ACEPTED_TOPIC);

      String payload = stringToJsonString("Hola desde " + String(THING_NAME) + "! tengo la version " + String(VERSION), "message");
      sendMessage(AWS_TEST_TOPIC, payload);

      sendMessage(AWS_JOB_GET_TOPIC, "{}");

      Serial.println("📤 Mensaje enviado a " + String(AWS_TEST_TOPIC));
    }
    else
    {
      attempts++;
      Serial.print("❌ Fallo, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" 🔄 intentando de nuevo en 5 segundos");
      delay(5000);
    }
    if (attempts >= 2)
    {
      Serial.println("❌ No se pudo conectar a AWS IoT después de 5 intentos. Reiniciando...");
      ESP.restart();
    }
  }
}

void IoTApp::mqttCallback(char *topic, byte *payload, unsigned int length)
{
  Serial.print("📩 Mensaje recibido en ");
  Serial.println(topic);
  payload[length] = '\0';
  String message = String((char *)payload);
  Serial.println("Payload: " + message);
  String topicStr = String(topic);
  if (topicStr == AWS_JOB_GET_ACEPTED_TOPIC)
  {
    handleAceptedJobMessage(message);
    return;
  }
  if (topicStr.startsWith(AWS_JOB_TOPIC) || topicStr == AWS_TEST_TOPIC || topicStr == THING_NAME)
  {
    handleJobMessage(message);
  }
}

void IoTApp::handleAceptedJobMessage(const String &message)
{
  JsonDocument doc;
  if (deserializeJson(doc, message))
  {
    Serial.println("❌ Error parseando JSON del job");
    return;
  }

  String jobId = doc["queuedJobs"][0]["jobId"];

  Serial.println("🔄 Job recibido: " + jobId);

  if (!doc["queuedJobs"][0]["jobId"].isNull() &&
      !doc["queuedJobs"][0]["jobId"].as<String>().isEmpty())
  {
    requestFullJobDocument(jobId);
    return;
  }
}

void IoTApp::handleJobMessage(const String &message)
{
  JsonDocument doc;
  if (deserializeJson(doc, message))
  {
    Serial.println("❌ Error parseando JSON del job");
    return;
  }

  if (doc["execution"]["jobDocument"]["afr_ota"]["files"][0]["url"].isNull() ||
      doc["execution"]["jobDocument"]["afr_ota"]["files"][0]["url"].as<String>().isEmpty())
  {
    Serial.println("⚠️  No se encontró una URL válida en el job. Solicitando Job completo...");
    sendMessage(AWS_JOB_GET_TOPIC, "{}");
    return;
  }

  String jobId = doc["execution"]["jobId"];
  String firmwareUrl = doc["execution"]["jobDocument"]["afr_ota"]["files"][0]["url"];

  if (downloadAndUpdate(firmwareUrl, jobId))
  {
    Serial.println("✅ Actualización OTA completada");
    delay(1000);
    notifyJobStatus(jobId, "SUCCEEDED");

    delay(5000);

    ESP.restart();
  }
  else
  {
    Serial.println("❌ Fallo en la actualización OTA");
    notifyJobStatus(jobId, "FAILED");
  }
}

void IoTApp::sendMessage(const String &topic, const String &message)
{
  Serial.println("📤 [MQTT] Enviando mensaje...");
  Serial.print("🔹 Tópico: ");
  Serial.println(topic);
  Serial.print("🔹 Payload: ");
  Serial.println(message);

  if (mqttClient.publish(topic.c_str(), message.c_str()))
  {
    Serial.println("✅ [MQTT] Mensaje enviado con éxito");
  }
  else
  {
    Serial.println("❌ [MQTT] Error al enviar mensaje");
  }
}

bool IoTApp::downloadAndUpdate(const String &url, const String &jobId)
{
  Serial.println("🚀 Iniciando descarga...");
  if (!downloadFirmware(url, "/firmware.bin"))
  {
    Serial.println("❌ Error descargando el firmware");
    notifyJobStatus(jobId, "FAILED");
    return false;
  }
  Serial.println("✅ Firmware descargado");

  if (!updateFirmware("/firmware.bin"))
  {
    Serial.println("❌ Fallo en la actualización");
    notifyJobStatus(jobId, "FAILED");
    return false;
  }

  return true;
}

bool IoTApp::downloadFirmware(const String &url, const char *filename)
{
  HTTPClient http;
  http.begin(url);
  int httpCode = http.GET();

  if (httpCode != HTTP_CODE_OK)
  {
    Serial.printf("❌ Error HTTP: %d\n", httpCode);
    http.end();
    return false;
  }

  if (!LittleFS.begin(true))
  {
    Serial.println("❌ Error iniciando LittleFS");
    http.end();
    return false;
  }

  File file = LittleFS.open(filename, FILE_WRITE);
  if (!file)
  {
    Serial.println("❌ Error al abrir archivo para escribir");
    http.end();
    return false;
  }

  WiFiClient *stream = http.getStreamPtr();
  uint8_t buffer[512];
  size_t len = http.getSize();

  while (http.connected() && len > 0)
  {
    size_t bytesRead = stream->readBytes(buffer, sizeof(buffer));
    file.write(buffer, bytesRead);
    len -= bytesRead;
  }

  file.close();
  http.end();
  return true;
}

bool IoTApp::updateFirmware(const char *firmwareFile)
{

  if (!LittleFS.begin(true))
  {
    Serial.println("❌ Error iniciando LittleFS");
    return false;
  }

  Serial.println("🔍 Abriendo Archivo firmware...");
  File file = LittleFS.open(firmwareFile, FILE_READ);
  if (!file)
  {
    Serial.println("❌ Error al abrir el archivo de firmware");
    return false;
  }

  size_t fileSize = file.size();
  Serial.printf("📦 Tamaño del firmware: %d bytes\n", fileSize);

  if (!Update.begin(fileSize))
  {
    Serial.println("❌ Error en Update.begin()");
    file.close();
    return false;
  }

  uint8_t buffer[512];
  while (size_t bytesRead = file.read(buffer, sizeof(buffer)))
  {
    if (Update.write(buffer, bytesRead) != bytesRead)
    {
      Serial.println("❌ Error al escribir en la flash");
      file.close();
      return false;
    }
  }

  if (!Update.end())
  {
    Serial.println("❌ Error en Update.end()");
    file.close();
    return false;
  }

  Serial.println("✅ Funcion de actualización completada");
  file.close();
  return true;
}

void IoTApp::notifyJobStatus(const String &jobId, const String &status)
{
  JsonDocument doc;
  doc["status"] = status;

  String payload;
  serializeJson(doc, payload);

  String jobUpdateTopic = String("$aws/things/") + THING_NAME + "/jobs/" + jobId + "/update";
  sendMessage(jobUpdateTopic, payload);
}

void IoTApp::requestFullJobDocument(const String &jobId)
{
  String jobGetTopic = String("$aws/things/") + THING_NAME + "/jobs/" + jobId + "/get";

  Serial.println("📤 Solicitando job completo en: " + jobGetTopic);
  sendMessage(jobGetTopic, "{}");
}

String IoTApp::stringToJsonString(const String &message, const String &fieldName)
{
  JsonDocument doc;
  doc[fieldName] = message;

  String payload;
  serializeJson(doc, payload);

  return payload;
}