// StateConditionHandler.cpp
#include "StateConditionHandler.h"

// ===== CONSTANTS =====
static const unsigned long COOLING_WAIT_PERIOD = 10 * 60 * 1000; // 10 menit dalam ms

// ===== CONSTRUCTOR / DESTRUCTOR =====

StateConditionHandler::StateConditionHandler(
    SensorManager* sensors,
    ActuatorControl* actuators,
    NextionOutput* display
) 
    : sensor(sensors)
    , actuator(actuators)
    , nextion(display)
    , coolingTarget(0)
    , coolingStartTime(0)
    , compressorOffTime(0)
    , compressorActive(false)
    , inWaitPeriod(false)
    , lastFlowState(false)
    , lastFloatState(false)
    , floatTrueStartTime(0)
{
    Serial.println("[StateConditionHandler] Initialized");
}

StateConditionHandler::~StateConditionHandler() {
    // Cleanup
}

// ===== FILLING METHODS =====

void StateConditionHandler::startFilling() {
    Serial.println("[FILLING] Starting filling sequence");
    
    // Open inlet valve
    actuator->setValveInlet(true);
    
    // Reset error blink jika ada
    nextion->setErrorBlink(false);
    
    // Reset flow state tracking
    lastFlowState = sensor->getFlowSwitch();
    
    // Reset float sensor debounce
    lastFloatState = false;
    floatTrueStartTime = 0;
    Serial.println("[FILLING] Float sensor debounce reset");
}

FillingStatus StateConditionHandler::checkFillingCondition() {
    SensorData data = sensor->getData();
    
    // ===== FLOAT SENSOR DEBOUNCING =====
    if (data.floatSensor) {
        // Float sensor TRUE
        if (!lastFloatState) {
            // Baru pertama kali TRUE - mulai tracking
            floatTrueStartTime = millis();
            lastFloatState = true;
            Serial.println("[FILLING] Float sensor rising edge - Starting debounce");
        } else {
            // Sudah TRUE sebelumnya - cek durasi
            unsigned long floatTrueDuration = millis() - floatTrueStartTime;
            
            if (floatTrueDuration >= FLOAT_DEBOUNCE_MS) {
                // Sudah stabil selama 2 detik - VALID!
                Serial.print("[FILLING] Float sensor STABLE for ");
                Serial.print(floatTrueDuration);
                Serial.println("ms - COMPLETE");
                return FillingStatus::FILLING_COMPLETE;
            } else {
                // Masih dalam debounce period
                Serial.print("[FILLING] Float sensor debouncing... ");
                Serial.print(floatTrueDuration);
                Serial.print("/");
                Serial.print(FLOAT_DEBOUNCE_MS);
                Serial.println("ms");
            }
        }
    } else {
        // Float sensor FALSE - reset debounce
        if (lastFloatState) {
            Serial.println("[FILLING] Float sensor falling edge - Reset debounce");
        }
        lastFloatState = false;
        floatTrueStartTime = 0;
    }
    
    // Priority 2: Check flow switch error (TIDAK ADA ALIRAN)
    // Hanya check error jika valve sedang terbuka (filling active)
    bool valveOpen = actuator->getValveInletState();
    
    if (valveOpen && !data.flowSwitch) {
        // Valve terbuka tapi tidak ada flow - ERROR
        if (lastFlowState != data.flowSwitch) {
            Serial.println("[FILLING] Flow switch error - NO FLOW DETECTED");
            nextion->setErrorBlink(true);
        }
        lastFlowState = data.flowSwitch;
        return FillingStatus::FILLING_ERROR;
    }
    
    // Normal: Continue filling
    // Clear error blink jika flow kembali normal
    if (!lastFlowState && data.flowSwitch) {
        nextion->setErrorBlink(false);
        Serial.println("[FILLING] Flow restored");
    }
    
    lastFlowState = data.flowSwitch;
    return FillingStatus::FILLING_CONTINUE;
}

void StateConditionHandler::stopFilling() {
    Serial.println("[FILLING] Stopping filling sequence");
    
    // Close inlet valve
    actuator->setValveInlet(false);
    
    // JANGAN clear error blink di sini
    // Biarkan ERROR state atau IDLE state yang handle
}

