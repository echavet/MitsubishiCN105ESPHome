# Dual Setpoint Climate Control for Mitsubishi CN105 Heat Pumps

This example demonstrates how to implement intelligent dual setpoint climate control for Mitsubishi CN105 heat pump units using ESPHome and Home Assistant. The system provides comfort zones with both heating and cooling setpoints, humidity-based DRY mode activation, and multiple comfort presets.

> **⚠️ Important Note**: The logging code for preset changes in `climate-common-meta-thermostat.yaml` is currently commented out. If you want to monitor preset changes in the logs, uncomment the `preset_change` section in the meta-thermostat configuration file. But a few people (including the author of this YAML) noticed it caused faults for some reason, and it isnt critical to the functionality.

## 🌟 Key Features

- **Dual Setpoint Control**: Maintains temperature within a comfort zone (low/high setpoints) rather than a single target
- **Humidity-Based DRY Mode**: Automatically activates dehumidification when humidity exceeds 65%
- **Multiple Comfort Presets**: Pre-configured scenarios for different times and seasons
- **Intelligent Hysteresis**: Prevents short cycling with configurable deadbands and timing controls
- **Remote Temperature Sensing**: Uses Home Assistant sensors for accurate temperature control
- **Visual Status Indication**: RGB LED for connection status and debugging

## 📁 File Structure

```
example-dual setpoint/
├── README.md                              # This documentation
├── living-room-split-unit.yaml           # Main device configuration
├── climate-common-meta-thermostat.yaml  # Dual setpoint thermostat logic
├── climate-common-non-climate.yaml       # Hardware and sensor configuration
└── common/                                # Shared configuration files
    ├── ota.yaml                          # Over-the-air updates
    ├── web_server.yaml                   # Web interface
    ├── time.yaml                         # Time synchronization
    ├── wifi.yaml                         # WiFi configuration
    └── sensors/                          # Sensor configurations
        ├── wifi_text_sensors.yaml
        └── version_text_sensors.yaml
```

This example contains three main YAML configuration files that work together to create a complete dual setpoint climate control system for Mitsubishi CN105 heat pumps.

## 🔧 Hardware Requirements

- **ESP32-S3 Board**: M5Stack Stamp S3 or ESP32-S3 DevKit C1
- **CN105 Interface**: UART connection to Mitsubishi heat pump
- **Status LED**: RGB LED (SK6812) for visual feedback
- **Reset Button**: Physical button for device reset
- **Temperature Sensor**: Home Assistant entity for room temperature
- **Humidity Sensor**: Home Assistant entity for room humidity

## ⚙️ Configuration Files Explained

### 1. `living-room-split-unit.yaml` - Main Device Configuration

This is the primary configuration file that defines the ESP32 device and includes all necessary components:

- **Device Identity**: Hostname, friendly name, and room assignment
- **Hardware Setup**: ESP32-S3 board configuration with ESP-IDF framework
- **Boot Sequence**: Extended initialization delay for CN105 component
- **Include Management**: References to the other configuration files

**Key Substitutions:**
```yaml
substitutions:
  devicename: LivingRoomSplitUnit
  hostname: living-room-split-unit
  friendly_name: Living Room Split Unit
  remote_temp_sensor: sensor.living_room_average_remote_split_temp
  remote_humid_sensor: sensor.living_room_average_humidity
```

**Configuration Includes:**
- `climate-common-non-climate.yaml` - Hardware and sensor configuration
- `climate-common-meta-thermostat.yaml` - Dual setpoint thermostat logic

### 2. `climate-common-meta-thermostat.yaml` - Dual Setpoint Logic

This file implements the intelligent dual setpoint thermostat with the following features:

#### **Comfort Presets**
- **Seasonal Presets**: "Home in Summer" (cooling-focused), "Home in Winter" (heating-focused)
- **Daily Presets**: "Home" (dual-mode), "Sleep" (energy-saving), "Night" (moderate)
- **Performance Presets**: "Quick Cool", "Quick Heat", "Comfort" (high fan speed)
- **Special Modes**: "Away" (energy conservation), "Fan Only" (circulation)

#### **Dual Setpoint Control Logic**
```yaml
# Temperature ranges create comfort zones
default_target_temperature_low: 21.11    # 70°F - minimum comfort
default_target_temperature_high: 23.89   # 75°F - maximum comfort
```

