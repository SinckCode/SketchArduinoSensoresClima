#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <DHT.h>
#include <BH1750.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME680.h>
#include "esp_system.h"  // Para esp_reset_reason()

// ===================== CONFIGURACIÓN WIFI ======================
const char* ssid     = "LaSalleBajio";
const char* password = "";

// URLs separadas
const char* apiUrlDhtLight = "https://sensores.angelonesto.com/api/dht-light-readings";
const char* apiUrlBme      = "https://sensores.angelonesto.com/api/bme-readings";

// ===================== IDENTIFICADORES DE DISPOSITIVO ======================
const char* DEVICE_ID_DHT_LIGHT = "esp32-dht-light-01";
const char* DEVICE_ID_BME       = "esp32-bme-01";

// ===================== I2C COMPARTIDO ======================
#define SDA_I2C 21
#define SCL_I2C 22

// ===================== DHT22 ======================
#define DHTPIN  13
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// ===================== BH1750 ======================
BH1750 lightSensor;
bool    bh1750_ok            = false;
uint8_t bh1750_failCount     = 0;
const uint8_t BH1750_MAX_FAILS = 5;

// ===================== BME680 ======================
Adafruit_BME680 bme;    // usará Wire / I2C
bool bmeOk = false;

// ===================== TIMERS ======================
const unsigned long INTERVALO_DHT_BH_MS = 5000;
const unsigned long INTERVALO_BME_MS    = 5000;

unsigned long ultimoDhtBhMs = 0;
unsigned long ultimoBmeMs   = 0;

// ===================== PROTOCOLO DE RECUPERACIÓN ======================

// Máximos de reintentos y fallos antes de reiniciar
const uint8_t WIFI_MAX_RETRIES    = 10;
const uint8_t HTTP_MAX_FAILS      = 10;

// Contadores de fallos HTTP
uint8_t httpFailsDhtBh = 0;
uint8_t httpFailsBme   = 0;

// --------------------------------------------------------------------
// Revisa WiFi, intenta reconectar y si no, reinicia el ESP32
// --------------------------------------------------------------------
void ensureWiFiConnected() {
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  Serial.println("[WIFI] Conexión perdida. Intentando reconectar...");

  WiFi.disconnect(true);
  delay(500);
  WiFi.begin(ssid, password);

  uint8_t intentos = 0;
  while (WiFi.status() != WL_CONNECTED && intentos < WIFI_MAX_RETRIES) {
    delay(500);
    Serial.print(".");
    intentos++;
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("[WIFI] Reconectado. IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("[WIFI] No se pudo reconectar tras varios intentos. Reiniciando ESP32...");
    esp_restart();  // Reinicio completo del chip
  }
}

// --------------------------------------------------------------------
// Maneja fallos HTTP, y si se pasa del límite, reinicia
// --------------------------------------------------------------------
void handleHttpFailure(bool isDhtBh) {
  if (isDhtBh) {
    httpFailsDhtBh++;
    Serial.printf("[HTTP DHT+BH] Fallos consecutivos: %u\n", httpFailsDhtBh);
    if (httpFailsDhtBh >= HTTP_MAX_FAILS) {
      Serial.println("[HTTP DHT+BH] Demasiados fallos HTTP. Reiniciando ESP32...");
      esp_restart();
    }
  } else {
    httpFailsBme++;
    Serial.printf("[HTTP BME] Fallos consecutivos: %u\n", httpFailsBme);
    if (httpFailsBme >= HTTP_MAX_FAILS) {
      Serial.println("[HTTP BME] Demasiados fallos HTTP. Reiniciando ESP32...");
      esp_restart();
    }
  }
}

void resetHttpFailureCounter(bool isDhtBh) {
  if (isDhtBh) {
    if (httpFailsDhtBh > 0) {
      Serial.println("[HTTP DHT+BH] Reset de contador de fallos.");
      httpFailsDhtBh = 0;
    }
  } else {
    if (httpFailsBme > 0) {
      Serial.println("[HTTP BME] Reset de contador de fallos.");
      httpFailsBme = 0;
    }
  }
}

// ===============================================================
//                 FUNCIONES AUXILIARES BH1750
// ===============================================================
bool initBH1750() {
  Serial.println("[BH1750] Inicializando...");

  delay(10);

  // Primero probamos 0x23
  if (lightSensor.begin(BH1750::CONTINUOUS_HIGH_RES_MODE, 0x23, &Wire)) {
    Serial.println("[BH1750] Encontrado en 0x23");
    bh1750_ok = true;
    bh1750_failCount = 0;
    return true;
  }

  Serial.println("[BH1750] No encontrado en 0x23, probando 0x5C...");

  // Luego probamos 0x5C
  if (lightSensor.begin(BH1750::CONTINUOUS_HIGH_RES_MODE, 0x5C, &Wire)) {
    Serial.println("[BH1750] Encontrado en 0x5C");
    bh1750_ok = true;
    bh1750_failCount = 0;
    return true;
  }

  Serial.println("[BH1750] ERROR: no se encontró ni en 0x23 ni en 0x5C");
  bh1750_ok = false;
  return false;
}

float leerBH1750() {
  if (!bh1750_ok) {
    if (!initBH1750()) {
      return -2.0f; // error
    }
  }

  float lux = lightSensor.readLightLevel();

  // Validación básica
  if (lux < 0.0f || lux > 65535.0f) {
    bh1750_failCount++;
    Serial.printf("[BH1750] ERROR: lectura inválida (lux=%.2f) fallo #%u\n",
                  lux, bh1750_failCount);

    if (bh1750_failCount >= BH1750_MAX_FAILS) {
      Serial.println("[BH1750] Demasiados fallos, re-inicializando sensor...");
      initBH1750();
    }

    return -2.0f;
  }

  bh1750_failCount = 0;
  return lux;
}

// ===============================================================
//                 FUNCIONES AUXILIARES BME680
// ===============================================================
bool initBME680() {
  Serial.println("[BME680] Inicializando...");

  // Probamos 0x76 y después 0x77
  if (bme.begin(0x76, &Wire)) {
    Serial.println("[BME680] Encontrado en 0x76");
    bmeOk = true;
  } else if (bme.begin(0x77, &Wire)) {
    Serial.println("[BME680] Encontrado en 0x77");
    bmeOk = true;
  } else {
    Serial.println("[BME680] ERROR: no se encontró BME680 en 0x76 ni 0x77");
    bmeOk = false;
    return false;
  }

  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);

  // Heater apagado para reducir consumo
  bme.setGasHeater(0, 0);  // heater OFF

  Serial.println("[BME680] Configuración completada (heater OFF)");
  return true;
}

