#include "NextionGateWay.h"
#include "SystemState.h"
#include "SystemVariables.h"
#include "RTCManager.h"
#include "SensorManager.h"              // ← ADD
#include "SensorDisplayManager.h"       // ← ADD
#include "esp_task_wdt.h"

NextionGateWay nextion;
SystemStateMachine fsm;
SystemStorage storage;
RTCManager rtcManager;
SensorManager sensorManager;            // ← ADD
SensorDisplayManager displayManager;    // ← ADD

TaskHandle_t nextionTaskHandle;
TaskHandle_t debugTaskHandle;
TaskHandle_t rtcTaskHandle;

void nextionTask(void *pvParameters) {
    for (;;) {
        nextion.readTask();
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

// ===== DEBUG TASK (Updated) =====
void debugStateTask(void *pvParameters) {
    SystemState lastState = SystemState::STATE_ERROR;

    for (;;) {
        NextionData data = nextion.getData();
        
        // Update Storage dari Nextion
        storage.updateFromNextion(data);
        
        // ===== UPDATE SENSORS ===== ← ADD
        sensorManager.update();
        
        // ===== SYNC SENSORS TO NEXTION ===== ← ADD
        displayManager.syncToNextion();
        
        // Update FSM
        fsm.update(data);

        if (fsm.getState() != lastState) {
            lastState = fsm.getState();
            Serial.print("[FSM] State changed → ");
            Serial.println(fsm.stateName());
        }

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

// ===== RTC TASK =====
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

void setup() {
    Serial.begin(115200);
    delay(500);

    // ===== WATCHDOG TIMER INIT =====
    Serial.println("[WDT] Initializing Watchdog Timer (30 seconds)...");
    esp_task_wdt_init(30, true);
    esp_task_wdt_add(NULL);
    Serial.println("[WDT] Watchdog Timer enabled");

    nextion.begin();
    storage.begin();
    rtcManager.begin();
    rtcManager.setNextionGateway(&nextion);
    
    // ===== SENSOR INITIALIZATION ===== ← ADD
    sensorManager.begin();
    displayManager.begin(&sensorManager, &nextion);
    
    Serial.println("[SYSTEM] All systems initialized");

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
