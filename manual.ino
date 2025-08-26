// Fungsi sistem manual
void processManualSystem() {
  // PENTING: Hanya proses filling/draining jika di mode manual
  if (!autoMode) {
    // Proses filling
    if (fillingActive) {
      processFilling();
    }
    
    // Proses draining
    if (drainingActive) {
      processDraining();
    }
    
    // Kontrol suhu
    kontrolSuhuManual();
  }
}

// Di manual.ino - ganti fungsi processFilling()
void processFilling() {
  unsigned long now = millis();
  
  // Debug filling process setiap 2 detik
  if (debug && (now - lastFillingDebug >= 2000)) {
    bool currentFloatState = digitalRead(FLOAT_SENSOR_PIN);
    Serial.print(">> FILLING Debug - Stage: ");
    Serial.print(fillingStage);
    Serial.print(" | Started: "); Serial.print(fillingStarted);
    Serial.print(" | Float: "); Serial.print(currentFloatState == LOW ? "TRIGGERED" : "NORMAL");
    Serial.print(" | Time: "); Serial.print((now - drainStartTime) / 1000);
    Serial.println("s");
    
    // Deteksi perubahan float sensor
    if (currentFloatState != lastFloatState) {
      Serial.print(">> FLOAT SENSOR BERUBAH: ");
      Serial.println(currentFloatState == LOW ? "TRIGGERED" : "NORMAL");
      lastFloatState = currentFloatState;
    }
    
    lastFillingDebug = now;
  }
  
  if (!fillingStarted) {
    // STAGE 1: DRAINING
    digitalWrite(VALVE_DRAIN_PIN, HIGH);
    digitalWrite(VALVE_INLET_PIN, LOW); // Pastikan inlet tertutup
    drainStartTime = now;
    fillingStarted = true;
    fillingStage = 1;
    
    if (debug) Serial.println(">> FILLING: Stage 1 - DRAINING dimulai");
    
  } else {
    if (fillingStage == 1 && (now - drainStartTime >= 5000)) {
      // STAGE 2: FILLING
      digitalWrite(VALVE_DRAIN_PIN, LOW);
      delay(500); // Tunggu valve drain benar-benar tertutup
      digitalWrite(VALVE_INLET_PIN, HIGH);
      fillingStage = 2;
      
      if (debug) Serial.println(">> FILLING: Stage 2 - INLET dibuka");
    }
    
    if (fillingStage == 2) {
      // Cek float sensor dengan debouncing
      bool floatTriggered = digitalRead(FLOAT_SENSOR_PIN) == LOW;
      
      if (floatTriggered) {
        // Double check float sensor untuk mencegah false trigger
        delay(100);
        if (digitalRead(FLOAT_SENSOR_PIN) == LOW) {
          if (debug) Serial.println(">> FILLING: Float sensor triggered - STOPPING");
          stopFillingBySensor();
        }
      }
      
      // Safety timeout - stop filling setelah 10 menit
      if (now - drainStartTime >= 600000) {
        if (debug) Serial.println(">> FILLING: Timeout reached - STOPPING");
        stopFillingBySensor();
      }
    }
  }
}