bool leerBME680(float &temp, float &hum, float &pres, float &gas) {
  if (!bmeOk) {
    Serial.println("[BME680] No inicializado, no se puede leer");
    return false;
  }

  if (!bme.performReading()) {
    Serial.println("[BME680] ERROR: performReading() fallo");
    return false;
  }

  temp = bme.temperature;        // °C
  hum  = bme.humidity;           // %
  pres = bme.pressure / 100.0f;  // hPa
  gas  = bme.gas_resistance;     // ohms

  return true;
}

// ===============================================================
//                 ENVÍO A LA API (DHT+BH)
// ===============================================================
void enviarDhtBhALaApi(float tempDht, float humDht, float lightLux) {
  // 1) Asegurar conexión WiFi (si no puede, reinicia)
  ensureWiFiConnected();

  HTTPClient http;
  http.begin(apiUrlDhtLight);
  http.addHeader("Content-Type", "application/json");

  String payload = "{";
  payload += "\"deviceId\":\"" + String(DEVICE_ID_DHT_LIGHT) + "\",";
  payload += "\"sensors\":{";
  payload += "\"temp_dht_c\":"   + String(tempDht, 2) + ",";
  payload += "\"humidity_pct\":" + String(humDht, 2)  + ",";
  payload += "\"light_lux\":"    + String(lightLux, 2);
  payload += "}}";

  Serial.println("--- JSON DHT+BH1750 a enviar ---");
  Serial.println(payload);

  int httpCode = http.POST(payload);
  Serial.print("[HTTP DHT+BH] Response code: ");
  Serial.println(httpCode);

  if (httpCode > 0 && httpCode >= 200 && httpCode < 300) {
    String resp = http.getString();
    Serial.println("Respuesta DHT+BH:");
    Serial.println(resp);
    resetHttpFailureCounter(true);
  } else {
    Serial.println("[HTTP DHT+BH] Error en POST.");
    handleHttpFailure(true);
  }

  http.end();
}

