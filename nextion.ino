// ======== NEXTION.INO ========

// Di nextion.ino - tambahkan di bagian atas
void readNextion();
void processNextionCommand(String command);
void handleNextionCommunication();

// Fungsi pembantu untuk mengirim 0xFF
void sendFF() {
  Serial2.write(0xFF);
  Serial2.write(0xFF);
  Serial2.write(0xFF);
}

// Handle komunikasi Nextion
void handleNextionCommunication() {
  readNextion();
  
  // Baca parameter dari Nextion dengan interval
  if (millis() - lastNextionRead >= intervalNextionRead) {
    bacaParameterDariNextion();
    lastNextionRead = millis();
  }
}

// Baca perintah dari Nextion
void readNextion() {
  while (Serial2.available()) {
    uint8_t ch = Serial2.read();
    if (ch == 0xFF) {
      ffCount++;
      if (ffCount == 3) {
        cmd.trim();
        if (cmd.length() > 0) {
          processNextionCommand(cmd);
        }
        cmd = "";
        ffCount = 0;
      }
    } else {
      cmd += (char)ch;
      ffCount = 0;
    }
  }
}

// Proses perintah dari Nextion
void processNextionCommand(String command) {
  // Debug semua perintah yang diterima
  if (debug && command.length() > 0) {
    Serial.print(">> RAW COMMAND: '");
    Serial.print(command);
    Serial.println("'");
  }
  
  if (command.indexOf("FILLING_ON") >= 0) {
    Serial.println(">> DEBUG: FILLING_ON command detected");
    if (!fillingActive) {
      fillingActive = true;
      fillingStarted = false;
      fillingStage = 0;
      lastFloatState = digitalRead(FLOAT_SENSOR_PIN);
      
      Serial.println(">> DEBUG: Filling activated, state set");
      
      // Feedback ke Nextion - Filling ON (pic 7)
      Serial2.print("blinkingFM.val=1");
      sendFF();
      Serial2.print("tBlinkFM.en=1");
      sendFF();
      Serial2.print("pFillingMan.pic=7"); // ON state
      sendFF();
      Serial2.print("activeProcess.val=2"); // Set active process ke filling
      sendFF();
      
      Serial.println(">> DEBUG: Nextion feedback sent");
    } else {
      Serial.println(">> DEBUG: Filling already active");
    }
  }
  else if (command.indexOf("FILLING_OFF") >= 0) {
    Serial.println(">> DEBUG: FILLING_OFF command detected");
    stopFillingManual();
  }
  else if (command.indexOf("COOLING_ON") >= 0) {
    Serial.println(">> DEBUG: COOLING_ON command detected");
    if (activeProcess != 1) {
      activeProcess = 1;
      // Feedback ke Nextion - Cooling ON (pic 9)
      Serial2.print("pCoolingMan.pic=9"); // ON state
      sendFF();
      Serial2.print("activeProcess.val=1"); // Set active process ke cooling
      sendFF();
    }
  }
  else if (command.indexOf("COOLING_OFF") >= 0) {
    Serial.println(">> DEBUG: COOLING_OFF command detected");
    if (activeProcess == 1) {
      activeProcess = 0;
      // Feedback ke Nextion - Cooling OFF (pic 8)
      Serial2.print("pCoolingMan.pic=8"); // OFF state
      sendFF();
      Serial2.print("activeProcess.val=0"); // Reset active process
      sendFF();
    }
  }
  else if (command.indexOf("DRAINING_ON") >= 0) {
    Serial.println(">> DEBUG: DRAINING_ON command detected");
    if (!drainingActive) {
      digitalWrite(VALVE_DRAIN_PIN, HIGH);
      drainingActive = true;
      pulseCount = 0;
      lastFlowCheckTime = millis();
      
      // Feedback ke Nextion - Draining ON (pic 11)
      Serial2.print("blinkingDM.val=1");
      sendFF();
      Serial2.print("tBlinkDM.en=1");
      sendFF();
      Serial2.print("pDrainingMan.pic=11"); // ON state
      sendFF();
      Serial2.print("activeProcess.val=3"); // Set active process ke draining
      sendFF();
    }
  }
  else if (command.indexOf("DRAINING_OFF") >= 0) {
    Serial.println(">> DEBUG: DRAINING_OFF command detected");
    stopDraining();
  }
// Di nextion.ino - processNextionCommand() untuk AUTO_ON
else if (command.indexOf("AUTO_ON") >= 0) {
    Serial.println(">> DEBUG: AUTO_ON command detected");
    autoMode = true;
    circulationActive = false;
    fillingActive = false;
    drainingActive = false;
    
    // Reset flag pre-fill
    preFillScheduled = false;
    preFillExecuted = false;
    
    // Matikan semua aktuator
    digitalWrite(VALVE_DRAIN_PIN, LOW);
    digitalWrite(VALVE_INLET_PIN, LOW);
    digitalWrite(COMPRESSOR_PIN, LOW);
    digitalWrite(PUMP_UV_PIN, LOW);
    
    // Reset status
    statusKompresor = false;
    sistemAktif = false;
    
    // Cek apakah ada parameter dalam perintah
    if (command.length() > 7) { // "AUTO_ON" = 7 karakter
        // Extract parameter dari perintah
        // Format: AUTO_ON,suhu,hari
        String params = command.substring(8); // Skip "AUTO_ON,"
        if (debug) {
            Serial.print(">> Extracted params: ");
            Serial.println(params);
        }
        
        // Parse parameter dengan split
        int paramCount = 0;
        int lastIndex = 0;
        for (int i = 0; i < params.length(); i++) {
            if (params.charAt(i) == ',') {
                String param = params.substring(lastIndex, i);
                if (paramCount == 0) {
                    // Parameter pertama adalah suhu
                    param.replace("°C", "");
                    param.replace(" ", "");
                    autoTempTarget = param.toInt();
                    if (autoTempTarget < 1 || autoTempTarget > 30) autoTempTarget = 3;
                } else if (paramCount == 1) {
                    // Parameter kedua adalah hari
                    param.replace("hari", "");
                    param.replace(" ", "");
                    autoChangeDay = param.toInt();
                    if (autoChangeDay < 1 || autoChangeDay > 365) autoChangeDay = 7;
                }
                paramCount++;
                lastIndex = i + 1;
            }
        }
        
        // Ambil parameter terakhir jika ada
        if (lastIndex < params.length() && paramCount == 1) {
            String param = params.substring(lastIndex);
            param.replace("hari", "");
            param.replace(" ", "");
            autoChangeDay = param.toInt();
            if (autoChangeDay < 1 || autoChangeDay > 365) autoChangeDay = 7;
        }
        
        if (debug) {
            Serial.print(">> Parsed temperature: ");
            Serial.print(autoTempTarget);
            Serial.println("°C");
            Serial.print(">> Parsed day: ");
            Serial.println(autoChangeDay);
        }
    } else {
        // Jika tidak ada parameter, gunakan default
        autoTempTarget = 3;
        autoChangeDay = 7;
        if (debug) Serial.println(">> No parameters, using defaults");
    }
    
    // Baca pengaturan waktu dari Nextion
    readAutoSettings();
    
    // Feedback ke Nextion - Auto ON (pic 15)
    Serial2.print("tStatus.txt=\"Auto Mode\"");
    sendFF();
    Serial2.print("tStatus.pic=15"); // ON state
    sendFF();
    Serial2.print("pAutoProcess.pic=15"); // ON state
    sendFF();
    
    // Debug jadwal
    DateTime now = rtc.now();
    int targetTime = autoHour1 * 10 + autoHour2;
    int targetMinute = autoMin1 * 10 + autoMin2;
    int totalTargetMinutes = targetTime * 60 + targetMinute;
    int preFillTime = totalTargetMinutes - 180;
    if (preFillTime < 0) preFillTime += 1440;
    
    Serial.print(">> Auto Mode Activated - Target: ");
    Serial.print(targetTime);
    Serial.print(":");
    if (targetMinute < 10) Serial.print("0");
    Serial.print(targetMinute);
    Serial.print(" | Pre-fill at: ");
    Serial.print(preFillTime / 60);
    Serial.print(":");
    if (preFillTime % 60 < 10) Serial.print("0");
    Serial.print(preFillTime % 60);
    Serial.println(" (3 hours before)");
}


  else if (command.indexOf("AUTO_OFF") >= 0) {
    Serial.println(">> DEBUG: AUTO_OFF command detected");
    autoMode = false;
    circulationActive = false;
    
    // Reset semua state proses
    fillingActive = false;
    fillingStarted = false;
    fillingStage = 0;
    drainingActive = false;
    graceActive = false;
    flowOK = false;
    
    // Matikan semua aktuator
    digitalWrite(VALVE_DRAIN_PIN, LOW);
    digitalWrite(VALVE_INLET_PIN, LOW);
    digitalWrite(COMPRESSOR_PIN, LOW);
    digitalWrite(PUMP_UV_PIN, LOW);
    
    // Reset status
    statusKompresor = false;
    sistemAktif = false;
    receivingAutoParams = false;
    paramIndex = 0;
    manualCirculationOff = false;
    manualCirculationOn = false;  // Reset flag manual circulation
    
    // Feedback ke Nextion - Auto OFF (pic 14)
    Serial2.print("tStatus.txt=\"Manual Mode\"");
    sendFF();
    Serial2.print("tStatus.pic=0"); // Default status pic
    sendFF();
    Serial2.print("pAutoProcess.pic=14"); // OFF state
    sendFF();
    
    // Reset gambar tombol
    Serial2.print("pFillingMan.pic=6"); // OFF state
    sendFF();
    Serial2.print("pCoolingMan.pic=8"); // OFF state
    sendFF();
    Serial2.print("pDrainingMan.pic=10"); // OFF state
    sendFF();
  }
  // Tambahkan di dalam fungsi processNextionCommand()
  else if (command.indexOf("CIRCULATION_ON") >= 0) {
      Serial.println(">> DEBUG: CIRCULATION_ON command detected");
    
      // Hanya aktifkan jika mode auto aktif
      if (autoMode) {
        startManualCirculation();
      } else {
        if (debug) Serial.println(">> Manual circulation ignored - auto mode not active");
      }
  }

  else if (command.indexOf("CIRCULATION_OFF") >= 0) {
    Serial.println(">> DEBUG: CIRCULATION_OFF command detected");
    if (autoMode && circulationActive) {
      stopCirculation();
      manualCirculationOff = true;
    }
  }
  else if (receivingAutoParams) {
    // Simpan parameter
    autoParams[paramIndex] = command;
    paramIndex++;
    if (debug) {
      Serial.print(">> Received auto param ");
      Serial.print(paramIndex);
      Serial.print(": '");
      Serial.print(command);
      Serial.println("'");
    }
    
    // Jika sudah menerima semua parameter (6 parameter)
    if (paramIndex >= 6) {
      processAutoParameters();
      receivingAutoParams = false;
      paramIndex = 0;
    }
  }
}

