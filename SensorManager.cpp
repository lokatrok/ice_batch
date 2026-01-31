#include "SensorManager.h"

// Pin definitions (sesuaikan dengan HardwareConfig.h Anda)
#define PIN_TEMP_SENSOR   32
#define PIN_FLOAT_SENSOR  5
#define PIN_FLOW_SENSOR   14
#define PIN_FLOW_SWITCH   35
#define PIN_TDS_SENSOR    34

// Flow sensor calibration (YF-S201)
const float SensorManager::FLOW_CALIBRATION = 450.0f;  // pulses per liter
const float SensorManager::FLOW_MAX_RATE = 30.0f;       // L/min cap

// Static instance untuk ISR
SensorManager* SensorManager::instance = nullptr;

// ===== ISR untuk Flow Sensor =====
void IRAM_ATTR SensorManager::flowISR() {
    if (instance) {
        instance->pulseCount++;
    }
}

// ===== CONSTRUCTOR / DESTRUCTOR =====

SensorManager::SensorManager()
    : pulseCount(0)
    , lastPulseCount(0)
    , lastFlowReadMs(0)
{
    mutex = xSemaphoreCreateMutex();
    instance = this;  // Set static instance
    
    // Init data
    data.temperature = -99.0f;
    data.tempValid = false;
    data.flowRate = 0.0f;
    data.totalPulses = 0;
    data.floatSensor = false;
    data.flowSwitch = false;
    data.tdsValue = -1;
    data.tdsValid = false;
    data.lastUpdate = 0;
    
    oneWire = nullptr;
    ds18b20 = nullptr;
}

SensorManager::~SensorManager() {
    if (ds18b20) delete ds18b20;
    if (oneWire) delete oneWire;
    if (mutex) vSemaphoreDelete(mutex);
}

// ===== PUBLIC API =====

void SensorManager::begin() {
    // ===== TEMPERATURE SENSOR (DS18B20) =====
    oneWire = new OneWire(PIN_TEMP_SENSOR);
    ds18b20 = new DallasTemperature(oneWire);
    ds18b20->begin();
    ds18b20->setResolution(12);
    ds18b20->requestTemperatures();
    
    // ===== FLOW SENSOR (YF-S201) =====
    pinMode(PIN_FLOW_SENSOR, INPUT_PULLUP);
    attachInterrupt(
        digitalPinToInterrupt(PIN_FLOW_SENSOR),
        flowISR,
        RISING
    );
    pulseCount = 0;
    lastPulseCount = 0;
    lastFlowReadMs = millis();
    
    // ===== DIGITAL INPUTS =====
    pinMode(PIN_FLOAT_SENSOR, INPUT_PULLUP);
    pinMode(PIN_FLOW_SWITCH, INPUT_PULLUP);  // External pull-up
    
    // ===== TDS SENSOR (Analog) =====
    pinMode(PIN_TDS_SENSOR, INPUT);
    
    Serial.println("[SENSOR] All sensors initialized");
}

void SensorManager::update() {
    lock();
    
    updateTemperature();
    updateFlow();
    updateTDS();
    updateDigitalInputs();
    
    data.lastUpdate = millis();
    
    unlock();
}

// ===== GETTERS =====

SensorData SensorManager::getData() {
    SensorData copy;
    lock();
    copy = data;
    unlock();
    return copy;
}

float SensorManager::getTemperature() {
    lock();
    float temp = data.temperature;
    unlock();
    return temp;
}

float SensorManager::getFlowRate() {
    lock();
    float flow = data.flowRate;
    unlock();
    return flow;
}

bool SensorManager::getFloatSensor() {
    lock();
    bool state = data.floatSensor;
    unlock();
    return state;
}

bool SensorManager::getFlowSwitch() {
    lock();
    bool state = data.flowSwitch;
    unlock();
    return state;
}

int SensorManager::getTDS() {
    lock();
    int tds = data.tdsValue;
    unlock();
    return tds;
}

// ===== DEBUG =====

      void SensorManager::printSensorData() {
          lock();
          
          Serial.println("╔════════════════════════════════════╗");
          Serial.println("║      SENSOR DATA                   ║");
          Serial.println("╠════════════════════════════════════╣");
          
          Serial.print("║ Temperature  : ");
          if (data.tempValid) {
              Serial.print(data.temperature, 1);
              Serial.println(" °C     ║");
          } else {
              Serial.println("INVALID       ║");
          }
          
          Serial.print("║ Flow Rate    : ");
          Serial.print(data.flowRate, 2);
          Serial.println(" L/min  ║");
          
          Serial.print("║ Total Pulses : ");
          Serial.print(data.totalPulses);
          Serial.println("           ║");
          
          Serial.print("║ Float Sensor : ");
          Serial.println(data.floatSensor ? "DETECTED  ║" : "EMPTY     ║");
          
          Serial.print("║ Flow Switch  : ");
          Serial.println(data.flowSwitch ? "FLOW      ║" : "NO FLOW   ║");
          
          Serial.print("║ TDS          : ");
          if (data.tdsValid) {
              Serial.print(data.tdsValue);
              Serial.println(" ppm       ║");
          } else {
              Serial.println("INVALID       ║");
          }
          
          Serial.println("╚════════════════════════════════════╝");
          
          unlock();
      }