// Di manual.ino - processDraining()
void processDraining() {
    unsigned long now = millis();
    
    // Mulai grace period jika baru start
    if (!graceActive && !flowOK) {
        graceActive = true;
        graceStart = millis();
        // NYALAKAN VALVE DRAIN DAN PUMP UV
        digitalWrite(VALVE_DRAIN_PIN, HIGH);
        digitalWrite(PUMP_UV_PIN, HIGH);
        pulseCount = 0;
        lastFlowCheckTime = now;
        lowFlowCount = 0; // Reset counter flow rendah
        
        if (debug) {
            Serial.println(">> DRAINING: Started - Valve and Pump UV ON");
            Serial.println(">> DRAINING: 5 second grace period started");
        }
    }
    
    // Cek flow rate setiap detik
    if (millis() - lastFlowCheckTime >= 1000) {
        noInterrupts();
        unsigned long pulses = pulseCount;
        pulseCount = 0;
        interrupts();
        float flowRate = pulses / 7.5;
        
        if (debug) {
            Serial.print(">> DRAINING: Flow rate: ");
            Serial.print(flowRate);
            Serial.println(" L/min");
        }
        
        // Grace period 5 detik
        if (graceActive && !flowOK) {
            if (flowRate >= 0.3) {
                flowOK = true;
                graceActive = false;
                lowFlowCount = 0; // Reset counter
                if (debug) Serial.println(">> DRAINING: Flow detected, grace period ended");
            } else if (millis() - graceStart >= 5000) {
                // Setelah 5 detik grace period, cek flow rate
                if (flowRate < 0.1) {
                    if (debug) Serial.println(">> DRAINING: Flow rate < 0.1 after grace period, stopping");
                    stopDraining();
                } else {
                    flowOK = true;
                    graceActive = false;
                    lowFlowCount = 0; // Reset counter
                    if (debug) Serial.println(">> DRAINING: Flow OK after grace period, continuing");
                }
            }
        }
        // Setelah flow OK, cek tangki kosong dengan logika yang lebih robust
        else if (flowOK) {
            // Gunakan counter untuk mendeteksi flow rate rendah secara konsisten
            if (flowRate < 0.1) {
                lowFlowCount++;
                if (debug) {
                    Serial.print(">> DRAINING: Low flow count: ");
                    Serial.println(lowFlowCount);
                }
                
                // Hanya hentikan jika flow rate rendah selama 3 detik berturut-turut
                if (lowFlowCount >= 5) {
                    if (debug) Serial.println(">> DRAINING: Flow rate consistently low, tank empty, stopping");
                    stopDraining();
                }
            } else {
                // Reset counter jika flow rate normal
                lowFlowCount = 0;
            }
        }
        lastFlowCheckTime = millis();
    }
}

// Kontrol suhu manual (ganti nama fungsi)
void kontrolSuhuManual() {
 unsigned long sekarang = millis();
 
 if (sekarang - waktuTerakhirCek >= intervalCek) {
   waktuTerakhirCek = sekarang;
   
   // Validasi suhu sensor
   if (currentTemp < -50 || currentTemp > 100) {
     if (debug) Serial.println(">> ERROR: Suhu sensor tidak valid!");
     return;
   }
   
   // Debug info (dikurangi frekuensinya)
   if (debug) {
     Serial.print(">> Status: "); Serial.print(activeProcess);
     Serial.print(" | Mode: "); Serial.print(initialCoolingMode ? "INITIAL" : "HISTERESIS");
     Serial.print(" | Target: "); Serial.print(suhuTarget);
     Serial.print("°C | Actual: "); Serial.print(currentTemp);
     Serial.print("°C | Kompressor: "); Serial.println(statusKompresor ? "ON" : "OFF");
   }
   
   // Kontrol sistem berdasarkan activeProcess
   if (activeProcess == 1) {
     if (!sistemAktif) {
       sistemAktif = true;
       digitalWrite(PUMP_UV_PIN, HIGH);
       
       // RESET MODE SAAT SISTEM BARU AKTIF
       initialCoolingMode = true;
       targetReached = false;
       
       if (debug) Serial.println(">> SISTEM KONTROL SUHU DIHIDUPKAN - MODE INITIAL COOLING");
     }
     
     // LOGIKA KONTROL YANG DIPERBAIKI
     float selisih = currentTemp - suhuTarget;
     
     if (initialCoolingMode) {
       // MODE AWAL: Cooling sampai target tercapai
       if (currentTemp > suhuTarget) {
         // Suhu masih di atas target - nyalakan kompressor
         if (!statusKompresor) {
           digitalWrite(COMPRESSOR_PIN, HIGH);
           statusKompresor = true;
           if (debug) Serial.println(">> INITIAL COOLING - KOMPRESOR ON");
         }
       } else {
         // Target tercapai - masuk mode histeresis
         if (statusKompresor) {
           digitalWrite(COMPRESSOR_PIN, LOW);
           statusKompresor = false;
         }
         initialCoolingMode = false;
         targetReached = true;
         if (debug) Serial.println(">> TARGET TERCAPAI - MASUK MODE HISTERESIS");
       }
     } else {
       // MODE HISTERESIS: Kontrol normal dengan histeresis
       if (selisih > histeresis) {
         // Suhu terlalu tinggi - nyalakan kompressor
         if (!statusKompresor) {
           digitalWrite(COMPRESSOR_PIN, HIGH);
           statusKompresor = true;
           if (debug) Serial.println(">> HISTERESIS - SUHU TINGGI - KOMPRESOR ON");
         }
       } 
       else if (selisih < 0) {
         // Suhu sudah di bawah target - matikan kompressor
         if (statusKompresor) {
           digitalWrite(COMPRESSOR_PIN, LOW);
           statusKompresor = false;
           if (debug) Serial.println(">> HISTERESIS - TARGET TERCAPAI - KOMPRESOR OFF");
         }
       }
       // Jika dalam rentang 0 sampai histeresis, pertahankan status kompressor
     }
     
   } else {
     // Matikan sistem jika activeProcess == 0
     if (sistemAktif) {
       sistemAktif = false;
       digitalWrite(COMPRESSOR_PIN, LOW);
       digitalWrite(PUMP_UV_PIN, LOW);
       statusKompresor = false;
       initialCoolingMode = false;
       targetReached = false;
       if (debug) Serial.println(">> SISTEM KONTROL SUHU DIMATIKAN");
     }
   }
 }
}

