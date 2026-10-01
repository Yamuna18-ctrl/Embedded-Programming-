// ================================================================
// INTELLIGENT GAS MONITORING SYSTEM
// Environment-Compensated Gas Leakage Detection Using ESP32
// ================================================================
//
// Hardware:
// DHT11       -> GPIO33
// MQ-2 A0     -> GPIO34
// MQ-2 D0     -> GPIO27
// LCD SDA     -> GPIO26
// LCD SCL     -> GPIO25
// Buzzer +    -> GPIO32
// Buzzer -    -> GND
//
// Blynk:
// V0 = Temperature
// V1 = Humidity
// V2 = Raw MQ2
// V3 = Compensation Factor
// V4 = Corrected Gas
// V5 = Gas Index
// V6 = Gas Status
// V7 = Buzzer
//
// ================================================================

#define BLYNK_PRINT Serial

#define BLYNK_TEMPLATE_ID "Yamuna P"
#define BLYNK_TEMPLATE_NAME "Intelligent Gas Monitoring"
#define BLYNK_AUTH_TOKEN "jzrddrPA2n-V5bu33NSXIioJYccC3EwU"

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>

// ================================================================
// WIFI DETAILS
// ================================================================

char ssid[] = "Yamuna";
char pass[] = "yamuna123";

// ================================================================
// PIN DEFINITIONS
// ================================================================

#define DHT_PIN       33
#define DHT_TYPE      DHT11

#define MQ2_A0        34
#define MQ2_D0        27

#define LCD_SDA       26
#define LCD_SCL       25

#define BUZZER_PIN    32

// ================================================================
// OBJECTS
// ================================================================

DHT dht(DHT_PIN, DHT_TYPE);

LiquidCrystal_I2C lcd(0x27, 16, 2);

// ================================================================
// ENVIRONMENT COMPENSATION SETTINGS
// ================================================================

const float REFERENCE_TEMP = 31.0;
const float REFERENCE_HUM  = 45.0;

const float TEMP_COEFF = 0.008;
const float HUM_COEFF  = 0.003;

const float MIN_COMPENSATION = 0.70;
const float MAX_COMPENSATION = 1.30;

// ================================================================
// GAS THRESHOLDS
// ================================================================

// Demo thresholds
const float WARNING_INDEX = 92.0;
const float DANGER_INDEX  = 105.0;

// ================================================================
// CALIBRATION SETTINGS
// ================================================================

const int CALIBRATION_SAMPLES = 20;

const unsigned long WARMUP_TIME =
  30000UL;

const unsigned long CALIBRATION_INTERVAL =
  2500UL;

// ================================================================
// VARIABLES
// ================================================================

float temperature = 0.0;
float humidity = 0.0;

float rawGas = 0.0;
float correctedGas = 0.0;

float compensationFactor = 1.0;

float baselineGas = 1.0;

float gasIndex = 0.0;

int mq2Digital = HIGH;

String gasStatus = "SAFE";

bool buzzerState = false;

bool calibrationComplete = false;

int calibrationCount = 0;

unsigned long startTime = 0;
unsigned long lastCalibrationTime = 0;

unsigned long lastSensorRead = 0;
unsigned long lastBlynkSend = 0;

unsigned long lastBuzzerTime = 0;
unsigned long lastLCDChange = 0;

unsigned long lastWiFiAttempt = 0;

int lcdPage = 0;

// ================================================================
// WIFI / BLYNK SETTINGS
// ================================================================

const unsigned long WIFI_RETRY_INTERVAL = 10000UL;

// ================================================================
// FUNCTION DECLARATIONS
// ================================================================

void readSensors();

void calculateCompensation();

void calculateGasIndex();

void determineGasStatus();

void controlBuzzer();

void updateLCD();

void sendToBlynk();

void connectWiFi();

void calibrateBaseline();

// ================================================================
// SETUP
// ================================================================