// Di nextion.ino - bacaKomponenWaktu()
int bacaKomponenWaktu(String namaKomponen, int minVal, int maxVal) {
    // Flush buffer
    while (Serial2.available()) Serial2.read();
    
    Serial2.print("get ");
    Serial2.print(namaKomponen);
    Serial2.print(".val");
    sendFF();
    delay(100);
    
    int value = 0; // Default value
    
    if (Serial2.available() >= 4) {
        uint8_t header = Serial2.read();
        if (header == 0x71) {
            value = Serial2.read();
            Serial2.read();
            Serial2.read();
            
            // Debug nilai yang dibaca
            if (debug) {
                Serial.print(">> Read ");
                Serial.print(namaKomponen);
                Serial.print(": ");
                Serial.println(value);
            }
            
            // Validasi nilai
            if (value < minVal || value > maxVal) {
                if (debug) {
                    Serial.print(">> Invalid ");
                    Serial.print(namaKomponen);
                    Serial.print(": ");
                    Serial.print(value);
                    Serial.print(", using default: ");
                    Serial.println(minVal);
                }
                value = minVal;
            }
        }
    } else {
        if (debug) {
            Serial.print(">> No data for ");
            Serial.println(namaKomponen);
        }
    }
    
    return value;
}

// Fungsi untuk membaca teks dari Nextion
String bacaTeksDariNextion(String namaKomponen) {
  // Flush buffer
  while (Serial2.available()) Serial2.read();
  
  Serial2.print("get ");
  Serial2.print(namaKomponen);
  Serial2.print(".txt");
  sendFF();
  delay(100);
  
  String result = "";
  
  // Baca data dari Nextion
  while (Serial2.available()) {
    char c = Serial2.read();
    if (c == 0xFF) {
      // Jika ada 0xFF, kita skip dan berhenti
      break;
    }
    result += c;
  }
  
  // Debug nilai yang dibaca
  if (debug) {
    Serial.print(">> Read ");
    Serial.print(namaKomponen);
    Serial.print(": '");
    Serial.print(result);
    Serial.println("'");
  }
  
  return result;
}

