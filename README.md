# 🎵 Spotify OLED Display — ESP32 + Discord Bot

This project displays your currently playing Spotify track **in real-time** on a 1.3" OLED display (SH1106 128×64), featuring **synchronized lyrics**, an animated idle expression system, a music progress bar, and an on-screen equalizer.

---

## 🗂️ Project Structure


```

├── bot.py          # Discord bot + HTTP JSON API server (runs on VPS/PC)
├── esp32.ino       # Arduino firmware tailored for ESP32
└── README.md

```

---

## 🧠 How It Works


```

Spotify ──► Discord (Rich Presence) ──► Discord Bot (Python)
│
Fetch synced lyrics from lrclib.net
│
Serve via HTTP Endpoint :3000
│
ESP32 Network Polling
│
Render on OLED SH1106

```

1. You play a track on Spotify → Discord automatically broadcasts your activity via Rich Presence.
2. The Python Discord bot (`bot.py`) captures this real-time presence data.
3. The bot automatically fetches timestamped lyrics from [lrclib.net](https://lrclib.net) (Free, no API key required).
4. The ESP32 micro-controller continuously polls the local/public server endpoint `/now-playing` every 2 seconds.
5. The OLED display renders the dynamic track title, artist name, synchronized lyric line highlights, and a micro-progress bar.

---

## 🎧 Prerequisites — Connecting Spotify to Discord

The backend system functions by reading your **Discord Presence** broadcast data. For the script to hook into your activity, your Spotify profile must be linked to Discord.

### Step-by-Step Integration:

1. Launch **Discord** → click the ⚙️ **User Settings** icon (bottom-left corner).
2. On the left sidebar, navigate to **Connections**.
3. Click the **Spotify** icon (the green circular logo).
4. Log into your Spotify account via the external browser pop-up → authorize by clicking **Agree**.
5. Once finalized, Spotify will be listed under your active accounts grid.

### Activating Profile Broadcasting:

Once your Spotify account is connected, verify that the toggle switch labeled **"Display Spotify as your status"** is actively flipped **ON** (Green).

> ⚠️ **CRITICAL:** If this toggle remains disabled, Discord will block your Spotify streaming metrics, making it impossible for the Python server backend to scrape your music status.

### Checking Discord Visibility Status:

Ensure that your public Discord visibility status is **NOT** configured to *Invisible*. The presence payload can only be fetched when set to:

```

✅ Online        → Accessible
✅ Idle          → Accessible
✅ Do Not Disturb → Accessible
❌ Invisible    → INACCESSIBLE (Blocks Data Payload)

```

---

## 🖥️ Section 1 — Python Server Backend Setup (VPS / PC)

### Requirements

| Software | Minimum Version |
|----------|-----------------|
| Python   | 3.9+            |
| pip      | Latest stable   |

### Dependency Installation

Ensure Python is actively initialized inside your system variable PATH layout. Verify via your command terminal:
```bash
python --version
# or alternatively
python3 --version

```

Execute the command below to install the core asynchronous wrappers:

```bash
pip install discord.py aiohttp

```

> ⚙️ **Note:** This structure requires **discord.py version 2.x** or greater. If your environment maintains an antiquated legacy setup, flush it via: `pip uninstall discord.py` followed by `pip install "discord.py>=2.0"`.

### Local Script Adjustments inside `bot.py`

Open `bot.py` using your preferred IDE, and input your credential keys:

```python
BOT_TOKEN  = "YOUR_DISCORD_BOT_TOKEN_HERE"
USER_ID    = 123456789012345678   # Your absolute Discord Snowflake User ID
PORT       = 3000                  # Designated local network port layout
LYRIC_LEAD = 2.4                  # Time offset in seconds (delivers lyrics early)

```

#### Acquiring your `BOT_TOKEN`:

1. Navigate to the [Discord Developer Portal](https://discord.com/developers/applications).
2. Click **New Application** → define a custom application name.
3. Access the **Bot** tab → click **Reset Token** → copy the alphanumeric string securely.
4. Scroll down to **Privileged Gateway Intents**, and explicitly enable:
* ✅ **Presence Intent**
* ✅ **Server Members Intent**


5. Move to **OAuth2 → URL Generator**, check the `bot` scope checkbox, and assign the `Read Messages/View Channels` permission flag.
6. Copy the compiled URL script → paste it into your web browser tab → invite your custom bot into your target Discord server setup.

#### Acquiring your `USER_ID`:

1. Open Discord Settings → Advanced → toggle **Developer Mode** to ON.
2. Right-click on your absolute user profile avatar image element → click **Copy User ID**.

### Executing the Host Script

```bash
python bot.py

```

Expected operational terminal output structure:

```
[server] HTTP API initialized at [http://0.0.0.0:3000/now-playing]
[bot] Logged in successfully as YourBotName#1234
[bot] Monitoring target Discord User ID: 123456789012345678

```

---

## ☁️ Cloud VPS Deployment (Recommended for 24/7 Uptime)

To prevent your local hardware machine from needing to remain constantly active, deploy the script onto an external virtual private cloud server framework (e.g., DigitalOcean, Vultr, AWS, Contabo).

### 0. Mapping Your Server's Architecture Interface

Local machine configurations point towards an internal routing system. When operating on a cloud network or local mesh framework, verify your active connectivity pointers:

* **Local Machine Deployment (Same Local Wi-Fi Mesh):**
Run `ipconfig` inside your Windows command terminal. For example, your assigned network adapter IP address might display as:
```
IPv4 Address. . . . . . . . . . . : 192.168.3.37

```


This explicit string acts as your localized hardware path index inside `esp32.ino`:
```cpp
const char* SERVER_URL = "[http://192.168.3.37:3000/now-playing](http://192.168.3.37:3000/now-playing)";

```


*Note: Localhost loops (`127.0.0.1`) are internal to your PC and will fail if called externally by the ESP32.*
* **Cloud VPS Deployment Architecture:**
Scrape your dynamic public address mapping from your cloud platform infrastructure administration console panel, or ping an external echo route from your active SSH session console:
```bash
curl ifconfig.me

```


Apply the returned public routing string value directly to your ESP32 configuration block layout.

---

### 1. File Push Protocol

```bash
scp bot.py root@your-vps-ip-address:/home/user/spotify-oled/

```

### 2. Dependency Resolution inside the Linux Host

```bash
sudo apt update
sudo apt install python3 python3-pip -y
pip3 install discord.py aiohttp

```

### 3. Background Persistence Management via `screen`

```bash
screen -S spotify-bot
python3 bot.py
# Detach cleanly by executing the key sequence: Ctrl + A, then press D

```

To re-attach to the ongoing terminal execution block thread:

```bash
screen -r spotify-bot

```

### 4. Firewall Rule Authorization Mapping

```bash
sudo ufw allow 3000

```

---

## 🔌 Section 2 — ESP32 Hardware Integration

### Hardware Checklist

| Component Name | Specific Metric Model Layout |
| --- | --- |
| ESP32 DevKit | NodeMCU ESP32, WEMOS D1 R32, etc. |
| OLED Screen | SH1106 1.3-inch 128×64 (I2C interface) |
| Jumper Cables | Female-to-Female physical wires |

### Pinout Wiring Diagram (I2C Layout)

```
ESP32 Microcontroller      SH1106 OLED Display Panel
      GPIO 21 ───────────► SDA (Serial Data Bus)
      GPIO 22 ───────────► SCL (Serial Clock Bus)
      3.3V    ───────────► VCC (Power Input Line)
      GND     ───────────► GND (Common Ground Earth)

```

### Essential Arduino IDE Libraries

Launch your local Arduino desktop IDE interface, and open the integrated asset panel manager through **Tools → Manage Libraries...** (`Ctrl + Shift + I`). Search for and install:

1. **U8g2** (Maintained by **oliver**) — Specialized high-performance monochrome graphics handler. *Select "Install All" to fulfill nested elements.*
2. **ArduinoJson** (Maintained by **Benoit Blanchon**) — Optimized micro-parsing buffer stream logic framework.
3. **HTTPClient & WiFi** — Native baseline micro-firmware drivers included directly with the core Espressif ESP32 hardware index framework stack.

### Firmware Setup & Variable Compilation inside `esp32.ino`

Adjust your hardware pointers to match your localized routing metrics:

```cpp
const char* WIFI_SSID  = "Your_WiFi_Network_Name";
const char* WIFI_PASS  = "Your_WiFi_Password";
const char* SERVER_URL = "http://192.168.3.37:3000/now-playing";  // Change to your server using IPV4 address. "

```

### Flashing Code into the Board Environment

1. Select your target development architecture node framework via **Tools → Board → ESP32 Dev Module**.
2. Identify your active interface device layout path index via the **Tools → Port** listing matrix.
3. Initialize execution by clicking the **Upload** arrow icon.

---

## 🔧 Deep-Dive System Troubleshooting

### Local Machine Hardware Lock on "Hard resetting via RTS pin..."

If your local deployment compiling console hangs indefinitely at the physical termination indicator block line during a flash sequence while your connected display remains blank, **your code has compiled and transferred perfectly**. This occurs when the host system USB-to-UART bridge driver hardware chip fails to cleanly trigger the automatic electrical sequence to reset the physical ESP32 pin lines.

* **Immediate Fix:** Press the onboard **EN** (or **RST**) tactile button located directly adjacent to the physical Micro-USB/Type-C interface slot once. This forces a cold hardware restart execution cycle.
* **Continuous Error Prevention:** Completely isolate the microcontroller device unit framework asset pipeline by detaching the physical USB cable connector entirely for 10 full seconds, allowing internal board capacitors to discharge completely. Re-attach to an alternative native port layout channel interface hub.

### Memory Optimization Mapping (83%+ Flash Allocation Warning)

The comprehensive cryptographic structure blocks utilized by automated JSON stream data structures consume significant space within the micro-controller flash allocations. If your Arduino compiling screen returns an absolute allocation alert boundary notice, switch the system partitioning configuration template:

1. Open **Tools** menu panel structure on Arduino IDE.
2. Access the **Partition Scheme** structural configuration drop-down index block track layout row.
3. Switch configuration from *Default* to **"Huge APP (3MB No OTA / 1MB SPIFFS)"**.
4. Re-run your compile framework program pipeline to reduce your system storage profile down safely below a clean ~35% threshold boundary.

### Terminal Communication Tracking via `HTTP code: -1`

If your Serial Monitor tracking window continuously loops a `-1` status return, it indicates **`HTTPC_ERROR_CONNECTION_REFUSED`**. The ESP32 is functioning normally but can't establish a handshake with the host server address.

* Ensure both the ESP32 and the machine hosting `bot.py` are attached to the exact same Wi-Fi network SSID configuration name mesh infrastructure.
* Confirm your host laptop's local IPv4 network address mapping index hasn't shifted by validating metrics via `ipconfig`.
* Add an absolute inbound exclusion exception rule to your native Windows Defender or Linux OS Firewall security management framework layer blocks to permit traffic over port `3000`.

---

## 📡 API Endpoint Definitions

### `GET /now-playing`

Returns the structural status of the active tracking stream parameters:

```json
{
  "title": "Tampar",
  "artist": "Juicy Luicy",
  "playing": true,
  "position_ms": 171136,
  "duration_ms": 202824,
  "lyric_prev": "Tutup air mata",
  "lyric_curr": "Tiga tahun tak terasa, masih kau yang ada",
  "lyric_next": "Bodoh yang sebenarnya"
}

```


## Credit

Created by EkiZR

Instagram: https://www.instagram.com/ekizr_/?hl=id
