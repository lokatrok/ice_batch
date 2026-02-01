// StateConditionHandler.h
#ifndef STATE_CONDITION_HANDLER_H
#define STATE_CONDITION_HANDLER_H

#include <Arduino.h>
#include "SensorManager.h"
#include "ActuatorControl.h"
#include "NextionOutput.h"

// ===== CONDITION STATUS ENUMS =====

enum class FillingStatus : uint8_t {
    FILLING_CONTINUE = 0,    // Continue filling
    FILLING_COMPLETE,        // Float sensor triggered (penuh)
    FILLING_ERROR            // Flow switch error (tidak ada aliran)
};

enum class DrainingStatus : uint8_t {
    DRAINING_CONTINUE = 0,   // Continue draining
    DRAINING_COMPLETE        // Flow stopped (selesai)
};

// ===== STATE CONDITION HANDLER CLASS =====
class StateConditionHandler {
public:
    StateConditionHandler(
        SensorManager* sensors,
        ActuatorControl* actuators,
        NextionOutput* display
    );
    ~StateConditionHandler();
    
    // ===== FILLING =====
    FillingStatus checkFillingCondition();
    void startFilling();
    void stopFilling();
    
    // ===== COOLING =====
    void startCooling(uint8_t targetTemp);
    void updateCooling();  // Dipanggil berkala dari FSM update loop
    void stopCooling();
    
    // ===== DRAINING =====
    DrainingStatus checkDrainingCondition();
    void startDraining();
    void stopDraining();
    
    // ===== ERROR RECOVERY =====
    bool isErrorCleared();
    
private:
    // Dependencies
    SensorManager* sensor;
    ActuatorControl* actuator;
    NextionOutput* nextion;
    
    // ===== COOLING STATE =====
    uint8_t coolingTarget;              // Target temperature (°C)
    unsigned long coolingStartTime;     // Waktu mulai cooling (ms)
    unsigned long compressorOffTime;    // Waktu saat compressor OFF (ms)
    bool compressorActive;              // Status compressor
    bool inWaitPeriod;                  // Sedang dalam periode tunggu 10 menit
    
    // ===== FILLING STATE =====
    bool lastFlowState;                 // State flow switch sebelumnya
    bool lastFloatState;                // State float sensor sebelumnya
    unsigned long floatTrueStartTime;   // Waktu pertama float jadi TRUE
    static const unsigned long FLOAT_DEBOUNCE_MS = 2000;  // 2 detik debounce
    
    // ===== DRAINING STATE =====
    // ✅ FIX: Tambah state tracking untuk draining
    float drainingFlowThreshold;        // Target flow rate threshold (L/min)
    unsigned long drainingLowFlowStartTime;  // Waktu flow mulai <= threshold
    bool drainingLowFlowDetected;       // Apakah flow sudah <= threshold
    static constexpr unsigned long DRAINING_LOW_FLOW_TIMEOUT_MS = 5000;  // 5 detik
    static constexpr float DRAINING_FLOW_THRESHOLD = 0.1f;  // 0.1 L/min
    
    // ===== HELPER METHODS =====
    void updateCoolingDisplay();
    uint16_t getElapsedMinutes(unsigned long startTime);
};

#endif