// Baca status dari Nextion
int bacaStatusDariNextion() {
  // Flush buffer
  while (Serial2.available()) Serial2.read();
  
  Serial2.print("get activeProcess.val");
  sendFF();
  delay(100);
  
  int value = cachedActiveProcess != -1 ? cachedActiveProcess : 0;
  
  if (Serial2.available() >= 4) {
    uint8_t header = Serial2.read();
    if (header == 0x71) {
      value = Serial2.read();
      Serial2.read();
      Serial2.read();
    }
  }
  
  return value;
}

// Baca target suhu dari Nextion
int bacaTargetDariNextion() {
  // Flush buffer
  while (Serial2.available()) Serial2.read();
  
  Serial2.print("get nTargetTempMan.val");
  sendFF();
  delay(100);
  
  int value = cachedSuhuTarget != -1 ? cachedSuhuTarget : lastValidTarget;
  
  if (Serial2.available() >= 4) {
    uint8_t header = Serial2.read();
    if (header == 0x71) {
      uint8_t lowByte = Serial2.read();
      uint8_t highByte = Serial2.read();
      Serial2.read();
      int rawValue = lowByte | (highByte << 8);
      
      // Debug raw value
      if (debug && rawValue != value) {
        Serial.print(">> Raw target dari Nextion: ");
        Serial.print(rawValue);
        Serial.print(" (0x");
        Serial.print(rawValue, HEX);
        Serial.println(")");
      }
      
      // Gunakan hanya jika dalam range yang wajar
      if (rawValue >= 5 && rawValue <= 50) {
        value = rawValue;
      } else {
        if (debug) {
          Serial.print(">> Raw value diluar range, menggunakan cached: ");
          Serial.println(value);
        }
      }
    }
  }
  
  return value;
}