// ===== COOLING METHODS =====

void StateConditionHandler::startCooling(uint8_t targetTemp) {
    Serial.print("[COOLING] Starting cooling - Target: ");
    Serial.print(targetTemp);
    Serial.println("°C");
    
    coolingTarget = targetTemp;
    coolingStartTime = millis();
    compressorOffTime = 0;
    compressorActive = true;
    inWaitPeriod = false;
    
    // Start compressor & pump
    actuator->setCompressor(true);
    actuator->setPumpUV(true);
    
    // Update display
    updateCoolingDisplay();
}

void StateConditionHandler::updateCooling() {
    SensorData data = sensor->getData();
    
    // Update display setiap call (akan di-throttle oleh Nextion jika terlalu cepat)
    updateCoolingDisplay();
    
    // ===== STATE 1: COMPRESSOR ACTIVE (Cooling down) =====
    if (compressorActive && !inWaitPeriod) {
        if (data.temperature <= coolingTarget) {
            // Target tercapai - matikan compressor
            Serial.print("[COOLING] Target reached (");
            Serial.print(data.temperature);
            Serial.println("°C) - Compressor OFF");
            
            actuator->setCompressor(false);
            compressorActive = false;
            compressorOffTime = millis();
            inWaitPeriod = true;
            
            // Pump tetap jalan (tidak dimatikan)
        }
    }
    
    // ===== STATE 2: WAITING PERIOD (10 menit) =====
    else if (inWaitPeriod) {
        unsigned long elapsedWait = millis() - compressorOffTime;
        
        if (elapsedWait >= COOLING_WAIT_PERIOD) {
            // 10 menit sudah lewat - cek temperature
            Serial.println("[COOLING] Wait period complete - Checking temperature");
            
            if (data.temperature > coolingTarget) {
                // Temperature naik lagi - restart compressor
                Serial.print("[COOLING] Temperature rose to ");
                Serial.print(data.temperature);
                Serial.println("°C - Restarting compressor");
                
                actuator->setCompressor(true);
                compressorActive = true;
                inWaitPeriod = false;
            } else {
                // Masih di bawah target - reset timer, tunggu lagi
                Serial.println("[COOLING] Temperature still OK - Continue waiting");
                compressorOffTime = millis();
            }
        }
    }
}

void StateConditionHandler::stopCooling() {
    Serial.println("[COOLING] Stopping cooling sequence");
    
    // Matikan compressor & pump
    actuator->setCompressor(false);
    actuator->setPumpUV(false);
    
    // Reset state
    compressorActive = false;
    inWaitPeriod = false;
}

void StateConditionHandler::updateCoolingDisplay() {
    uint16_t elapsedMinutes = getElapsedMinutes(coolingStartTime);
    nextion->updateCoolingDuration(elapsedMinutes);
}

// ===== DRAINING METHODS =====

void StateConditionHandler::startDraining() {
    Serial.println("[DRAINING] Starting draining sequence");
    
    // Open drain valve
    actuator->setValveDrain(true);
}

DrainingStatus StateConditionHandler::checkDrainingCondition() {
    SensorData data = sensor->getData();
    
    // Check if flow stopped (selesai drain)
    if (!data.flowSwitch) {
        Serial.println("[DRAINING] Flow stopped - COMPLETE");
        return DrainingStatus::DRAINING_COMPLETE;
    }
    
    // Continue draining
    return DrainingStatus::DRAINING_CONTINUE;
}

void StateConditionHandler::stopDraining() {
    Serial.println("[DRAINING] Stopping draining sequence");
    
    // Close drain valve
    actuator->setValveDrain(false);
}

// ===== ERROR RECOVERY =====

bool StateConditionHandler::isErrorCleared() {
    SensorData data = sensor->getData();
    
    // Error cleared jika flow switch kembali normal
    if (data.flowSwitch) {
        Serial.println("[ERROR] Flow restored - Error cleared");
        return true;
    }
    
    return false;
}

// ===== HELPER METHODS =====

uint16_t StateConditionHandler::getElapsedMinutes(unsigned long startTime) {
    unsigned long elapsed = millis() - startTime;
    return elapsed / 60000;  // Convert ms to minutes
}