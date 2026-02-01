// NextionOutput.cpp
#include "NextionOutput.h"

// ===== CONSTRUCTOR / DESTRUCTOR =====

NextionOutput::NextionOutput() {
    // Constructor kosong, inisialisasi di begin()
}

NextionOutput::~NextionOutput() {
    // Cleanup jika diperlukan
}

// ===== PUBLIC METHODS =====

void NextionOutput::begin() {
    Serial.println("[NextionOutput] Initialized");
}

void NextionOutput::updateCoolingDuration(uint16_t minutes) {
    String cmd = "nCoolDurMan.val=" + String(minutes);
    sendCommand(cmd);
    
    Serial.print("[NextionOutput] Cooling duration updated: ");
    Serial.print(minutes);
    Serial.println(" min");
}

void NextionOutput::setErrorBlink(bool enable) {
    if (enable) {
        // Enable error blinking animation
        sendCommand("blinkingEF.val=1");
        sendCommand("tBlinkEF.en=1");
        Serial.println("[NextionOutput] Error blink ENABLED");
    } else {
        // Disable error blinking animation
        sendCommand("blinkingEF.val=0");
        sendCommand("tBlinkEF.en=0");
        Serial.println("[NextionOutput] Error blink DISABLED");
    }
}

void NextionOutput::forceFillingOff() {
    // Update visual components first
    sendCommand("blinkingFM.val=0");
    sendCommand("tBlinkFM.en=0");
    sendCommand("pFillingMan.pic=6");
    
    // Change activeProcess to 0
    // Note: This will trigger Nextion button logic, but we handle it in FSM
    sendCommand("activeProcess.val=0");
    
    Serial.println("[NextionOutput] Force FILLING OFF (auto-complete)");
}

void NextionOutput::forceDrainingOff() {
    // Update visual components first
    sendCommand("blinkingDM.val=0");
    sendCommand("tBlinkDM.en=0");
    sendCommand("pDrainingMan.pic=10");
    
    // Change activeProcess to 0
    // Note: This will trigger Nextion button logic, but we handle it in FSM
    sendCommand("activeProcess.val=0");
    
    Serial.println("[NextionOutput] Force DRAINING OFF (auto-complete)");
}

// ===== PRIVATE HELPERS =====

void NextionOutput::sendCommand(const String& cmd) {
    Serial2.print(cmd);
    sendFF();
}

void NextionOutput::sendFF() {
    // Nextion protocol: setiap command harus diakhiri 3x 0xFF
    Serial2.write(0xFF);
    Serial2.write(0xFF);
    Serial2.write(0xFF);
}