void setup()
{
  Serial.begin(115200);

  delay(1000);

  Serial.println();
  Serial.println("======================================");
  Serial.println("INTELLIGENT GAS MONITORING SYSTEM");
  Serial.println("======================================");

  // ------------------------------------------------
  // PIN SETUP
  // ------------------------------------------------

  pinMode(MQ2_D0, INPUT);

  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(BUZZER_PIN, LOW);

  // ------------------------------------------------
  // I2C SETUP
  // ------------------------------------------------

  Wire.begin(LCD_SDA, LCD_SCL);

  // ------------------------------------------------
  // LCD SETUP
  // ------------------------------------------------

  lcd.init();

  lcd.backlight();

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("INTELLIGENT GAS");

  lcd.setCursor(0, 1);
  lcd.print("MONITORING SYSTEM");

  delay(2000);

  // ------------------------------------------------
  // DHT SETUP
  // ------------------------------------------------

  dht.begin();

  // ------------------------------------------------
  // WIFI SETUP
  // ------------------------------------------------

  WiFi.mode(WIFI_STA);

  WiFi.setAutoReconnect(true);

  WiFi.begin(ssid, pass);

  Serial.println();
  Serial.println("Connecting to WiFi...");

  // ------------------------------------------------
  // NON-BLOCKING WIFI CHECK
  // ------------------------------------------------

  int wifiAttempts = 0;

  while (WiFi.status() != WL_CONNECTED &&
         wifiAttempts < 10)
  {
    delay(500);

    Serial.print(".");

    wifiAttempts++;
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("WiFi CONNECTED!");

    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    Serial.print("Signal Strength: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");

    // ------------------------------------------------
    // BLYNK CONFIGURATION
    // ------------------------------------------------

    Blynk.config(BLYNK_AUTH_TOKEN);

    Serial.println("Connecting to Blynk...");

    if (Blynk.connect(3000))
    {
      Serial.println("Blynk CONNECTED!");
    }
    else
    {
      Serial.println("Blynk not connected.");
      Serial.println("System will continue without Blynk.");
    }
  }
  else
  {
    Serial.println("WiFi NOT CONNECTED.");

    Serial.println("System will continue offline.");

    Serial.println("Sensors, LCD and buzzer will still work.");
  }

  // ------------------------------------------------
  // START TIMER
  // ------------------------------------------------

  startTime = millis();

  Serial.println();
  Serial.println("System starting...");
  Serial.println("MQ-2 warm-up in progress...");

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("MQ2 WARM-UP");

  lcd.setCursor(0, 1);
  lcd.print("Please wait...");

  // ------------------------------------------------
  // CALIBRATION VARIABLES
  // ------------------------------------------------

  calibrationComplete = false;

  calibrationCount = 0;

  baselineGas = 1.0;
}

// ================================================================
// LOOP
// ================================================================

void loop()
{
  // ------------------------------------------------
  // BLYNK RUN ONLY IF CONNECTED
  // ------------------------------------------------

  if (Blynk.connected())
  {
    Blynk.run();
  }

  // ------------------------------------------------
  // WIFI / BLYNK RECONNECTION
  // ------------------------------------------------

  if (millis() - lastWiFiAttempt >= WIFI_RETRY_INTERVAL)
  {
    lastWiFiAttempt = millis();

    // ----------------------------------------------
    // WIFI RECONNECT
    // ----------------------------------------------

    if (WiFi.status() != WL_CONNECTED)
    {
      Serial.println();
      Serial.println("Trying WiFi reconnect...");

      WiFi.disconnect();

      WiFi.begin(ssid, pass);
    }

    // ----------------------------------------------
    // BLYNK RECONNECT
    // ----------------------------------------------

    if (WiFi.status() == WL_CONNECTED &&
        !Blynk.connected())
    {
      Serial.println("Trying Blynk reconnect...");

      Blynk.connect(1000);
    }
  }

  // ------------------------------------------------
  // MQ2 WARM-UP
  // ------------------------------------------------

  if (!calibrationComplete)
  {
    if (millis() - startTime < WARMUP_TIME)
    {
      // Still warming up

      return;
    }

    // ------------------------------------------------
    // CALIBRATION
    // ------------------------------------------------

    calibrateBaseline();

    return;
  }

  // ------------------------------------------------
  // SENSOR READING EVERY 2 SECONDS
  // ------------------------------------------------

  if (millis() - lastSensorRead >= 2000)
  {
    lastSensorRead = millis();

    readSensors();

    calculateCompensation();

    calculateGasIndex();

    determineGasStatus();

    controlBuzzer();

    updateLCD();

    sendToBlynk();

    // ------------------------------------------------
    // SERIAL MONITOR
    // ------------------------------------------------

    Serial.println();
    Serial.println("--------------------------------------");

    Serial.print("Temperature      : ");
    Serial.print(temperature, 1);
    Serial.println(" °C");

    Serial.print("Humidity         : ");
    Serial.print(humidity, 1);
    Serial.println(" %");

    Serial.print("Raw MQ2 ADC      : ");
    Serial.println(rawGas, 1);

    Serial.print("Compensation     : ");
    Serial.println(compensationFactor, 2);

    Serial.print("Corrected Gas ADC: ");
    Serial.println(correctedGas, 1);

    Serial.print("Gas Index        : ");
    Serial.print(gasIndex, 1);
    Serial.println(" % of baseline");

    Serial.print("MQ2 D0           : ");

    if (mq2Digital == LOW)
    {
      Serial.println("LOW");
    }
    else
    {
      Serial.println("HIGH");
    }

    Serial.print("Gas Status       : ");
    Serial.println(gasStatus);

    Serial.print("Buzzer           : ");

    if (buzzerState)
    {
      Serial.println("ON");
    }
    else
    {
      Serial.println("OFF");
    }

    Serial.print("WiFi Status      : ");

    if (WiFi.status() == WL_CONNECTED)
    {
      Serial.println("CONNECTED");
    }
    else
    {
      Serial.println("DISCONNECTED");
    }

    Serial.print("Blynk Status     : ");

    if (Blynk.connected())
    {
      Serial.println("CONNECTED");
    }
    else
    {
      Serial.println("DISCONNECTED");
    }

    Serial.println("--------------------------------------");
  }
}

