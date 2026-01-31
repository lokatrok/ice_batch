// SystemState.h
#ifndef SYSTEM_STATE_H
#define SYSTEM_STATE_H

#include <Arduino.h>
#include "NextionGateWay.h"

// ===== STATE DEFINITIONS =====
enum class SystemState : uint8_t {
    STATE_IDLE = 0,
    STATE_FILLING,
    STATE_COOLING,
    STATE_DRAINING,
    STATE_AUTO,                    // Auto mode tanpa circulation
    STATE_AUTO_CIRCULATION,        // Auto mode dengan circulation
    STATE_BYPASS_MENU,
    STATE_ERROR
};

// ===== STATE MACHINE CLASS =====
class SystemStateMachine {
public:
    SystemStateMachine();
    ~SystemStateMachine();

    void update(const NextionData &data);
    
    SystemState getState() const;
    String stateName() const;
    
    bool isInAutoMode() const;  // Helper untuk cek apakah di AUTO atau AUTO_CIRCULATION
    
    void reset();

private:
    SystemState currentState;
    SystemState previousState;
    
    unsigned long stateEntryTime;
    SemaphoreHandle_t mutex;

    void transitionTo(SystemState newState);
    SystemState determineState(const NextionData &data);
    
    void onStateEntry(SystemState state);
    void onStateExit(SystemState state);
    
    void lock();
    void unlock();
};

#endif