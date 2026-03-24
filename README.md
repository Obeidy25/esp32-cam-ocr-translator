# 📸 ESP32-CAM OCR Translator

> A smart IoT device that captures images, extracts text using AI-powered OCR, and translates it in real-time — all from a sleek web interface served directly by the ESP32-CAM.

![ESP32-CAM](https://img.shields.io/badge/ESP32--CAM-AI%20Thinker-blue)
![Python](https://img.shields.io/badge/Python-3.10+-green)
![Flask](https://img.shields.io/badge/Flask-2.3-red)
![License](https://img.shields.io/badge/License-MIT-yellow)

---

## ✨ Features

| Feature | Description |
|---------|-------------|
| 📷 **Live Camera Stream** | Real-time MJPEG stream from ESP32-CAM with adjustable resolution (QVGA / VGA / SVGA) |
| 🔤 **AI-Powered OCR** | Extract text from captured images using Google Gemini, OpenAI GPT-4o, or Anthropic Claude |
| 🌍 **Auto Translation** | Arabic → English and English → Arabic automatic translation |
| 📋 **Copy to Clipboard** | Works on both HTTP and HTTPS contexts (with legacy fallback) |
| 🔊 **Text-to-Speech (TTS)** | Read extracted/translated text aloud with manual voice selection dropdown |
| ✈️ **Send to Telegram** | Share results directly to a Telegram bot with one click |
| ✉️ **Send via Email** | Open default email client with pre-filled text |
| 💡 **Flash LED Control** | Toggle the onboard flash LED for low-light captures |
| 🔌 **Multi-Provider Support** | Switch between AI providers on-the-fly from the UI |
| 🔑 **API Key Verification** | Real-time validation of API keys before use |

---

## 🏗️ Architecture

```
┌─────────────────┐        ┌──────────────────┐        ┌─────────────────┐
│   ESP32-CAM     │  HTTP  │  Python Server   │  API   │  AI Providers   │
│   (Web UI +     │◄──────►│  (Flask Backend)  │◄──────►│  - Gemini       │
│    Camera)      │        │                  │        │  - OpenAI       │
│                 │        │  + Telegram Bot  │        │  - Anthropic    │
└─────────────────┘        └──────────────────┘        └─────────────────┘
     Port 80                   Port 5000
```

---

## 📁 Project Structure

```
esp32_cam_ocr_translator/
├── sketch_mar5a/
│   └── sketch_mar5a.ino    # ESP32-CAM firmware (Arduino) + embedded Web UI
├── server.py               # Flask backend server
├── ai_provider.py          # AI provider abstraction (Gemini, OpenAI, Anthropic)
├── requirements.txt        # Python dependencies
├── .env                    # API keys & configuration (not tracked by git)
├── .gitignore              # Git ignore rules
└── README.md               # This file
```

---

## 🚀 Getting Started

### Prerequisites

- **Hardware**: ESP32-CAM (AI-Thinker module) + USB programmer (FTDI)
- **Software**:
  - [Arduino IDE](https://www.arduino.cc/en/software) (with ESP32 board support)
  - [Python 3.10+](https://www.python.org/downloads/)
  - At least one AI API key (Gemini, OpenAI, or Anthropic)

### 1️⃣ Clone the Repository

```bash
git clone https://github.com/YOUR_USERNAME/esp32_cam_ocr_translator.git
cd esp32_cam_ocr_translator
```

### 2️⃣ Setup Python Server

```bash
# Create virtual environment
python -m venv venv

# Activate it
# Windows:
venv\Scripts\activate
# Linux/Mac:
source venv/bin/activate

# Install dependencies
pip install -r requirements.txt
```

### 3️⃣ Configure Environment Variables

Create a `.env` file in the project root:

```env
# === AI Providers (at least one required) ===
GEMINI_API_KEY=your_gemini_api_key_here
OPENAI_API_KEY=your_openai_api_key_here
ANTHROPIC_API_KEY=your_anthropic_api_key_here

# === Default Provider ===
DEFAULT_PROVIDER=gemini

# === Telegram Bot (optional) ===
TELEGRAM_BOT_TOKEN=your_bot_token_here
TELEGRAM_CHAT_ID=your_chat_id_here
```

### 4️⃣ Flash ESP32-CAM

1. Open `sketch_mar5a/sketch_mar5a.ino` in Arduino IDE.
2. **Update Wi-Fi credentials** (line 7-8):
   ```cpp
   const char* ssid = "YOUR_WIFI_NAME";
   const char* password = "YOUR_WIFI_PASSWORD";
   ```
3. **Update Server IP** (line 11-12):
   ```cpp
   #define SERVER_HOST "YOUR_PC_IP_ADDRESS"
   #define SERVER_PORT 5000
   ```
4. Select **Board**: `AI Thinker ESP32-CAM`
5. Upload the sketch.

### 5️⃣ Run the Server

```bash
python server.py
```

The server will start on `http://0.0.0.0:5000`.

### 6️⃣ Open the Web UI

Open your browser and navigate to:
- `http://esp32cam.local` (if mDNS is supported)
- Or the ESP32's IP address shown in the Serial Monitor (e.g., `http://192.168.1.100`)

---

## 🔧 Configuration

### Wi-Fi Setup
Edit lines 7-8 in `sketch_mar5a.ino`:
```cpp
const char* ssid = "your_wifi";
const char* password = "your_password";
```

### AI Provider Selection
The UI provides a dropdown to switch between configured AI providers in real-time. Only providers with valid API keys in `.env` will appear.

### Telegram Bot Setup
1. Create a bot via [@BotFather](https://t.me/BotFather) on Telegram.
2. Get your Chat ID via [@userinfobot](https://t.me/userinfobot).
3. Add both values to `.env`.

### Text-to-Speech (TTS)
The web UI includes a voice selection dropdown that lists all TTS voices installed on your device. To add Arabic TTS on Windows:
1. Go to **Settings** → **Time & Language** → **Speech**
2. Click **Manage voices** → **Add voices**
3. Search for **Arabic** and install
4. Restart your browser

---

## 🔌 Power Supply Guide

> ⚠️ **IMPORTANT**: Never exceed 5V on the ESP32-CAM's 5V pin!

| Method | Voltage | Safe? | Notes |
|--------|---------|-------|-------|
| USB Cable (phone charger) | 5V | ✅ Yes | Best for desk use |
| Single 3.7V Li-ion → **5V pin** | 3.7-4.2V | ⚠️ Unstable | May brownout under load |
| Single 3.7V Li-ion → **Boost Converter** → 5V pin | 5V | ✅ Yes | Best for portable use |
| 2x 3.7V Li-ion (7.4V) → **5V pin directly** | 7.4V | ❌ **DANGER** | Will burn the board! |
| 2x 3.7V Li-ion → **Buck Converter** → 5V pin | 5V | ✅ Yes | Best for long runtime |
| 18650 Battery Shield (USB out) | 5V | ✅ Yes | Safest portable option |

---

## 🛠️ API Endpoints

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/` | GET | Web UI |
| `/stream` | GET | MJPEG camera stream |
| `/capture?provider=X` | GET | Capture + OCR + Translate |
| `/led?state=on\|off` | GET | Toggle flash LED |
| `/res?size=QVGA\|VGA\|SVGA` | GET | Change camera resolution |
| `/providers` | GET | List available AI providers |
| `/verify?provider=X` | GET | Verify API key validity |
| `/send-telegram` | POST | Send text to Telegram bot |
| `/health` | GET | Server health check |

---

## 📄 License

This project is open source and available under the [MIT License](LICENSE).

---

## 🤝 Contributing

Contributions are welcome! Feel free to open an issue or submit a pull request.

---

<div align="center">
  <b>Built with ❤️ using ESP32-CAM + AI</b>
</div>
