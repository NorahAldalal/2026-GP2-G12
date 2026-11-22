<p align="center">
  <img src="siraj/Siraj.jpg" width="70%">
</p>

# Siraj

## About the Project

Siraj is a smart IoT and Machine Learning-based street lighting system designed to monitor and reduce urban light pollution while optimizing energy consumption. The system combines ESP32 microcontrollers, environmental and radar-based activity sensors, and an **XGBoost regression model** to dynamically predict and adjust streetlight brightness (`PWM` duty cycle) based on ambient illuminance and real-time pedestrian and vehicle activity.

*This project aims to support Saudi Vision 2030's environmental sustainability goals by reducing light pollution and energy waste in urban lighting infrastructure.*

## Key Features

- **ML-Driven Adaptive Dimming:** Predicts optimal PWM brightness levels using a trained XGBoost regression model backed by runtime daylight gating and safety bounds.
- **Multi-Modal Activity & Kinematics Detection:** Distinguishes idle, pedestrian, and vehicle states—factoring in distance, speed, and approach direction—for responsive illumination.
- **Real-Time Light Pollution & Sky-Glow Monitoring:** Tracks ambient illuminance, upward sky-sensor frequency, and live streetlight telemetry.
- **Interactive Web Dashboard & City Map:** Visualizes streetlight status, zone pollution levels, fault reports, and recorded field experiment benchmarks (`45.84%` sky-sensor signal reduction vs. `100% PWM`).
- **Multi-Zone Management & Fault Tracking:** Supports area-based monitoring, maintenance workflows, and extensible weekly analytics.
- **Auto-Failure Compensation:** Enhances lighting reliability across connected streetlight nodes.

## Technologies Used

### Software & Machine Learning
- Python (XGBoost, Scikit-learn, Pandas, NumPy)
- PHP
- MySQL / MariaDB (phpMyAdmin)
- HTML, CSS, JavaScript
- Arduino IDE

### Hardware
- ESP32 Microcontroller
- PWM-Controlled LED Streetlight
- Radar & Presence Sensors (Human & Vehicle Detection, Distance, Speed, and Approach Tracking)
- Ambient Light Sensor (Lux)
- Sky-Glow Light-to-Frequency Sensor (Hz)
- GPS Module

## Repository Structure

```
├── README.md
└── siraj/
    ├── config/db.example.php            # copy to config/db.php and fill in your settings
    ├── siraj_cloud_config.example.php   # copy to siraj_cloud_config.php
    ├── database/siraj.sql               # database schema + seed data
    ├── firmware/siraj_motion_detection/ # ESP32 Arduino code
    ├── api/  includes/  cron/  assets/  lang/
    └── *.php                            # dashboard pages (admin / employee)
```

## How to Run the Project
1. Clone the repository
2. Create a MySQL database and import `siraj/database/siraj.sql`
3. Copy `siraj/config/db.example.php` to `siraj/config/db.php` and enter your database, SMTP, and site URL settings
4. Copy `siraj/siraj_cloud_config.example.php` to `siraj/siraj_cloud_config.php` and enter the cloud API URL and read key
5. Run the `siraj` folder on a local server (e.g., MAMP or XAMPP)
6. Log in at `/login.php` using one of the accounts below
7. Open `siraj/firmware/siraj_motion_detection/siraj_motion_detection.ino` in Arduino IDE, set your WiFi name, password, and server address, then upload it to the ESP32
8. Make sure the ESP32 is connected to WiFi and can reach the server

**Login accounts** (created by `siraj.sql`):

| Role | Email | Password |
|---|---|---|
| Admin | `admin@siraj.city` | `password` |
| Employee | `employee@siraj.city` | `Em@98765` |

> `config/db.php` and `siraj_cloud_config.php` contain secrets and are excluded by `.gitignore`. Never commit them.

## Team
 
**Institution:** King Saud University - CCIS  
**Course:** IT 496 - Graduation Project   
**Students:** <br>
Norah Abdulrahman Aldalal <br>
Aseel Abdulaziz Almubaddel <br>
Deema Abdullah Alfarhoud <br>
Reema Abdulaziz Alnajim

## Supervisor
Dr. Ameera Masoud Almasoudd
