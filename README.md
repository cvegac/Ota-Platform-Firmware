# Ota-Platform-Firmware

Firmware del agente de actualización **Over-The-Air (OTA)** para microcontroladores
**ESP32 / ESP32-S3**, desarrollado con **PlatformIO** sobre el framework Arduino.

Es el componente de dispositivo de la plataforma OTA: se conecta a **AWS IoT Core**
por MQTT sobre TLS, escucha los *Jobs* de actualización creados por el backend y
descarga, valida y aplica el nuevo binario de firmware de forma autónoma.

Repos relacionados:
- Backend (Java / Spring Boot): <https://github.com/cvegac/Ota-Platform-Backend>
- Frontend (Angular): <https://github.com/cvegac/Ota-Platform-Frontend>

---

## Requisitos

- **Hardware:** ESP32 DevKit o ESP32-S3 (en el repo se incluye la definición de
  placa `boards/esp32-s3-devkitc-1-n16r8v.json`).
- **Toolchain:** [PlatformIO](https://platformio.org/) (extensión de VS Code o `platformio-core`).
- Una cuenta de **AWS** con IoT Core habilitado y un *Thing* registrado (la plataforma
  lo crea automáticamente al dar de alta el dispositivo desde la interfaz web).

## Estructura del proyecto

```
include/IoTApp.h     Declaración de la clase IoTApp y parámetros de configuración
src/IoTApp.cpp       Lógica del agente: WiFi, MQTT/TLS, manejo de Jobs y flasheo
src/main.cpp         Punto de entrada: instancia IoTApp y llama a setup()/loop()
data/                Certificados y llaves que se cargan en LittleFS (NO versionados)
boards/              Definiciones de placa personalizadas
platformio.ini       Configuración de build, librerías y filesystem
```

## Configuración

Toda la configuración está centralizada en [`include/IoTApp.h`](include/IoTApp.h):

```cpp
#define VERSION            0          // Versión del firmware; increméntala en cada release
#define WIFI_SSID          "..."      // Red WiFi
#define WIFI_PASSWORD      "..."      // Clave WiFi
#define AWS_IOT_ENDPOINT   "xxxxxxxx-ats.iot.us-east-2.amazonaws.com"
#define CA_PATH            "/root_cert_auth.crt"
```

El identificador del dispositivo (`THING_NAME`) **no se configura a mano**: se deduce
en tiempo de ejecución a partir del nombre del archivo de certificado presente en
LittleFS.

### Certificados (LittleFS)

Al crear el dispositivo en la plataforma web se descarga un paquete con sus
credenciales. Copiá esos archivos en `data/` con estos nombres:

| Archivo en `data/` | Contenido |
|---|---|
| `<THING_NAME>.cert.pem.crt` | Certificado del dispositivo |
| `<THING_NAME>.private.pem.key` | Llave privada |
| `root_cert_auth.crt` | CA raíz de Amazon |

Luego subí el filesystem a la placa:

```bash
pio run --target uploadfs
```

> Los archivos `*.crt` y `*.key` están excluidos por `.gitignore`: son secretos por
> dispositivo y no deben subirse al repositorio.

## Compilación y carga

```bash
pio run                        # Compilar
pio run --target upload        # Flashear el firmware
pio run --target uploadfs      # Cargar los certificados (LittleFS)
pio device monitor -b 115200   # Monitor serie
```

## Flujo de actualización OTA

1. En el arranque (`setup()`) el dispositivo monta LittleFS, deduce su `THING_NAME`,
   se conecta al WiFi, carga los certificados y establece la sesión MQTT/TLS con
   AWS IoT Core (puerto 8883).
2. Se suscribe a los tópicos de *Jobs* de su *Thing* (`$aws/things/<thing>/jobs/#`).
3. En `loop()`, cada hora (`jobCheckInterval`), consulta si hay Jobs pendientes.
4. Ante un Job de actualización, extrae la URL prefirmada de S3, descarga el binario
   a `/firmware.bin` en LittleFS y verifica el tamaño.
5. Escribe el binario en la partición de aplicación mediante la librería `Update`
   del ESP32 y reinicia.
6. Reporta el resultado a AWS en `$aws/things/<thing>/jobs/<jobId>/update` con estado
   `SUCCEEDED` o `FAILED`. Si la descarga o el flasheo fallan, el dispositivo continúa
   ejecutando el firmware anterior.

## Dependencias

Declaradas en `platformio.ini`:

- `bblanchon/ArduinoJson`
- `knolleary/PubSubClient` (MQTT)
- `mrfaptastic/Json Streaming Parser 2`

---

*Proyecto de grado — plataforma de gestión de actualizaciones OTA para dispositivos IoT.*
