# Intelligent Gas Monitoring System
### Environment-Compensated Gas Leakage Detection Using ESP32

<p align="center">
  <b>An IoT-based gas monitoring system using ESP32, MQ-2 and DHT11 with environmental compensation</b>
</p>

---

## 📌 Project Overview

The **Intelligent Gas Monitoring System** is an embedded IoT-based prototype designed to monitor gas-related conditions while considering the effects of **temperature and humidity** on MQ-2 gas sensor readings.

Traditional low-cost gas monitoring systems may rely directly on raw gas sensor values. However, gas sensor responses can vary with environmental conditions such as temperature and humidity. This project addresses this issue by combining:

- **MQ-2 Gas Sensor** – provides the raw gas sensor response
- **DHT11 Sensor** – measures temperature and humidity
- **ESP32** – processes sensor data and performs environmental compensation
- **16×2 I2C LCD** – provides local monitoring
- **Buzzer** – provides local warning/alarm
- **Blynk IoT** – provides remote monitoring

The ESP32 calculates an environmental compensation factor and uses it to generate a **corrected gas value and gas index**, which are then used for gas-status determination and alert control.

---

## 🎯 Objectives

The main objectives of the project are:

1. Interface the MQ-2 gas sensor with the ESP32 and acquire raw sensor readings.
2. Measure temperature and humidity using the DHT11 sensor.
3. Calculate an environmental compensation factor.
4. Generate a corrected gas value and gas index.
5. Determine the gas status as **SAFE, WARNING, or DANGER**.
6. Control a buzzer based on the compensated gas condition.
7. Display monitoring information on a 16×2 I2C LCD.
8. Publish sensor and calculated values to the Blynk IoT dashboard.

---

## ❓ Problem Statement

Low-cost gas sensors can be affected by environmental conditions such as temperature and humidity. If raw sensor readings are used directly for decision-making, environmental variations may influence the readings and potentially contribute to false alarms.

The project therefore aims to develop an embedded gas-monitoring prototype that considers real-time environmental conditions while interpreting the MQ-2 sensor response.

---

## 💡 Proposed Solution

The proposed system follows this processing flow:

```text
        MQ-2 Gas Sensor
              │
              │ Raw Gas Reading
              ▼
        ┌─────────────┐
        │             │
        │    ESP32    │
        │             │
        └──────┬──────┘
               ▲
               │
       Temperature & Humidity
               │
             DHT11
               
               │
               ▼
    Environmental Compensation
               │
               ▼
       Corrected Gas Value
               │
               ▼
          Gas Index
               │
               ▼
       Gas Status Decision
        ┌──────┼──────┐
        ▼      ▼      ▼
       SAFE  WARNING DANGER
        │      │      │
        └──────┴──────┘
               │
        ┌──────┴─────────┐
        ▼                ▼
      Buzzer          LCD Display
                           │
                           ▼
                    Blynk IoT Dashboard
