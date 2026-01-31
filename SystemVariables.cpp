// SystemVariables.cpp
#include "SystemVariables.h"

// ===== CONSTRUCTOR / DESTRUCTOR =====

SystemStorage::SystemStorage() {
    mutex = xSemaphoreCreateMutex();
    initDefaults();
}

SystemStorage::~SystemStorage() {
    if (mutex) vSemaphoreDelete(mutex);
}

// ===== PUBLIC API =====

void SystemStorage::begin() {
    Serial.println("[STORAGE] Initialized with default values");
    printVariables();
}

void SystemStorage::updateFromNextion(const NextionData &data) {
    lock();
    
    // Update RTC Time (format HH:MM)
    if (data.setTime != variables.setTime && data.setTime.length() > 0) {
        if (isValidTimeFormat(data.setTime)) {
            variables.setTime = data.setTime;
            variables.lastUpdateTime = millis();
            Serial.print("[STORAGE] setTime → ");
            Serial.println(variables.setTime);
        } else {
            Serial.print("[STORAGE] Invalid setTime format: ");
            Serial.println(data.setTime);
        }
    }
    
    // Update Auto Config (format HH:MM)
    if (data.setAuto != variables.setAuto && data.setAuto.length() > 0) {
        if (isValidTimeFormat(data.setAuto)) {
            variables.setAuto = data.setAuto;
            variables.lastUpdateTime = millis();
            Serial.print("[STORAGE] setAuto → ");
            Serial.println(variables.setAuto);
        } else {
            Serial.print("[STORAGE] Invalid setAuto format: ");
            Serial.println(data.setAuto);
        }
    }
    
    // Update Days Value
    if (data.daysValue != variables.daysValue) {
        variables.daysValue = data.daysValue;
        variables.lastUpdateTime = millis();
        Serial.print("[STORAGE] daysValue → ");
        Serial.println(variables.daysValue);
    }
    
    // Update Auto Temperature
    if (data.autoTemp != variables.autoTemp) {
        variables.autoTemp = data.autoTemp;
        variables.lastUpdateTime = millis();
        Serial.print("[STORAGE] autoTemp → ");
        Serial.println(variables.autoTemp);
    }
    
    // Update Count Timer
    if (data.countValue != variables.countValue) {
        variables.countValue = data.countValue;
        variables.lastUpdateTime = millis();
        Serial.print("[STORAGE] countValue → ");
        Serial.println(variables.countValue);
    }
    
    // Update Cooling Value
    if (data.coolingValue != variables.coolingValue) {
        variables.coolingValue = data.coolingValue;
        variables.lastUpdateTime = millis();
        Serial.print("[STORAGE] coolingValue → ");
        Serial.println(variables.coolingValue);
    }
    
    unlock();
}

SystemVariables SystemStorage::getVariables() {
    SystemVariables copy;
    lock();
    copy = variables;
    unlock();
    return copy;
}

// ===== INDIVIDUAL GETTERS =====

String SystemStorage::getSetTime() {
    lock();
    String value = variables.setTime;
    unlock();
    return value;
}

String SystemStorage::getSetAuto() {
    lock();
    String value = variables.setAuto;
    unlock();
    return value;
}

uint16_t SystemStorage::getDaysValue() {
    lock();
    uint16_t value = variables.daysValue;
    unlock();
    return value;
}

uint8_t SystemStorage::getAutoTemp() {
    lock();
    uint8_t value = variables.autoTemp;
    unlock();
    return value;
}

uint16_t SystemStorage::getCountValue() {
    lock();
    uint16_t value = variables.countValue;
    unlock();
    return value;
}

uint8_t SystemStorage::getCoolingValue() {
    lock();
    uint8_t value = variables.coolingValue;
    unlock();
    return value;
}

// ===== VALIDATION =====

bool SystemStorage::isValidTimeFormat(const String &time) {
    // Format harus HH:MM (5 karakter)
    if (time.length() != 5) {
        return false;
    }
    
    // Cek posisi ':' di index 2
    if (time.charAt(2) != ':') {
        return false;
    }
    
    // Cek apakah HH adalah digit
    if (!isDigit(time.charAt(0)) || !isDigit(time.charAt(1))) {
        return false;
    }
    
    // Cek apakah MM adalah digit
    if (!isDigit(time.charAt(3)) || !isDigit(time.charAt(4))) {
        return false;
    }
    
    // Extract HH dan MM
    int hours = time.substring(0, 2).toInt();
    int minutes = time.substring(3, 5).toInt();
    
    // Validasi range
    if (hours < 0 || hours > 23) {
        return false;
    }
    
    if (minutes < 0 || minutes > 59) {
        return false;
    }
    
    return true;
}

// ===== DEBUG UTILITIES =====

void SystemStorage::printVariables() {
    lock();
    
    Serial.println("╔════════════════════════════════════════╗");
    Serial.println("║       SYSTEM VARIABLES STORAGE         ║");
    Serial.println("╠════════════════════════════════════════╣");
    
    Serial.print("║ setTime      : "); 
    if (variables.setTime.length() > 0) {
        Serial.print(variables.setTime);
        for(int i = variables.setTime.length(); i < 23; i++) Serial.print(" ");
    } else {
        Serial.print("--:--");
        for(int i = 5; i < 23; i++) Serial.print(" ");
    }
    Serial.println("║");
    
    Serial.print("║ setAuto      : "); 
    if (variables.setAuto.length() > 0) {
        Serial.print(variables.setAuto);
        for(int i = variables.setAuto.length(); i < 23; i++) Serial.print(" ");
    } else {
        Serial.print("--:--");
        for(int i = 5; i < 23; i++) Serial.print(" ");
    }
    Serial.println("║");
    
    Serial.print("║ daysValue    : "); 
    Serial.print(variables.daysValue);
    Serial.print(" days");
    int len = String(variables.daysValue).length() + 5;
    for(int i = len; i < 23; i++) Serial.print(" ");
    Serial.println("║");
    
    Serial.print("║ autoTemp     : "); 
    Serial.print(variables.autoTemp);
    Serial.print("°C");
    len = String(variables.autoTemp).length() + 2;
    for(int i = len; i < 23; i++) Serial.print(" ");
    Serial.println("║");
    
    Serial.print("║ countValue   : "); 
    Serial.print(variables.countValue);
    Serial.print(" sec");
    len = String(variables.countValue).length() + 4;
    for(int i = len; i < 23; i++) Serial.print(" ");
    Serial.println("║");
    
    Serial.print("║ coolingValue : "); 
    Serial.print(variables.coolingValue);
    Serial.print("°C");
    len = String(variables.coolingValue).length() + 2;
    for(int i = len; i < 23; i++) Serial.print(" ");
    Serial.println("║");
    
    Serial.println("╚════════════════════════════════════════╝");
    
    unlock();
}

// ===== PRIVATE METHODS =====

void SystemStorage::initDefaults() {
    variables.setTime = "";
    variables.setAuto = "";
    variables.daysValue = 0;
    variables.autoTemp = 0;
    variables.countValue = 0;
    variables.coolingValue = 0;
    variables.lastUpdateTime = 0;
}

// ===== MUTEX =====

void SystemStorage::lock() {
    if (mutex) {
        xSemaphoreTake(mutex, portMAX_DELAY);
    }
}

void SystemStorage::unlock() {
    if (mutex) {
        xSemaphoreGive(mutex);
    }
}