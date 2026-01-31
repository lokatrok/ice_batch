#ifndef ACTUATOR_CONTROL_H
#define ACTUATOR_CONTROL_H

#include <Arduino.h>

// ===== ACTUATOR CONTROLLER CLASS =====
class ActuatorControl {
public:
    ActuatorControl();
    ~ActuatorControl();
    
    void begin();
    
    // ===== VALVE CONTROL =====
    void setValveDrain(bool state);
    void setValveInlet(bool state);
    
    // ===== COOLING CONTROL =====
    void setCompressor(bool state);
    
    // ===== PUMP & TREATMENT =====
    void setPumpUV(bool state);
    void setOzone(bool state);
    
    // ===== BUZZER =====
    void setBuzzer(bool state);
    void beep(uint16_t durationMs);  // Beep dengan durasi
    
    // ===== EMERGENCY =====
    void allOff();  // Matikan semua actuator
    
    // ===== GETTERS =====
    bool getValveDrainState() const;
    bool getValveInletState() const;
    bool getCompressorState() const;
    bool getPumpUVState() const;
    bool getOzoneState() const;
    bool getBuzzerState() const;
    
    // ===== DEBUG =====
    void printStatus();

private:
    SemaphoreHandle_t mutex;
    
    // State tracking
    bool valveDrainState;
    bool valveInletState;
    bool compressorState;
    bool pumpUVState;
    bool ozoneState;
    bool buzzerState;
    
    void lock();
    void unlock();
};

#endif