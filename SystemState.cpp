// SystemState.cpp - FIXED VERSION
#include "SystemState.h"
#include "StateConditionHandler.h"
#include "NextionOutput.h"
#include "SensorManager.h"
#include "ActuatorControl.h"

// ===== STATE NAME TABLE =====
static const char* STATE_NAMES[] = {
    "IDLE",
    "FILLING",
    "COOLING",
    "DRAINING",
    "AUTO",
    "AUTO_CIRCULATION",
    "BYPASS_MENU",
    "ERROR"
};

// ===== CONSTRUCTOR / DESTRUCTOR =====

SystemStateMachine::SystemStateMachine(
    SensorManager* sensors,
    ActuatorControl* actuators,
    NextionOutput* display,
    StateConditionHandler* condHandler,
    SystemStorage* storage,
    NextionGateWay* gateway  // ← TAMBAHAN: parameter baru
) 
    : sensorManager(sensors)
    , actuatorControl(actuators)
    , nextionOutput(display)
    , conditionHandler(condHandler)
    , systemStorage(storage)
    , nextionGateway(gateway)  // ← TAMBAHAN: initialize pointer
    , currentState(SystemState::STATE_IDLE)
    , previousState(SystemState::STATE_IDLE)
    , stateEntryTime(0)
    , lastFillingForceOffTime(0)
    , lastDrainingForceOffTime(0)
{
    mutex = xSemaphoreCreateMutex();
    stateEntryTime = millis();
    
    Serial.println("[FSM] SystemStateMachine initialized");
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
    
    // ===== CONTINUOUS STATE CHECKS =====
    
    // Update cooling jika sedang di state cooling
    if (currentState == SystemState::STATE_COOLING) {
        conditionHandler->updateCooling();
    }
    
    // Check filling condition jika sedang di state filling
    if (currentState == SystemState::STATE_FILLING) {
        // ===== FIX: Check valve state sebelum proses condition =====
        // Jika valve sudah ditutup, jangan proses sensor
        bool valveOpen = actuatorControl->getValveInletState();
        
        if (!valveOpen) {
            // Valve sudah ditutup - skip condition check
            Serial.println("[FSM] Valve closed while in FILLING state - skipping condition check");
        } else {
            // Valve masih terbuka - process condition normally
            FillingStatus status = conditionHandler->checkFillingCondition();
            
            if (status == FillingStatus::FILLING_COMPLETE) {
                // Float sensor penuh - auto stop
                Serial.println("[FSM] Filling complete - Auto transition to IDLE");
                transitionTo(SystemState::STATE_IDLE);
            } 
            else if (status == FillingStatus::FILLING_ERROR) {
                // Flow error - auto transition to ERROR
                Serial.println("[FSM] Filling flow error - Auto transition to ERROR");
                transitionTo(SystemState::STATE_ERROR);
            }
        }
    }
    
    // Check draining condition jika sedang di state draining
    if (currentState == SystemState::STATE_DRAINING) {
        DrainingStatus status = conditionHandler->checkDrainingCondition();
        
        if (status == DrainingStatus::DRAINING_COMPLETE) {
            // Flow stopped - auto stop
            Serial.println("[FSM] Draining complete - Auto transition to IDLE");
            transitionTo(SystemState::STATE_IDLE);
        }
    }
    
    unlock();
}

SystemState SystemStateMachine::getState() const {
    return currentState;
}

