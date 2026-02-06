# 🌡️ ESP32 Environmental Monitoring System (DHT22 + BH1750 + BME680)

Sistema de monitoreo ambiental en tiempo real basado en **ESP32**, diseñado para recopilar datos de múltiples sensores y enviarlos a una **API REST** mediante **HTTP** de forma robusta y tolerante a fallos.

Este proyecto está pensado para entornos reales como **aulas, laboratorios y espacios cerrados**, integrando reconexión WiFi automática, manejo de errores HTTP, reintentos, validación de sensores y reinicio controlado del microcontrolador.

---

## 🚀 Características

- 📡 Conectividad WiFi con reconexión automática
- 🌡️ Lectura de temperatura y humedad con **DHT22**
- 💡 Medición de iluminación ambiental con **BH1750**
- 🌬️ Medición avanzada ambiental con **BME680**:
  - Temperatura
  - Humedad
  - Presión atmosférica
  - Resistencia de gas
- 🔁 Envío periódico de datos en formato **JSON**
- 🛡️ Sistema de recuperación ante fallos:
  - Reintentos WiFi
  - Contadores de errores HTTP
  - Re-inicialización de sensores
  - Reinicio automático del ESP32 si es necesario
- 🧠 Uso de bus **I2C compartido**
- 📊 Endpoints independientes por bloque de sensores

---

## 🧩 Arquitectura general

- **ESP32** como nodo IoT principal
- Sensores:
  - DHT22 (GPIO)
  - BH1750 (I2C)
  - BME680 (I2C)
- API REST externa
- Comunicación vía HTTP POST con payload JSON

---

## 🔌 Sensores utilizados

| Sensor  | Tipo | Variables |
|-------|------|----------|
| DHT22 | Digital | Temperatura (°C), Humedad (%) |
| BH1750 | I2C | Iluminación (lux) |
| BME680 | I2C | Temperatura, Humedad, Presión (hPa), Gas (Ω) |

---

## 🔧 Conexiones de hardware

### I2C

SDA → GPIO 21
SCL → GPIO 22


### DHT22

DATA → GPIO 13


### 📡 Endpoints de la API

POST /api/dht-light-readings
POST /api/bme-readings


### Ejemplo de payload (DHT22 + BH1750)

{
  "deviceId": "esp32-dht-light-01",
  "sensors": {
    "temp_dht_c": 25.40,
    "humidity_pct": 48.20,
    "light_lux": 312.50
  }
}



### Ejemplo de payload (BME680)

{
  "deviceId": "esp32-bme-01",
  "sensors": {
    "temp_bme_c": 24.90,
    "humidity_bme_pct": 46.80,
    "pressure_hpa": 1013.20,
    "gas_resistance_ohms": 83215.00
  }
}


### ⏱️ Intervalos de envío

| Bloque de sensores | Intervalo  |
| ------------------ | ---------- |
| DHT22 + BH1750     | 5 segundos |
| BME680             | 5 segundos |



### 🛡️ Manejo de errores

❌ Lectura inválida de sensor → paquete descartado

🔁 Fallos HTTP consecutivos → reinicio del ESP32

📡 Pérdida de WiFi → reconexión automática

🔄 Fallo de reconexión → reinicio completo

🧠 Motivo del último reset visible en Serial Monitor

### 📚 Librerías utilizadas

-WiFi.h

-HTTPClient.h

-Wire.h

-DHT.h

-BH1750.h

-Adafruit_Sensor.h

-Adafruit_BME680.h

Instalar desde el Library Manager del Arduino IDE.

### ▶️ Uso del proyecto

-Clona este repositorio

-Abre el sketch en Arduino IDE

-Configura tus credenciales WiFi:

const char* ssid = "TU_WIFI";
const char* password = "TU_PASSWORD";


-Ajusta las URLs de la API si es necesario

-Selecciona la placa ESP32

-Compila y sube el código

-Abre el Serial Monitor (115200 baud)

### 🧪 Casos de uso

-Monitoreo ambiental en aulas

-Laboratorios

-Proyectos IoT académicos

-Dashboards ambientales en tiempo real

-Sistemas de control climático

### 📌 Estado del proyecto

✅ Estable
🧩 Modular
🔒 Robusto
🚀 Listo para producción educativa / IoT

### 👨‍💻 Autor

Ángel David Onesto Frías
Proyecto IoT – ESP32 + Sensores + API REST

### 📜 Licencia

Proyecto con fines educativos y de portafolio.