// ===== PRIVATE UPDATE METHODS =====

      void SensorManager::updateTemperature() {
          static unsigned long lastTempReadMs = 0;
          
          if (millis() - lastTempReadMs >= 1000) {  // Update setiap 1 detik
              ds18b20->requestTemperatures();
              float temp = ds18b20->getTempCByIndex(0);
              
              // Validasi (DS18B20 return -127 atau 85 jika error)
              if (temp == -127.0f || temp == 85.0f) {
                  data.temperature = -99.0f;
                  data.tempValid = false;
              } else {
                  data.temperature = temp;
                  data.tempValid = true;
              }
              
              lastTempReadMs = millis();
          }
      }

      void SensorManager::updateFlow() {
          unsigned long now = millis();
          unsigned long elapsedMs = now - lastFlowReadMs;
          
          if (elapsedMs >= 500) {  // Update setiap 500ms
              noInterrupts();
              uint32_t pulses = pulseCount - lastPulseCount;
              lastPulseCount = pulseCount;
              data.totalPulses = pulseCount;
              interrupts();
              
              if (pulses == 0 || elapsedMs == 0) {
                  data.flowRate = 0.0f;
              } else {
                  float liters = pulses / FLOW_CALIBRATION;
                  data.flowRate = (liters * 60000.0f) / elapsedMs;  // Convert to L/min
                  
                  // Cap maximum flow rate
                  if (data.flowRate > FLOW_MAX_RATE) {
                      data.flowRate = FLOW_MAX_RATE;
                  }
              }
              
              lastFlowReadMs = now;
          }
      }

void SensorManager::updateTDS() {
    constexpr int SCOUNT = 15;          // cukup, tidak berat
    static int adcBuf[SCOUNT];
    static int idx = 0;
    static bool bufFull = false;
    
    // === Filter EMA untuk hasil TDS agar lebih stabil ===
    static float filteredTDS = 0.0f;
    static bool firstRun = true;
    constexpr float ALPHA = 0.1f;      // smoothing factor (0.1-0.3 bagus)

    // === Sampling ADC ===
    adcBuf[idx++] = analogRead(PIN_TDS_SENSOR);
    if (idx >= SCOUNT) {
        idx = 0;
        bufFull = true;
    }

    // Buffer belum penuh → jangan hitung dulu
    if (!bufFull) {
        data.tdsValid = false;
        return;
    }

    // === Copy buffer untuk median filter ===
    int temp[SCOUNT];
    for (int i = 0; i < SCOUNT; i++) {
        temp[i] = adcBuf[i];
    }

    // === Median filter ===
    for (int i = 0; i < SCOUNT - 1; i++) {
        for (int j = 0; j < SCOUNT - i - 1; j++) {
            if (temp[j] > temp[j + 1]) {
                int t = temp[j];
                temp[j] = temp[j + 1];
                temp[j + 1] = t;
            }
        }
    }

    int medianRaw = (SCOUNT & 1)
        ? temp[SCOUNT / 2]
        : (temp[SCOUNT / 2] + temp[SCOUNT / 2 - 1]) / 2;

    // === Konversi ADC → Tegangan untuk 5V ===
    float voltage = medianRaw * (5.0f / 4095.0f);

    // Validasi dasar
    if (voltage < 0.1f || voltage > 4.9f) {
        data.tdsValue = -1;
        data.tdsValid = false;
        return;
    }

    // === Temperature compensation ===
    float compensation = 1.0f;
    if (data.tempValid && data.temperature > -10.0f && data.temperature < 80.0f) {
        compensation = 1.0f + 0.02f * (data.temperature - 25.0f);
    }

    float compensatedVoltage = voltage / compensation;

    // === Rumus TDS untuk 5V (koefisien disesuaikan) ===
    // Rumus polynomial yang umum untuk TDS meter 5V:
    float tds = (133.42f * compensatedVoltage * compensatedVoltage * compensatedVoltage
                - 255.86f * compensatedVoltage * compensatedVoltage
                + 857.39f * compensatedVoltage) * 0.5f;

    // Clamp nilai mentah
    if (tds < 0) tds = 0;
    if (tds > 9999) tds = 9999;

    // === Filter EMA untuk stabilkan output ===
    if (firstRun) {
        filteredTDS = tds;
        firstRun = false;
    } else {
        filteredTDS = ALPHA * tds + (1.0f - ALPHA) * filteredTDS;
    }

    // === Anti-jitter: hanya update jika perubahan > threshold ===
    constexpr float CHANGE_THRESHOLD = 7.0f;  // ppm
    if (abs(filteredTDS - data.tdsValue) >= CHANGE_THRESHOLD || !data.tdsValid) {
        data.tdsValue = static_cast<int>(filteredTDS);
    }
    
    data.tdsValid = true;
  }


      void SensorManager::updateDigitalInputs() {
          // Float sensor (Active LOW)
          data.floatSensor = (digitalRead(PIN_FLOAT_SENSOR) == LOW);
          
          // Flow switch (Active LOW)
          data.flowSwitch = (digitalRead(PIN_FLOW_SWITCH) == LOW);
      }

// ===== MUTEX =====

void SensorManager::lock() {
    if (mutex) {
        xSemaphoreTake(mutex, portMAX_DELAY);
    }
}

void SensorManager::unlock() {
    if (mutex) {
        xSemaphoreGive(mutex);
    }
}