// Baca parameter dari Nextion
void bacaParameterDariNextion() {
  // Baca status hanya jika perlu
  int newActiveProcess = bacaStatusDariNextion();
  if (newActiveProcess != cachedActiveProcess) {
    activeProcess = newActiveProcess;
    cachedActiveProcess = newActiveProcess;
    
    if (debug) {
      Serial.print(">> Status berubah ke ");
      Serial.println(activeProcess);
    }
  }
  
  // Baca target dengan validasi dan debouncing
  int newSuhuTarget = bacaTargetDariNextion();
  
  // Validasi target suhu (harus dalam range wajar)
  if (newSuhuTarget >= 1 && newSuhuTarget <= 70) {
    if (newSuhuTarget != cachedSuhuTarget) {
      // Debouncing - cegah perubahan terlalu cepat
      unsigned long now = millis();
      if (now - lastTargetChange >= TARGET_CHANGE_DEBOUNCE) {
        
        // Jika target berubah saat sistem aktif, reset ke mode initial
        if (sistemAktif && newSuhuTarget != suhuTarget) {
          initialCoolingMode = true;
          targetReached = false;
          if (debug) Serial.println(">> TARGET BERUBAH - RESET KE INITIAL COOLING MODE");
        }
        
        lastValidTarget = newSuhuTarget;
        suhuTarget = newSuhuTarget;
        cachedSuhuTarget = newSuhuTarget;
        lastTargetChange = now;
        
        if (debug) {
          Serial.print(">> Target suhu berubah ke ");
          Serial.print(suhuTarget);
          Serial.println("°C");
        }
      } else {
        if (debug) {
          Serial.print(">> Target change ignored (debounce): ");
          Serial.print(newSuhuTarget);
          Serial.println("°C");
        }
      }
    }
  } else {
    // Target tidak valid - gunakan nilai terakhir yang valid
    if (debug && newSuhuTarget != cachedSuhuTarget) {
      Serial.print(">> Target tidak valid diabaikan: ");
      Serial.print(newSuhuTarget);
      Serial.print("°C, menggunakan: ");
      Serial.print(lastValidTarget);
      Serial.println("°C");
    }
    
    // Reset ke nilai valid terakhir
    suhuTarget = lastValidTarget;
    cachedSuhuTarget = lastValidTarget;
  }
}

