// SystemState.h
#ifndef SYSTEM_STATE_H
#define SYSTEM_STATE_H

#include <Arduino.h>
#include "NextionGateWay.h"
#include "SystemVariables.h"  // ← untuk SystemStorage

// Forward declarations
class StateConditionHandler;
class NextionOutput;
class SensorManager;
class ActuatorControl;

// ===== STATE DEFINITIONS =====
enum class SystemState : uint8_t {
    STATE_IDLE = 0,
    STATE_FILLING,
    STATE_COOLING,
    STATE_DRAINING,
    STATE_AUTO,
    STATE_AUTO_CIRCULATION,
    STATE_BYPASS_MENU,
    STATE_ERROR
};

// ===== STATE MACHINE CLASS =====
class SystemStateMachine {
public:
    // Constructor with dependencies
    SystemStateMachine(
        SensorManager* sensors,
        ActuatorControl* actuators,
        NextionOutput* display,
        StateConditionHandler* condHandler,
        SystemStorage* storage,
        NextionGateWay* gateway  // ← TAMBAHAN: pointer ke NextionGateWay
    );
    ~SystemStateMachine();

    void update(const NextionData &data);
    
    SystemState getState() const;
    String stateName() const;
    
    bool isInAutoMode() const;
    
    void reset();

private:
    // Dependencies (injected via constructor)
    StateConditionHandler* conditionHandler;
    NextionOutput* nextionOutput;
    SensorManager* sensorManager;
    ActuatorControl* actuatorControl;
    SystemStorage* systemStorage;
    NextionGateWay* nextionGateway;  // ← TAMBAHAN: untuk clear status
    
    // State tracking
    SystemState currentState;
    SystemState previousState;
    
    unsigned long stateEntryTime;
    SemaphoreHandle_t mutex;
    
    // Auto-complete tracking (to prevent re-entry)
    unsigned long lastFillingForceOffTime;
    unsigned long lastDrainingForceOffTime;
    static const unsigned long FORCE_OFF_DEBOUNCE_MS = 500;  // 500ms debounce

    void transitionTo(SystemState newState);
    SystemState determineState(const NextionData &data);
    
    void onStateEntry(SystemState state);
    void onStateExit(SystemState state);
    
    // State-specific action handlers
    void handleIdleEntry();
    void handleFillingEntry();
    void handleFillingExit();
    void handleCoolingEntry();
    void handleCoolingExit();
    void handleDrainingEntry();
    void handleDrainingExit();
    void handleAutoEntry();
    void handleAutoExit();
    void handleAutoCirculationEntry();
    void handleAutoCirculationExit();
    void handleBypassMenuEntry();
    void handleErrorEntry();
    
    void lock();
    void unlock();
    
    // Helper untuk convert state ke string
    const char* getStateName(SystemState state) const;
};

#endif