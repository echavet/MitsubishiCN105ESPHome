# Mitsubishi CN105 ESPHome

> [!WARNING]
> Due to a change in ESPHome 2025.2.0, some users are reporting build problems related to the loading of the `uptime_seconds_sensor` class. If you get a compile error for this reason, manually add an uptime sensor to your YAML configuration as below, clean your build files, and recompile. Once the root cause is identified this note will be removed.
>
> ```yaml
> sensor:
>   - platform: uptime
>     name: Uptime
> ```

> [!WARNING]
> Due to a change in ESPHome 2025.8.0, some users are facing UART connection issues after a cold boot. Forcing the firmware esphome version to a previous release (2025.7.5 and below) solves the issue (no cold boot required). Alternative is to force ESP32 IDF version to 5.4.0. Note that OTA updates to 2025.8.0+ may work but can break after a subsequent cold boot.
>
> _"commit 116c91e9c5fc6d0d32191bd4e6d6e406e2bff6bf Author: Jonathan Swoboda <154711427+swoboda1337@users.noreply.github.com> Date: Tue Jul 22 19:15:31 2025 -0400_
>
> _Bump ESP32 IDF version to 5.4.2 and Arduino version to 3.2.1 (#9770)"_
>
> [!IMPORTANT]
> Temporary fix included: This component now implements a fallback low-level UART reinitialization that triggers only if the initial (normal) connection fails at boot. It reconfigures the UART controller linked to your `uart:` block (clock source, reapplies baudrate, RX pull-up, flush, etc.). No YAML `on_boot` workaround is required. This aims to mitigate ESP-IDF 5.4.x regressions observed on some ESP32 at low baud (2400, 8E1).
>
> If this fallback still doesn’t work on your hardware, you can temporarily force ESP‑IDF 5.4.0 in your YAML:
>
> ```yaml
> esp32:
>   board: esp32-s3-devkitc-1
>   framework:
>     type: esp-idf
>     version: 5.4.0
>   variant: esp32s3
>   flash_size: 8MB
> ```

This project is a firmware for ESP32 microcontrollers supporting UART communication via the CN105 Mitsubishi connector. Its purpose is to enable complete control of a compatible Mitsubishi heat pump through Home Assistant, a web interface, or any MQTT client.

It uses the ESPHome framework and is compatible with the Arduino framework and ESP-IDF.

This component version is an adaptation of [geoffdavis's esphome-mitsubishiheatpump](https://github.com/geoffdavis/esphome-mitsubishiheatpump). Its purpose is to integrate the Mitsubishi heat pump protocol (enabled by the [SwiCago library](https://github.com/SwiCago/HeatPump)) directly into the ESPHome component classes for a more seamless integration.

The intended use case is for owners of a Mitsubishi Electric heat pump or air conditioner that includes a CN105 communication port to directly control their air handler or indoor unit using local communication through a web browser, or most commonly, the [HomeAssistant](https://www.home-assistant.io/) home automation platform. Installation requires the use of a WiFi capable ESP32 or ESP8266 device, modified to include a 5 pin plug to connect to the heat pump indoor unit. ESPHome is used to load the custom firmware onto the device, and the web browser or HomeAssistant software is used to send temperature setpoints, external temperature references, and settings to the heat pump. Installation requires basic soldering skills, and basic skills in flashing a firmware to a microcontroller (though ESPHome makes this as painless as possible).

The benefits include fully local control over your heat pump system, without reliance on a vendor network. Additional visibility, finer control, and even improved energy efficiency and comfort are possible when utilizing the remote temperature features.

> [!CAUTION]
> Use at your own risk.
> This is an unofficial implementation of the reverse-engineered Mitsubishi protocol based on the Swicago library. The authors and contributors have extensively tested this firmware across several similar implementations and forks. However, it's important to note that not all units support every feature. While free to use, it is at your own risk. If you are seeking an officially supported method to remotely control your Mitsubishi device via WiFi, a commercial solution is available [here](https://www.mitsubishi-electric.co.nz/wifi/).

### New Features

- Support Fahrenheit users better by mapping unit conversions to Mitsubishi's "creative" math, ensuring that HomeAssistant and external thermostats stay in sync. Thanks [@ams2990](https://github.com/ams2990) and [@dsstewa](https://github.com/dsstewa)!
- Additional components for supported units: vane orientation (fully supporting the Swicago map), compressor frequency for energy monitoring, and i-see sensor.
- Additional diagnostic sensors for understanding the behavior of the indoor units while in AUTO mode.
- Additional sensors for power usage and outdoor temperature (not supported by all units).
- Code is divided into distinct concerns for better readability.
- Extensive logging for easier troubleshooting and development.
- Ongoing refactoring to further improve the code quality.
- Enhanced UART communication with the Heatpump to eliminate delays in the ESPHome loop(), which was a limitation of the original [SwiCago library](https://github.com/SwiCago/HeatPump).
- Byte-by-byte reading within the loop() function ensures no data loss or lag, as the component continuously reads without blocking ESPHome.
- UART writes are followed by non-blocking reads. The responses are accumulated byte-by-byte in the loop() method and processed when complete, allowing command stacking without delays for a more responsive UI.

### Retained Features

This project maintains all functionalities of the original [geoffdavis](https://github.com/geoffdavis/esphome-mitsubishiheatpump) project, including:

- Wireless Mitsubishi Comfort HVAC equipment control via ESP8266 or ESP32, using the [ESPHome](https://esphome.io) framework.
- Instant feedback of command changes via RF Remote to HomeAssistant or MQTT.
- Direct control independent of the remote.
- A slightly modified version of the [SwiCago/HeatPump](https://github.com/SwiCago/HeatPump) Arduino library for direct communication via the internal `CN105` connector.
- Full mode and vane orientation support (added as an extra component within the Core Climate Component).
- Thermostat in HomeAssistant with compressor frequency monitoring (an extra component within the Core Climate Component).

## Requirements

- [ESPHome](https://esphome.io/) - Minimum version 1.18.0, installed independently or as an add-on in HomeAssistant

## Supported Microcontrollers

> [!IMPORTANT]
> ESP8266 boards such as the WeMos D1 Mini clones (LOLIN in particular) tend to be unreliable in this application, and may require an external voltage regulator to work. While some users have successfully used ESP8266 based devices, if you are purchasing new hardware for use with this project, it is recommended to focus on the more modern and powerful ESP32-S3 based devices.

- Generic ESP32 Dev Kit (ESP32): tested
- M5Stack ATOM Lite : tested
- M5Stack ATOM S3 Lite: tested w/ [modifications](https://github.com/echavet/MitsubishiCN105ESPHome/discussions/83)
- M5Stack NanoC6: [tested over both wifi and thread](https://github.com/echavet/MitsubishiCN105ESPHome/discussions/340)
- M5Stack StampS3
- Seeed Studios Xiao ESP32S3: tested
- WeMos D1 Mini Pro (ESP8266): tested (but not currently recommended, see above)

## Supported Mitsubishi Climate Units

Generally, indoor units with a `CN105` header are compatible. Refer to the [HeatPump wiki](https://github.com/SwiCago/HeatPump/wiki/Supported-models) for a comprehensive list. Additionally, Mitsubishi units listed as compatible with the [Mitsubishi PAC-USWHS002-WF-2 Kumo Cloud interface](https://mylinkdrive.com/USA/Controls/kumo_cloud/kumo_cloud_Devices/PAC_USWHS002_WF_2?product) will _likely_ be compatible with this project, as they use the same CN105 connector and serial protocol.

Units tested by project contributors include:

- `MSZ-SF50VE3`
- `MSZ-SF35VE3`
- `MSZ-GLxxNA`
- `MSZ-AP20VGK` (https://github.com/echavet/MitsubishiCN105ESPHome/discussions/83)
- `MSZ-AP42VGK`
- `MSZ-AP35VGD2` (https://github.com/echavet/MitsubishiCN105ESPHome/discussions/254)
- `MSZ-AY35VGKP`
- `MSZ-FSxxNA`
- `MSZ-FHxxNA`
- `MSZ-EF42VE`
- `MSXY-FN10VE` (https://github.com/echavet/MitsubishiCN105ESPHome/discussions/368)
- `MSZ-AP20VGK`
- `MSZ-FT50VG2`

## Usage

### Step 1: Building the Control Circuit

Follow the [SwiCago/HeatPump README](https://github.com/SwiCago/HeatPump/blob/master/README.md#demo-circuit) for building a control circuit using either an ESP8266 or ESP32.

### Step 2: Using ESPHome

Add a new device in your ESPHome dashboard. Create a yaml configuration file for the new device using the templates below, and flash to your device. Refer to the ESPHome documentation for guides on how to install ESPHome, add new devices, and flash the initial firmware.

- [Getting Started with ESPHome and HomeAssistant](https://esphome.io/guides/getting_started_hassio)
- [Installing ESPHome Locally](https://esphome.io/guides/installing_esphome)

> [!NOTE]
> This code uses the ESPHome [external components](https://esphome.io/components/external_components.html) integration feature. This means the project is not part of the ESPHome framework, it is an external component not managed by the core ESPHome project.

### Step 3: Configure the board and UART settings

Your ESPHome device configuration file starts with common defaults for ESPHome. To these defaults, add these minimum sections:

#### For ESP32-based Devices

```yaml
esp32:
  board: esp32doit-devkit-v1 #or esp32-s3-devkitc-1
  framework:
    type: esp-idf

uart:
  id: HP_UART
  baud_rate: 2400
  tx_pin: GPIO17
  rx_pin: GPIO16
```

#### For ESP8266-based Devices

```yaml
esp8266:
  board: d1_mini

uart:
  id: HP_UART
  baud_rate: 2400
  tx_pin: 1
  rx_pin: 3
```

### Step 4: Configure the climate component

Add these sections to load the external component, setup logging, and enable the climate entity.

```yaml
# External component reference
external_components:
  - source: github://echavet/MitsubishiCN105ESPHome

# Climate entity configuration
climate:
  - platform: cn105
    id: hp
    name: "My Heat Pump"
    update_interval: 2s # update interval can be adjusted after a first run and logs monitoring

# Default logging level
logger:
  #  hardware_uart: UART1 # Uncomment on ESP8266 devices
  level: INFO
```

#### Adjusting the `update_interval`

An ESPHome firmware implements the esphome::Component interface to be integrated into the Inversion Of Control mechanism of the ESPHome framework.
The main method of this process is the `loop()` method. MitsubishiCN105ESPHome performs a series of exchanges with the heat pump through a cycle. This cycle is timed, and its duration is displayed in the logs, provided the `CYCLE` logger is set to at least `INFO`.

If this is the case, you will see logs in the form:

```
[09:48:36][I][CYCLE:052]: 6: Cycle ended in 1.2 seconds (with timeout?: NO)
```

This will give you a good idea of your microcontroller's performance in completing an entire cycle. It is unnecessary to set the `update_interval` below this value.
In this example, setting an `update_interval` to 1500ms could be a fine tuned value.

> [!TIP]
> An `update_interval` between 1s and 4s is recommended, because the underlying process divides this into three separate requests which need time to complete. If some updates get "missed" from your heatpump, consider making this interval longer.

### Step 5: Optional components and variables

These optional additional configurations add customization and additional capabilities. The examples below assume you have added a substitutions component to your configuration file to allow for easy renaming, and that you have added a `secrets.yaml` file to your ESPHome configuration to hide private variables like your random API keys, OTA passwords, and Wifi passwords.

```yaml
substitutions:
  name: heatpump-1 # Do not use underscores, which are not fully compatible with mDNS
  friendly_name: My Heatpump 1
```

#### Climate component full example

This example adds support for configuring the temperature steps, adding an icon, and the optional climate sensors supported by SwiCago (but not supported by all indoor units), `compressor_frequency_sensor`, `vertical_vane_select`, `horizontal_vane_select` and `isee_sensor`. Supports many of the other features of the [ESPHome climate component](https://esphome.io/components/climate/index.html) as well for additional customization.

The `remote_temperature_timeout` setting allows the unit to revert back to the internal temperature measurement if it does not receive an update in the specified time range (highly recommended if using remote temperature updates).

`debounce_delay` adds a small delay to the command processing to account for some HomeAssistant buttons that may send repeat commands too quickly. A shorter value creates a more responsive UI, a longer value protects against repeat commands. (See https://github.com/echavet/MitsubishiCN105ESPHome/issues/21)

`connection_bootstrap_delay` delays the initial CN105 UART connection sequence (UART init + CONNECT handshake) **after boot**, to ensure the OTA log stream has time to attach. This is useful when troubleshooting cold-boot issues remotely: without a delay, the very first connection logs can be missed because the log client connects a few seconds after reboot. While this delay is active, the component will **not start communication cycles** until the heatpump replies with the connection success packet.

Recommended: start with `10s` and increase (e.g., `30s`) if you still miss early logs. A safety fallback starts anyway after 120s.

`installer_mode` enables an extended CN105 connection handshake (CONNECT command `0x5B`) instead of the standard handshake (`0x5A`). Some indoor units (notably some ducted SEZ variants) may require this to unlock installer/service privileges so that Function Settings (ISU / `hardware_settings`) return real values instead of `0`. Default is `false` for maximum compatibility.

`fahrenheit_compatibility` improves compatibility with HomeAssistant installations using Fahrenheit units. Mitsubishi uses a custom lookup table to convert F to C which doesn't correspond to the actual math in all cases. This can result in external thermostats and HomeAssistant "disagreeing" on what the current setpoint is. Setting this value to `standard` (or `alt` for alternative conversion tables) forces the component to use the same lookup tables, resulting in more consistent display of setpoints. Recommended for Fahrenheit users. (See https://github.com/echavet/MitsubishiCN105ESPHome/pull/298.)

`use_as_operating_fallback` in the `stage_sensor` enables a fallback mechanism for the activity indicator (idle/heating/cooling/etc.). By default, the activity status is based on the compressor running state. When this option is enabled, the system uses an OR logic: it shows active status if the compressor is running OR if the stage sensor indicates activity (not IDLE). This is particularly useful for 2-stage heating systems where the second stage (e.g., gas heating) may be active while the compressor is off. (See https://github.com/echavet/MitsubishiCN105ESPHome/issues/277 and https://github.com/echavet/MitsubishiCN105ESPHome/issues/469)

```yaml
climate:
  - platform: cn105
    id: hp
    name: "${friendly_name}"
    icon: mdi:heat-pump
    visual:
      min_temperature: 10 # Adjust to your unit's min temp. SmartSet units can go to 10C for heating
      max_temperature: 31
      temperature_step:
        target_temperature: 1
        current_temperature: 0.5
    # Fahrenheit compatibility mode - uses Mitsubishi's "custom" unit conversions, set to
    # "standard" (or "alt") for better support of Fahrenheit units in HomeAssistant.
    # Options: "disabled" (default), "standard", "alt"
    fahrenheit_compatibility: "disabled"
    # Timeout and communication settings
    remote_temperature_timeout: 30min
    update_interval: 2s
    debounce_delay: 100ms
    # Delay the initial UART/CONNECT bootstrap to avoid missing early OTA logs
    connection_bootstrap_delay: 30s
    # Optional: use extended CONNECT handshake (0x5B) for installer/service privileges
    installer_mode: false
    # Various optional sensors, not all sensors are supported by all heatpumps
    compressor_frequency_sensor:
      name: Compressor Frequency
      entity_category: diagnostic
      disabled_by_default: true
    outside_air_temperature_sensor:
      name: Outside Air Temp
      disabled_by_default: true
    vertical_vane_select:
      name: Vertical Vane
      disabled_by_default: false
    horizontal_vane_select:
      name: Horizontal Vane
      disabled_by_default: true
    isee_sensor:
      name: ISEE Sensor
      disabled_by_default: true
    stage_sensor:
      name: Stage
      # use_as_operating_fallback: false     # set to true for 2-stage systems or if activity indicator is unreliable
      entity_category: diagnostic
      disabled_by_default: true
    sub_mode_sensor:
      name: Sub Mode
      entity_category: diagnostic
      disabled_by_default: true
    auto_sub_mode_sensor:
      name: Auto Sub Mode
      entity_category: diagnostic
      disabled_by_default: true
    input_power_sensor:
      name: Input Power
      disabled_by_default: true
    kwh_sensor:
      name: Energy Usage
      disabled_by_default: true
    runtime_hours_sensor:
      name: Runtime Hours
      entity_category: diagnostic
      disabled_by_default: true
    air_purifier_switch:
      name: Air purifier
      disabled_by_default: true
    night_mode_switch:
      name: Night mode
      disabled_by_default: true
    circulator_switch:
      name: Circulator
      disabled_by_default: true
    airflow_control_select:
      name: Airflow Control
      disabled_by_default: true
    supports:
      # Explicitly control dual setpoint support in the UI/traits
      # Defaults to false when omitted
      dual_setpoint: true
      # You can still specify supported modes as before
      mode: [HEAT_COOL, COOL, HEAT, DRY, FAN_ONLY]
      fan_mode: [AUTO, QUIET, LOW, MEDIUM, HIGH]
      swing_mode: ["OFF", VERTICAL]
      # Specify which options to display in horizontal_vane_select dropdown
      # Defaults to all options: ["←←", "←", "|", "→", "→→", "←→", "SWING", "AIRFLOW CONTROL"]
      # Example to hide "←→" and "AIRFLOW CONTROL" if not supported by your unit:
      horizontal_vane_mode: ["←←", "←", "|", "→", "→→", SWING]
```

#### HEAT_COOL Mode and Dual Setpoint

Mitsubishi's AUTO mode is mapped to Home Assistant's `HEAT_COOL` mode. In this mode, the heat pump automatically switches between heating and cooling based on room temperature.

**Important limitation:** Mitsubishi heat pumps only accept a single temperature setpoint via the CN105 protocol. The heat pump manages its own internal hysteresis around that value.

When `dual_setpoint: true` is enabled, Home Assistant displays two temperature sliders (low and high). This component implements a **thermostat-like behavior with deadband** to manage these dual setpoints:

**How it works:**

| Room Temperature                | Heat Pump Setpoint | Behavior                    |
| ------------------------------- | ------------------ | --------------------------- |
| Below LOW setpoint              | Set to LOW         | Heat pump heats toward LOW  |
| Above HIGH setpoint             | Set to HIGH        | Heat pump cools toward HIGH |
| Between LOW and HIGH (deadband) | Follows room temp  | Heat pump stays idle        |

**Example with setpoints [18°C - 26°C]:**

```
Room at 22°C → Heat pump set to 22°C → Idle (setpoint = room temp)
Room drifts to 20°C → Heat pump set to 20°C → Idle
Room drops to 17°C → Heat pump set to 18°C → Heats toward 18°C
Room rises to 27°C → Heat pump set to 26°C → Cools toward 26°C
```

The room temperature can drift naturally within the deadband zone without the heat pump intervening. The heat pump only activates when the temperature crosses the LOW or HIGH boundaries.

> [!NOTE]
> The deadband algorithm runs automatically whenever the heat pump reports a new temperature reading. This ensures responsive control without manual intervention.

> [!NOTE] > **Transition delay in AUTO mode:** When operating in HEAT_COOL mode, the Mitsubishi heat pump may take several minutes (typically 5-15 minutes) to switch between heating and cooling after a significant temperature change. This is normal behavior - the heat pump uses its own internal logic to decide when to act, and it may remain idle temporarily even when the temperature crosses a setpoint boundary. The setpoint commands are sent immediately by the component, but the heat pump decides when to start operating.

#### Logger granularity

This firmware supports detailed log granularity for troubleshooting. Below is the full list of logger components and recommended defaults.

```yaml
logger:
  # hardware_uart: UART1 # Uncomment on ESP8266 devices
  level: INFO
  logs:
    EVT_SETS: INFO
    WIFI: INFO
    MQTT: INFO
    WRITE_SETTINGS: INFO
    SETTINGS: INFO
    STATUS: INFO
    # CN105 connection/bootstrap diagnostics (UART init + CONNECT handshake)
    CN105_CONN: INFO
    CN105Climate: WARN
    CN105: INFO
    climate: WARN
    sensor: WARN
    chkSum: INFO
    WRITE: WARN
    READ: WARN
    Header: INFO
    Decoder: INFO
    CONTROL_WANTED_SETTINGS: INFO
# Swap the above settings with these debug settings for development or troubleshooting
#  level: DEBUG
#  logs:
#    EVT_SETS : DEBUG
#    WIFI : INFO
#    MQTT : INFO
#    WRITE_SETTINGS : DEBUG
#    SETTINGS : DEBUG
#    STATUS : INFO
#    CN105Climate: WARN
#    CN105: DEBUG
#    climate: WARN
#    sensor: WARN
#    chkSum : INFO
#    WRITE : WARN
#    READ : WARN
#    Header: INFO
#    Decoder : DEBUG
#    CONTROL_WANTED_SETTINGS: DEBUG
```

### Step 6: Build the project and install

Build the project in ESPHome and install to your device. Install the device in your indoor unit connected to the CN105 port, and confirm that it powers up and connects to the Wifi. Visit the local IP address of the device, and confirm that you can change modes and temperature setpoints. HomeAssistant should now include a climate entity for your heatpump.

## Example Configuration - Minimal

This minimal configuration includes the basic components necessary for the firmware to operate. Note that you need to choose between the ESP32 and the ESP8266 sections to get the correct UART configuration. Utilizes a `secrets.yaml` file to store your credentials.

<details>

<summary>Minimal Configuration</summary>

```yaml
esphome:
  name: heatpump-1
  friendly_name: My Heatpump 1

# For ESP8266 Devices

#esp8266:

# board: d1_mini

#uart:

# id: HP_UART

# baud_rate: 2400

# tx_pin: 1

# rx_pin: 3

# For ESP32 Devices

esp32:
board: esp32doit-devkit-v1
framework:
type: esp-idf

uart:
id: HP_UART
baud_rate: 2400
tx_pin: GPIO17
rx_pin: GPIO16

external_components:

- source: github://echavet/MitsubishiCN105ESPHome

# Climate entity configuration

climate:

- platform: cn105
  name: "My Heat Pump"
  update_interval: 2s

# Default logging level

logger:

# hardware_uart: UART1 # Uncomment on ESP8266 devices

level: INFO

# Enable Home Assistant API

api:
encryption:
key: !secret api_key

ota:
platform: esphome # Required for ESPhome 2024.6.0 and greater
password: !secret ota_password

wifi:
ssid: !secret wifi_ssid
password: !secret wifi_password

# Enable fallback hotspot (captive portal) in case wifi connection fails

  ap:
    ssid: "Heatpump Fallback Hotspot"
    password: !secret fallback_password

captive_portal:

```

</details>

## Example Configuration - Complete

This example includes all the bells and whistles, optional components, remote temperature sensing, reboot button, and additional sensors in HomeAssistant including uptime, the current wifi SSID, and signal strength. Note that you need to choose between the ESP32 and the ESP8266 sections to get the correct UART configuration. Utilizes a `secrets.yaml` file to store your credentials. Comment out or remote features your unit doesn't support (such as the isee sensor or horizontal vane). It doesn't hurt to try them, but if your unit doesn't support that feature then it will be inactive.

<details>

<summary>Complete Configuration</summary>

```yaml
substitutions:
  name: heatpump-1
  friendly_name: My Heatpump 1
  remote_temp_sensor: sensor.my_room_temperature # Homeassistant sensor providing remote temperature

esphome:
  name: ${name}
  friendly_name: ${friendly_name}

# For ESP8266 Devices
#esp8266:
#  board: d1_mini
#
#uart:
#  id: HP_UART
#  baud_rate: 2400
#  tx_pin: 1
#  rx_pin: 3

# For ESP32 Devices
esp32:
  board: esp32doit-devkit-v1
  framework:
    type: esp-idf

uart:
  id: HP_UART
  baud_rate: 2400
  tx_pin: GPIO17
  rx_pin: GPIO16

external_components:
  - source: github://echavet/MitsubishiCN105ESPHome
#    refresh: 0s

wifi:
  ssid: !secret ssid
  password: !secret password
  domain: !secret domain

  # Enable fallback hotspot (captive portal) in case wifi connection fails
  ap:
    ssid: "${friendly_name} ESP"
    password: !secret fallback_password

captive_portal:

# Enable logging
logger:
#  hardware_uart: UART1 # Uncomment on ESP8266 devices
  level: INFO
  logs:
    EVT_SETS : INFO
    WIFI : INFO
    MQTT : INFO
    WRITE_SETTINGS : INFO
    SETTINGS : INFO
    STATUS : INFO
    CN105Climate: WARN
    CN105: INFO
    climate: WARN
    sensor: WARN
    chkSum : INFO
    WRITE : WARN
    READ : WARN
    Header: INFO
    Decoder : INFO
    CONTROL_WANTED_SETTINGS: INFO
#  level: DEBUG
#  logs:
#    EVT_SETS : DEBUG
#    WIFI : INFO
#    MQTT : INFO
#    WRITE_SETTINGS : DEBUG
#    SETTINGS : DEBUG
#    STATUS : INFO
#    CN105Climate: WARN
#    CN105: DEBUG
#    climate: WARN
#    sensor: WARN
#    chkSum : INFO
#    WRITE : WARN
#    READ : WARN
#    Header: INFO
#    Decoder : DEBUG
#    CONTROL_WANTED_SETTINGS: DEBUG

# Enable Home Assistant API
api:
  encryption:
    key: !secret api_key

sensor:
  - platform: homeassistant
    name: "Remote Temperature Sensor"
    entity_id: ${remote_temp_sensor} # Replace with your HomeAssistant remote sensor entity id, or include in substitutions
    internal: false
    disabled_by_default: true
    device_class: temperature
    state_class: measurement
    unit_of_measurement: "°C"
    filters:
    # Uncomment the lambda line to convert F to C on incoming temperature
    #  - lambda: return (x - 32) * (5.0/9.0);
      - clamp: # Limits values to range accepted by Mitsubishi units
          min_value: 1
          max_value: 40
          ignore_out_of_range: true
      - throttle: 30s
    on_value:
      then:
        - logger.log:
            level: INFO
            format: "Remote temperature received from HA: %.1f C"
            args: [ 'x' ]
        - lambda: 'id(hp).set_remote_temperature(x);'

ota:
  platform: esphome # Required for ESPhome 2024.6.0 and greater

# Enable Web server.
web_server:
  port: 80

# Sync time with Home Assistant.
time:
  - platform: homeassistant
    id: homeassistant_time

# Text sensors with general information.
text_sensor:
  # Expose ESPHome version as sensor.
  - platform: version
    name: ESPHome Version
  # Expose WiFi information as sensors.
  - platform: wifi_info
    ip_address:
      name: IP
    ssid:
      name: SSID
    bssid:
      name: BSSID

# Sensors with general information.
sensor:
  # Uptime sensor.
  - platform: uptime
    name: Uptime

  # WiFi Signal sensor.
  - platform: wifi_signal
    name: WiFi Signal
    update_interval: 120s

# Create a button to restart the unit from HomeAssistant. Rarely needed, but can be handy.
button:
  - platform: restart
    name: "Restart ${friendly_name}"

# Creates the sensor used to receive the remote temperature from Home Assistant
# Uses sensor selected in substitutions area at top of config
# Customize the filters to your application:
#   Uncomment the first line to convert F to C when remote temps are sent
#   If you have a fast or noisy sensor, consider some of the other filter
#   options such as throttle_average.
climate:
  - platform: cn105
    id: hp
    name: "${friendly_name}"
    icon: mdi:heat-pump
    visual:
      min_temperature: 10 # Adjust to your unit's min temp. SmartSet units can go to 10C for heating
      max_temperature: 31
      temperature_step:
        target_temperature: 1
        current_temperature: 0.5
    # Fahrenheit compatibility mode - uses Mitsubishi's "custom" unit conversions, set to
    # "standard" (or "alt") for better support of Fahrenheit units in HomeAssistant.
    # Options: "disabled" (default), "standard", "alt"
    fahrenheit_compatibility: "disabled"
    # Timeout and communication settings
    remote_temperature_timeout: 30min
    update_interval: 2s
    debounce_delay : 100ms
    # Various optional sensors, not all sensors are supported by all heatpumps
    compressor_frequency_sensor:
      name: Compressor Frequency
      entity_category: diagnostic
      disabled_by_default: true
    outside_air_temperature_sensor:
      name: Outside Air Temp
      disabled_by_default: true
    vertical_vane_select:
      name: Vertical Vane
      disabled_by_default: false
    horizontal_vane_select:
      name: Horizontal Vane
      disabled_by_default: true
    isee_sensor:
      name: ISEE Sensor
      disabled_by_default: true
    stage_sensor:
      name: Stage
      entity_category: diagnostic
      disabled_by_default: true
    sub_mode_sensor:
      name: Sub Mode
      entity_category: diagnostic
      disabled_by_default: true
    auto_sub_mode_sensor:
      name: Auto Sub Mode
      entity_category: diagnostic
      disabled_by_default: true
    input_power_sensor:
      name: Input Power
      disabled_by_default: true
    kwh_sensor:
      name: Energy Usage
      disabled_by_default: true
    runtime_hours_sensor:
      name: Runtime Hours
      entity_category: diagnostic
      disabled_by_default: true
```

</details>

## Methods for updating external temperature

There are several methods for updating the unit with an remote temperature value. This replaces the heat pump's internal temperature measurement with an external temperature measurement as the Mitsubishi wireless thermostats do, allowing you to more precisely control room comfort and improve energy efficiency by increasing cycle length.

### Recommended - Get external temperature from a [HomeAssistant Sensor](https://esphome.io/components/sensor/homeassistant.html) through the HomeAssistant API

Creates the sensor used to receive the remote temperature from Home Assistant. Uses sensor selected in substitutions area at top of config or manually entered into the sensor configuration. When the HomeAssistant sensor updates, it will send the new value to the ESP device, which will update the heatpump's remote temperature value.

Customize the filters to your application:

- Uncomment the first line to convert F to C when remote temps are sent.
- If you have a fast or noisy sensor, consider some of the other filter options such as throttle_average.

```yaml
sensor:
  - platform: homeassistant
    name: "Remote Temperature Sensor"
    entity_id: ${remote_temp_sensor} # Replace with your HomeAssistant remote sensor entity id, or include in substitutions
    internal: false
    disabled_by_default: true
    device_class: temperature
    state_class: measurement
    unit_of_measurement: "°C"
    filters:
      # Uncomment the lambda line to convert F to C on incoming temperature
      #  - lambda: return (x - 32) * (5.0/9.0);
      - clamp: # Limits values to range accepted by Mitsubishi units
          min_value: 1
          max_value: 40
          ignore_out_of_range: true
      - throttle: 30s
    on_value:
      then:
        - logger.log:
            level: INFO
            format: "Remote temperature received from HA: %.1f C"
            args: ["x"]
        - lambda: "id(hp).set_remote_temperature(x);"
```

### Alternate - Get external temperature from a networked sensor with a throttle filter

```yaml
sensor:
  - platform: pvvx_mithermometer
    mac_address: "A4:C1:38:XX:XX:XX"
    temperature:
      name: Thermometer
      id: temperature
      device_class: temperature
      state_class: measurement
      filters:
        throttle_average: 90s
      on_value:
        then:
          - lambda: "id(hp).set_remote_temperature(x);"
```

### Alternate - HomeAssistant Action

This example extends to default `api:` component to add a `set_remote_temperature` action that can be called from within HomeAssistant, allowing you to send a remote temperature value to the heat pump. You will need to include an automation in HomeAssistant to periodically call the action and update the temperature with `set_remote_temperature`, or disable remote temperature with `use_internal_temperature`. No longer recommended as the default method of updating remote temperature.

```yaml
api:
  encryption:
    key: !secret api_key
  services:
    - service: set_remote_temperature
      variables:
        temperature: float
      then:
        # Select between the C version and the F version
        # Uncomment just ONE of the below lines. The top receives the temperature value in C,
        # the bottom receives the value in F, converting to C here.
        - lambda: "id(hp).set_remote_temperature(temperature);"
    #        - lambda: 'id(hp).set_remote_temperature((temperature - 32.0) * (5.0 / 9.0));'
    - service: use_internal_temperature
      then:
        - lambda: "id(hp).set_remote_temperature(0);"
```

## Diagnostic Sensors

### Outside Air Temperature

This sensor reads the outdoor unit's air temperature reading, in 1.0 degree C increments. Not all outdoor units support this sensor. Some outdoor units will send an accurate value while the unit is operating, or in heat/cool mode, but will send -63.5C when offline.

```yaml
outside_air_temperature_sensor:
  name: Outside Air Temperature
```

Compatible units (as reported by users):

| Indoor         | Outdoor          | Temperature                        |
| -------------- | ---------------- | ---------------------------------- |
| MSZ-AP25VGD    | MXZ-4F80VGD      | Works                              |
| MSZ-AP35VGD    | MUZ-AP35VG       | Works but reports -63.5C when idle |
| MSZ-AP60VGD    | MUZ-AP60VG       | Works                              |
| MSZ-AP71VGD    | MUZ-AP71VG       | Works but reports -63.5C when idle |
| MSZ-AY35VGKP   | MUZ-AY35VG       | Works                              |
| MSZ-GLxxNA     | MXZ-SM42NAMHZ    | Works                              |
|                | MXZ-3C24NA2      | Not working                        |
| MSZ-RW25VG-SC1 | MUZ-RW25VGHZ-SC1 | Works                              |
| MSZ-FSxxNA     | MXZ-4C36NA2      | Works                              |
|                | MUZ-FD25NA       | Not working                        |
| MSZ-LN35       | MUZ-LN35         | Not working                        |
| MSZ-AP20VGK    | MXZ-4F83VF       | Works                              |
| MSZ-FT50VG2    | MUZ-FT50VG       | Works                              |

### Auto and Stage Sensors

The below sensors were added recently based on the work of others in sorting out other messages and bytes. The names are likely to change as we work to determine exactly what the units are doing.

```yaml
stage_sensor:
  name: Stage Sensor
sub_mode_sensor:
  name: Sub Mode Sensor
auto_sub_mode_sensor:
  name: Auto Sub Mode Sensor
```

- `stage_sensor` is the actual fan speed of the indoor unit. This is called stage in some documentation. Reported speeds include `IDLE`, `LOW`, `GENTLE`, `MEDIUM`, `MODERATE`, `HIGH` and `DIFFUSE`, named using Mitsubishi documentation conventions.

- `auto_sub_mode_sensor` indicates what actual mode the unit is in when in AUTO. Modes are `AUTO_OFF`, meaning AUTO is disabled, `AUTO_COOL`, meaning AUTO and cooling, `AUTO_HEAT`, meaning AUTO and heating and `AUTO_LEADER`, meaning this unit is the leader in a multi-head unit and selects the heat/cool mode that the others follow.

- `sub_mode_sensor` indicates additional detail on the current behavior of the unit. The Sub Modes are:
  - `NORMAL` - the unit is in an active mode (heat, cool, dry, etc.) and is either running, or waiting to run
  - `PREHEAT` - a cold-climate feature that electrically preheats the compressor windings prior to start of operation
  - `DEFROST` - a cold climate behavior that runs a short AC cycle during heating mode to melt ice from the coils
  - `STANDBY` - unit is off, or has been put into a "sleep" state through AUTO operation on another indoor unit

Some examples of how these all fit together: Unit 1 is in AUTO set to 20C and Unit 2 is in AUTO and set to 20C. Unit 1 senses that the room is 24C and tries to enter `AUTO_COOL`. If Unit 2 wants to heat the room it is in, it will enter `STANDBY` (and in the case of a few units tested, this mean it will go to "sleep" as if it is off, but not really be off) making Unit 1 enter `AUTO_LEADER` sub mode. In future releases, it is planned to make the ACTION in HA match these modes. But at this time this is not implemented.

It is also important to note that the Kumo adapter has many more settings that impact the behaviour above (such as thermal fan behaviour) and if you have set these the exact actions the untis take in these modes/submodes/stages is determined by those. Some of these can also be set by remotes and other devices. The setup you have will dictate the exact actions you see. If you have permutations, please share!

### UART Diagnostic Sensors

The following ESPHome sensors will not be needed by most users, but can be helpful in diagnosting problems with UART connectivity. Only implement if you are currently troubleshooting or developing new functionality.

```yaml
sensor:
  - platform: template
    name: "dg_uart_connected"
    entity_category: DIAGNOSTIC
    lambda: |-
      return (bool) id(hp).isUARTConnected_;
    update_interval: 30s
  - platform: template
    name: "dg_complete_cycles"
    entity_category: DIAGNOSTIC
    accuracy_decimals: 0
    lambda: |-
      return (unsigned long) id(hp).nbCompleteCycles_;
    update_interval: 60s
  - platform: template
    name: "dg_total_cycles"
    accuracy_decimals: 0
    entity_category: DIAGNOSTIC
    lambda: |-
      return (unsigned long) id(hp).nbCycles_;
    update_interval: 60s
  - platform: template
    name: "dg_nb_hp_connections"
    accuracy_decimals: 0
    entity_category: DIAGNOSTIC
    lambda: |-
      return (unsigned int) id(hp).nbHeatpumpConnections_;
    update_interval: 60s
  - platform: template
    name: "dg_complete_cycles_percent"
    unit_of_measurement: "%"
    accuracy_decimals: 1
    entity_category: DIAGNOSTIC
    lambda: |-
      unsigned long nbCompleteCycles = id(hp).nbCompleteCycles_;
      unsigned long nbCycles = id(hp).nbCycles_;
      if (nbCycles == 0) {
        return 0.0;
      }
      return (float) nbCompleteCycles / nbCycles * 100.0;
    update_interval: 60s
```

## Hardware Settings (Function Settings)

This advanced feature allows you to read and modify the internal "Function Settings" (ISU) of your Mitsubishi unit directly from Home Assistant. These settings control hardware behaviors like auto-restart, temperature sensing location, or static pressure.

> [!NOTE]
> This feature depends on your unit's compatibility. If your unit returns only zeros, it likely does not support reading/writing function settings via CN105, or it may require installer/service privileges. Try setting `installer_mode: true` if your unit supports Function Settings but reports `0` values (seen on some SEZ units). The component will automatically detect fully-zero responses and disable the polling to save resources.
> Note that the firmware autor's units do not support theses functions settings. So implementation might not be reliable.

### Configuration

Add the `hardware_settings` block to your configuration. You can choose which codes to expose and customize the labels.

```yaml
climate:
  - platform: cn105
    # ... your existing config ...

    # Configure the update interval for reading settings (default: 24h)
    # These settings rarely change, so a long interval is recommended.
    hardware_settings:
      update_interval: 20s
      list:
        # Code 101: Auto Restart
        - code: 101
          name: "Auto Restart after Power Failure"
          icon: "mdi:restart"
          options:
            1: "ON (Default)"
            2: "OFF"

        # Code 102: Temperature Sensing Source
        # Important for remote temperature control!
        - code: 102
          name: "Temperature Source"
          icon: "mdi:thermometer-check"
          options:
            1: "Indoor Unit (Default)"
            2: "Remote Controller"
            3: "External (CN105/WiFi)"

        # Code 103: Ventilation / Lossnay interaction
        - code: 103
          name: "Ventilation Link"
          options:
            1: "None (Default)"
            2: "With Lossnay"
            3: "Forced"

        # Code 105: Auto Energy Saving
        - code: 105
          name: "Auto Energy Saving"
          options:
            1: "ON (Default)"
            2: "OFF"

        # Code 107: Filter Sign Interval
        - code: 107
          name: "Filter Sign Interval"
          icon: "mdi:air-filter"
          options:
            1: "100 Hours (Default)"
            2: "2500 Hours"
            3: "No Indication"

        # Code 108: Ceiling Height / Static Pressure
        - code: 108
          name: "Ceiling Height Mode"
          icon: "mdi:arrow-expand-vertical"
          options:
            1: "Standard (Default)"
            2: "High Ceiling"
            3: "Low Ceiling"

        # Code 109: Number of Air Outlets (Cassette models only)
        - code: 109
          name: "Air Outlets"
          options:
            1: "4 Directions (Default)"
            2: "3 Directions"
            3: "2 Directions"

        # Code 110: Auto Mode Switching Logic
        - code: 110
          name: "Auto Mode Logic"
          icon: "mdi:sync"
          options:
            1: "Energy Saving (Default)"
            2: "Comfort / Performance"

        # Code 111: Vane Setting (for specific models)
        - code: 111
          name: "Vane Geometry"
          options:
            1: "Standard (Default)"
            2: "Type 1"
            3: "Type 2"

        # Code 117: Defrost Control
        - code: 117
          name: "Defrost Logic"
          icon: "mdi:snowflake-melt"
          options:
            1: "Standard (Default)"
            2: "High Humidity / Frequent"

        # Code 124: Heating Temperature Offset
        - code: 124
          name: "Heating Offset (+2°C)"
          options:
            1: "ON (Default)"
            2: "OFF"

        # Code 125: Fan behavior during Thermo-OFF (Heating)
        - code: 125
          name: "Fan during Thermo-OFF (Heat)"
          icon: "mdi:fan-off"
          options:
            1: "Extra Low (Default)"
            2: "Stop"
            3: "Set Speed"

        # Code 127: Fan behavior during Thermo-OFF (Cooling)
        - code: 127
          name: "Fan during Thermo-OFF (Cool)"
          icon: "mdi:fan-off"
          options:
            1: "Set Speed (Default)"
            2: "Stop"

        # Code 128: System Error Display
        - code: 128
          name: "Error Display on Remote"
          options:
            1: "ON (Default)"
            2: "OFF"


## Other Implementations

- [esphome-mitsubishiheatpump](https://github.com/geoffdavis/esphome-mitsubishiheatpump) - The original esphome project from which this one is forked.
- [gysmo38/mitsubishi2MQTT](https://github.com/gysmo38/mitsubishi2MQTT) - Direct MQTT controls, robust but with a less stable WiFi stack.
- ESPHome's built-in [Mitsubishi](https://github.com/esphome/esphome/blob/dev/esphome/components/mitsubishi/mitsubishi.h) climate component - Uses IR Remote commands, lacks bi-directional communication.

## Reference Documentation

Refer to these for further understanding:

- [ESPHome Custom Sensors](https://esphome.io/components/sensor/custom.html)
- [ESPHome Custom Climate Components](https://esphome.io/components/climate/custom.html)
- [ESPHome External Components](https://esphome.io/components/external_components.html)
- [ESPHome's Climate Component Source](https://github.com/esphome/esphome/tree/master/esphome/components/climate)

---
```
