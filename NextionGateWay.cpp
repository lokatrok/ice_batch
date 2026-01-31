#include "NextionGateWay.h"

// ===== CONSTRUCTOR / DESTRUCTOR =====

NextionGateWay::NextionGateWay() {
    mutex = xSemaphoreCreateMutex();
}

NextionGateWay::~NextionGateWay() {
    if (mutex) vSemaphoreDelete(mutex);
}

// ===== PUBLIC API =====

void NextionGateWay::begin() {
    NEXTION.begin(9600, SERIAL_8N1, RXD2, TXD2);
}

void NextionGateWay::readTask() {
    while (NEXTION.available()) {
        buffer[bufferIndex++] = NEXTION.read();
        lastReceiveTime = millis();

        if (bufferIndex >= BUFFER_SIZE) {
            processBuffer();
            bufferIndex = 0;
        }
    }

    if (bufferIndex > 0 && millis() - lastReceiveTime > RECEIVE_TIMEOUT) {
        processBuffer();
        bufferIndex = 0;
    }
}

void NextionGateWay::send(const String &cmd) {
    NEXTION.print(cmd);
    NEXTION.write(0xFF);
    NEXTION.write(0xFF);
    NEXTION.write(0xFF);
}

NextionData NextionGateWay::getData() {
    NextionData copy;
    lock();
    copy = data;
    unlock();
    return copy;
}

// ===== INTERNAL =====

void NextionGateWay::processBuffer() {
    // ===== CEK COOLING_ON + RAW BYTE =====
    static const char COOLING_KEYWORD[] = "COOLING_ON";
    static const uint8_t COOLING_KEYWORD_LEN = 10;
    
    bool foundCoolingValue = false;
    
    for (uint16_t i = 0; i + COOLING_KEYWORD_LEN < bufferIndex; i++) {
        if (memcmp(&buffer[i], COOLING_KEYWORD, COOLING_KEYWORD_LEN) == 0) {
            uint8_t value = buffer[i + COOLING_KEYWORD_LEN];
            if (value >= 1 && value <= 100) {
                lock();
                data.coolingValue = value;
                unlock();
                
                Serial.print("[RX] COOLING_ON with value = ");
                Serial.println(value);
                
                foundCoolingValue = true;
            }
            break;  // ← Ganti return jadi break
        }
    }
    
    // ===== EXTRACT MESSAGE STRING =====
    String msg = extractMessage();
    if (msg.length()) {
        handleMessage(msg);  // ← Ini akan process "COOLING_ON" untuk state change
    }
}

String NextionGateWay::extractMessage() {
    String msg;
    for (uint16_t i = 0; i < bufferIndex; i++) {
        uint8_t c = buffer[i];
        if (c == 0xFF) break;
        if (c >= 32 && c <= 126) msg += char(c);
    }
    msg.trim();
    return msg;
}

void NextionGateWay::handleMessage(const String &msg) {
    lock();

    Serial.print("[RX] '");
    Serial.print(msg);
    Serial.println("'");

    // ===== SIMPLE TOGGLES =====
    if (msg == "COOLING_ON")             data.coolingStatus = true;
    else if (msg == "COOLING_OFF")       data.coolingStatus = false;

    else if (msg == "FILLING_ON")        data.fillingStatus = true;
    else if (msg == "FILLING_OFF")       data.fillingStatus = false;

    else if (msg == "DRAINING_ON")       data.drainingStatus = true;
    else if (msg == "DRAINING_OFF")      data.drainingStatus = false;

    else if (msg == "AUTO_ON")           data.autoStatus = true;
    else if (msg == "AUTO_OFF")          data.autoStatus = false;

    else if (msg == "CIRCULATION_ON")    data.circulationStatus = true;
    else if (msg == "CIRCULATION_OFF")   data.circulationStatus = false;

    // ===== COMBINED =====
    else if (msg == "CIRCULATION_OFFAUTO_OFF") {
        data.circulationStatus = false;
        data.autoStatus = false;
    }

    // ===== BYPASS =====
    else if (msg == "bypassInlet")   data.bypassInlet   = !data.bypassInlet;
    else if (msg == "bypassDrain")   data.bypassDrain   = !data.bypassDrain;
    else if (msg == "bypassCompre")  data.bypassCompre  = !data.bypassCompre;
    else if (msg == "bypassPumpUV")  data.bypassPumpUV  = !data.bypassPumpUV;
    else if (msg == "bypassOzone")   data.bypassOzone   = !data.bypassOzone;
    else if (msg == "bypassHydro")   data.bypassHydro   = !data.bypassHydro;

    // ===== MENU =====
    else if (msg == "GOTO_BYPASS_MENU")  data.inBypassMenu = true;
    else if (msg == "EXIT_BYPASS_MENU")  data.inBypassMenu = false;

    // ===== VALUES =====
    else if (msg.startsWith("settime="))   data.setTime    = msg.substring(8);
    else if (msg.startsWith("setAuto="))   data.setAuto    = msg.substring(8);
    else if (msg.startsWith("count"))      data.countValue = msg.substring(5).toInt();
    else if (msg.startsWith("days:"))      data.daysValue  = msg.substring(5).toInt();
    else if (msg.startsWith("autotemp"))   data.autoTemp   = msg.substring(8).toInt();

    unlock();
}

// ===== MUTEX =====

void NextionGateWay::lock() {
    xSemaphoreTake(mutex, portMAX_DELAY);
}

void NextionGateWay::unlock() {
    xSemaphoreGive(mutex);
}
