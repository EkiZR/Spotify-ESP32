# 🎵 Spotify OLED Display — ESP32 + Discord Bot

Proyek ini menampilkan lagu Spotify yang sedang kamu putar **secara real-time** di layar OLED 1.3" (SH1106 128×64), lengkap dengan **lirik sinkron**, animasi mata idle, progress bar, dan equalizer animasi.

---

## 🗂️ Struktur Proyek

```
├── bot.py          # Discord bot + HTTP server (dijalankan di VPS/PC)
├── esp32.ino       # Firmware Arduino untuk ESP32
└── README.md
```

---

## 🧠 Cara Kerja

```
Spotify ──► Discord (Rich Presence) ──► Discord Bot (Python)
                                              │
                                    Ambil lirik dari lrclib.net
                                              │
                                    Serve via HTTP :3000
                                              │
                                         ESP32 polling
                                              │
                                     Tampil di OLED SH1106
```

1. Kamu main Spotify → Discord otomatis detect aktivitas
2. Bot Discord (`bot.py`) baca data lagu dari Discord presence
3. Bot fetch lirik dari [lrclib.net](https://lrclib.net) (gratis, no API key)
4. ESP32 polling endpoint `/now-playing` setiap 2 detik
5. OLED tampilkan judul, artist, lirik sinkron, dan progress bar

---

## 🎧 Prasyarat — Hubungkan Spotify ke Discord

Bot ini bekerja dengan membaca **Discord Presence** (status aktivitas) yang otomatis muncul saat kamu main Spotify. Supaya ini bisa terbaca, kamu harus pastikan Spotify sudah terhubung ke Discord dulu.

### Langkah-langkah:

1. Buka **Discord** → klik ikon ⚙️ **User Settings** (pojok kiri bawah)
2. Di sidebar kiri, pilih **Connections**
3. Klik ikon **Spotify** (logo hijau)
4. Login ke akun Spotify kamu di browser yang muncul → klik **Agree**
5. Setelah berhasil, Spotify akan muncul di daftar koneksi

### Aktifkan "Display on profile":

Setelah Spotify terhubung, pastikan toggle **"Display Spotify as your status"** dalam keadaan **ON** (hijau).

> ⚠️ Kalau toggle ini mati, Discord tidak akan broadcast aktivitas Spotify kamu, dan bot **tidak akan bisa membaca lagu yang sedang diputar**.

### Cek status Discord kamu:

Pastikan status kamu **bukan** Invisible. Bot hanya bisa membaca presence kalau status kamu Online, Idle, atau Do Not Disturb.

```
✅ Online       → bisa terbaca
✅ Idle         → bisa terbaca
✅ Do Not Disturb → bisa terbaca
❌ Invisible    → TIDAK terbaca
```

---

## 🖥️ Bagian 1 — Setup Bot Python (VPS / PC)

### Requirement

| Software | Versi minimal |
|----------|--------------|
| Python | 3.9+ |
| pip | terbaru |

### Install Dependencies

Pastikan Python sudah terinstall dulu. Cek dengan:
```bash
python --version
# atau
python3 --version
```

Kalau belum ada, download di [python.org](https://www.python.org/downloads/) → centang **"Add Python to PATH"** saat install (Windows).

Setelah Python siap, install library yang dibutuhkan:

```bash
pip install discord.py aiohttp
```

atau kalau pakai `pip3`:
```bash
pip3 install discord.py aiohttp
```

Verifikasi berhasil terinstall:
```bash
pip show discord.py
pip show aiohttp
```

> **Catatan:** `discord.py` yang dibutuhkan adalah versi **2.x** (bukan yang lama). Kalau sebelumnya pernah install versi lama, uninstall dulu:
> ```bash
> pip uninstall discord.py
> pip install "discord.py>=2.0"
> ```

### Konfigurasi di `bot.py`

Buka `bot.py`, edit bagian ini:

```python
BOT_TOKEN = "isi_token_bot_kamu_di_sini"
USER_ID   = 123456789012345678   # Discord User ID kamu (bukan username)
PORT      = 3000                  # Port HTTP server
LYRIC_LEAD = 2.4                  # Detik offset lirik (sesuaikan selera)
```

#### Cara dapat `BOT_TOKEN`:
1. Buka [Discord Developer Portal](https://discord.com/developers/applications)
2. Klik **New Application** → beri nama
3. Masuk ke menu **Bot** → klik **Reset Token** → copy tokennya
4. Di menu **Privileged Gateway Intents**, aktifkan:
   - ✅ **Presence Intent**
   - ✅ **Server Members Intent**
5. Masuk menu **OAuth2 → URL Generator**, centang `bot`, lalu centang permission `Read Messages/View Channels`
6. Copy URL yang dihasilkan → buka di browser → invite bot ke server Discord kamu

#### Cara dapat `USER_ID`:
1. Buka Discord → Settings → Advanced → aktifkan **Developer Mode**
2. Klik kanan nama kamu → **Copy User ID**

### Jalankan Bot

```bash
python bot.py
```

Output yang benar:
```
[server] http://0.0.0.0:3000/now-playing
[bot] logged in as NamaBotKamu#1234
[bot] watching user ID: 713597471414026240
```

### Test Manual

Buka browser atau curl ke:
```
http://localhost:3000/now-playing
```

Contoh response saat musik diputar:
```json
{
  "title": "Blinding Lights",
  "artist": "The Weeknd",
  "playing": true,
  "position_ms": 45000,
  "duration_ms": 200000,
  "lyric_prev": "I've been tryna call",
  "lyric_curr": "I need your love",
  "lyric_next": "I've been on my own for long enough"
}
```

---

## ☁️ Deploy ke VPS (Opsional tapi Direkomendasikan)

Kalau mau bot jalan 24/7 tanpa PC nyala, gunakan VPS (Contoh: DigitalOcean, Vultr, Contabo, dll.).

### 1. Upload file ke VPS

```bash
scp bot.py user@ip-vps-kamu:/home/user/spotify-oled/
```

### 2. Install Python & dependencies di VPS

```bash
sudo apt update
sudo apt install python3 python3-pip -y
pip3 install discord.py aiohttp
```

### 3. Jalankan pakai `screen` (biar tetap jalan walau SSH ditutup)

```bash
screen -S spotify-bot
python3 bot.py
# Tekan Ctrl+A lalu D untuk detach
```

Untuk kembali ke sesi:
```bash
screen -r spotify-bot
```

### 4. Alternatif: pakai `systemd` (lebih proper)

Buat file service:
```bash
sudo nano /etc/systemd/system/spotify-oled.service
```

Isi:
```ini
[Unit]
Description=Spotify OLED Discord Bot
After=network.target

[Service]
User=ubuntu
WorkingDirectory=/home/ubuntu/spotify-oled
ExecStart=/usr/bin/python3 /home/ubuntu/spotify-oled/bot.py
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target
```

Aktifkan:
```bash
sudo systemctl daemon-reload
sudo systemctl enable spotify-oled
sudo systemctl start spotify-oled
sudo systemctl status spotify-oled
```

### 5. Buka port firewall di VPS

```bash
sudo ufw allow 3000
```

> ⚠️ **Keamanan:** Port 3000 akan terbuka ke publik. Kalau ingin lebih aman, batasi hanya IP tertentu yang bisa akses, atau gunakan VPN.

---

## 🔌 Bagian 2 — Setup ESP32

### Hardware yang Dibutuhkan

| Komponen | Keterangan |
|----------|-----------|
| ESP32 (board apapun) | NodeMCU ESP32, WEMOS, dll |
| OLED SH1106 128×64 | Koneksi I2C |
| Kabel jumper | 4 kabel |

### Wiring I2C

```
ESP32        OLED SH1106
GPIO 21  ──► SDA
GPIO 22  ──► SCL
3.3V     ──► VCC
GND      ──► GND
```

### Library Arduino yang Diperlukan

Buka Arduino IDE, lalu buka **Library Manager** lewat:
- Menu **Sketch → Include Library → Manage Libraries...**
- atau tekan `Ctrl+Shift+I`

Cari dan install satu per satu:

**1. U8g2**
- Ketik di pencarian: `U8g2`
- Pilih yang by **oliver** (bukan U8x8)
- Klik **Install** → kalau muncul popup *"Install dependencies?"* klik **Install All**

**2. ArduinoJson**
- Ketik di pencarian: `ArduinoJson`
- Pilih yang by **Benoit Blanchon**
- Klik **Install**

**3. HTTPClient & WiFi**
- Kedua library ini **sudah bawaan** saat board ESP32 terinstall, tidak perlu install manual
- Kalau muncul error `HTTPClient.h not found`, berarti board ESP32 belum terinstall (lihat langkah di bawah)

### Install ESP32 Board di Arduino IDE

1. Buka **File → Preferences**
2. Di kolom *Additional Board Manager URLs*, tambahkan:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
3. Buka **Tools → Board → Board Manager** → cari `esp32` → Install

### Konfigurasi di `esp32.ino`

```cpp
const char* WIFI_SSID  = "NamaWiFiKamu";
const char* WIFI_PASS  = "PasswordWiFiKamu";
const char* SERVER_URL = "http://192.168.x.x:3000/now-playing";
//                                  ↑ IP VPS atau IP PC yang jalankan bot.py
```

> Kalau bot jalan di PC lokal (jaringan sama): pakai IP lokal PC kamu.
> Kalau bot jalan di VPS: pakai IP publik VPS kamu.

### Upload ke ESP32

1. Pilih board: **Tools → Board → ESP32 Dev Module**
2. Pilih port COM yang benar
3. Klik **Upload**

---

## 📺 Tampilan Layar

### Idle (tidak ada musik)
Menampilkan animasi mata dengan berbagai ekspresi acak (kedip, wink, senang, ngantuk, kaget, dll.) dan teks `SPOTIFY IS SLEEPING`.

### Now Playing
- Header: logo Spotify + teks "now playing" + animasi EQ bar
- Judul lagu (scroll otomatis kalau terlalu panjang)
- Nama artist
- Progress bar + timer posisi/durasi

### Lyrics Mode
Aktif otomatis kalau lagu punya lirik tersinkronisasi. Menampilkan:
- Baris lirik **sekarang** (highlight/invert)
- Baris lirik **berikutnya** (kecil, dengan prefix `>>`)
- Progress bar di bawah

---

## 🔧 Troubleshooting

### Bot tidak detect lagu Spotify
- Pastikan **Presence Intent** sudah diaktifkan di Developer Portal
- Pastikan akun Discord kamu ada di server yang sama dengan bot
- Pastikan status Discord kamu **tidak** di-set ke invisible
- Pastikan aplikasi Spotify terhubung ke Discord (Settings Discord → Connections)

### ESP32 tidak bisa konek ke server
- Cek IP address di `SERVER_URL` sudah benar
- Pastikan port 3000 tidak diblokir firewall
- Test dulu dari PC: `curl http://IP_KAMU:3000/now-playing`

### Lirik tidak muncul / tidak sinkron
- Tidak semua lagu ada di database lrclib.net
- Coba sesuaikan nilai `LYRIC_LEAD` di `bot.py` (default `2.4` detik)
- Cek endpoint debug: `http://IP_KAMU:3000/debug`

### OLED tidak menyala
- Cek wiring SDA/SCL
- Cek alamat I2C OLED kamu (default SH1106 biasanya `0x3C`)
- Kalau pakai pin I2C berbeda, ubah di `Wire.begin(SDA_PIN, SCL_PIN)`

---

## 📡 API Endpoint

### `GET /now-playing`
Response saat musik diputar:
```json
{
  "title": "string",
  "artist": "string",
  "playing": true,
  "position_ms": 12345,
  "duration_ms": 200000,
  "lyric_prev": "string",
  "lyric_curr": "string",
  "lyric_next": "string"
}
```

Response saat tidak ada musik:
```json
{ "playing": false }
```

### `GET /debug`
Menampilkan state internal bot dan sample lirik yang tersimpan di cache.

---

## 📦 Dependencies Lengkap

### Python (`bot.py`)
```
discord.py>=2.0
aiohttp
```
(Semua lainnya — `asyncio`, `json`, `re`, `urllib` — sudah bawaan Python)

### Arduino (`esp32.ino`)
```
U8g2
ArduinoJson
WiFi (bawaan ESP32)
HTTPClient (bawaan ESP32)
```

---

## Credit

Created by EkiZR  
Instagram: https://www.instagram.com/ekizr_/?hl=id