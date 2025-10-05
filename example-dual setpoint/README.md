# Dual Setpoint Climate Control for Mitsubishi CN105 Heat Pumps

This example demonstrates how to implement intelligent dual setpoint climate control for Mitsubishi CN105 heat pump units using ESPHome and Home Assistant. The system provides comfort zones with both heating and cooling setpoints, humidity-based DRY mode activation, and multiple comfort presets.

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

## 🚀 Installation and Setup

### 1. Prerequisites

- ESPHome installed and configured
- Home Assistant with temperature and humidity sensors
- Mitsubishi CN105 heat pump unit
- ESP32-S3 development board

### 2. Hardware Connections

```yaml
# UART for CN105 communication
tx_pin: 15    # ESP32 -> CN105
rx_pin: 13    # CN105 -> ESP32

# Status LED
pin: GPIO21   # RGB LED data pin

# Reset Button
pin: GPIO0    # Boot button (active low)
```

### 3. Configuration Steps

1. **Download the Files**:
   Copy the three YAML files to your ESPHome configuration directory:
   - `living-room-split-unit.yaml`
   - `climate-common-meta-thermostat.yaml`
   - `climate-common-non-climate.yaml`

2. **Update Substitutions**:
   Edit `living-room-split-unit.yaml` and modify the substitutions section:
   ```yaml
   substitutions:
     hostname: your-device-name
     friendly_name: Your Device Name
     remote_temp_sensor: sensor.your_temperature_sensor
     remote_humid_sensor: sensor.your_humidity_sensor
   ```

3. **Configure Secrets**:
   Create or update your `secrets.yaml`:
   ```yaml
   api_key_1: "your-api-encryption-key"
   wifi_ssid: "your-wifi-network"
   wifi_password: "your-wifi-password"
   ```

4. **Compile and Upload**:
   ```bash
   esphome run living-room-split-unit.yaml
   ```

### 4. Home Assistant Integration

1. **Add to ESPHome Integration**: The device will appear automatically in Home Assistant
2. **Configure Sensors**: Ensure your temperature and humidity sensors are available
3. **Test Services**: Use the custom services to test remote temperature control

## 🎛️ Usage and Operation

### Comfort Presets

The system provides 11 pre-configured comfort presets:

| Preset | Low Temp | High Temp | Mode | Fan | Use Case |
|--------|----------|-----------|------|-----|----------|
| Home in Summer | 70°F | 74°F | COOL | AUTO | Summer comfort |
| Home in Winter | 72°F | 78°F | HEAT | AUTO | Winter comfort |
| Home | 70°F | 75°F | HEAT_COOL | AUTO | General use |
| Sleep | 65°F | 72°F | HEAT_COOL | AUTO | Night time |
| Night | 65°F | 74°F | HEAT_COOL | AUTO | Evening |
| Away | 65°F | 78°F | HEAT_COOL | AUTO | Energy saving |
| Comfort | 72°F | 74°F | HEAT_COOL | HIGH | Precise control |
| Quick Cool | 65°F | 72°F | COOL | HIGH | Rapid cooling |
| Quick Heat | 70°F | 74°F | HEAT | HIGH | Rapid heating |
| Pre Cool | 65°F | 72°F | COOL | HIGH | Preparation |
| Fan Only | 70°F | 74°F | FAN_ONLY | AUTO | Circulation |

### Dual Setpoint Operation

1. **Set Comfort Zone**: Configure both low and high temperature setpoints
2. **Automatic Control**: The system automatically:
   - Heats when temperature drops below the low setpoint
   - Cools when temperature rises above the high setpoint
   - Maintains fan-only operation when within the comfort zone
3. **Humidity Control**: Automatically switches to DRY mode when humidity exceeds 65%

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

## 🐛 Troubleshooting

### Common Issues

1. **CN105 Communication Problems**:
   - Check UART connections (TX: GPIO15, RX: GPIO13)
   - Verify baud rate is 2400
   - Ensure proper wiring to CN105 unit

2. **Temperature Sensor Issues**:
   - Verify Home Assistant sensor entities exist
   - Check sensor data format (Celsius vs Fahrenheit)
   - Test with `use_internal_temperature` service

3. **Connection Problems**:
   - Check WiFi configuration
   - Verify API encryption key
   - Monitor status LED for connection state

### Debug Logging

Enable detailed logging for troubleshooting:

```yaml
logger:
  level: DEBUG
  logs:
    CN105: DEBUG
    climate: DEBUG
    DUAL_SETPOINT: DEBUG
    HUMIDITY_CONTROL: DEBUG
```

### Status LED Indicators

- **Off**: Connected to Home Assistant (normal operation)
- **Red Strobing**: Connection issue or error
- **Brightness**: Adjusts based on night mode setting

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
