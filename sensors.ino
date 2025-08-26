// Variabel sensor
unsigned long lastFlowRead = 0;

// TAMBAHKAN: Variable untuk mode setting waktu
bool timeSettingMode = true;
int timeSettingStep = 0; // 0=waiting, 1=setting hour, 2=setting minute
int tempHour = 0;
int tempMinute = 0;

// Setup sensor
void setupSensors() {
  // Setup pin sensor
  pinMode(FLOAT_SENSOR_PIN, INPUT_PULLUP);
  pinMode(FLOW_SENSOR_PIN, INPUT_PULLUP);
  pinMode(TDS_SENSOR_PIN, INPUT);
  
  // Setup aktuator
  pinMode(VALVE_DRAIN_PIN, OUTPUT);
  pinMode(VALVE_INLET_PIN, OUTPUT);
  pinMode(COMPRESSOR_PIN, OUTPUT);
  pinMode(PUMP_UV_PIN, OUTPUT);
  
  // Setup interrupt flow sensor
  attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR_PIN), flowISR, RISING);
  
  // Setup temperature sensor
  sensors.begin();
  
  // Set semua output ke LOW
  digitalWrite(VALVE_DRAIN_PIN, LOW);
  digitalWrite(VALVE_INLET_PIN, LOW);
  digitalWrite(COMPRESSOR_PIN, LOW);
  digitalWrite(PUMP_UV_PIN, LOW);
}

// Interrupt service routine untuk flow sensor
void IRAM_ATTR flowISR() {
  pulseCount++;
}

// TAMBAHKAN: Fungsi untuk setting waktu via Serial Monitor
void handleTimeSetting() {
  if (Serial.available()) {
    String input = Serial.readString();
    input.trim();
    
    if (input.equalsIgnoreCase("SETTIME")) {
      timeSettingMode = true;
      timeSettingStep = 1;
      Serial.println(">> TIME SETTING MODE ACTIVATED");
      Serial.println(">> Enter hour (0-23):");
      return;
    }
    
    if (timeSettingMode) {
      int value = input.toInt();
      
      switch (timeSettingStep) {
        case 1: // Setting hour
          if (value >= 0 && value <= 23) {
            tempHour = value;
            timeSettingStep = 2;
            Serial.println(">> Hour set to: " + String(tempHour));
            Serial.println(">> Enter minute (0-59):");
          } else {
            Serial.println(">> Invalid hour! Enter hour (0-23):");
          }
          break;
          
        case 2: // Setting minute
          if (value >= 0 && value <= 59) {
            tempMinute = value;
            timeSettingStep = 3;
            Serial.println(">> Minute set to: " + String(tempMinute));
            Serial.println(">> Confirm time " + String(tempHour) + ":" + 
                         (tempMinute < 10 ? "0" : "") + String(tempMinute) + "? (Y/N)");
          } else {
            Serial.println(">> Invalid minute! Enter minute (0-59):");
          }
          break;
          
        case 3: // Confirmation
          if (input.equalsIgnoreCase("Y") || input.equalsIgnoreCase("YES")) {
            // Set waktu RTC
            DateTime now = rtc.now();
            rtc.adjust(DateTime(now.year(), now.month(), now.day(), 
                              tempHour, tempMinute, 0));
            Serial.println(">> Time successfully set to: " + 
                          String(tempHour) + ":" + 
                          (tempMinute < 10 ? "0" : "") + String(tempMinute));
            timeSettingMode = false;
            timeSettingStep = 0;
          } else if (input.equalsIgnoreCase("N") || input.equalsIgnoreCase("NO")) {
            Serial.println(">> Time setting cancelled");
            timeSettingMode = false;
            timeSettingStep = 0;
          } else {
            Serial.println(">> Please enter Y/YES or N/NO");
          }
          break;
      }
    }
  }
}

// Baca semua sensor
void readSensors() {
  // TAMBAHKAN: Handle time setting
  handleTimeSetting();
  
  // Baca suhu
  sensors.requestTemperatures();
  currentTemp = sensors.getTempCByIndex(0);
  
  // Validasi suhu
  if (currentTemp < -50 || currentTemp > 100) {
    if (debug) Serial.println(">> WARNING: Temperature reading out of range!");
    currentTemp = 25.0; // Gunakan nilai default jika reading tidak valid
  }
  
  // Baca flow rate setiap 1 detik
  if (millis() - lastFlowRead >= 1000) {
    // Flow rate dihitung di interrupt
    lastFlowRead = millis();
  }
  
  // Baca TDS
  int raw = analogRead(TDS_SENSOR_PIN);
  float voltage = raw * (3.3 / 4095.0);
  
  // Validasi voltage
  if (voltage < 0 || voltage > 3.3) {
    if (debug) Serial.println(">> WARNING: TDS voltage out of range!");
    voltage = 0.5; // Gunakan nilai default jika tidak valid
  }
  
  float ec = (133.42 * voltage * voltage * voltage
             - 255.86 * voltage * voltage
             + 857.39 * voltage);
  
  // Validasi EC
  if (ec < 0) {
    ec = 0;
  }
  
  tdsValue = ec * 0.5;

    //rtc waktu tampilan
    static unsigned long lastTimeUpdate = 0;
    if (millis() - lastTimeUpdate >= 100) {
        DateTime now = rtc.now();
        String timeString = "";
        if (now.hour() < 10) timeString += "0";
        timeString += String(now.hour());
        timeString += ":";
        if (now.minute() < 10) timeString += "0";
        timeString += String(now.minute());
        
        Serial2.print("tClock.txt=\"");
        Serial2.print(timeString);
        Serial2.print("\"");
        sendFF();
        
        lastTimeUpdate = millis();
    }
  
  // Update display Nextion
  updateNextionDisplay();
  
  // Debug sensor
  if (debug) {
    Serial.print(">> Sensor - Temp: ");
    Serial.print(currentTemp);
    Serial.print("°C | TDS: ");
    Serial.print(tdsValue);
    Serial.print(" | Flow: ");
    Serial.println(pulseCount / 7.5);
  }
  
  // TAMBAHKAN: Info mode setting waktu
  if (timeSettingMode) {
    static unsigned long lastPrompt = 0;
    if (millis() - lastPrompt >= 5000) { // Reminder setiap 5 detik
      switch (timeSettingStep) {
        case 1:
          Serial.println(">> TIME SETTING: Enter hour (0-23)");
          break;
        case 2:
          Serial.println(">> TIME SETTING: Enter minute (0-59)");
          break;
        case 3:
          Serial.println(">> TIME SETTING: Confirm time? (Y/N)");
          break;
      }
      lastPrompt = millis();
    }
  }
}