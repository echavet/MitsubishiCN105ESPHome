# MitsubishiCN105ESPHome

## Warning: "main" branch has been merged with 'uart_config_change".

This is a major change in UART configuration. But not so scary!
If you upgrade to the head of main you will have to change the way you configure the uart in your yaml files. Look at the step 4 in this document.
The reason is #6. The old configuration did not allow to use ESP32 ESP-IDF framework.

If you don't want this change you must configure your external_components to point to the [tagged v1.0.3] https://github.com/echavet/MitsubishiCN105ESPHome/tree/v1.0.3 this way:

```yaml
external_components:
  - source: github://echavet/MitsubishiCN105ESPHome@v1.0.3
```

This component is an adaptation of [geoffdavis's esphome-mitsubishiheatpump](https://github.com/geoffdavis/esphome-mitsubishiheatpump). Its purpose is to integrate the Mitsubishi heat pump protocol (enabled by the [SwiCago library](https://github.com/SwiCago/HeatPump)) directly into the ESPHome component classes for a more seamless integration.

## What's New:

- Enhanced UART communication with the Heatpump to eliminate delays in the ESPHome loop(), which was a limitation of the original [SwiCago library](https://github.com/SwiCago/HeatPump).
- Byte-by-byte reading within the loop() function ensures no data loss or lag, as the component continuously reads without blocking ESPHome.
- UART writes are followed by non-blocking reads. The responses are accumulated byte-by-byte in the loop() method and processed when complete, allowing command stacking without delays for a more responsive UI.
- Code is divided into distinct concerns for better readability.
- Additional components: vane orientation (fully supporting the Swicago map), compressor frequency for energy monitoring, and i-see sensor.
- Extensive logging for easier troubleshooting and development.
- Ongoing refactoring to further improve the code quality.

## Retained Features:

This project maintains all functionalities of the original geoffdavis project, including:

- Wireless Mitsubishi Comfort HVAC equipment control via ESP8266 or ESP32, using the [ESPHome](https://esphome.io) framework.
- Instant feedback of command changes via RF Remote to HomeAssistant or MQTT.
- Direct control independent of the remote.
- A slightly modified version of the [SwiCago/HeatPump](https://github.com/SwiCago/HeatPump) Arduino library for direct communication via the internal `CN105` connector.
- Full modes vane orientation support (added as an extra component within the Core Climate Component).
- Thermostat in HomeAssistant with compressor Frequency monitoring (an extra component within the Core Climate Component).

## Requirements:

- ESPHome

## Supported Microcontrollers:

- WeMos D1 Mini (ESP8266): tested
- M5Stack ATOM Lite : tested
- Generic ESP32 Dev Kit (ESP32): tested

## Supported Mitsubishi Climate Units:

Primarily, units with a `CN105` header are compatible. Refer to the [HeatPump wiki](https://github.com/SwiCago/HeatPump/wiki/Supported-models) for a comprehensive list.
Tested units include:

- `MSZ-SF50VE3`
- `MSZ-SF35VE3`

## Usage:

### Step 1: Building the Control Circuit

Follow the [SwiCago/HeatPump README](https://github.com/SwiCago/HeatPump/blob/master/README.md#demo-circuit) for building a control circuit using either an ESP8266 or ESP32.

### Step 2: Using ESPHome

This code utilizes features in ESPHome 1.18.0, including various Fan modes and [external components](https://esphome.io/components/external_components.html).

### Step 3: Add as an External Component

In your ESPHome config, add this repository:

```yaml
external_components:
  - source: github://echavet/MitsubishiCN105ESPHome
```

### Step 4: Configuring the Heatpump

In your ESPHome config, configure an uart and add a `cn105` component:

```yaml
uart:
  id: HP_UART
  baud_rate: 2400
  tx_pin: 1
  rx_pin: 3

climate:
  - platform: cn105 # Choose your platform
    name: "My Heat Pump"
    update_interval: 4s
```

Note: The `update_interval` is set here to 4s for debugging purposes. However, it is recommended to use a interval longer or equal to 1s because the underlying process divides this interval into three separate requests.

For ESP8266, disable logging to serial to avoid conflicts:

```yaml
logger:
  baud_rate: 0
```

Or redirect logs to UART1:

```yaml
logger:
  hardware_uart: UART1
```

## Example Configuration:

Below is a sample configuration including wireless strength indicators and OTA updates. You'll need a `secrets.yaml` file with the necessary credentials.

```yaml
substitutions:
  name: hptest
  friendly_name: Test Heatpump

esphome:
  name: ${name}
  friendly_name: ${friendly_name}

esp8266:
  board: d1_mini

# Enable logging
logger:
  hardware_uart: UART1
  level: INFO

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password

  # Enable fallback hotspot (captive portal) in case wifi connection fails
  ap:
    ssid: "${friendly_name} Fallback Hotspot"
    password: !secret fallback_password

# Enable Home Assistant API
api:

ota:

external_components:
  - source: github://echavet/MitsubishiCN105ESPHome

uart:
  id: HP_UART
  baud_rate: 2400
  tx_pin: 1
  rx_pin: 3

climate:
  - platform: cn105 
    name: ${friendly_name}
    id: "clim_id"

    # this update interval is not the same as the original geoffdavis parameter
    # this one activates the autoupdate feature (or not is set to 0)
    # the underlying component is not a PollingComponent so the component just uses
    # the espHome scheduler to program the heatpump requests at the given interval.
    # 3 requests are sent each update_interval with an interval of update_interval/4 or 300ms.
    update_interval: 4s
```


## External temperature

This example uses a homeassistant sensor to update the room temperature and uses esp32 with esp-idf framework which allows to keep logging enabled on the main serial port while another UART is configured for the heatpump connection.

```yaml
substitutions:
  name: "esp32-hp"
  friendly_name: Clim Sejour

esphome:
  name: ${name}
  friendly_name: ${friendly_name}

esp32:
  board: esp32doit-devkit-v1
  framework:
    type: esp-idf

# Enable logging
logger:    
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

api:
  services:    
    - service: set_remote_temperature
      variables:
        temperature: float
      then:
        - lambda: 'id(esp32_clim).set_remote_temperature(temperature);'

    - service: use_internal_temperature
      then:
        - lambda: 'id(esp32_clim).set_remote_temperature(0);'
  encryption:
    key: !secret encryption_key

ota:  
  password: !secret ota_pwd

# external temperature
sensor:
  - platform: homeassistant
    id: ha_cdeg_sejour_et_cuisine
    entity_id: sensor.cdeg_sejour_et_cuisine
    internal: true
    on_value:
      then:
        - lambda: |-
            id(esp32_clim).set_remote_temperature(x);

uart:
  id: HP_UART
  baud_rate: 2400
  tx_pin: GPIO17
  rx_pin: GPIO16

climate:
  - platform: cn105  
    name: ${friendly_name}
    id: "esp32_clim"
    compressor_frequency_sensor:
      name: Compressor frequency (clim Sejour)    
    update_interval: 10s         # shouldn't be less than 1 second
```

Another example with a physical sensor and a throttle average filter:

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
          - lambda: 'id(use_your_name).set_remote_temperature(x);'
```

For more configuration options, see the provided hp-debug.yaml and hp-sejour.yaml examples or refer to the original project.

## Other Implementations:

- [esphome-mitsubishiheatpump](https://github.com/geoffdavis/esphome-mitsubishiheatpump) - The original esphome project from which this one is forked.
- [gysmo38/mitsubishi2MQTT](https://github.com/gysmo38/mitsubishi2MQTT) - Direct MQTT controls, robust but with a less stable WiFi stack.
- ESPHome's built-in [Mitsubishi](https://github.com/esphome/esphome/blob/dev/esphome/components/mitsubishi/mitsubishi.h) climate component - Uses IR Remote commands, lacks bi-directional communication.

## Reference Documentation:

Refer to these for further understanding:

- [ESPHome Custom Sensors](https://esphome.io/components/sensor/custom.html)
- [ESPHome Custom Climate Components](https://esphome.io/components/climate/custom.html)
- [ESPHome External Components](https://esphome.io/components/external_components.html)
- [ESPHome's Climate Component Source](https://github.com/esphome/esphome/tree/master/esphome/components/climate)

---