#### **Intelligent Control Algorithm**
1. **Humidity Priority**: If humidity > 65%, activate DRY mode regardless of temperature
2. **Temperature Control**: Only in HEAT_COOL mode
   - If temp < (low_setpoint - deadband): HEAT
   - If temp > (high_setpoint + deadband): COOL
   - If temp within range: IDLE (fan only)

#### **Hysteresis and Timing Controls**
```yaml
# Prevents short cycling
min_heating_off_time: 180s      # 3 minutes between heating cycles
min_heating_run_time: 420s      # 7 minutes minimum heating runtime
min_cooling_off_time: 300s      # 5 minutes between cooling cycles
min_cooling_run_time: 600s      # 10 minutes minimum cooling runtime
```

### 3. `climate-common-non-climate.yaml` - Hardware Configuration

This file configures the underlying hardware and sensors:

#### **Home Assistant API Services**
```yaml
services:
  - service: set_remote_temperature    # Override internal sensor
  - service: use_internal_temperature # Revert to internal sensor
```

#### **Hardware Components**
- **UART Communication**: CN105 protocol communication (2400 baud, GPIO15/13)
- **External Components**: CN105 ESPHome component integration
- **Reset Button**: Physical button on GPIO0 for device restart
- **Status LED**: RGB LED (SK6812) on GPIO21 for visual feedback

#### **Sensor Configuration**
- **Remote Temperature**: Home Assistant entity with temperature conversion and validation
- **Remote Humidity**: Home Assistant entity for humidity-based control
- **Internal Temperature**: ESP32 fallback sensor
- **WiFi Signal**: Network monitoring sensors
- **Uptime**: System health monitoring

#### **Status LED Control**
- **Connected**: LED off (normal operation)
- **Disconnected**: Red strobing LED (connection issue)
- **Night Mode**: Full brightness for visibility
- **Day Mode**: Dim brightness to avoid distraction

## 🚀 Installation and Setup - Step by Step Guide

> **📌 Quick Start**: If you're experienced with ESPHome, you can skip to the configuration steps. If you're new, read everything carefully!

### 1. Prerequisites Checklist

Before you start, make sure you have:

- ✅ **ESPHome installed and configured**
  - If you don't have ESPHome, install it first: https://esphome.io/guides/installing_esphome.html
  - You need ESPHome version 2023.12.0 or later
  - ESPHome can be installed via Home Assistant add-on, Docker, or Python pip

- ✅ **Home Assistant running** (with ESPHome integration configured)
  - The ESPHome integration must be set up in Home Assistant
  - Go to Settings → Devices & Services → Add Integration → ESPHome

- ✅ **Temperature and Humidity Sensors in Home Assistant**
  - You need at least one temperature sensor entity (e.g., `sensor.living_room_temperature`)
  - You need at least one humidity sensor entity (e.g., `sensor.living_room_humidity`)
  - These can be from any Home Assistant integration (Zigbee, Z-Wave, WiFi, etc.)
  - **How to find your sensor names**: Go to Home Assistant → Developer Tools → States, search for "temperature" and "humidity"

- ✅ **Mitsubishi CN105 heat pump unit** (the actual HVAC unit)
  - This must be a Mitsubishi unit with CN105 interface capability
  - You'll need to wire the ESP32 to the CN105 connector on the unit

- ✅ **ESP32-S3 development board**
  - M5Stack Stamp S3 (recommended) OR ESP32-S3 DevKit C1
  - Make sure it's an ESP32-S3, not ESP32 or ESP32-C3 (those won't work!)

### 2. Hardware Connections - Detailed Wiring Guide

> **⚠️ WARNING**: Double-check all connections before powering on! Incorrect wiring can damage your equipment.

#### Required Connections:

**CN105 UART Connection (Most Important!):**
```
ESP32-S3          CN105 Interface
--------          --------------
GPIO15 (TX)   →   RX (receive pin on CN105)
GPIO13 (RX)   →   TX (transmit pin on CN105)
GND           →   GND (ground)
3.3V          →   VCC (if CN105 needs power, check your interface board)
```

**Status LED (Optional but Recommended):**
```
ESP32-S3          RGB LED (SK6812)
--------          --------------
GPIO21        →   Data pin (DIN)
3.3V          →   VCC (+)
GND           →   GND (-)
```

**Reset Button (Optional):**
```
ESP32-S3          Button
--------          --------------
GPIO0         →   One side of button
GND           →   Other side of button
```

> **💡 Pro Tip**: If you're using an M5Stack Stamp S3, the button on the board is already connected to GPIO0, so you don't need to wire a separate button.

#### Common Wiring Mistakes to Avoid:

