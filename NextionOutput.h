// NextionOutput.h
#ifndef NEXTION_OUTPUT_H
#define NEXTION_OUTPUT_H

#include <Arduino.h>

// ===== NEXTION OUTPUT CLASS =====
// Handle pengiriman data ke Nextion display
class NextionOutput {
public:
    NextionOutput();
    ~NextionOutput();
    
    void begin();
    
    // ===== COOLING DISPLAY =====
    void updateCoolingDuration(uint16_t minutes);
    
    // ===== ERROR ANIMATION =====
    void setErrorBlink(bool enable);
    
    // ===== FORCE STATE OFF (Mirror Nextion Button Behavior) =====
    void forceFillingOff();
    void forceDrainingOff();
    
    // ===== FUTURE: Tambahan untuk animasi lain =====
    // void setDrainingAnimation(bool enable);
    
private:
    // Helper untuk kirim command ke Nextion
    void sendCommand(const String& cmd);
    void sendFF();  // Nextion protocol terminator
};

#endif