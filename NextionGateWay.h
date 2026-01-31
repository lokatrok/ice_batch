#ifndef NEXTION_GATEWAY_H
#define NEXTION_GATEWAY_H

#include <Arduino.h>

// ===== SERIAL CONFIG =====
#define NEXTION Serial2
#define RXD2 16
#define TXD2 17

// ===== BUFFER CONFIG =====
#define BUFFER_SIZE 100
#define RECEIVE_TIMEOUT 50

// ===== DATA MODEL =====
struct NextionData {
    uint8_t coolingValue = 0;
    
    bool fillingStatus = false;
    bool coolingStatus = false;
    bool drainingStatus = false;
    bool autoStatus = false;
    bool circulationStatus = false;

    bool bypassInlet = false;
    bool bypassDrain = false;
    bool bypassCompre = false;
    bool bypassPumpUV = false;
    bool bypassOzone = false;
    bool bypassHydro = false;

    String setTime;
    String setAuto;

    uint16_t countValue = 0;
    uint16_t daysValue = 0;
    uint8_t autoTemp = 0;

    bool inBypassMenu = false;
};

class NextionGateWay {
public:
    NextionGateWay();
    ~NextionGateWay();

    void begin();
    void readTask();
    void send(const String &cmd);

    NextionData getData();

private:
    uint8_t buffer[BUFFER_SIZE];
    uint16_t bufferIndex = 0;
    unsigned long lastReceiveTime = 0;

    NextionData data;
    SemaphoreHandle_t mutex;

    void processBuffer();
    String extractMessage();
    void handleMessage(const String &msg);

    void lock();
    void unlock();
};

#endif
