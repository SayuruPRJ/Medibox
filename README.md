#  Medibox – ESP32-based IoT system

**Medibox** is an intelligent, IoT-based medicine storage solution powered by the **ESP32** microcontroller.  
It continuously monitors **temperature** and **light intensity** to maintain ideal storage conditions and features an automated **servo-controlled access system**.  
All sensor data and controls are integrated into a **Node-RED dashboard** via **MQTT**, enabling real-time monitoring and configuration.

This project runs entirely on **Wokwi** for simulation, allowing easy testing and demonstration without the need for physical hardware.

---

##  Key Features

-  **Temperature Monitoring** – Uses a **DHT22** sensor to track storage temperature.
-  **Light Intensity Measurement** – LDR sensor for monitoring light exposure.
-  **Automated Access** – Servo motor for secure and smart medicine dispensing.
-  **Live Dashboards** – Node-RED UI with gauges, charts, and controls.
-  **MQTT Connectivity** – Communicates via the public **HiveMQ** broker.
-  **Adjustable Parameters** – Modify sampling/sending intervals, target temperature, and more in real-time.
-  **Fully Simulated** – Runs on Wokwi with no additional hardware required.


##  File Overview

| File            | Description |
|-----------------|-------------|
| **main.cpp**        | Initializes hardware and coordinates module functions via `setup()` and `loop()` for the Medibox system. |
| **Alarm.cpp**       | Implements alarm setting, triggering, and viewing functionalities. |
| **Display.cpp**     | Handles OLED display operations such as clearing the screen and printing text. |
| **InputMenu.cpp**   | Handles button input processing and menu navigation for user interaction. |
| **MQTT.cpp**        | Manages MQTT connection, reconnection, and publishing of sensor data. |
| **Sensor.cpp**      | Processes sensor data from DHT22 and LDR for temperature, humidity, and light intensity. |
| **servocontrol.cpp**| Controls the servo motor based on calculated angles from sensor data. |
| **time.cpp**        | Manages time-related functions including updating time via NTP and setting time zones. |
| **NodeRed.json**    | Node-RED flow for UI and MQTT topics. |
| **diagram.json**    | Defines the circuit schematic and component connections for the Medibox system, enabling visualization in Wokwi. |
