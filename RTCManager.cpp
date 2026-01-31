  #include "RTCManager.h"
  #include "NextionGateWay.h"
  // Extern untuk akses ke NextionGateWay


  // ===== CONSTRUCTOR / DESTRUCTOR =====

  RTCManager::RTCManager()
      : rtcInitialized(false)
      , timeWasSet(false)
      , lastDateTime(DateTime(2025, 1, 1, 0, 0, 0))
      , nextionPtr(nullptr)
  {
      mutex = xSemaphoreCreateMutex();
  }

  RTCManager::~RTCManager() {
      if (mutex) vSemaphoreDelete(mutex);
  }

  void RTCManager::setNextionGateway(NextionGateWay* nxt) {
    nextionPtr = nxt;
  }


  // ===== PUBLIC API =====

  void RTCManager::begin() {
      lock();
      
      if (!rtc.begin()) {
          Serial.println("[RTC] ERROR: RTC not found!");
          rtcInitialized = false;
          unlock();
          return;
      }
      
      rtcInitialized = true;
      
      // Cek apakah RTC kehilangan power
      if (rtc.lostPower()) {
          Serial.println("[RTC] WARNING: RTC lost power, setting default time");
          rtc.adjust(DateTime(2025, 1, 1, 0, 0, 0));
          timeWasSet = false;
      } else {
          Serial.println("[RTC] RTC initialized successfully");
          timeWasSet = true;
          lastDateTime = rtc.now();
      }
      
      unlock();
      
      printTime();
  }

  // ===== SET TIME =====

  bool RTCManager::setTime(const String &timeStr) {
      uint8_t hour, minute;
      
      if (!parseTimeString(timeStr, hour, minute)) {
          Serial.print("[RTC] ERROR: Invalid time format: ");
          Serial.println(timeStr);
          return false;
      }
      
      return setTime(hour, minute);
  }

  bool RTCManager::setTime(uint8_t hour, uint8_t minute) {
      if (!rtcInitialized) {
          Serial.println("[RTC] ERROR: RTC not initialized");
          return false;
      }
      
      if (hour > 23 || minute > 59) {
          Serial.println("[RTC] ERROR: Invalid time values");
          return false;
      }
      
      lock();
      
      // Get current date, hanya update time
      DateTime now = rtc.now();
      DateTime newTime(now.year(), now.month(), now.day(), hour, minute, 0);
      
      rtc.adjust(newTime);
      lastDateTime = newTime;
      timeWasSet = true;
      
      unlock();
      
      Serial.print("[RTC] Time set to: ");
      Serial.print(hour);
      Serial.print(":");
      if (minute < 10) Serial.print("0");
      Serial.println(minute);
      
      return true;
  }

  // ===== GET TIME =====

  String RTCManager::getTimeString() {
      if (!rtcInitialized) {
          return "--:--";
      }
      
      lock();
      DateTime now = rtc.now();
      lastDateTime = now;
      unlock();
      
      char buffer[6];
      snprintf(buffer, sizeof(buffer), "%02d:%02d", now.hour(), now.minute());
      return String(buffer);
  }

  String RTCManager::getTimeStringFull() {
      if (!rtcInitialized) {
          return "--:--:--";
      }
      
      lock();
      DateTime now = rtc.now();
      lastDateTime = now;
      unlock();
      
      char buffer[9];
      snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d", 
              now.hour(), now.minute(), now.second());
      return String(buffer);
  }

  uint8_t RTCManager::getHour() {
      if (!rtcInitialized) return 0;
      
      lock();
      DateTime now = rtc.now();
      uint8_t hour = now.hour();
      unlock();
      
      return hour;
  }

  uint8_t RTCManager::getMinute() {
      if (!rtcInitialized) return 0;
      
      lock();
      DateTime now = rtc.now();
      uint8_t minute = now.minute();
      unlock();
      
      return minute;
  }

  uint8_t RTCManager::getSecond() {
      if (!rtcInitialized) return 0;
      
      lock();
      DateTime now = rtc.now();
      uint8_t second = now.second();
      unlock();
      
      return second;
  }

  // ===== STATUS =====

  bool RTCManager::isRTCValid() {
      return rtcInitialized;
  }

  bool RTCManager::isTimeSet() {
      return timeWasSet;
  }

  // ===== NEXTION SYNC =====

void RTCManager::sendToNextion(const String &componentName) {
    if (!rtcInitialized) {
        Serial.println("[RTC] ERROR: Cannot send to Nextion, RTC not initialized");
        return;
    }
    
    if (!nextionPtr) {  // ← TAMBAHKAN pengecekan
        Serial.println("[RTC] ERROR: Nextion not set");
        return;
    }
    
    String timeStr = getTimeString();
    String cmd = componentName + ".txt=\"" + timeStr + "\"";
    
    nextionPtr->send(cmd);  // ← UBAH dari nextion.send() ke nextionPtr->send()
    

}

  // ===== DEBUG =====

  void RTCManager::printTime() {
      if (!rtcInitialized) {
          Serial.println("[RTC] RTC not initialized");
          return;
      }
      
      lock();
      DateTime now = rtc.now();
      unlock();
      
      Serial.println("╔════════════════════════════════════╗");
      Serial.println("║         RTC TIME                   ║");
      Serial.println("╠════════════════════════════════════╣");
      
      char buffer[32];
      snprintf(buffer, sizeof(buffer), "║ %04d-%02d-%02d %02d:%02d:%02d          ║",
              now.year(), now.month(), now.day(),
              now.hour(), now.minute(), now.second());
      Serial.println(buffer);
      
      Serial.print("║ Status: ");
      Serial.println(timeWasSet ? "SET            ║" : "NOT SET        ║");
      
      Serial.println("╚════════════════════════════════════╝");
  }

  // ===== PRIVATE METHODS =====

  bool RTCManager::parseTimeString(const String &timeStr, uint8_t &hour, uint8_t &minute) {
      // Format expected: "HH:MM"
      if (timeStr.length() != 5) {
          return false;
      }
      
      if (timeStr.charAt(2) != ':') {
          return false;
      }
      
      // Parse hour
      String hourStr = timeStr.substring(0, 2);
      String minStr = timeStr.substring(3, 5);
      
      // Validate digits
      for (int i = 0; i < 2; i++) {
          if (!isDigit(hourStr[i]) || !isDigit(minStr[i])) {
              return false;
          }
      }
      
      hour = hourStr.toInt();
      minute = minStr.toInt();
      
      // Validate range
      if (hour > 23 || minute > 59) {
          return false;
      }
      
      return true;
  }

  // ===== MUTEX =====

  void RTCManager::lock() {
      if (mutex) {
          xSemaphoreTake(mutex, portMAX_DELAY);
      }
  }

  void RTCManager::unlock() {
      if (mutex) {
          xSemaphoreGive(mutex);
      }
  }