// ================================================================
// READ SENSORS
// ================================================================

void readSensors()
{
  // ------------------------------------------------
  // DHT11
  // ------------------------------------------------

  float newTemperature = dht.readTemperature();

  float newHumidity = dht.readHumidity();

  if (!isnan(newTemperature))
  {
    temperature = newTemperature;
  }

  if (!isnan(newHumidity))
  {
    humidity = newHumidity;
  }

  // ------------------------------------------------
  // MQ2
  // ------------------------------------------------

  rawGas = analogRead(MQ2_A0);

  // ------------------------------------------------
  // MQ2 DIGITAL OUTPUT
  // ------------------------------------------------

  mq2Digital = digitalRead(MQ2_D0);
}

// ================================================================
// CALCULATE ENVIRONMENT COMPENSATION
// ================================================================

void calculateCompensation()
{
  float tempDifference =
    temperature - REFERENCE_TEMP;

  float humidityDifference =
    humidity - REFERENCE_HUM;

  compensationFactor =
    1.0 +
    (TEMP_COEFF * tempDifference) +
    (HUM_COEFF * humidityDifference);

  // ------------------------------------------------
  // LIMIT COMPENSATION
  // ------------------------------------------------

  if (compensationFactor < MIN_COMPENSATION)
  {
    compensationFactor =
      MIN_COMPENSATION;
  }

  if (compensationFactor > MAX_COMPENSATION)
  {
    compensationFactor =
      MAX_COMPENSATION;
  }

  // ------------------------------------------------
  // CORRECTED GAS VALUE
  // ------------------------------------------------

  correctedGas =
    rawGas / compensationFactor;
}

// ================================================================
// CALCULATE GAS INDEX
// ================================================================

void calculateGasIndex()
{
  if (baselineGas <= 0)
  {
    baselineGas = 1.0;
  }

  gasIndex =
    (correctedGas / baselineGas) * 100.0;

  // Prevent negative values

  if (gasIndex < 0)
  {
    gasIndex = 0;
  }
}

// ================================================================
// DETERMINE GAS STATUS
// ================================================================

void determineGasStatus()
{
  if (gasIndex < WARNING_INDEX)
  {
    gasStatus = "SAFE";
  }

  else if (gasIndex < DANGER_INDEX)
  {
    gasStatus = "WARNING";
  }

  else
  {
    gasStatus = "DANGER";
  }
}

// ================================================================
// BUZZER CONTROL
// ================================================================

void controlBuzzer()
{
  // ------------------------------------------------
  // SAFE
  // ------------------------------------------------

  if (gasStatus == "SAFE")
  {
    digitalWrite(BUZZER_PIN, LOW);

    buzzerState = false;
  }

  // ------------------------------------------------
  // WARNING
  // ------------------------------------------------

  else if (gasStatus == "WARNING")
  {
    // Beep every 500 ms

    if (millis() - lastBuzzerTime >= 500)
    {
      lastBuzzerTime = millis();

      buzzerState = !buzzerState;

      digitalWrite(
        BUZZER_PIN,
        buzzerState ? HIGH : LOW
      );
    }
  }

  // ------------------------------------------------
  // DANGER
  // ------------------------------------------------

  else if (gasStatus == "DANGER")
  {
    digitalWrite(BUZZER_PIN, HIGH);

    buzzerState = true;
  }
}

// ================================================================
// LCD UPDATE
// ================================================================

