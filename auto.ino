// Hitung waktu pre-fill (3 jam = 180 menit sebelum operasi)
const int PRE_FILL_MINUTES = 180;  // 3 jam
const float TEMP_HYSTERESIS = 1.0; // 1 derajat

// Di auto.ino - setupRTC()
void setupRTC() {
    Wire.begin(RTC_SDA_PIN, RTC_SCL_PIN);
    
    if (!rtc.begin()) {
        Serial.println("Couldn't find RTC");
        while (1);
    }
    
    if (rtc.lostPower()) {
        Serial.println("RTC lost power, setting to compile time");
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
    
    // PERBAIKAN: Inisialisasi tanggal pergantian air terakhir ke masa lalu
    // agar sistem mulai dengan fresh schedule
    lastChangeDate = DateTime(2020, 1, 1);
    
    // PERBAIKAN: Inisialisasi tanggal pre-fill terakhir ke masa lalu
    lastPreFillDate = DateTime(2020, 1, 1);
    
    if (debug) {
        Serial.println(">> RTC initialized with fresh schedule");
    }
}

// Di auto.ino - readAutoSettings()
void readAutoSettings() {
    // Jangan baca dari Nextion jika sedang menerima parameter
    if (receivingAutoParams) return;
    
    // Baca komponen waktu satu per satu menggunakan perintah "get"
    autoHour1 = bacaKomponenWaktu("nHourAuto1", 0, 2); // Digit pertama jam (0-2)
    autoHour2 = bacaKomponenWaktu("nHourAuto2", 0, 9); // Digit kedua jam (0-9)
    autoMin1 = bacaKomponenWaktu("nMinAuto1", 0, 5);   // Digit pertama menit (0-5)
    autoMin2 = bacaKomponenWaktu("nMinAuto2", 0, 9);   // Digit kedua menit (0-9)
    
    // Gabungkan menjadi format jam:menit
    int jam = autoHour1 * 10 + autoHour2;
    int menit = autoMin1 * 10 + autoMin2;
    
    // Validasi waktu gabungan
    if (jam > 23 || menit > 59) {
        if (debug) {
            Serial.print(">> Invalid time: ");
            Serial.print(jam);
            Serial.print(":");
            Serial.println(menit);
            Serial.println(", using default 01:00");
        }
        // Set ke default
        autoHour1 = 0;
        autoHour2 = 1;
        autoMin1 = 0;
        autoMin2 = 0;
        jam = 1;
        menit = 0;
    }
    
    if (debug) {
        Serial.print(">> Auto Settings - Time: ");
        if (jam < 10) Serial.print("0");
        Serial.print(jam);
        Serial.print(":");
        if (menit < 10) Serial.print("0");
        Serial.print(menit);
        Serial.print(" | Temp: ");
        Serial.print(autoTempTarget);
        Serial.print("°C | Change every: ");
        Serial.print(autoChangeDay);
        Serial.println(" days");
    }
}

// PERBAIKAN: Helper function untuk deteksi apakah target hari ini atau besok
bool isTargetToday(int currentMinutes, int targetMinutes) {
    // Target masih bisa dicapai hari ini jika waktu sekarang belum melewati target
    return (currentMinutes < targetMinutes);
}

// PERBAIKAN: Helper function untuk deteksi apakah pre-fill hari ini atau besok
bool isPreFillToday(int currentMinutes, int targetMinutes) {
    int preFillMinutes = targetMinutes - PRE_FILL_MINUTES;
    if (preFillMinutes < 0) {
        // Pre-fill cross midnight, cek apakah masih bisa hari ini
        return false; // Pre-fill besok
    }
    // Pre-fill hari ini jika waktu masih memungkinkan
    return (currentMinutes < targetMinutes);
}

// PERBAIKAN: Helper function untuk cek apakah pre-fill sudah executed hari ini
bool preFillExecutedToday(DateTime now) {
    return (preFillExecuted && 
            lastPreFillDate.day() == now.day() &&
            lastPreFillDate.month() == now.month() &&
            lastPreFillDate.year() == now.year());
}

// Proses sistem auto
void processAutoSystem() {
  unsigned long now = millis();
  
  // Cek jadwal setiap menit
  if (now - lastAutoCheck >= autoCheckInterval) {
    checkAutoSchedule();
    lastAutoCheck = now;
  }
  
  // PENTING: Hanya proses filling/draining jika di mode auto
  if (autoMode) {
    // Proses filling jika dijadwalkan
    if (fillingActive) {
      processFilling();
    } 
    
    // Proses draining jika dijadwalkan
    if (drainingActive) {
      processDraining();
    }
    
    // Kontrol suhu jika sirkulasi aktif
    if (circulationActive) {
      kontrolSuhuAuto();
    }
  }
}

// Di auto.ino - checkAutoSchedule() - DIMODIFIKASI
void checkAutoSchedule() {
    DateTime now = rtc.now();
    
    // Baca pengaturan terbaru dari Nextion
    readAutoSettings();
    
    // Hitung waktu operasi (dalam menit sejak tengah malam)
    int targetTime = autoHour1 * 10 + autoHour2;
    int targetMinute = autoMin1 * 10 + autoMin2;
    int totalTargetMinutes = targetTime * 60 + targetMinute;
    
    // Validasi waktu target
    if (targetTime < 0 || targetTime > 23 || targetMinute < 0 || targetMinute > 59) {
        if (debug) Serial.println(">> Invalid auto time settings, using defaults");
        targetTime = 1;
        targetMinute = 0;
        totalTargetMinutes = 60; // 01:00
    }
    
    // Hitung waktu sekarang (dalam menit sejak tengah malam)
    int currentHours = now.hour();
    int currentMinutes = now.minute();
    int totalCurrentMinutes = currentHours * 60 + currentMinutes;
    
    // PERBAIKAN: Deteksi apakah target dan pre-fill hari ini atau besok
    bool targetIsToday = isTargetToday(totalCurrentMinutes, totalTargetMinutes);
    bool preFillIsToday = isPreFillToday(totalCurrentMinutes, totalTargetMinutes);
    
    // PERBAIKAN: Hitung pre-fill time berdasarkan deteksi hari
    int preFillTime;
    if (preFillIsToday) {
        preFillTime = totalTargetMinutes - PRE_FILL_MINUTES;
        if (preFillTime < 0) preFillTime = 0; // Safety untuk edge case
    } else {
        // Pre-fill besok, hitung cross-day
        preFillTime = totalTargetMinutes - PRE_FILL_MINUTES;
        if (preFillTime < 0) {
            preFillTime += 1440; // Tambah 24 jam jika negatif
        }
    }
    
    // PERBAIKAN: Hitung days since last dengan logika yang benar
    uint32_t nowTimestamp = now.unixtime();
    uint32_t lastPreFillTimestamp = lastPreFillDate.unixtime();
    int daysSinceLastPreFill = (nowTimestamp - lastPreFillTimestamp) / 86400; // 86400 detik = 1 hari
    
    // PERBAIKAN: Jika target hari ini dan belum pernah pre-fill, 
    // anggap sudah cukup lama untuk memulai cycle
    if (targetIsToday && !preFillExecutedToday(now) && daysSinceLastPreFill == 0) {
        // Jika baru setup atau reset, berikan kesempatan untuk langsung start
        DateTime yesterday = DateTime(now.unixtime() - 86400);
        if (lastPreFillDate.year() == 2020) { // Deteksi init value
            daysSinceLastPreFill = autoChangeDay; // Set ke threshold agar bisa langsung jalan
        }
    }
    
    // Debug scheduling - DIPERBAIKI dengan info tambahan
    if (debug) {
        Serial.print(">> Auto Schedule Check - Current: ");
        Serial.print(currentHours);
        Serial.print(":");
        if (currentMinutes < 10) Serial.print("0");
        Serial.print(currentMinutes);
        Serial.print(" | Target: ");
        Serial.print(targetTime);
        Serial.print(":");
        if (targetMinute < 10) Serial.print("0");
        Serial.print(targetMinute);
        Serial.print(" | Pre-fill: ");
        Serial.print(preFillTime / 60);
        Serial.print(":");
        if (preFillTime % 60 < 10) Serial.print("0");
        Serial.print(preFillTime % 60);
        Serial.print(" | Days since last: ");
        Serial.print(daysSinceLastPreFill);
        Serial.print(" | Change every: ");
        Serial.print(autoChangeDay);
        Serial.println(" days");
        
        // TAMBAHAN: Info scheduling decision
        Serial.print(">> Scheduling: Target today: ");
        Serial.print(targetIsToday ? "YES" : "NO");
        Serial.print(" | Pre-fill today: ");
        Serial.println(preFillIsToday ? "YES" : "NO");
    }
    
    // PERBAIKAN: Logika pre-fill dengan cross-day handling
    bool inPreFillWindow = false;
    
    if (preFillIsToday) {
        // Pre-fill hari ini
        inPreFillWindow = (totalCurrentMinutes >= preFillTime && totalCurrentMinutes < totalTargetMinutes);
    } else {
        // Pre-fill cross midnight - cek dua kondisi:
        // 1. Sore/malam hari ini (setelah preFillTime)
        // 2. Dini hari besok (sebelum target)
        bool lateToday = (totalCurrentMinutes >= preFillTime);
        bool earlyTomorrow = (totalCurrentMinutes < (totalTargetMinutes - 1440 + PRE_FILL_MINUTES));
        inPreFillWindow = (lateToday || earlyTomorrow);
        
        if (debug && (lateToday || earlyTomorrow)) {
            Serial.print(">> Cross-day pre-fill window: Late today=");
            Serial.print(lateToday);
            Serial.print(" | Early tomorrow=");
            Serial.println(earlyTomorrow);
        }
    }
    
    // Cek apakah sudah waktunya pre-fill
    if (inPreFillWindow) {
        // Cek apakah sudah waktunya melakukan water change berdasarkan cycle
        bool shouldDoWaterChange = (daysSinceLastPreFill >= autoChangeDay);
        
        // PERBAIKAN: Gunakan helper function yang lebih reliable
        bool preFillAlreadyDoneToday = preFillExecutedToday(now);
        
        if (shouldDoWaterChange && !preFillAlreadyDoneToday && !fillingActive && !drainingActive && !circulationActive) {
            preFillScheduled = true;
            if (debug) {
                Serial.println(">> Starting pre-fill - Water change cycle reached");
                Serial.print(">> Days since last: ");
                Serial.print(daysSinceLastPreFill);
                Serial.print(" >= ");
                Serial.print(autoChangeDay);
                Serial.println(" days");
            }
            
            // PERBAIKAN: Update tracking variables saat mulai
            preFillExecuted = true;
            lastPreFillDate = now;
            
            // Mulai proses filling
            fillingActive = true;
            fillingStarted = false;
            fillingStage = 0;
            
            // Update Nextion
            updateNextionStatus("Pre-Fill", 4);
            
        } else {
            // Tambahkan debug untuk kondisi yang tidak memenuhi
            if (debug) {
                Serial.println(">> Pre-fill conditions NOT met:");
                Serial.print("  shouldDoWaterChange (");
                Serial.print(daysSinceLastPreFill);
                Serial.print(" >= ");
                Serial.print(autoChangeDay);
                Serial.print("): ");
                Serial.println(shouldDoWaterChange);
                Serial.print("  preFillAlreadyDoneToday: ");
                Serial.println(preFillAlreadyDoneToday);
                Serial.print("  fillingActive: ");
                Serial.println(fillingActive);
                Serial.print("  drainingActive: ");
                Serial.println(drainingActive);
                Serial.print("  circulationActive: ");
                Serial.println(circulationActive);
                
                if (!shouldDoWaterChange) {
                    Serial.print("  Days remaining: ");
                    Serial.println(autoChangeDay - daysSinceLastPreFill);
                }
            }
        }
    }
    
    // Cek apakah sudah waktunya operasi cooling
    if (totalCurrentMinutes >= totalTargetMinutes && totalCurrentMinutes < totalTargetMinutes + 60 && !fillingActive && !drainingActive && !circulationActive) {
        // Hanya aktifkan sirkulasi jika tidak dimatikan secara manual
        if (!manualCirculationOff) {
            startCirculation();
        } else {
            if (debug) Serial.println(">> Circulation skipped due to manual override");
        }
    }
  
    // Cek apakah sudah waktunya berhenti (1 jam setelah operasi dimulai)
    if (circulationActive && totalCurrentMinutes >= totalTargetMinutes + 60) {
        stopCirculation();
    }
    
    // Reset flag pre-fill jika sudah lewat waktu operasi dan pre-fill sudah selesai
    if (totalCurrentMinutes >= totalTargetMinutes + 60 && preFillExecuted) {
        preFillScheduled = false;
        preFillExecuted = false;
    }
}

// Mulai proses pergantian air
void startWaterChange() {
  if (debug) {
    Serial.println(">> Starting scheduled water change");
  }
  
  // Mulai draining
  digitalWrite(VALVE_DRAIN_PIN, HIGH);
  digitalWrite(PUMP_UV_PIN, HIGH); // Nyalakan pump UV saat draining
  drainingActive = true;
  graceActive = false;
  flowOK = false;
  pulseCount = 0;
  lastFlowCheckTime = millis();
  
  // Update Nextion
  updateNextionStatus("Water Change", 7);
}

// Mulai pre-fill
void startPreFill() {
  if (debug) {
    Serial.println(">> Starting pre-fill");
  }
  
  // Cek float sensor
  bool floatState = digitalRead(FLOAT_SENSOR_PIN);
  
  if (floatState == HIGH) { // Air belum cukup
    fillingActive = true;
    fillingStarted = false;
    fillingStage = 0;
    lastFloatState = floatState;
    
    // Update Nextion
    updateNextionStatus("Pre-Fill", 4);
  } else {
    if (debug) {
      Serial.println(">> Water level already sufficient");
    }
  }
}

// Helper function untuk update Nextion
void updateNextionStatus(const char* statusText, uint8_t picNumber) {
    Serial2.print("tStatus.txt=\"");
    Serial2.print(statusText);
    Serial2.print("\"");
    sendFF();
    
    Serial2.print("tStatus.pic=");
    Serial2.print(picNumber);
    sendFF();
}

// Mulai sirkulasi
void startCirculation() {
  if (debug) {
    Serial.println(">> Starting circulation");
  }
  
  circulationActive = true;
  manualCirculationOn = false;  // Reset flag manual (ini untuk aktivasi otomatis)
  digitalWrite(PUMP_UV_PIN, HIGH);
  
  // Reset mode kontrol suhu
  initialCoolingMode = true;
  targetReached = false;
  
  // Reset flag manual override
  manualCirculationOff = false;
  
  // Update Nextion dengan jeda yang cukup
  updateNextionCirculation(true);
}

// Fungsi untuk mengaktifkan sirkulasi manual
void startManualCirculation() {
  if (debug) {
    Serial.println(">> Starting manual circulation");
  }
  
  // Hanya aktifkan jika mode auto aktif
  if (autoMode) {
    // Reset flag pre-fill
    preFillScheduled = false;
    preFillExecuted = false;
    
    // Aktifkan sirkulasi manual
    circulationActive = true;
    manualCirculationOn = true;  // Tandai sebagai aktivasi manual
    manualCirculationOff = false;
    
    // Reset mode kontrol suhu
    initialCoolingMode = true;
    targetReached = false;
    
    // Nyalakan pompa UV
    digitalWrite(PUMP_UV_PIN, HIGH);
    
    // Update Nextion
    updateNextionCirculation(true);
    
    if (debug) Serial.println(">> Manual circulation activated in auto mode");
  } else {
    if (debug) Serial.println(">> Manual circulation ignored - auto mode not active");
  }
}

void stopCirculation() {
  if (debug) {
    Serial.println(">> Stopping circulation");
  }
  
  circulationActive = false;
  manualCirculationOn = false;  // Reset flag manual
  digitalWrite(PUMP_UV_PIN, LOW);
  digitalWrite(COMPRESSOR_PIN, LOW);
  statusKompresor = false;
  
  // Update Nextion dengan jeda yang cukup
  updateNextionCirculation(false);
}

// Helper function untuk update UI sirkulasi
void updateNextionCirculation(bool isActive) {
    if (isActive) {
        Serial2.print("tStatus.txt=\"Circulation\"");
        sendFF();
        Serial2.print("tStatus.pic=8");
        sendFF();
        Serial2.print("bStopCir.pic=9"); // Ubah tampilan tombol menjadi ON
        sendFF();
        Serial2.print("blinkingSC.val=1"); // Aktifkan blinking
        sendFF();
        Serial2.print("tBlinkSC.en=1");
        sendFF();
        Serial2.print("cirActive.val=1"); // Update variabel Nextion
        sendFF();
    } else {
        Serial2.print("tStatus.txt=\"Auto Mode\"");
        sendFF();
        Serial2.print("tStatus.pic=9");
        sendFF();
        Serial2.print("bStopCir.pic=10"); // Ubah tampilan tombol menjadi OFF
        sendFF();
        Serial2.print("blinkingSC.val=0"); // Matikan blinking
        sendFF();
        Serial2.print("tBlinkSC.en=0");
        sendFF();
        Serial2.print("cirActive.val=0"); // Update variabel Nextion
        sendFF();
    }
}

// Kontrol suhu untuk mode auto
void kontrolSuhuAuto() {
  unsigned long sekarang = millis();
  
  // Cek suhu setiap 5 detik
  if (sekarang - waktuTerakhirCek >= 5000) {
    waktuTerakhirCek = sekarang;
    
    // Validasi suhu sensor
    if (currentTemp < -50 || currentTemp > 100) {
      if (debug) Serial.println(">> ERROR: Suhu sensor tidak valid!");
      return;
    }
    
    // Debug info
    if (debug) {
      Serial.print(">> Auto Mode - Target: ");
      Serial.print(autoTempTarget);
      Serial.print("°C | Actual: ");
      Serial.print(currentTemp);
      Serial.print("°C | Compressor: ");
      Serial.println(statusKompresor ? "ON" : "OFF");
    }
    
    // Logika kontrol suhu
    float selisih = currentTemp - autoTempTarget;
    
    if (initialCoolingMode) {
      // Mode awal: Cooling sampai target tercapai
      if (currentTemp > autoTempTarget) {
        // Suhu masih di atas target - nyalakan kompressor
        if (!statusKompresor) {
          digitalWrite(COMPRESSOR_PIN, HIGH);
          statusKompresor = true;
          if (debug) Serial.println(">> AUTO INITIAL COOLING - KOMPRESOR ON");
        }
      } else {
        // Target tercapai - masuk mode histeresis
        if (statusKompresor) {
          digitalWrite(COMPRESSOR_PIN, LOW);
          statusKompresor = false;
        }
        initialCoolingMode = false;
        targetReached = true;
        if (debug) Serial.println(">> AUTO TARGET TERCAPAI - MASUK MODE HISTERESIS");
      }
    } else {
      // Mode histeresis: Kontrol normal dengan histeresis
      if (selisih > TEMP_HYSTERESIS) { // Histeresis 1 derajat
        // Suhu terlalu tinggi - nyalakan kompressor
        if (!statusKompresor) {
          digitalWrite(COMPRESSOR_PIN, HIGH);
          statusKompresor = true;
          if (debug) Serial.println(">> AUTO HISTERESIS - SUHU TINGGI - KOMPRESOR ON");
        }
      } 
      else if (selisih < 0) {
        // Suhu sudah di bawah target - matikan kompressor
        if (statusKompresor) {
          digitalWrite(COMPRESSOR_PIN, LOW);
          statusKompresor = false;
          if (debug) Serial.println(">> AUTO HISTERESIS - TARGET TERCAPAI - KOMPRESOR OFF");
        }
      }
      // Jika dalam rentang 0 sampai histeresis, pertahankan status kompressor
    }
  }
}