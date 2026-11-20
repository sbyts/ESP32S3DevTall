# 📡 Wi-Fi Provisioning, Web Control & Wireless OTA Guide

This guide covers network setup, multi-AP roaming configuration, Web Admin authentication, and **Over-The-Air (OTA)** wireless firmware updates for the **Ridy-10-Lite Smart Desk Lamp (ESP32-S3)**—eliminating the need to disassemble the enclosure once sealed.

---

## 1. Initial Wi-Fi Provisioning (Fallback AP Mode)

On the very first boot (when no saved Wi-Fi networks are stored in NVS flash), the ESP32-S3 automatically switches to **Autonomous Access Point (AP) Mode**:

1. **Connect to the Lamp's Hotspot:**
   - **SSID:** `RidyLamp-AP`
   - **Password:** `12345678` (will be changed during first run)
2. **Open the Embedded Web Admin Panel:**
   - Navigate to **`http://192.168.4.1`** in any mobile or desktop browser.
   - Enter the default HTTP Basic Auth credentials:
     - **Username:** `admin`
     - **Password:** `admin`
3. **Configure Multi-AP Roaming & Security:**
   - Enter up to **6 Wi-Fi networks** (`Slot 1` – `Slot 6`). On every subsequent boot, the controller scans the RF spectrum (`WiFi.scanNetworks()`) and automatically connects to the strongest known signal (highest RSSI).
   - *(Recommended)* Set a custom **Web/OTA Password** and customize the mDNS **Hostname** (default: `ridy-lamp`).
4. **Apply & Reboot:**
   - Click **Save & Reboot**. The credentials are written persistently to ESP32-S3 NVS (`Preferences`), and the lamp restarts into Station (`STA`) mode.

---

## 2. Accessing the Web Dashboard on Your Local Network

Once connected to your router, you can access the lighting controls and live hardware telemetry via either mDNS or direct IP:

- **mDNS Address:** `http://ridy-lamp.local` *(or `http://<your-custom-hostname>.local`)*
- **Direct IP Address:** Check your router's DHCP client list or view the boot log via Serial Monitor (`115200 baud`).

> **Note for Windows / Android users:** If `.local` mDNS resolution is unavailable on your OS, use the direct IP address assigned by your router.

---

## 3. Wireless Firmware Updates (ArduinoOTA via PlatformIO)

After the initial firmware has been flashed over USB-UART, all future updates can be deployed wirelessly over Wi-Fi.

### Step 1: Enable OTA in `platformio.ini`
Open `platformio.ini` and uncomment the OTA upload parameters at the bottom of the file:

```ini
; --- Uncomment after initial USB flash for wireless OTA updates ---
upload_protocol = espota
upload_port = ridy-lamp.local
upload_flags =
    --auth=admin
```

- **`upload_port`:** Use `ridy-lamp.local` (or replace with the lamp's static IP address, e.g., `192.168.1.105`, if mDNS is blocked by your firewall).
- **`--auth=`:** Must match your current Web Admin / OTA password (default is `admin`).

### Step 2: Flash Wirelessly
1. Ensure your computer and the ESP32-S3 lamp are on the **same Wi-Fi network** (and client isolation / AP isolation is disabled on your router).
2. Click the standard **PlatformIO: Upload** (`→`) button in VS Code (or run `pio run -t upload` in the terminal).
3. PlatformIO will compile the binary, authenticate via MD5 challenge-response, stream the firmware over TCP/UDP, and automatically reboot the lamp.

---

## 4. Troubleshooting & Recovery

| Symptom | Cause | Resolution |
| :--- | :--- | :--- |
| **`No response from device` during OTA** | Windows Firewall blocking incoming UDP packets from ESP32 | Allow `python.exe` (PlatformIO) through Windows Defender Firewall on **Private Networks**, or use the lamp's IP address in `upload_port`. |
| **`Authentication Failed` during OTA** | Password mismatch between Web UI NVS and `platformio.ini` | Update `--auth=<your_password>` in `platformio.ini` to match the password saved in the Web Admin panel. |
| **Lamp boots into `RidyLamp-AP` unexpectedly** | Configured router is offline or out of range (`10s` timeout exceeded) | Normal fallback behavior. Connect to `RidyLamp-AP` (`192.168.4.1`) to verify SSID/password spelling. |
| **Forgotten Web/OTA Password** | Custom password lost in NVS | Connect via USB-UART and run `pio run -t erase` followed by `pio run -t upload` to factory-reset NVS storage. |
| **mDNS `.local` not resolving** | Router DNS/mDNS proxy issue | Replace `ridy-lamp.local` with the device's local IPv4 address in `platformio.ini`. |

---

## 5. Security Best Practices

- **Change Default Credentials:** Always change the default `admin` password via the Web UI before leaving the device permanently powered on your home/office network.
- **Keep Secrets Out of Git:** Never commit personal Wi-Fi SSIDs, passwords, or custom `--auth=` flags to a public GitHub repository. Keep `DEFAULT_SSID` as placeholder text and provision real networks exclusively through the Web UI into NVS flash.