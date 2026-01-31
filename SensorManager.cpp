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
    int raw = analogRead(PIN_TDS_SENSOR);
    float voltage = raw * (3.3f / 4095.0f);  // ESP32 ADC 12-bit, 3.3V
    
    // Validasi range
    if (raw < 100 || raw > 4000 || voltage < 0.1f || voltage > 3.2f) {
        data.tdsValue = -1;
        data.tdsValid = false;
        return;
    }
    
    // Konversi voltage ke EC (Electrical Conductivity)
    float ec = 133.42f * voltage * voltage * voltage
             - 255.86f * voltage * voltage
             + 857.39f * voltage;
    
    // Temperature compensation
    float refTemp = 25.0f;
    float coef = 0.02f;
    
    if (data.tempValid && data.temperature > -50.0f && data.temperature < 80.0f) {
        ec /= (1.0f + coef * (data.temperature - refTemp));
    }
    
    // Convert EC to TDS (ppm)
    data.tdsValue = static_cast<int>(ec * 0.5f);
    
    // Clamp range
    if (data.tdsValue < 0) data.tdsValue = 0;
    if (data.tdsValue > 9999) data.tdsValue = 9999;
    
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