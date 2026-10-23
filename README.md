# EcoPlant IOE 🌱
### Smart Plant Monitoring and Automated Irrigation System

EcoPlant IOE is a smart plant monitoring and irrigation project developed to make everyday plant care easier and more efficient. Instead of relying entirely on manual observation or fixed watering schedules, the system uses sensors to monitor plant and environmental conditions and helps manage irrigation accordingly.

The project combines hardware, cloud connectivity, and a web-based dashboard to bring plant monitoring and irrigation control into one place.

## About the Project

Taking care of plants requires regular attention to soil moisture, temperature, humidity, and water availability. Missing these conditions can lead to overwatering, underwatering, and unnecessary water consumption.

EcoPlant addresses this problem by connecting sensors, an ESP8266 microcontroller, a water pump, Firebase, and a web dashboard. Users can view sensor readings, monitor irrigation activity, review historical data, and control the pump remotely when the system is connected.

## Features

- **Live Sensor Monitoring** — View soil moisture, temperature, humidity, and water availability.
- **Automated Irrigation** — Operate the water pump based on configured environmental thresholds.
- **Manual Pump Control** — Switch between automatic and manual control modes through the dashboard.
- **Interactive Dashboard** — Access plant status, sensor trends, system information, and irrigation controls.
- **Alerts and Notifications** — View alerts related to changing sensor conditions and irrigation activity.
- **Historical Data** — Store sensor readings in Firebase and review past measurements.
- **Reports and Analytics** — View summarized sensor readings and trends.
- **Responsive Interface** — Access the web dashboard on desktop and mobile screens.

## Screenshots



 Dashboard
<img width="1382" height="667" alt="image" src="https://github.com/user-attachments/assets/9fd5fb97-2355-4233-a00c-708d150d361b" />
Live Sensor Data 
 <img width="1385" height="680" alt="image" src="https://github.com/user-attachments/assets/348e6f86-85b9-483b-9a03-79737c21c496" />
 

Irrigation Control 

 <img width="1472" height="615" alt="image" src="https://github.com/user-attachments/assets/c765af2c-8441-48d8-88ad-640194f8ed38" />
 Alerts and Report
 <img width="1315" height="647" alt="image" src="https://github.com/user-attachments/assets/ccc9df2b-84ab-47e2-801f-02b4346ad691" />
 

## Hardware Requirements

The project uses the following components:

- ESP8266 NodeMCU
- YL-69 soil moisture sensor with LM393 module
- DHT11 temperature and humidity sensor
- Water level sensor
- 1-channel relay module
- Mini DC water pump
- Silicone water tube
- Breadboard and jumper wires
- Suitable power supply

## Technology Stack

| Component | Technology |
|---|---|
| Frontend | HTML, CSS, JavaScript |
| Sensor controller | ESP8266 NodeMCU |
| Cloud database | Firebase Realtime Database |
| Hardware programming | Arduino IDE / C++ |
| Data visualization | Chart.js |
| UI icons | Font Awesome |
| Typography | Google Fonts |

## How It Works

1. The sensors collect soil moisture, temperature, humidity, and water-level readings.
2. The ESP8266 reads and processes the sensor values.
3. The irrigation logic checks the configured thresholds and determines whether watering is required.
4. The relay controls the water pump, subject to the water-level safety check.
5. When Wi-Fi and Firebase are available, sensor readings are uploaded to the cloud.
6. The web dashboard displays current readings, trends, alerts, and historical information.
7. In manual mode, the system can receive pump commands from the dashboard.

The irrigation logic also runs locally on the microcontroller, so the basic sensor-based pump operation does not depend entirely on an internet connection.

## Getting Started

### 1. Clone the repository

```bash
git clone https://github.com/YOUR-USERNAME/YOUR-REPOSITORY.git
cd YOUR-REPOSITORY
```

Replace `YOUR-USERNAME` and `YOUR-REPOSITORY` with your GitHub details.

### 2. Open the web dashboard

Open `public/index.html` in your browser or use the VS Code Live Server extension for local development.

The dashboard's live functionality requires the appropriate Firebase configuration and a working connection to the database.

### 3. Set up the Arduino firmware

1. Open the `Arduino/EcoPlant/` folder.
2. Copy `secrets.example.h` to a new file named `secrets.h`.
3. Add your own Wi-Fi credentials and Firebase configuration to `secrets.h`.
4. Install the required ESP8266 board support and Arduino libraries.
5. Select the appropriate ESP8266 board and port in Arduino IDE.
6. Upload `EcoPlant.ino` to the microcontroller.

**Important:** Never commit `secrets.h`, Wi-Fi passwords, service-account keys, or other private credentials to GitHub.

### 4. Configure Firebase

Configure Firebase Realtime Database and ensure that the database rules and authentication method match the firmware and frontend implementation. Do not expose the database with unrestricted public read/write access.

### 5. Connect the hardware

Connect the sensors, relay module, and water pump according to the project's circuit diagram. Verify the sensor readings and relay behavior before operating the pump with water connected.

## Project Structure

```text
EcoPlant/
├── public/
│   └── index.html
├── Arduino/
│   └── EcoPlant/
│       ├── EcoPlant.ino
│       └── secrets.example.h
├── firebase.json
├── .firebaserc
├── .gitignore
└── README.md
```

*The structure above represents the intended repository layout; adjust it if your actual files are organized differently.*

## Future Improvements

- Support for multiple plants and irrigation zones
- Improved sensor calibration and plant-specific thresholds
- Weather-aware irrigation scheduling
- Solar-powered operation
- Plant condition monitoring using images
- Integration with additional smart-home devices



---

EcoPlant IOE explores how connected devices, sensor data, cloud services, and automation can work together to make plant care more convenient and water usage more efficient.
