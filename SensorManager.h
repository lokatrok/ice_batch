#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// ===== SENSOR DATA STRUCT =====
struct SensorData {
    // Temperature
    float temperature;        // °C
    bool tempValid;           // Temperature sensor valid
    
    // Flow
    float flowRate;           // L/min
    uint32_t totalPulses;     // Total pulse count
    
    // Water Level & Switch
    bool floatSensor;         // true = water detected
    bool flowSwitch;          // true = flow detected
    
    // TDS (Total Dissolved Solids)
    int tdsValue;             // ppm
    bool tdsValid;            // TDS sensor valid
    
    // Metadata
    unsigned long lastUpdate;
};

// ===== SENSOR MANAGER CLASS =====
class SensorManager {
public:
    SensorManager();
    ~SensorManager();
    
    void begin();
    void update();  // Call dari task berkala
    
    // ===== GETTERS =====
    SensorData getData();
    
    float getTemperature();
    float getFlowRate();
    bool getFloatSensor();
    bool getFlowSwitch();
    int getTDS();
    
    // ===== DEBUG =====
    void printSensorData();
    
private:
    SemaphoreHandle_t mutex;
    SensorData data;
    
    // Temperature (DS18B20)
    OneWire* oneWire;
    DallasTemperature* ds18b20;
    
    // Flow sensor (YF-S201)
    volatile uint32_t pulseCount;
    uint32_t lastPulseCount;
    unsigned long lastFlowReadMs;
    static const float FLOW_CALIBRATION;
    static const float FLOW_MAX_RATE;
    static void IRAM_ATTR flowISR();
    static SensorManager* instance;  // For ISR
    
    // Private methods
    void updateTemperature();
    void updateFlow();
    void updateTDS();
    void updateDigitalInputs();
    
    void lock();
    void unlock();
};

#endif