String SystemStateMachine::stateName() const {
    return String(getStateName(currentState));
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
    // Priority 0: Bypass Menu (highest priority - dapat override semua)
    if (data.inBypassMenu) {
        return SystemState::STATE_BYPASS_MENU;
    }
    
    // Priority 1: Error recovery check
    if (currentState == SystemState::STATE_ERROR) {
        // Check apakah error berasal dari filling
        bool fromFillingError = (previousState == SystemState::STATE_FILLING);
        
        if (fromFillingError) {
            // Error recovery untuk FILLING
            if (conditionHandler->isErrorCleared()) {
                // Flow kembali normal - langsung ke FILLING
                Serial.println("[FSM] Flow restored - Auto recovery to FILLING");
                return SystemState::STATE_FILLING;
            }
            
            // User tekan FILLING OFF (bukan ON, bukan tombol lain)
            if (!data.fillingStatus) {
                Serial.println("[FSM] User pressed FILLING OFF - Exit ERROR to IDLE");
                return SystemState::STATE_IDLE;
            }
            
            // Masih error dan button masih ON - tetap di ERROR
            return SystemState::STATE_ERROR;
        }
        
        // Error dari state lain (future-proof)
        if (conditionHandler->isErrorCleared()) {
            return SystemState::STATE_IDLE;
        }
        return SystemState::STATE_ERROR;
    }
    
    // Priority 2: Auto Mode (dengan sub-state circulation)
    if (data.autoStatus) {
        if (data.circulationStatus) {
            return SystemState::STATE_AUTO_CIRCULATION;
        } else {
            return SystemState::STATE_AUTO;
        }
    }
    
    // Priority 3: Manual Operations
    
    // FILLING - Check debounce after force off
    if (data.fillingStatus) {
        // Ignore if within debounce period after force off
        unsigned long timeSinceForceOff = millis() - lastFillingForceOffTime;
        if (timeSinceForceOff < FORCE_OFF_DEBOUNCE_MS) {
            Serial.println("[FSM] Ignoring FILLING ON (debounce after auto-complete)");
            return SystemState::STATE_IDLE;
        }
        return SystemState::STATE_FILLING;
    }
    
    if (data.coolingStatus) {
        return SystemState::STATE_COOLING;
    }
    
    // DRAINING - Check debounce after force off
    if (data.drainingStatus) {
        // Ignore if within debounce period after force off
        unsigned long timeSinceForceOff = millis() - lastDrainingForceOffTime;
        if (timeSinceForceOff < FORCE_OFF_DEBOUNCE_MS) {
            Serial.println("[FSM] Ignoring DRAINING ON (debounce after auto-complete)");
            return SystemState::STATE_IDLE;
        }
        return SystemState::STATE_DRAINING;
    }
    
    // Default: Idle
    return SystemState::STATE_IDLE;
}

void SystemStateMachine::onStateEntry(SystemState state) {
    Serial.print("[FSM] Entering state: ");
    Serial.print(getStateName(state));
    Serial.print(" (from ");
    Serial.print(getStateName(previousState));
    Serial.println(")");
    
    // Call state-specific handlers
    switch (state) {
        case SystemState::STATE_IDLE:
            handleIdleEntry();
            break;
            
        case SystemState::STATE_FILLING:
            handleFillingEntry();
            break;
            
        case SystemState::STATE_COOLING:
            handleCoolingEntry();
            break;
            
        case SystemState::STATE_DRAINING:
            handleDrainingEntry();
            break;
            
        case SystemState::STATE_AUTO:
            handleAutoEntry();
            break;
            
        case SystemState::STATE_AUTO_CIRCULATION:
            handleAutoCirculationEntry();
            break;
            
        case SystemState::STATE_BYPASS_MENU:
            handleBypassMenuEntry();
            break;
            
        case SystemState::STATE_ERROR:
            handleErrorEntry();
            break;
    }
}

void SystemStateMachine::onStateExit(SystemState state) {
    unsigned long duration = millis() - stateEntryTime;
    
    Serial.print("[FSM] Exiting state: ");
    Serial.print(getStateName(state));
    Serial.print(" (duration: ");
    Serial.print(duration);
    Serial.println(" ms)");
    
    // Call state-specific exit handlers
    switch (state) {
        case SystemState::STATE_FILLING:
            handleFillingExit();
            break;
            
        case SystemState::STATE_COOLING:
            handleCoolingExit();
            break;
            
        case SystemState::STATE_DRAINING:
            handleDrainingExit();
            break;
            
        case SystemState::STATE_AUTO:
            handleAutoExit();
            break;
            
        case SystemState::STATE_AUTO_CIRCULATION:
            handleAutoCirculationExit();
            break;
            
        case SystemState::STATE_ERROR:
            // Clear error blink saat keluar dari ERROR state
            nextionOutput->setErrorBlink(false);
            Serial.println("[ACTION] Exiting ERROR - Clearing error display");
            break;
            
        default:
            break;
    }
}

// ===== STATE-SPECIFIC HANDLERS =====

