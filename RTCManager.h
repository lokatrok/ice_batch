#ifndef RTC_MANAGER_H
#define RTC_MANAGER_H

#include <Arduino.h>
#include <RTClib.h>  // Library Adafruit RTClib untuk DS3231/DS1307

class NextionGateWay;

// ===== RTC MANAGER CLASS =====
class RTCManager {
public:
    RTCManager();
    ~RTCManager();
    
    void begin();
    void setNextionGateway(NextionGateWay* nxt);

    // ===== SET TIME =====
    bool setTime(const String &timeStr);  // Format "HH:MM"
    bool setTime(uint8_t hour, uint8_t minute);
    
    // ===== GET TIME =====
    String getTimeString();     // Return "HH:MM"
    String getTimeStringFull(); // Return "HH:MM:SS"
    uint8_t getHour();
    uint8_t getMinute();
    uint8_t getSecond();
    
    // ===== STATUS =====
    bool isRTCValid();
    bool isTimeSet();
    
    // ===== NEXTION SYNC =====
    void sendToNextion(const String &componentName = "tClock");
    
    // ===== DEBUG =====
    void printTime();

private:
    RTC_DS3231 rtc;  // Ganti dengan RTC_DS1307 jika pakai DS1307
    SemaphoreHandle_t mutex;
    
    bool rtcInitialized;
    bool timeWasSet;
    
    DateTime lastDateTime;

    NextionGateWay* nextionPtr; 
    
    bool parseTimeString(const String &timeStr, uint8_t &hour, uint8_t &minute);
    
    void lock();
    void unlock();
};

#endif