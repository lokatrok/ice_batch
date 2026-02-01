//ice_batch.ino

#include "NextionGateWay.h"
#include "SystemState.h"
#include "SystemVariables.h"
#include "RTCManager.h"
#include "SensorManager.h"
#include "SensorDisplayManager.h"
#include "ActuatorControl.h"              // ← ADD
#include "NextionOutput.h"                // ← ADD
#include "StateConditionHandler.h"        // ← ADD
#include "esp_task_wdt.h"

// ===== GLOBAL INSTANCES =====
NextionGateWay nextion;
SystemStorage storage;
RTCManager rtcManager;
SensorManager sensorManager;
SensorDisplayManager displayManager;
ActuatorControl actuatorControl;          // ← ADD
NextionOutput nextionOutput;              // ← ADD
StateConditionHandler conditionHandler(   // ← ADD (with dependencies)
    &sensorManager,
    &actuatorControl,
    &nextionOutput
);
SystemStateMachine fsm(                   // ← UPDATE (with dependencies)
    &sensorManager,
    &actuatorControl,
    &nextionOutput,
    &conditionHandler,
    &storage,
    &nextion  // ← BENAR (object yang sudah dideklarasi di line 15)
);

TaskHandle_t nextionTaskHandle;
TaskHandle_t debugTaskHandle;
TaskHandle_t rtcTaskHandle;

// ===== TASKS =====

void nextionTask(void *pvParameters) {
    for (;;) {
        nextion.readTask();
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void debugStateTask(void *pvParameters) {
    SystemState lastState = SystemState::STATE_ERROR;

    for (;;) {
        NextionData data = nextion.getData();
        
        // Update Storage dari Nextion
        storage.updateFromNextion(data);
        
        // ===== UPDATE SENSORS =====
        sensorManager.update();
        
        // ===== SYNC SENSORS TO NEXTION =====
        displayManager.syncToNextion();
        
        // ===== UPDATE FSM =====
        fsm.update(data);

        // Debug log state changes
        if (fsm.getState() != lastState) {
            lastState = fsm.getState();
            Serial.print("[FSM] State changed → ");
            Serial.println(fsm.stateName());
        }

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

void rtcTask(void *pvParameters) {
    static String lastSetTime = "";
    
    for (;;) {
        // Cek apakah ada setTime baru dari storage
        String currentSetTime = storage.getSetTime();
        
        if (currentSetTime.length() > 0 && currentSetTime != lastSetTime) {
            // Ada perubahan setTime, update RTC
            if (rtcManager.setTime(currentSetTime)) {
                Serial.print("[RTC] Time updated from storage: ");
                Serial.println(currentSetTime);
                lastSetTime = currentSetTime;
            }
        }
        
        // Kirim waktu ke Nextion setiap 1 detik
        rtcManager.sendToNextion("tClock");
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// ===== SETUP =====

void setup() {
    Serial.begin(115200);
    delay(500);

    // ===== WATCHDOG TIMER INIT =====
    Serial.println("[WDT] Initializing Watchdog Timer (30 seconds)...");
    esp_task_wdt_init(30, true);
    esp_task_wdt_add(NULL);
    Serial.println("[WDT] Watchdog Timer enabled");

    // ===== INITIALIZE MODULES =====
    nextion.begin();
    storage.begin();
    rtcManager.begin();
    rtcManager.setNextionGateway(&nextion);
    
    sensorManager.begin();
    displayManager.begin(&sensorManager, &nextion);
    
    actuatorControl.begin();              // ← ADD
    nextionOutput.begin();                // ← ADD
    
    Serial.println("[SYSTEM] All systems initialized");

    // ===== CREATE TASKS =====
    xTaskCreatePinnedToCore(
        nextionTask,
        "NextionTask",
        4096,
        nullptr,
        2,
        &nextionTaskHandle,
        1
    );

    xTaskCreatePinnedToCore(
        debugStateTask,
        "DebugStateTask",
        2048,
        nullptr,
        1,
        &debugTaskHandle,
        0
    );

    xTaskCreatePinnedToCore(
        rtcTask,
        "RTCTask",
        2048,
        nullptr,
        1,
        &rtcTaskHandle,
        0
    );
}

void loop() {
    esp_task_wdt_reset();
    vTaskDelay(pdMS_TO_TICKS(100));
}