void SystemStateMachine::handleIdleEntry() {
    Serial.println("[ACTION] System idle - All actuators off");
    actuatorControl->allOff();
    
    // Jika masuk IDLE dari FILLING, force OFF di Nextion
    if (previousState == SystemState::STATE_FILLING) {
        // ===== FIX: Auto-clear fillingStatus di NextionGateWay =====
        nextionGateway->clearFillingStatus();
        
        nextionOutput->forceFillingOff();
        lastFillingForceOffTime = millis();  // Record timestamp
        Serial.println("[ACTION] Exited FILLING - Cleared fillingStatus & Forced Nextion button OFF");
    }
    
    // Jika masuk IDLE dari DRAINING, force OFF di Nextion
    if (previousState == SystemState::STATE_DRAINING) {
        // ===== FIX: Auto-clear drainingStatus di NextionGateWay =====
        nextionGateway->clearDrainingStatus();
        
        nextionOutput->forceDrainingOff();
        lastDrainingForceOffTime = millis();  // Record timestamp
        Serial.println("[ACTION] Exited DRAINING - Cleared drainingStatus & Forced Nextion button OFF");
    }
}

void SystemStateMachine::handleFillingEntry() {
    Serial.println("[ACTION] Start filling sequence");
    
    // Start filling (valve open)
    conditionHandler->startFilling();
    
    // NOTE: Nextion already handles animation when button pressed
    // No need to send animation command here
}

void SystemStateMachine::handleFillingExit() {
    Serial.println("[ACTION] Stop filling");
    conditionHandler->stopFilling();
    
    // Note: Button dan animasi akan dimatikan di handleIdleEntry
    // jika transition ke IDLE
}

void SystemStateMachine::handleCoolingEntry() {
    Serial.println("[ACTION] Start cooling sequence");
    
    // Get cooling target dari SystemStorage
    uint8_t coolingTarget = systemStorage->getCoolingValue();
    
    Serial.print("[COOLING] Target from storage: ");
    Serial.print(coolingTarget);
    Serial.println("°C");
    
    conditionHandler->startCooling(coolingTarget);
}

void SystemStateMachine::handleCoolingExit() {
    Serial.println("[ACTION] Stop cooling");
    conditionHandler->stopCooling();
}

void SystemStateMachine::handleDrainingEntry() {
    Serial.println("[ACTION] Start draining sequence");
    conditionHandler->startDraining();
}

void SystemStateMachine::handleDrainingExit() {
    Serial.println("[ACTION] Stop draining");
    conditionHandler->stopDraining();
}

void SystemStateMachine::handleAutoEntry() {
    Serial.println("[ACTION] Entering AUTO mode (no circulation)");
    // TODO: Implementasi AUTO mode logic
}

void SystemStateMachine::handleAutoExit() {
    Serial.println("[ACTION] Exiting AUTO mode");
    // TODO: Cleanup AUTO mode
}

void SystemStateMachine::handleAutoCirculationEntry() {
    Serial.println("[ACTION] AUTO mode WITH circulation");
    // TODO: Implementasi AUTO + CIRCULATION logic
}

void SystemStateMachine::handleAutoCirculationExit() {
    Serial.println("[ACTION] Exiting AUTO with circulation");
    // TODO: Cleanup AUTO + CIRCULATION
}

void SystemStateMachine::handleBypassMenuEntry() {
    Serial.println("[ACTION] Entered bypass menu - System paused");
    // Nextion sudah auto-turn off semua button saat masuk bypass
    // Tidak perlu action tambahan
}

void SystemStateMachine::handleErrorEntry() {
    Serial.println("[ACTION] ERROR state - Emergency stop");
    
    // Emergency stop semua actuator
    actuatorControl->allOff();
    
    // Pastikan error blink ditampilkan
    nextionOutput->setErrorBlink(true);
    
    Serial.println("[ERROR] All actuators stopped, error animation displayed");
}

// ===== HELPER FUNCTIONS =====

const char* SystemStateMachine::getStateName(SystemState state) const {
    uint8_t index = static_cast<uint8_t>(state);
    if (index < 8) {
        return STATE_NAMES[index];
    }
    return "UNKNOWN";
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