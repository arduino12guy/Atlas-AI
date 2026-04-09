# 🌍 Atlas AI
### A pocket travel guide powered by LLaMA 3.3 on ESP32

Ask it about any place on Earth. Get back everything you need to know — instantly.

![Boot screen](images/20260408_102458.jpg)

---

## What It Does

Type a place name. Atlas AI connects to Groq's cloud inference API, runs Meta's LLaMA-3.3-70B model, and displays a clean travel card on a 2.4" TFT screen in under 5 seconds.

No app. No phone. No subscription.

![Result screen - Tokyo](images/20260408_102139.jpg)

---

## Hardware

| Part | Notes |
|------|-------|
| ESP32 DevKit v1 | Any 38-pin ESP32 |
| ILI9341 TFT (2.4") | 240×320, SPI |
| Jumper wires | — |

**Total cost: ~$10**

---

## Wiring

| TFT | ESP32 |
|-----|-------|
| VCC | 3.3V |
| GND | GND |
| CS | GPIO 17 |
| RESET | GPIO 5 |
| DC | GPIO 16 |
| MOSI | GPIO 23 |
| MISO | GPIO 19 |
| SCK | GPIO 18 |
| LED | 3.3V |

---

## Setup

**1. Install libraries** (Arduino Library Manager)
- `TFT_eSPI` by Bodmer
- `ArduinoJson` by Benoit Blanchon (v7+)

**2. Get a free Groq API key**

→ [console.groq.com](https://console.groq.com)

**3. Fill in your credentials**

```cpp
const char* WIFI_SSID    = "YOUR_WIFI_SSID";
const char* WIFI_PASS    = "YOUR_WIFI_PASSWORD";
const char* GROQ_API_KEY = "YOUR_GROQ_API_KEY";
```

**4. Flash**

- Board: `ESP32 Dev Module`
- Baud: `115200`
- Serial Monitor: `Newline` line ending

---

## Usage

Power on. The device boots, then connects to WiFi:

![WiFi connecting](images/20260408_102504.jpg)

Once connected, the ready screen appears:

![Ready screen](images/20260408_102529.jpg)

Type any place name in Serial Monitor and hit Enter. While it fetches the response:

![Loading spinner](images/20260408_102600.jpg)

Then the travel card appears:

![Travel card - Delhi](images/20260408_102607.jpg)

```
Enter place name:
> Taj Mahal

--- PLACE INFO ---
Name       : Taj Mahal
Specialty  : Marble mausoleum & Mughal architecture
Rating     : 4.8/5

Highlights:
  - White marble structure
  - UNESCO World Heritage Site
  - Symmetrical Mughal gardens

Best Time  : October to March
Famous For : Symbol of eternal love by Shah Jahan
-------------------
```

---

## Tech Stack

```
ESP32  →  WiFi  →  Groq API  →  LLaMA-3.3-70B
                                      ↓
                              JSON response
                                      ↓
                         TFT display  +  Serial Monitor
```

- **FreeRTOS** — API call on core 0, animation on core 1
- **ArduinoJson v7** — two-stage JSON parsing
- **TFT_eSPI** — hardware-accelerated SPI display

---

## Troubleshooting

| Problem | Fix |
|---------|-----|
| Blank screen | Check `User_Setup.h` driver and pins |
| HTTP -1 error | WiFi credentials wrong or out of range |
| HTTP 401 error | Invalid Groq API key |
| No serial input | Set Serial Monitor to **Newline** |
| Colors wrong | Add `tft.invertDisplay(true)` after `tft.init()` |

---

*Built by [arduino_guy](https://www.instructables.com/member/arduino12guy/)*
