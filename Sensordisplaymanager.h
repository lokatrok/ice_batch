#ifndef SENSOR_DISPLAY_MANAGER_H
#define SENSOR_DISPLAY_MANAGER_H

#include <Arduino.h>
#include "SensorManager.h"
#include "NextionGateWay.h"

// ===== SENSOR DISPLAY MANAGER CLASS =====
class SensorDisplayManager {
public:
    SensorDisplayManager();
    ~SensorDisplayManager();
    
    void begin(SensorManager* sensor, NextionGateWay* nextion);
    
    // Main update method - call dari task
    void syncToNextion();
    
    // Individual component senders (optional)
    void sendTemperature();
    void sendTDS();
    void sendFlow();
    
    // Debug
    void printLastSync();

private:
    SensorManager* sensorPtr;
    NextionGateWay* nextionPtr;
    
    // Last synced data for change detection
    int lastTemp;
    int lastTDS;
    float lastFlow;
    unsigned long lastSyncMs;
    
    SemaphoreHandle_t mutex;
    
    // Change detection thresholds
    static constexpr int TEMP_THRESHOLD = 1;        // 1°C change
    static constexpr int TDS_THRESHOLD = 10;        // 10 ppm change
    static constexpr float FLOW_THRESHOLD = 0.5f;   // 0.5 L/min change
    static constexpr unsigned long FORCE_SYNC_MS = 5000;  // Force update every 5s
    
    // Private methods
    bool isSensorDataValid();
    bool isTemperatureChanged(int currentTemp);
    bool isTDSChanged(int currentTDS);
    bool isFlowChanged(float currentFlow);
    bool isForceSync();
    
    void lock();
    void unlock();
};

#endif