// Di manual.ino - stopFillingBySensor()
void stopFillingBySensor() {
    digitalWrite(VALVE_INLET_PIN, LOW);
    digitalWrite(VALVE_DRAIN_PIN, LOW);
    fillingActive = false;
    fillingStarted = false;
    fillingStage = 0;
    
    // Jika ini pre-fill, tandai sudah dieksekusi dan simpan tanggalnya
    if (preFillScheduled) {
        preFillExecuted = true;
        preFillScheduled = false;
        lastPreFillDate = rtc.now(); // Simpan tanggal pre-fill terakhir
        
        if (debug) {
            Serial.print(">> Pre-fill completed on: ");
            Serial.print(lastPreFillDate.day());
            Serial.print("/");
            Serial.print(lastPreFillDate.month());
            Serial.print("/");
            Serial.println(lastPreFillDate.year());
        }
    }
    
    if (debug) Serial.println(">> FILLING STOPPED BY SENSOR");
    
    // Update Nextion
    Serial2.print("activeProcess.val=0"); 
    sendFF();
    delay(100);
    
    Serial2.print("blinkingFM.val=0"); 
    sendFF();
    delay(100);
    
    Serial2.print("tBlinkFM.en=0"); 
    sendFF();
    delay(100);
    
    Serial2.print("pFillingMan.pic=6"); // OFF state
    sendFF();
    
    // Update status untuk mode auto
    if (autoMode) {
        Serial2.print("tStatus.txt=\"Auto Mode\"");
        sendFF();
        Serial2.print("tStatus.pic=15");
        sendFF();
    }
}

// Di manual.ino - stopFillingManual()
void stopFillingManual() {
  digitalWrite(VALVE_DRAIN_PIN, LOW);
  digitalWrite(VALVE_INLET_PIN, LOW);
  fillingActive = false;
  fillingStarted = false;
  fillingStage = 0; // Reset stage
  
  if (debug) Serial.println(">> FILLING STOPPED MANUALLY");
  
  // Update Nextion
  Serial2.print("activeProcess.val=0"); 
  sendFF();
  delay(100);
  
  Serial2.print("blinkingFM.val=0"); 
  sendFF();
  delay(100);
  
  Serial2.print("tBlinkFM.en=0"); 
  sendFF();
  delay(100);
  
  Serial2.print("pFillingMan.pic=6"); // OFF state
  sendFF();
}

// Di manual.ino - stopDraining()
void stopDraining() {
    digitalWrite(VALVE_DRAIN_PIN, LOW);
    digitalWrite(PUMP_UV_PIN, LOW); // Matikan pump UV
    drainingActive = false;
    graceActive = false;
    flowOK = false;
    
    if (debug) Serial.println(">> DRAINING STOPPED - Valve and Pump UV OFF");
    
    // Update Nextion dengan jeda yang cukup
    Serial2.print("activeProcess.val=0"); 
    sendFF();
    delay(150);
    
    Serial2.print("blinkingDM.val=0"); 
    sendFF();
    delay(150);
    
    Serial2.print("tBlinkDM.en=0"); 
    sendFF();
    delay(150);
    
    Serial2.print("pDrainingMan.pic=10"); // OFF state
    sendFF();
}