// ===============================================================
//                 ENVÍO A LA API (BME)
// ===============================================================
void enviarBmeALaApi(float tempBme, float humBme, float pres, float gas) {
  // 1) Asegurar conexión WiFi (si no puede, reinicia)
  ensureWiFiConnected();

  HTTPClient http;
  http.begin(apiUrlBme);
  http.addHeader("Content-Type", "application/json");

  String payload = "{";
  payload += "\"deviceId\":\"" + String(DEVICE_ID_BME) + "\",";
  payload += "\"sensors\":{";
  payload += "\"temp_bme_c\":"         + String(tempBme, 2) + ",";
  payload += "\"humidity_bme_pct\":"   + String(humBme, 2)  + ",";
  payload += "\"pressure_hpa\":"       + String(pres, 2)    + ",";
  payload += "\"gas_resistance_ohms\":"+ String(gas, 2);
  payload += "}}";

  Serial.println("--- JSON BME a enviar ---");
  Serial.println(payload);

  int httpCode = http.POST(payload);
  Serial.print("[HTTP BME] Response code: ");
  Serial.println(httpCode);

  if (httpCode > 0 && httpCode >= 200 && httpCode < 300) {
    String resp = http.getString();
    Serial.println("Respuesta BME:");
    Serial.println(resp);
    resetHttpFailureCounter(false);
  } else {
    Serial.println("[HTTP BME] Error en POST.");
    handleHttpFailure(false);
  }

  http.end();
}

// ===============================================================
//                         SETUP
// ===============================================================
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("Iniciando ESP32 DHT+BH1750+BME680...");

  // Mostrar motivo del último reset
  esp_reset_reason_t reason = esp_reset_reason();
  Serial.print("Reset reason (int): ");
  Serial.println((int)reason);

  // I2C compartido
  Wire.begin(SDA_I2C, SCL_I2C);

  // DHT
  dht.begin();

  // BH1750
  initBH1750();

  // BME680
  initBME680();

  // WiFi inicial
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Conectando a WiFi");
  uint8_t intentos = 0;
  while (WiFi.status() != WL_CONNECTED && intentos < WIFI_MAX_RETRIES) {
    delay(500);
    Serial.print(".");
    intentos++;
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi conectado. IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("[WIFI] No se logró conexión inicial, reiniciando ESP32...");
    esp_restart();
  }

  Serial.println("Setup completo (DHT+BH+ BME).");
}

// ===============================================================
//                         LOOP
// ===============================================================
void loop() {
  unsigned long ahora = millis();

  // ---------- BLOQUE DHT + BH1750 ----------
  if (ahora - ultimoDhtBhMs >= INTERVALO_DHT_BH_MS) {
    ultimoDhtBhMs = ahora;

    Serial.println("===== NUEVA LECTURA DHT+BH1750 =====");

    // ---- DHT22 ----
    float tempDht = dht.readTemperature();
    float humDht  = dht.readHumidity();

    bool dhtOk = true;
    if (isnan(tempDht) || isnan(humDht)) {
      Serial.println("[DHT22] ERROR: lectura inválida, NO se envía este paquete");
      dhtOk = false;
    }

    // ---- BH1750 ----
    float lightLux = leerBH1750();
    bool bhOk = true;
    if (lightLux < 0.0f) {
      Serial.println("[BH1750] ERROR: lectura inválida, NO se envía este paquete");
      bhOk = false;
    }

    if (dhtOk && bhOk) {
      Serial.printf("DHT22  -> T: %.2f C, H: %.2f %%\n", tempDht, humDht);
      Serial.printf("BH1750 -> %.2f lux\n", lightLux);
      enviarDhtBhALaApi(tempDht, humDht, lightLux);
    } else {
      Serial.println("[DHT+BH] Paquete descartado por errores en sensores.");
    }
  }

  // ---------- BLOQUE BME680 ----------
  if (ahora - ultimoBmeMs >= INTERVALO_BME_MS) {
    ultimoBmeMs = ahora;

    Serial.println("===== NUEVA LECTURA BME680 =====");

    float tempBme = -1.0f, humBme = -1.0f, pres = -1.0f, gas = -1.0f;
    if (!leerBME680(tempBme, humBme, pres, gas)) {
      Serial.println("[BME680] Lectura inválida, NO se envía este paquete");
    } else {
      Serial.printf("BME680 -> T: %.2f C, H: %.2f %%, P: %.2f hPa, Gas: %.2f ohms\n",
                    tempBme, humBme, pres, gas);
      enviarBmeALaApi(tempBme, humBme, pres, gas);
    }
  }

  // Loop libre, sin delay grande
}
