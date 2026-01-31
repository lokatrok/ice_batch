// SystemState.cpp
#include "SystemState.h"

// ===== CONSTRUCTOR / DESTRUCTOR =====

SystemStateMachine::SystemStateMachine() 
    : currentState(SystemState::STATE_IDLE)
    , previousState(SystemState::STATE_IDLE)
    , stateEntryTime(0)
{
    mutex = xSemaphoreCreateMutex();
    stateEntryTime = millis();
}

SystemStateMachine::~SystemStateMachine() {
    if (mutex) vSemaphoreDelete(mutex);
}

// ===== PUBLIC API =====

void SystemStateMachine::update(const NextionData &data) {
    lock();
    
    SystemState newState = determineState(data);
    
    if (newState != currentState) {
        transitionTo(newState);
    }
    
    unlock();
}

SystemState SystemStateMachine::getState() const {
    return currentState;
}

String SystemStateMachine::stateName() const {
    switch (currentState) {
        case SystemState::STATE_IDLE:              return "IDLE";
        case SystemState::STATE_FILLING:           return "FILLING";
        case SystemState::STATE_COOLING:           return "COOLING";
        case SystemState::STATE_DRAINING:          return "DRAINING";
        case SystemState::STATE_AUTO:              return "AUTO";
        case SystemState::STATE_AUTO_CIRCULATION:  return "AUTO_CIRCULATION";
        case SystemState::STATE_BYPASS_MENU:       return "BYPASS_MENU";
        case SystemState::STATE_ERROR:             return "ERROR";
        default:                                   return "UNKNOWN";
    }
}

bool SystemStateMachine::isInAutoMode() const {
    return (currentState == SystemState::STATE_AUTO || 
            currentState == SystemState::STATE_AUTO_CIRCULATION);
}

void SystemStateMachine::reset() {
    lock();
    transitionTo(SystemState::STATE_IDLE);
    unlock();
}

// ===== PRIVATE METHODS =====

void SystemStateMachine::transitionTo(SystemState newState) {
    onStateExit(currentState);
    
    previousState = currentState;
    currentState = newState;
    stateEntryTime = millis();
    
    onStateEntry(currentState);
}

SystemState SystemStateMachine::determineState(const NextionData &data) {
    // Priority 1: Bypass Menu
    if (data.inBypassMenu) {
        return SystemState::STATE_BYPASS_MENU;
    }
    
    // Priority 2: Auto Mode (dengan sub-state circulation)
    if (data.autoStatus) {
        // Di dalam AUTO, cek apakah circulation aktif
        if (data.circulationStatus) {
            return SystemState::STATE_AUTO_CIRCULATION;
        } else {
            return SystemState::STATE_AUTO;
        }
    }
    
    // Priority 3: Manual Operations (circulation tidak bisa aktif di sini)
    if (data.fillingStatus) {
        return SystemState::STATE_FILLING;
    }
    
    if (data.coolingStatus) {
        return SystemState::STATE_COOLING;
    }
    
    if (data.drainingStatus) {
        return SystemState::STATE_DRAINING;
    }
    
    // Default: Idle
    return SystemState::STATE_IDLE;
}