void updateLCD()
{
  // Change LCD page every 3 seconds

  if (millis() - lastLCDChange >= 3000)
  {
    lastLCDChange = millis();

    lcdPage++;

    if (lcdPage > 2)
    {
      lcdPage = 0;
    }
  }

  lcd.clear();

  // ==============================================================
  // PAGE 0
  // ==============================================================

  if (lcdPage == 0)
  {
    lcd.setCursor(0, 0);

    lcd.print("T:");
    lcd.print(temperature, 1);

    lcd.print((char)223);
    lcd.print("C ");

    lcd.print("H:");
    lcd.print(humidity, 0);
    lcd.print("%");

    lcd.setCursor(0, 1);

    lcd.print("Comp:");
    lcd.print(compensationFactor, 2);
  }

  // ==============================================================
  // PAGE 1
  // ==============================================================

  else if (lcdPage == 1)
  {
    lcd.setCursor(0, 0);

    lcd.print("Raw:");
    lcd.print(rawGas, 0);

    lcd.setCursor(0, 1);

    lcd.print("Corr:");
    lcd.print(correctedGas, 0);
  }

  // ==============================================================
  // PAGE 2
  // ==============================================================

  else if (lcdPage == 2)
  {
    lcd.setCursor(0, 0);

    lcd.print("Gas:");
    lcd.print(gasStatus);

    lcd.setCursor(0, 1);

    lcd.print("Buzzer:");

    if (gasStatus == "WARNING")
    {
      lcd.print("BEEP");
    }

    else if (gasStatus == "DANGER")
    {
      lcd.print("ON ");
    }

    else
    {
      lcd.print("OFF");
    }
  }
}

// ================================================================
// BLYNK DATA
// ================================================================

void sendToBlynk()
{
  // Only send if Blynk is connected

  if (!Blynk.connected())
  {
    return;
  }

  // ------------------------------------------------
  // V0 - TEMPERATURE
  // ------------------------------------------------

  Blynk.virtualWrite(
    V0,
    temperature
  );

  // ------------------------------------------------
  // V1 - HUMIDITY
  // ------------------------------------------------

  Blynk.virtualWrite(
    V1,
    humidity
  );

  // ------------------------------------------------
  // V2 - RAW MQ2
  // ------------------------------------------------

  Blynk.virtualWrite(
    V2,
    rawGas
  );

  // ------------------------------------------------
  // V3 - COMPENSATION FACTOR
  // ------------------------------------------------

  Blynk.virtualWrite(
    V3,
    compensationFactor
  );

  // ------------------------------------------------
  // V4 - CORRECTED GAS
  // ------------------------------------------------

  Blynk.virtualWrite(
    V4,
    correctedGas
  );

  // ------------------------------------------------
  // V5 - GAS INDEX
  // ------------------------------------------------

  Blynk.virtualWrite(
    V5,
    gasIndex
  );

  // ------------------------------------------------
  // V6 - GAS STATUS
  // ------------------------------------------------

  int statusValue = 0;

  if (gasStatus == "WARNING")
  {
    statusValue = 1;
  }

  else if (gasStatus == "DANGER")
  {
    statusValue = 2;
  }

  Blynk.virtualWrite(
    V6,
    statusValue
  );

  // ------------------------------------------------
  // V7 - BUZZER
  // ------------------------------------------------

  Blynk.virtualWrite(
    V7,
    buzzerState ? 1 : 0
  );
}

// ================================================================
// BASELINE CALIBRATION
// ================================================================

void calibrateBaseline()
{
  // ------------------------------------------------
  // Read MQ2
  // ------------------------------------------------

  float reading =
    analogRead(MQ2_A0);

  // ------------------------------------------------
  // First calibration sample
  // ------------------------------------------------

  if (calibrationCount == 0)
  {
    calibrationCount = 1;

    baselineGas = reading;

    lastCalibrationTime = millis();

    Serial.println();
    Serial.println("Starting clean-air calibration...");

    Serial.print("Sample 1/");
    Serial.println(CALIBRATION_SAMPLES);

    return;
  }

  // ------------------------------------------------
  // Wait between samples
  // ------------------------------------------------

  if (millis() - lastCalibrationTime <
      CALIBRATION_INTERVAL)
  {
    return;
  }

  lastCalibrationTime = millis();

  // ------------------------------------------------
  // Running average
  // ------------------------------------------------

  baselineGas =
    ((baselineGas * calibrationCount) + reading)
    /
    (calibrationCount + 1);

  calibrationCount++;

  Serial.print("Calibration sample ");
  Serial.print(calibrationCount);
  Serial.print("/");
  Serial.print(CALIBRATION_SAMPLES);
  Serial.print(" : ");
  Serial.println(reading);

  // ------------------------------------------------
  // Calibration complete
  // ------------------------------------------------

  if (calibrationCount >= CALIBRATION_SAMPLES)
  {
    calibrationComplete = true;

    Serial.println();
    Serial.println("======================================");
    Serial.println("CALIBRATION COMPLETE");
    Serial.println("======================================");

    Serial.print("Baseline Gas: ");
    Serial.println(baselineGas, 2);

    Serial.println();

    lcd.clear();

    lcd.setCursor(0, 0);
    lcd.print("CALIBRATION");

    lcd.setCursor(0, 1);
    lcd.print("COMPLETE");

    delay(2000);
  }
}