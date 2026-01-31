#include "SensorDisplayManager.h"

// ===== CONSTRUCTOR / DESTRUCTOR =====

SensorDisplayManager::SensorDisplayManager()
    : sensorPtr(nullptr)
    , nextionPtr(nullptr)
    , lastTemp(-99)
    , lastTDS(-1)
    , lastFlow(-1.0f)
    , lastSyncMs(0)
{
    mutex = xSemaphoreCreateMutex();
}

SensorDisplayManager::~SensorDisplayManager() {
    if (mutex) vSemaphoreDelete(mutex);
}

// ===== PUBLIC API =====

void SensorDisplayManager::begin(SensorManager* sensor, NextionGateWay* nextion) {
    if (!sensor || !nextion) {
        Serial.println("[SENSOR_DISPLAY] ERROR: Invalid pointers");
        return;
    }
    
    lock();
    sensorPtr = sensor;
    nextionPtr = nextion;
    lastSyncMs = millis();
    unlock();
    
    Serial.println("[SENSOR_DISPLAY] Initialized successfully");
}

void SensorDisplayManager::syncToNextion() {
    if (!sensorPtr || !nextionPtr) {
        Serial.println("[SENSOR_DISPLAY] ERROR: Not initialized");
        return;
    }
    
    // Get current sensor data
    SensorData current = sensorPtr->getData();
    
    // Validate sensor data
    if (!isSensorDataValid()) {
        Serial.println("[SENSOR_DISPLAY] WARNING: Invalid sensor data");
        return;
    }
    
    lock();
    
    // Check if any sensor changed or force sync time exceeded
    bool needsUpdate = false;
    
    if (isTemperatureChanged(current.temperature)) {
        needsUpdate = true;
        lastTemp = (int)current.temperature;
    }
    
    if (isTDSChanged(current.tdsValue)) {
        needsUpdate = true;
        lastTDS = current.tdsValue;
    }
    
    if (isFlowChanged(current.flowRate)) {
        needsUpdate = true;
        lastFlow = current.flowRate;
    }
    
    // Force update every 5 seconds
    if (isForceSync()) {
        needsUpdate = true;
        lastTemp = (int)current.temperature;
        lastTDS = current.tdsValue;
        lastFlow = current.flowRate;
        lastSyncMs = millis();
    }
    
    unlock();
    
    // Send to Nextion if update needed
    if (needsUpdate) {
        sendTemperature();
        sendTDS();
        sendFlow();
    }
}

// ===== INDIVIDUAL SENDERS =====

void SensorDisplayManager::sendTemperature() {
    if (!sensorPtr || !nextionPtr) return;
    
    SensorData data = sensorPtr->getData();
    
    if (!data.tempValid) {
        nextionPtr->send("nTemp.val=0");  // ← CHANGED
        return;
    }
    
    int tempInt = (int)round(data.temperature);
    
    if (tempInt < -50) tempInt = -50;
    if (tempInt > 100) tempInt = 100;
    
    String cmd = "nTemp.val=" + String(tempInt);  // ← CHANGED (from .txt to .val)
    nextionPtr->send(cmd);
}

void SensorDisplayManager::sendTDS() {
    if (!sensorPtr || !nextionPtr) return;
    
    SensorData data = sensorPtr->getData();
    
    if (!data.tdsValid) {
        nextionPtr->send("nTDS.val=0");  // ← CHANGED
        return;
    }
    
    int tdsValue = data.tdsValue;
    
    if (tdsValue < 0) tdsValue = 0;
    if (tdsValue > 9999) tdsValue = 9999;
    
    String cmd = "nTDS.val=" + String(tdsValue);  // ← CHANGED (from .txt to .val)
    nextionPtr->send(cmd);
}

void SensorDisplayManager::sendFlow() {
    if (!sensorPtr || !nextionPtr) return;
    
    SensorData data = sensorPtr->getData();
    
    char buffer[10];
    snprintf(buffer, sizeof(buffer), "%.2f", data.flowRate);
    
    String cmd = "tFlow.txt=\"" + String(buffer) + "\"";  // ← SAMA (text component)
    nextionPtr->send(cmd);
}


// ===== DEBUG =====

void SensorDisplayManager::printLastSync() {
    lock();
    
    Serial.println("╔════════════════════════════════════╗");
    Serial.println("║   SENSOR DISPLAY LAST SYNC         ║");
    Serial.println("╠════════════════════════════════════╣");
    
    Serial.print("║ Last Temp    : ");
    Serial.print(lastTemp);
    Serial.println(" °C        ║");
    
    Serial.print("║ Last TDS     : ");
    Serial.print(lastTDS);
    Serial.println(" ppm       ║");
    
    Serial.print("║ Last Flow    : ");
    Serial.print(lastFlow, 2);
    Serial.println(" L/min   ║");
    
    unsigned long timeSinceSync = millis() - lastSyncMs;
    Serial.print("║ Time since sync : ");
    Serial.print(timeSinceSync);
    Serial.println("ms    ║");
    
    Serial.println("╚════════════════════════════════════╝");
    
    unlock();
}

// ===== PRIVATE METHODS =====

bool SensorDisplayManager::isSensorDataValid() {
    SensorData data = sensorPtr->getData();
    
    // At least one sensor should be valid or have meaningful data
    if (!data.tempValid && !data.tdsValid && data.flowRate < 0) {
        return false;
    }
    
    // Check if data is stale (more than 10 seconds old)
    if (millis() - data.lastUpdate > 10000) {
        return false;
    }
    
    return true;
}

bool SensorDisplayManager::isTemperatureChanged(int currentTemp) {
    if (!isDigit(currentTemp)) {
        // Temperature not valid, skip
        return false;
    }
    
    int current = (int)round(currentTemp);
    
    if (abs(current - lastTemp) >= TEMP_THRESHOLD) {
        return true;
    }
    
    return false;
}

bool SensorDisplayManager::isTDSChanged(int currentTDS) {
    if (currentTDS < 0) {
        // TDS not valid, skip
        return false;
    }
    
    if (abs(currentTDS - lastTDS) >= TDS_THRESHOLD) {
        return true;
    }
    
    return false;
}

bool SensorDisplayManager::isFlowChanged(float currentFlow) {
    if (currentFlow < 0) {
        // Flow error, skip
        return false;
    }
    
    if (abs(currentFlow - lastFlow) >= FLOW_THRESHOLD) {
        return true;
    }
    
    return false;
}

bool SensorDisplayManager::isForceSync() {
    unsigned long elapsed = millis() - lastSyncMs;
    return (elapsed >= FORCE_SYNC_MS);
}

// ===== MUTEX =====

void SensorDisplayManager::lock() {
    if (mutex) {
        xSemaphoreTake(mutex, portMAX_DELAY);
    }
}

void SensorDisplayManager::unlock() {
    if (mutex) {
        xSemaphoreGive(mutex);
    }
}