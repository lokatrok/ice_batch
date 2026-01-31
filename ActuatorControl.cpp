#include "ActuatorControl.h"

// Pin definitions (sesuaikan dengan HardwareConfig.h Anda)
#define PIN_VALVE_DRAIN   33
#define PIN_VALVE_INLET   26
#define PIN_COMPRESSOR    25
#define PIN_PUMP_UV       27
#define PIN_OZONE         12
#define PIN_BUZZER        18

// ===== CONSTRUCTOR / DESTRUCTOR =====

ActuatorControl::ActuatorControl()
    : valveDrainState(false)
    , valveInletState(false)
    , compressorState(false)
    , pumpUVState(false)
    , ozoneState(false)
    , buzzerState(false)
{
    mutex = xSemaphoreCreateMutex();
}

ActuatorControl::~ActuatorControl() {
    if (mutex) vSemaphoreDelete(mutex);
}

// ===== PUBLIC API =====

void ActuatorControl::begin() {
    // Setup pins sebagai OUTPUT
    pinMode(PIN_VALVE_DRAIN, OUTPUT);
    pinMode(PIN_VALVE_INLET, OUTPUT);
    pinMode(PIN_COMPRESSOR, OUTPUT);
    pinMode(PIN_PUMP_UV, OUTPUT);
    pinMode(PIN_OZONE, OUTPUT);
    pinMode(PIN_BUZZER, OUTPUT);
    
    // Initial state: ALL OFF
    allOff();
    
    Serial.println("[ACTUATOR] Initialized - All actuators OFF");
}

// ===== VALVE CONTROL =====

void ActuatorControl::setValveDrain(bool state) {
    lock();
    valveDrainState = state;
    digitalWrite(PIN_VALVE_DRAIN, state ? HIGH : LOW);
    unlock();
    
    Serial.print("[ACTUATOR] Valve Drain: ");
    Serial.println(state ? "OPEN" : "CLOSED");
}

void ActuatorControl::setValveInlet(bool state) {
    lock();
    valveInletState = state;
    digitalWrite(PIN_VALVE_INLET, state ? HIGH : LOW);
    unlock();
    
    Serial.print("[ACTUATOR] Valve Inlet: ");
    Serial.println(state ? "OPEN" : "CLOSED");
}

// ===== COOLING CONTROL =====

void ActuatorControl::setCompressor(bool state) {
    lock();
    compressorState = state;
    digitalWrite(PIN_COMPRESSOR, state ? HIGH : LOW);
    unlock();
    
    Serial.print("[ACTUATOR] Compressor: ");
    Serial.println(state ? "ON" : "OFF");
}

// ===== PUMP & TREATMENT =====

void ActuatorControl::setPumpUV(bool state) {
    lock();
    pumpUVState = state;
    digitalWrite(PIN_PUMP_UV, state ? HIGH : LOW);
    unlock();
    
    Serial.print("[ACTUATOR] Pump UV: ");
    Serial.println(state ? "ON" : "OFF");
}

void ActuatorControl::setOzone(bool state) {
    lock();
    ozoneState = state;
    digitalWrite(PIN_OZONE, state ? HIGH : LOW);
    unlock();
    
    Serial.print("[ACTUATOR] Ozone: ");
    Serial.println(state ? "ON" : "OFF");
}

// ===== BUZZER =====

void ActuatorControl::setBuzzer(bool state) {
    lock();
    buzzerState = state;
    digitalWrite(PIN_BUZZER, state ? HIGH : LOW);
    unlock();
    
    Serial.print("[ACTUATOR] Buzzer: ");
    Serial.println(state ? "ON" : "OFF");
}

void ActuatorControl::beep(uint16_t durationMs) {
    setBuzzer(true);
    delay(durationMs);
    setBuzzer(false);
}

// ===== EMERGENCY =====

void ActuatorControl::allOff() {
    lock();
    
    valveDrainState = false;
    valveInletState = false;
    compressorState = false;
    pumpUVState = false;
    ozoneState = false;
    buzzerState = false;
    
    digitalWrite(PIN_VALVE_DRAIN, LOW);
    digitalWrite(PIN_VALVE_INLET, LOW);
    digitalWrite(PIN_COMPRESSOR, LOW);
    digitalWrite(PIN_PUMP_UV, LOW);
    digitalWrite(PIN_OZONE, LOW);
    digitalWrite(PIN_BUZZER, LOW);
    
    unlock();
    
    Serial.println("[ACTUATOR] ALL OFF");
}

// ===== GETTERS =====

bool ActuatorControl::getValveDrainState() const {
    return valveDrainState;
}

bool ActuatorControl::getValveInletState() const {
    return valveInletState;
}

bool ActuatorControl::getCompressorState() const {
    return compressorState;
}

bool ActuatorControl::getPumpUVState() const {
    return pumpUVState;
}

bool ActuatorControl::getOzoneState() const {
    return ozoneState;
}

bool ActuatorControl::getBuzzerState() const {
    return buzzerState;
}

// ===== DEBUG =====

void ActuatorControl::printStatus() {
    lock();
    
    Serial.println("╔════════════════════════════════════╗");
    Serial.println("║      ACTUATOR STATUS               ║");
    Serial.println("╠════════════════════════════════════╣");
    Serial.print("║ Valve Drain  : ");
    Serial.println(valveDrainState ? "OPEN      ║" : "CLOSED    ║");
    Serial.print("║ Valve Inlet  : ");
    Serial.println(valveInletState ? "OPEN      ║" : "CLOSED    ║");
    Serial.print("║ Compressor   : ");
    Serial.println(compressorState ? "ON        ║" : "OFF       ║");
    Serial.print("║ Pump UV      : ");
    Serial.println(pumpUVState ? "ON        ║" : "OFF       ║");
    Serial.print("║ Ozone        : ");
    Serial.println(ozoneState ? "ON        ║" : "OFF       ║");
    Serial.print("║ Buzzer       : ");
    Serial.println(buzzerState ? "ON        ║" : "OFF       ║");
    Serial.println("╚════════════════════════════════════╝");
    
    unlock();
}

// ===== MUTEX =====

void ActuatorControl::lock() {
    if (mutex) {
        xSemaphoreTake(mutex, portMAX_DELAY);
    }
}

void ActuatorControl::unlock() {
    if (mutex) {
        xSemaphoreGive(mutex);
    }
}