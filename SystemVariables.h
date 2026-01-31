// SystemVariables.h
#ifndef SYSTEM_VARIABLES_H
#define SYSTEM_VARIABLES_H

#include <Arduino.h>
#include "NextionGateWay.h"

// ===== STORAGE STRUCTURE =====
struct SystemVariables {
    // RTC Settings
    String setTime;              // Format dari settime=HH:MM
    
    // Auto Cycle Settings
    String setAuto;              // Konfigurasi auto cycle dari setAuto=HH:MM
    uint16_t daysValue;          // Interval cycle dalam hari dari days:X
    uint8_t autoTemp;            // Target temperature auto mode dari autotempX
    
    // Timer Settings
    uint16_t countValue;         // Countdown timer dalam detik dari countX
    
    // Manual Cooling Settings
    uint8_t coolingValue;        // Target temperature manual cooling dari coolingX
    
    // Metadata
    unsigned long lastUpdateTime;
};

// ===== STORAGE MANAGER CLASS =====
class SystemStorage {
public:
    SystemStorage();
    ~SystemStorage();
    
    void begin();
    
    // Update dari Nextion (hanya ambil value, bukan status button)
    void updateFromNextion(const NextionData &data);
    
    // Getter
    SystemVariables getVariables();
    
    // Individual getters
    String getSetTime();
    String getSetAuto();
    uint16_t getDaysValue();
    uint8_t getAutoTemp();
    uint16_t getCountValue();
    uint8_t getCoolingValue();
    
    // Validation helpers
    bool isValidTimeFormat(const String &time);
    
    // Debug utilities
    void printVariables();

private:
    SystemVariables variables;
    SemaphoreHandle_t mutex;
    
    void initDefaults();
    
    void lock();
    void unlock();
};

#endif