void SystemStateMachine::onStateEntry(SystemState state) {
    Serial.print("[FSM] Entering state: ");
    Serial.print(stateName());
    Serial.print(" (from ");
    
    switch (previousState) {
        case SystemState::STATE_IDLE:              Serial.print("IDLE"); break;
        case SystemState::STATE_FILLING:           Serial.print("FILLING"); break;
        case SystemState::STATE_COOLING:           Serial.print("COOLING"); break;
        case SystemState::STATE_DRAINING:          Serial.print("DRAINING"); break;
        case SystemState::STATE_AUTO:              Serial.print("AUTO"); break;
        case SystemState::STATE_AUTO_CIRCULATION:  Serial.print("AUTO_CIRCULATION"); break;
        case SystemState::STATE_BYPASS_MENU:       Serial.print("BYPASS_MENU"); break;
        case SystemState::STATE_ERROR:             Serial.print("ERROR"); break;
    }
    
    Serial.println(")");
    
    // State-specific entry actions
    switch (state) {
        case SystemState::STATE_FILLING:
            Serial.println("[ACTION] Start filling sequence");
            // digitalWrite(RELAY_INLET_VALVE, HIGH);
            break;
            
        case SystemState::STATE_COOLING:
            Serial.println("[ACTION] Start cooling sequence");
            // digitalWrite(RELAY_COMPRESSOR, HIGH);
            break;
            
        case SystemState::STATE_DRAINING:
            Serial.println("[ACTION] Start draining sequence");
            // digitalWrite(RELAY_DRAIN_VALVE, HIGH);
            break;
            
        case SystemState::STATE_AUTO:
            Serial.println("[ACTION] Entering AUTO mode (no circulation)");
            // Mulai logic AUTO
            // digitalWrite(RELAY_AUTO_SYSTEM, HIGH);
            break;
            
        case SystemState::STATE_AUTO_CIRCULATION:
            Serial.println("[ACTION] AUTO mode WITH circulation");
            // Mulai logic AUTO + aktifkan circulation
            // digitalWrite(RELAY_AUTO_SYSTEM, HIGH);
            // digitalWrite(RELAY_CIRCULATION_PUMP, HIGH);
            break;
            
        case SystemState::STATE_BYPASS_MENU:
            Serial.println("[ACTION] Entered bypass menu");
            break;
            
        case SystemState::STATE_IDLE:
            Serial.println("[ACTION] System idle");
            break;
            
        case SystemState::STATE_ERROR:
            Serial.println("[ACTION] ERROR state");
            break;
    }
}

void SystemStateMachine::onStateExit(SystemState state) {
    unsigned long duration = millis() - stateEntryTime;
    
    Serial.print("[FSM] Exiting state: ");
    
    switch (state) {
        case SystemState::STATE_IDLE:              Serial.print("IDLE"); break;
        case SystemState::STATE_FILLING:           Serial.print("FILLING"); break;
        case SystemState::STATE_COOLING:           Serial.print("COOLING"); break;
        case SystemState::STATE_DRAINING:          Serial.print("DRAINING"); break;
        case SystemState::STATE_AUTO:              Serial.print("AUTO"); break;
        case SystemState::STATE_AUTO_CIRCULATION:  Serial.print("AUTO_CIRCULATION"); break;
        case SystemState::STATE_BYPASS_MENU:       Serial.print("BYPASS_MENU"); break;
        case SystemState::STATE_ERROR:             Serial.print("ERROR"); break;
    }
    
    Serial.print(" (duration: ");
    Serial.print(duration);
    Serial.println(" ms)");
    
    // State-specific exit actions
    switch (state) {
        case SystemState::STATE_FILLING:
            Serial.println("[ACTION] Stop filling");
            // digitalWrite(RELAY_INLET_VALVE, LOW);
            break;
            
        case SystemState::STATE_COOLING:
            Serial.println("[ACTION] Stop cooling");
            // digitalWrite(RELAY_COMPRESSOR, LOW);
            break;
            
        case SystemState::STATE_DRAINING:
            Serial.println("[ACTION] Stop draining");
            // digitalWrite(RELAY_DRAIN_VALVE, LOW);
            break;
            
        case SystemState::STATE_AUTO:
            Serial.println("[ACTION] Exiting AUTO mode");
            // digitalWrite(RELAY_AUTO_SYSTEM, LOW);
            break;
            
        case SystemState::STATE_AUTO_CIRCULATION:
            Serial.println("[ACTION] Exiting AUTO with circulation");
            // digitalWrite(RELAY_AUTO_SYSTEM, LOW);
            // digitalWrite(RELAY_CIRCULATION_PUMP, LOW);
            break;
            
        default:
            break;
    }
}

// ===== MUTEX =====

void SystemStateMachine::lock() {
    if (mutex) {
        xSemaphoreTake(mutex, portMAX_DELAY);
    }
}

void SystemStateMachine::unlock() {
    if (mutex) {
        xSemaphoreGive(mutex);
    }
}