1. **❌ DON'T** connect TX to TX or RX to RX - they must be crossed!
   - ✅ DO: ESP32 TX → CN105 RX, ESP32 RX → CN105 TX

2. **❌ DON'T** use 5V for the CN105 interface if it's a 3.3V device
   - ✅ DO: Check your CN105 interface board specifications

3. **❌ DON'T** forget the ground connection - it's critical for communication
   - ✅ DO: Always connect GND between ESP32 and CN105

### 3. Configuration Steps - Detailed Walkthrough

#### Step 3.1: Download and Place the Files

1. **Find your ESPHome configuration directory:**
   - If using Home Assistant add-on: `/config/esphome/`
   - If using Docker: Usually in your Docker volume
   - If using standalone: Wherever you installed ESPHome

2. **Copy all three YAML files** to your ESPHome config directory:
   ```
   your-esphome-config/
   ├── living-room-split-unit.yaml
   ├── climate-common-meta-thermostat.yaml
   └── climate-common-non-climate.yaml
   ```
   > **⚠️ Important**: All three files must be in the SAME directory!

3. **Verify the files are there:**
   ```bash
   # If using command line:
   ls -la /path/to/esphome/config/
   # You should see all three .yaml files
   ```

#### Step 3.2: Update Substitutions (REQUIRED!)

**This is the most important step - don't skip it!**

1. **Open `living-room-split-unit.yaml` in a text editor**

2. **Find the `substitutions:` section** (near the top of the file)

3. **Update these values** (replace with YOUR actual values):

   ```yaml
   substitutions:
     # Device identification - change these to match your setup
     devicename: LivingRoomSplitUnit          # Internal name (no spaces, use CamelCase)
     hostname: living-room-split-unit          # Network hostname (lowercase, use hyphens)
     friendly_name: Living Room Split Unit     # Display name in Home Assistant
     location: Inside                          # Location type
     floor: 1stfloor                           # Floor number
     room: Living Room                         # Room name
     
     # Sensor configuration - THESE MUST MATCH YOUR HOME ASSISTANT ENTITIES!
     sensor_interval: 60s
     remote_temp_sensor: sensor.living_room_average_remote_split_temp  # ⚠️ CHANGE THIS!
     remote_humid_sensor: sensor.living_room_average_humidity         # ⚠️ CHANGE THIS!
     
     comment: M5 S3 Orange Stamp w/ CN105 Component
   ```

4. **How to find your sensor entity IDs:**
   - Open Home Assistant
   - Go to **Developer Tools** → **States**
   - Search for your temperature sensor (e.g., type "temperature")
   - Find the exact entity ID (it will look like `sensor.living_room_temperature`)
   - Copy the EXACT entity ID (case-sensitive!)
   - Do the same for humidity sensor

5. **Example of correct substitutions:**
   ```yaml
   substitutions:
     devicename: MasterBedroomAC
     hostname: master-bedroom-ac
     friendly_name: Master Bedroom AC
     room: Master Bedroom
     remote_temp_sensor: sensor.master_bedroom_temperature  # Your actual sensor
     remote_humid_sensor: sensor.master_bedroom_humidity     # Your actual sensor
   ```

> **❌ Common Mistake**: Using the wrong entity ID format. Make sure it starts with `sensor.` and matches EXACTLY what's in Home Assistant (including underscores and capitalization).

#### Step 3.3: Configure Secrets (REQUIRED!)

1. **Find or create `secrets.yaml`** in your ESPHome config directory
   - If it doesn't exist, create a new file named `secrets.yaml`
   - This file stores sensitive information (WiFi passwords, API keys)

2. **Add these secrets** (replace with YOUR actual values):

   ```yaml
   # WiFi Configuration
   wifi_ssid: "YourWiFiNetworkName"           # Your WiFi network name (SSID)
   wifi_password: "YourWiFiPassword"          # Your WiFi password
   
   # ESPHome API Encryption Key
   # Generate a random key or use an existing one
   # This should be a long random string (at least 32 characters)
   api_key_1: "your-very-long-random-encryption-key-here-minimum-32-chars"
   ```

3. **How to generate an API key:**
   - Use an online generator: https://www.random.org/strings/
   - Or use this command: `openssl rand -hex 32`
   - Or just use a long random string (at least 32 characters)

4. **Example `secrets.yaml`:**
   ```yaml
   wifi_ssid: "MyHomeWiFi"
   wifi_password: "MySecurePassword123"
   api_key_1: "a1b2c3d4e5f6g7h8i9j0k1l2m3n4o5p6q7r8s9t0u1v2w3x4y5z6"
   ```