// Kirim data ke Nextion
void updateNextionDisplay() {
  // Update temperature
  Serial2.print("nTemp.val=");
  Serial2.print((int)currentTemp);
  sendFF();
  
  // Update TDS
  Serial2.print("nTDS.val=");
  Serial2.print((int)tdsValue);
  sendFF();
  
  // Update flow rate
  noInterrupts();
  unsigned long pulses = pulseCount;
  pulseCount = 0;
  interrupts();
  float flowRate = pulses / 7.5;
  char flowStr[10];
  dtostrf(flowRate, 4, 1, flowStr);
  Serial2.print("tFlow.txt=\"");
  Serial2.print(flowStr);
  Serial2.print("\"");
  sendFF();
}

// Fungsi untuk memproses parameter auto yang diterima
void processAutoParameters() {
  // Debug parameter yang diterima
  if (debug) {
    Serial.println(">> Processing auto parameters:");
    for (int i = 0; i < 6; i++) {
      Serial.print(" Param ");
      Serial.print(i+1);
      Serial.print(": '");
      Serial.print(autoParams[i]);
      Serial.println("'");
    }
  }
  
  // Parameter 1: Jam digit pertama
  if (autoParams[0].length() > 0) {
    autoHour1 = autoParams[0].toInt();
    if (autoHour1 < 0 || autoHour1 > 2) autoHour1 = 0;
  } else {
    autoHour1 = 1;
  }
  
  // Parameter 2: Jam digit kedua
  if (autoParams[1].length() > 0) {
    autoHour2 = autoParams[1].toInt();
    if (autoHour2 < 0 || autoHour2 > 9) autoHour2 = 0;
  } else {
    autoHour2 = 1;
  }
  
  // Parameter 3: Menit digit pertama
  if (autoParams[2].length() > 0) {
    autoMin1 = autoParams[2].toInt();
    if (autoMin1 < 0 || autoMin1 > 5) autoMin1 = 0;
  } else {
    autoMin1 = 0;
  }
  
  // Parameter 4: Menit digit kedua
  if (autoParams[3].length() > 0) {
    autoMin2 = autoParams[3].toInt();
    if (autoMin2 < 0 || autoMin2 > 9) autoMin2 = 0;
  } else {
    autoMin2 = 0;
  }
  
  // Parameter 5: Suhu
  if (autoParams[4].length() > 0) {
    String tempStr = autoParams[4];
    tempStr.replace("°C", "");
    tempStr.replace(" ", "");
    autoTempTarget = tempStr.toInt();
    if (autoTempTarget < 1 || autoTempTarget > 30) autoTempTarget = 3;
  } else {
    autoTempTarget = 2;
  }
  
  // Parameter 6: Hari
  if (autoParams[5].length() > 0) {
    String dayStr = autoParams[5];
    dayStr.replace("hari", "");
    dayStr.replace(" ", "");
    autoChangeDay = dayStr.toInt();
    if (autoChangeDay < 1 || autoChangeDay > 365) autoChangeDay = 7;
  } else {
    autoChangeDay = 5;
  }
  
  if (debug) {
    Serial.print(">> Final Auto Settings - Time: ");
    Serial.print(autoHour1);
    Serial.print(autoHour2);
    Serial.print(":");
    Serial.print(autoMin1);
    Serial.print(autoMin2);
    Serial.print(" | Temp: ");
    Serial.print(autoTempTarget);
    Serial.print("°C | Change every: ");
    Serial.print(autoChangeDay);
    Serial.println(" days");
  }
}