> **⚠️ Security Warning**: Never commit `secrets.yaml` to git! It contains sensitive information. Make sure it's in your `.gitignore` file.

#### Step 3.4: Verify Your Configuration

Before compiling, double-check:

- ✅ All three YAML files are in the same directory
- ✅ `substitutions` section has YOUR sensor entity IDs (not the example ones)
- ✅ `secrets.yaml` exists and has YOUR WiFi credentials
- ✅ Sensor entity IDs match EXACTLY what's in Home Assistant
- ✅ No typos in entity IDs (they're case-sensitive!)

#### Step 3.5: Compile and Upload

1. **Open a terminal/command prompt**

2. **Navigate to your ESPHome config directory:**
   ```bash
   cd /path/to/your/esphome/config
   ```

3. **Validate the configuration first** (catches errors before uploading):
   ```bash
   esphome config living-room-split-unit.yaml
   ```
   - If you see errors, fix them before proceeding
   - Common errors: missing secrets, wrong entity IDs, syntax errors

4. **Compile and upload** (this will take a few minutes):
   ```bash
   esphome run living-room-split-unit.yaml
   ```

5. **What happens during upload:**
   - ESPHome compiles the firmware
   - Connects to your ESP32 (via USB or WiFi)
   - Uploads the firmware
   - Device reboots and connects to WiFi
   - You should see "Successfully uploaded program" message

6. **If upload fails:**
   - Make sure ESP32 is connected via USB (for first upload)
   - Check that you selected the correct serial port
   - Try holding the BOOT button while uploading
   - Check USB cable (some cables are power-only, not data)

### 4. Home Assistant Integration - First Time Setup

#### Step 4.1: Device Discovery

1. **Wait 1-2 minutes** after upload completes (device needs time to boot and connect)

2. **Open Home Assistant**

3. **Go to Settings → Devices & Services**

4. **Look for ESPHome integration** - your device should appear automatically
   - Device name will match your `friendly_name` from substitutions
   - If it doesn't appear, check the next section

5. **Click "Configure"** on your device

6. **Enter the encryption key** (the `api_key_1` from your secrets.yaml)

7. **Click "Submit"** - device should now be connected!

#### Step 4.2: Verify Sensors Are Working

1. **Go to Developer Tools → States**

2. **Search for your device name** (e.g., "Living Room Split Unit")

3. **Verify these entities exist:**
   - `climate.thermostat` (the main thermostat entity)
   - `sensor.remote_temperature` (should show your room temperature)
   - `sensor.remote_humidity` (should show your room humidity)
   - `sensor.uptime` (device uptime)

4. **Check sensor values:**
   - Temperature sensor should show a number (not "unknown" or "unavailable")
   - Humidity sensor should show a percentage (not "unknown" or "unavailable")
   - If they show "unavailable", your entity IDs in substitutions are wrong!

#### Step 4.3: Test the Thermostat

1. **Go to Home Assistant → Overview** (or your dashboard)

2. **Find the thermostat card** (should show "Thermostat")

3. **Try changing the mode:**
   - Click on the thermostat
   - Try selecting "Heat/Cool" mode
   - Set a temperature range (low and high setpoints)
   - Watch the logs to see if commands are sent to CN105

4. **Check the logs:**
   - In ESPHome dashboard, click on your device
   - Go to "Logs" tab
   - You should see messages like "DUAL_SETPOINT" and "HUMIDITY_CONTROL"
   - If you see errors, check the troubleshooting section

#### Step 4.4: Test Custom Services (Optional)

1. **Go to Developer Tools → Services**

2. **Find the service**: `esphome.your_device_name_set_remote_temperature`
   - Replace `your_device_name` with your actual hostname

3. **Test setting remote temperature:**
   ```yaml
   service: esphome.living_room_split_unit_set_remote_temperature
   data:
     temperature: 22.5
   ```

4. **Check if it worked:**
   - Look at the device logs
   - Should see "Remote temperature received from HA: 22.5 C"

### 5. Common First-Time Setup Issues

**Problem: Device doesn't appear in Home Assistant**
- ✅ Check WiFi credentials in secrets.yaml
- ✅ Check that ESPHome integration is installed
- ✅ Wait longer (can take 2-3 minutes)
- ✅ Check device logs in ESPHome dashboard
- ✅ Verify device is connected to WiFi (check your router)

**Problem: Sensors show "unavailable"**
- ✅ Check entity IDs in substitutions match Home Assistant exactly
- ✅ Verify sensors exist in Home Assistant (Developer Tools → States)
- ✅ Check for typos (entity IDs are case-sensitive!)
- ✅ Make sure sensors are actually reporting data

**Problem: Thermostat doesn't control the heat pump**
- ✅ Check UART wiring (TX/RX must be crossed!)
- ✅ Verify CN105 interface is properly connected
- ✅ Check device logs for communication errors
- ✅ Verify baud rate is 2400 (should be set automatically)

**Problem: Compilation errors**
- ✅ Check YAML syntax (indentation matters!)
- ✅ Verify all three files are in the same directory
- ✅ Check that secrets.yaml exists and has all required values
- ✅ Look at the error message - it usually tells you what's wrong

## 🎛️ Usage and Operation - How to Use Your Dual Setpoint Thermostat

### Understanding Dual Setpoints (Important!)

**Traditional Thermostat:**
- You set ONE temperature (e.g., 72°F)
- System heats until it reaches 72°F, then turns off
- System cools until it reaches 72°F, then turns off
- Constant on/off cycling

**Dual Setpoint Thermostat (This System):**
- You set TWO temperatures: LOW (e.g., 70°F) and HIGH (e.g., 75°F)
- System creates a "comfort zone" between these temperatures
- If temp < 70°F → HEAT
- If temp > 75°F → COOL
- If temp is 70-75°F → FAN ONLY (no heating/cooling, just air circulation)
- Much more comfortable and energy-efficient!

### Comfort Presets - Quick Reference Guide

The system provides 11 pre-configured comfort presets. Here's when to use each:

| Preset | Low Temp | High Temp | Mode | Fan | When to Use |
|--------|----------|-----------|------|-----|-------------|
| **Home in Summer** | 70°F | 74°F | COOL | AUTO | Hot summer days - cooling only |
| **Home in Winter** | 72°F | 78°F | HEAT | AUTO | Cold winter days - heating only |
| **Home** | 70°F | 75°F | HEAT_COOL | AUTO | **Default preset** - general daily use |
| **Sleep** | 65°F | 72°F | HEAT_COOL | AUTO | Bedtime - cooler for better sleep |
| **Night** | 65°F | 74°F | HEAT_COOL | AUTO | Evening hours - moderate comfort |
| **Away** | 65°F | 78°F | HEAT_COOL | AUTO | When you're gone - energy saving |
| **Comfort** | 72°F | 74°F | HEAT_COOL | HIGH | Precise temperature control needed |
| **Quick Cool** | 65°F | 72°F | COOL | HIGH | Room is too hot - rapid cooling |
| **Quick Heat** | 70°F | 74°F | HEAT | HIGH | Room is too cold - rapid heating |
| **Pre Cool** | 65°F | 72°F | COOL | HIGH | Before guests arrive - prepare room |
| **Fan Only** | 70°F | 74°F | FAN_ONLY | AUTO | Just want air circulation, no heating/cooling |

### How to Use Presets in Home Assistant

1. **Open Home Assistant** and find your thermostat entity

2. **Click on the thermostat card**

3. **Look for the "Preset" dropdown** (usually at the bottom of the card)

4. **Select a preset** from the dropdown:
   - The thermostat will automatically switch to that preset's settings
   - Temperature range, mode, and fan speed will change automatically
   - You'll see the new setpoints displayed

5. **Example**: Select "Sleep" preset
   - Low setpoint changes to 65°F
   - High setpoint changes to 72°F
   - Mode changes to HEAT_COOL
   - System will now maintain 65-72°F range

### Dual Setpoint Operation - Step by Step

#### Setting Up Your Comfort Zone

1. **Open the thermostat in Home Assistant**

2. **Set the mode to "Heat/Cool"** (this enables dual setpoint control)
   - If you don't see this option, the thermostat might be in a different mode
   - Switch to HEAT_COOL mode first

3. **Set the LOW setpoint** (minimum comfortable temperature):
   - This is the temperature below which the system will heat
   - Example: 70°F - if room gets colder than 70°F, system heats

4. **Set the HIGH setpoint** (maximum comfortable temperature):
   - This is the temperature above which the system will cool
   - Example: 75°F - if room gets warmer than 75°F, system cools

5. **The system automatically:**
   - **Heats** when temperature drops below (low setpoint - 1.8°F deadband)
   - **Cools** when temperature rises above (high setpoint + 1.8°F deadband)
   - **Circulates air** (fan only) when temperature is between the setpoints

#### Example Scenario

**Your Settings:**
- Low setpoint: 70°F
- High setpoint: 75°F
- Mode: HEAT_COOL

**What Happens:**
- Room temp is 68°F → System HEATS (below low setpoint)
- Room temp is 72°F → System runs FAN ONLY (in comfort zone)
- Room temp is 77°F → System COOLS (above high setpoint)
- Room temp is 73°F → System runs FAN ONLY (in comfort zone)

### Humidity Control - Automatic DRY Mode

The system automatically monitors humidity and activates DRY mode when needed:

1. **System checks humidity every time it evaluates temperature**

2. **If humidity > 65%:**
   - System immediately switches to DRY mode
   - Temperature control is temporarily paused
   - System dehumidifies until humidity drops below 65%
   - Then returns to normal temperature control

3. **Why 65%?**
   - Above 65% humidity can cause mold growth
   - This threshold prevents health issues
   - You can change this in the code if needed (see customization section)

4. **How to know it's in DRY mode:**
   - Check the thermostat status in Home Assistant
   - Look at device logs - you'll see "HUMIDITY_CONTROL" messages
   - The heat pump will be in DRY mode (not HEAT or COOL)

### Manual Temperature Adjustments

You can manually adjust setpoints at any time:

1. **Click on the thermostat in Home Assistant**

2. **Adjust the LOW setpoint slider** (left side)
   - Drag to your desired minimum temperature
   - System will heat if temp drops below this

3. **Adjust the HIGH setpoint slider** (right side)
   - Drag to your desired maximum temperature
   - System will cool if temp rises above this

4. **Changes take effect immediately**
   - System will adjust behavior based on new setpoints
   - You'll see log messages showing the change

### Custom Services

Use Home Assistant services to control the device:

```yaml
# Set remote temperature sensor
service: esphome.your_device_set_remote_temperature
data:
  temperature: 22.5

# Use internal temperature sensor
service: esphome.your_device_use_internal_temperature
```

## 🔧 Customization

### Modifying Presets

Edit the `preset` section in `climate-common-meta-thermostat.yaml`:

```yaml
preset:
  - name: Custom Preset
    default_target_temperature_low: 20.0    # 68°F
    default_target_temperature_high: 24.0   # 75°F
    mode: HEAT_COOL
    fan_mode: "AUTO"
    swing_mode: "BOTH"
```

### Adjusting Hysteresis

Modify timing controls to prevent short cycling:

```yaml
# Increase delays for more stable operation
min_heating_off_time: 300s      # 5 minutes
min_heating_run_time: 600s      # 10 minutes
min_cooling_off_time: 300s       # 5 minutes
min_cooling_run_time: 900s      # 15 minutes
```

### Custom Sensor Integration

Update the substitutions to use your sensors:

```yaml
substitutions:
  remote_temp_sensor: sensor.your_room_temperature
  remote_humid_sensor: sensor.your_room_humidity
```

## 🐛 Troubleshooting - Common Problems and Solutions

> **💡 Pro Tip**: Most problems are caused by configuration errors. Double-check your substitutions and secrets.yaml first!

### Problem 1: Device Doesn't Appear in Home Assistant

**Symptoms:**
- Device doesn't show up in ESPHome integration
- Can't find device in Home Assistant

**Solutions (try in order):**

1. **Check WiFi credentials:**
   ```yaml
   # In secrets.yaml, verify:
   wifi_ssid: "YourActualWiFiName"      # Must match EXACTLY (case-sensitive!)
   wifi_password: "YourActualPassword"  # Must be correct
   ```

2. **Check device is connected to WiFi:**
   - Look at your router's connected devices list
   - Device should appear with hostname from substitutions
   - If not connected, check WiFi signal strength

3. **Wait longer:**
   - First boot can take 2-3 minutes
   - Device needs time to connect to WiFi and register

4. **Check ESPHome integration:**
   - Go to Settings → Devices & Services
   - Make sure ESPHome integration is installed
   - If not, add it: Add Integration → ESPHome

5. **Check device logs:**
   - In ESPHome dashboard, open your device
   - Go to "Logs" tab
   - Look for WiFi connection errors
   - Look for "Connected successfully" message

6. **Manual discovery:**
   - In Home Assistant, go to ESPHome integration
   - Click "Add Device"
   - Enter device IP address (if you know it)
   - Or use device hostname

### Problem 2: Sensors Show "Unavailable" or "Unknown"

**Symptoms:**
- `sensor.remote_temperature` shows "unavailable"
- `sensor.remote_humidity` shows "unavailable"
- Thermostat doesn't show current temperature

**Solutions:**

1. **Verify entity IDs in substitutions:**
   ```yaml
   # In living-room-split-unit.yaml, check:
   remote_temp_sensor: sensor.your_actual_sensor_name  # Must match HA exactly!
   remote_humid_sensor: sensor.your_actual_sensor_name # Must match HA exactly!
   ```

2. **Find correct entity IDs:**
   - Open Home Assistant
   - Go to Developer Tools → States
   - Search for your temperature sensor
   - Copy the EXACT entity ID (including `sensor.` prefix)
   - Example: `sensor.living_room_temperature` (not `sensor.Living_Room_Temperature`)

3. **Check sensors exist and have data:**
   - In Developer Tools → States
   - Find your sensor entities
   - Make sure they show actual values (not "unknown")
   - If sensors show "unknown", fix the sensor first

4. **Check for typos:**
   - Entity IDs are case-sensitive!
   - `sensor.living_room_temp` ≠ `sensor.Living_Room_Temp`
   - Check underscores vs hyphens
   - Check spelling

5. **Test with internal sensor:**
   - Use the service: `esphome.your_device_use_internal_temperature`
   - This uses ESP32's internal sensor (less accurate but works for testing)
   - If this works, your remote sensor entity ID is wrong

### Problem 3: Thermostat Doesn't Control Heat Pump

**Symptoms:**
- Thermostat changes in Home Assistant
- But heat pump doesn't respond
- No heating/cooling happens

**Solutions:**

1. **Check UART wiring (MOST COMMON ISSUE):**
   ```
   ✅ CORRECT:
   ESP32 TX (GPIO15) → CN105 RX
   ESP32 RX (GPIO13) → CN105 TX
   ESP32 GND → CN105 GND
   
   ❌ WRONG:
   ESP32 TX → CN105 TX  (won't work!)
   ESP32 RX → CN105 RX  (won't work!)
   ```

2. **Verify CN105 interface connection:**
   - Check all wires are securely connected
   - Check for loose connections
   - Verify CN105 interface board is powered (if needed)

3. **Check device logs for errors:**
   - Look for "CN105" errors in logs
   - Look for "UART" errors
   - Look for "communication" errors
   - If you see errors, wiring is likely wrong

4. **Verify baud rate:**
   - Should be 2400 (set automatically in config)
   - Check `climate-common-non-climate.yaml`:
     ```yaml
     uart:
       baud_rate: 2400  # Must be 2400 for CN105
     ```

5. **Test CN105 interface:**
   - Check if CN105 interface board has status LEDs
   - Should blink when receiving data
   - If no activity, wiring is wrong

6. **Check heat pump is on:**
   - Make sure the actual heat pump unit is powered on
   - Check if heat pump responds to its remote control
   - If heat pump doesn't work at all, that's a different problem

### Problem 4: Compilation Errors

**Symptoms:**
- `esphome config` or `esphome run` fails
- Error messages about missing files or syntax errors

**Solutions:**

1. **Check YAML syntax:**
   - YAML is very sensitive to indentation
   - Use spaces, not tabs
   - Check for missing colons (`:`) or dashes (`-`)
   - Use a YAML validator: https://www.yamllint.com/

2. **Verify all files exist:**
   ```bash
   # Check all three files are in the same directory:
   ls -la living-room-split-unit.yaml
   ls -la climate-common-meta-thermostat.yaml
   ls -la climate-common-non-climate.yaml
   ```

3. **Check secrets.yaml:**
   - File must exist in same directory
   - Must have `wifi_ssid`, `wifi_password`, and `api_key_1`
   - Values must be in quotes if they contain special characters

4. **Check for missing includes:**
   - If error says "file not found", check include paths
   - All files should be in same directory (no subdirectories needed)

5. **Read the error message:**
   - ESPHome error messages are usually helpful
   - They tell you exactly what's wrong
   - Look for line numbers in error messages

### Problem 5: Device Keeps Disconnecting

**Symptoms:**
- Device connects then disconnects repeatedly
- Status LED flashes red
- Device shows as "unavailable" in Home Assistant

**Solutions:**

1. **Check WiFi signal strength:**
   - Device might be too far from router
   - Move device closer or add WiFi extender
   - Check WiFi signal sensor in device (should be > -70 dBm)

2. **Check WiFi credentials:**
   - Verify SSID and password are correct
   - Check for special characters that need escaping
   - Try re-entering credentials

3. **Check for IP conflicts:**
   - Another device might have same IP
   - Check router's DHCP settings
   - Assign static IP if needed

4. **Check power supply:**
   - ESP32 might not be getting enough power
   - Use a good quality USB cable
   - Use a powered USB hub if needed
   - Check for voltage drops

5. **Check for interference:**
   - Other 2.4GHz devices can interfere
   - Move device away from microwaves, Bluetooth devices
   - Change WiFi channel on router

### Problem 6: Temperature Readings Are Wrong

**Symptoms:**
- Temperature shown doesn't match actual room temperature
- Temperature is way off (e.g., shows 100°F when it's 72°F)

**Solutions:**

1. **Check sensor entity:**
   - Verify you're using the correct sensor
   - Check if sensor is calibrated correctly
   - Test sensor in Home Assistant (should show correct temp)

2. **Check temperature conversion:**
   - If sensor reports in Fahrenheit, it's converted to Celsius
   - Check `climate-common-non-climate.yaml`:
     ```yaml
     filters:
       - lambda: return (x - 32) * (5.0/9.0);  # Converts F to C
     ```
   - If your sensor is already in Celsius, remove this filter

3. **Check sensor location:**
   - Sensor might be in wrong location
   - Near heat sources (vents, windows, electronics)
   - Move sensor to better location

4. **Use internal sensor for testing:**
   - Test with ESP32's internal sensor
   - If internal sensor is also wrong, it's a device issue
   - If internal sensor is correct, it's a remote sensor issue

### Debug Logging - Enable Detailed Logs

If you need more information, enable debug logging:

1. **Edit `climate-common-non-climate.yaml`:**

2. **Find the `logger:` section:**

3. **Change to DEBUG level:**
   ```yaml
   logger:
     level: DEBUG  # Changed from INFO to DEBUG
     logs:
       CN105: DEBUG              # CN105 communication
       climate: DEBUG            # Climate component
       DUAL_SETPOINT: DEBUG      # Dual setpoint logic
       HUMIDITY_CONTROL: DEBUG   # Humidity control
       sensor: DEBUG             # Sensor readings
   ```

4. **Recompile and upload:**
   ```bash
   esphome run living-room-split-unit.yaml
   ```

5. **Check logs:**
   - Much more detailed information will appear
   - Look for the specific component that's having issues
   - **Warning**: Debug logs are very verbose - disable when done troubleshooting

### Status LED Indicators - What They Mean

- **LED Off**: ✅ Connected to Home Assistant (normal operation)
- **Red Strobing**: ❌ Connection issue - device can't reach Home Assistant
- **Red Solid**: Device is booting or has an error
- **Brightness Changes**: Adjusts based on night mode setting (dim during day, bright at night)

### Still Having Problems?

1. **Check the logs:**
   - ESPHome dashboard → Your device → Logs tab
   - Look for error messages (they're usually red)
   - Copy error messages for help

2. **Verify your configuration:**
   - Run `esphome config living-room-split-unit.yaml`
   - Fix any errors before asking for help

3. **Check GitHub issues:**
   - Search for similar problems: https://github.com/echavet/MitsubishiCN105ESPHome/issues
   - Someone might have had the same issue

4. **Ask for help:**
   - Include your error messages
   - Include relevant log excerpts
   - Describe what you've already tried
   - Include your configuration (remove secrets!)

## 📊 Monitoring and Maintenance

### Key Metrics to Monitor

- **Temperature Accuracy**: Compare remote sensor with internal sensor
- **Humidity Levels**: Monitor for DRY mode activation
- **Energy Usage**: Track heating/cooling cycles
- **Connection Stability**: Monitor uptime and WiFi signal strength

### Regular Maintenance

1. **Sensor Calibration**: Periodically verify temperature sensor accuracy
2. **Filter Cleaning**: Maintain heat pump filters for optimal performance
3. **Firmware Updates**: Keep ESPHome and CN105 component updated
4. **Log Review**: Monitor logs for unusual patterns or errors

## 🤝 Contributing

This example demonstrates dual setpoint climate control for Mitsubishi CN105 heat pumps. Contributions to improve the configuration are welcome:

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Submit a pull request

## 📄 License

This example is part of the MitsubishiCN105ESPHome project and is licensed under the MIT License.

## 🙏 Acknowledgments

- **CN105 Component**: echavet/MitsubishiCN105ESPHome
- **ESPHome**: ESPHome project for the framework
- **Home Assistant**: For the integration platform

---

For more information and support, visit the main repository: [MitsubishiCN105ESPHome](https://github.com/echavet/MitsubishiCN105ESPHome)
