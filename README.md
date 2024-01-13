# MitsubishiCN105ESPHome

This component is an adaptation of the original author from https://github.com/geoffdavis/esphome-mitsubishiheatpump

What's new in it:
- Communication with Heatpump via UART has been improved to prevent the use of delays in esphome loop() which couldn't be done with the original SwiCago lib.
- Reading is done byte by byte in the loop() function. No loss, no lag because the component never stops reading and never blocks esphome.
- Writing on the UART is not followed by a instant reading response. Response is processed in loop() when received. 
- Code is fragmented in different concerns for bettter reading.
- Extra components : van orientation (with full support of the swicago map), compressor freq for energy monitoring, i-see sensor.
- Logs, many logs.
- refactoring, code can always be improved and still needs to.

But it keeps allowing, even if it not tested by many people, to Wirelessly control your Mitsubishi Comfort HVAC equipment with an ESP8266 or
ESP32(not tested) using the [ESPHome](https://esphome.io) framework.

Code is a mix of the famous SwiGago/Heatpump library (https://github.com/SwiCago/HeatPump) and the non less famous esphome Climate component (https://github.com/geoffdavis/esphome-mitsubishiheatpump).


## Features

- Instant feedback of command changes via RF Remote to HomeAssistant or MQTT.
- Direct control without the remote.
- Integrates a slightly rewriting of the [SwiCago/HeatPump](https://github.com/SwiCago/HeatPump) Arduino
  libary to talk to the unit directly via the internal `CN105` connector.
- Full modes vane orientation support (extra component is added from within the Core Climate Component)
- Thermostat on homeassistant with compressor Frequency (extra component is added from within the Core Climate Component)<br/>
  Allow to monitor power consumption in Home Assistant
  <BR/>
  <IMG src="captures/Climate%20Thermostat%20.png"/>

- With options buttons to configure the connection <br/>
  <IMG src="captures/Debug%20settings.png"/>

## Requirements

- ESPHome

## Supported Microcontrollers

This library should work on most ESP8266 or ESP32 platforms. It has been tested
with the following MCUs:

- WeMos D1 Mini (ESP8266) : tested
- Generic ESP32 Dev Kit (ESP32) : Not tested
- Generic ESP-01S board (ESP8266) : Not tested

## Supported Mitsubishi Climate Units

The underlying HeatPump library works with a number of Mitsubishi HVAC
units. Basically, if the unit has a `CN105` header on the main board, it should
work with this library. The [HeatPump
wiki](https://github.com/SwiCago/HeatPump/wiki/Supported-models) has a more
exhaustive list.

The same `CN105` connector is used by the Mitsubishi KumoCloud remotes, which
have a
[compatibility list](https://www.mitsubishicomfort.com/kumocloud/compatibility)
available.

The whole integration with the integrated and underlying HeatPump library has been
tested on the following units:

- `MSZ-SF50VE3`
- `MSZ-SF35VE3`

## Usage

### Step 1: Build a control circuit.

Build a control circuit with your MCU as detailed in the [SwiCago/HeatPump
README](https://github.com/SwiCago/HeatPump/blob/master/README.md#demo-circuit).
You can use either an ESP8266 or an ESP32 for this.

Note: several users have reported that they've been able to get away with
not using the pull-up resistors, and just [directly connecting a Wemos D1 mini
to the control
board](https://github.com/SwiCago/HeatPump/issues/13#issuecomment-457897457)
via CN105.

Note: using an external regulator like the AMS1117 makes the connexion far more reliable.
I have tested with and without regulator on ESP8266 Wemos D1, some can manage to communicate on the UART without regulator others don't. 
The they all do it well when a regulator is installed:.
You can find it here: https://www.amazon.fr/dp/B07CPXVDDN?ref=ppx_yo2ov_dt_b_product_details&th=1

Note: auto_update is required for the moment. Just because no auto_update has not been tested for connection loss. 

### Step 2: Use ESPHome

The code in this repository makes use of a number of features in the 1.18.0
version of ESPHome, including various Fan modes and
[external components](https://esphome.io/components/external_components.html).

### Step 3: Add this repository as an external component

Add this repository to your ESPHome config:

```yaml
external_components:
  - source: github://echavet/MitsubishiCN105ESPHome
```

### Step 4: Configure the heatpump

Add a `mitsubishi_heatpump` to your ESPHome config:

```yaml
climate:
  - platform: cn105 # remplis avec la plateforme de ton choix
    name: "My Heat Pump"
    hardware_uart: UART0
    update_interval: 4s
```

On ESP8266 you'll need to disable logging to serial because it conflicts with
the heatpump UART:

```yaml
logger:
  baud_rate: 0
```

or the redirect logs to UART1:

```yaml
logger:
  hardware_uart: UART1
```

Be aware that you will have to use a USB to TTL UART Serial converter and to share the mass with the heatpump.

On ESP32 you can change `hardware_uart` to `UART1` or `UART2` and keep logging
enabled on the main serial port.

_Note:_ this component DOES NOT use the ESPHome `uart` component, as it
requires direct access to a hardware UART via the Arduino `HardwareSerial`
class. The Mitsubishi Heatpump units use an atypical serial port setting ("even
parity"). Parity bit support is not implemented in any of the existing
software serial libraries, including the one in ESPHome. There's currently no
way to guarantee access to a hardware UART nor retrieve the `HardwareSerial`
handle from the `uart` component within the ESPHome framework.

# Example configuration

Below is an example configuration which will include wireless strength
indicators and permit over the air updates. You'll need to create a
`secrets.yaml` file inside of your `esphome` directory with entries for the
various items prefixed with `!secret`.

```yaml
substitutions:
  name: hptest
  friendly_name: Test Heatpump

esphome:
  name: ${name}
  friendly_name: ${friendly_name}

esp8266:
  board: esp01_1m

# Enable logging
logger:
  hardware_uart: UART1
  level: DEBUG
  logs:
    chkSum: INFO

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password

  # Enable fallback hotspot (captive portal) in case wifi connection fails
  ap:
    ssid: "${friendly_name} Fallback Hotspot"
    password: !secret fallback_password

# Note: if upgrading from 1.x releases of esphome-mitsubishiheatpump, be sure
# to remove any old entries from the `libraries` and `includes` section.
#libraries:
# Remove reference to SwiCago/HeatPump

#includes:
# Remove reference to src/esphome-mitsubishiheatpump

captive_portal:

# Enable Home Assistant API
api:

ota:

# Enable Web server.
web_server:
  port: 80

external_components:
  - source: github://echavet/MitsubishiCN105ESPHome

climate:
  - platform: cn105 # remplis avec la plateforme de ton choix
    name: ${friendly_name}
    id: "clim_id"
    baud_rate: 0
    hardware_uart: UART0

    # this update interval is not the same as the original geoffdavis parameter
    # this one activate the autoupdate feature (or not is set to 0)
    # the underlying component is not a PollingComponent so the component just uses
    # the espHome scheduler to program the heatpump requests at the given interval.
    # the loop() is used for reading.
    update_interval: 4s
```

See the provided hp-debug.yaml and hp-sejour.yaml examples for more options. Or have a look at the original project as this is a copy of it.

## Other Implementations

The [gysmo38/mitsubishi2MQTT](https://github.com/gysmo38/mitsubishi2MQTT)
Arduino sketch also uses the `SwiCago/HeatPump`
library, and works with MQTT directly. The author of this implementation found
`mitsubishi2MQTT`'s WiFi stack to not be particularly robust, but the controls
worked fine. Like this ESPHome repository, `mitsubishi2MQTT` will automatically
register the device in your HomeAssistant instance if you have HA configured to do so.

There's also the built-in to ESPHome
[Mitsubishi](https://github.com/esphome/esphome/blob/dev/esphome/components/mitsubishi/mitsubishi.h)
climate component.
The big drawback with the built-in component is that it uses Infrared Remote
commands to talk to the Heat Pump. By contrast, the approach used by this
repository and it's underlying `HeatPump` library allows bi-directional
communication with the Mitsubishi system, and can detect when someone changes
the settings via an IR remote.

## Reference documentation

The author referred to the following documentation repeatedly:

- [ESPHome Custom Sensors Reference](https://esphome.io/components/sensor/custom.html)
- [ESPHome Custom Climate Components Reference](https://esphome.io/components/climate/custom.html)
- [ESPHome External Components Reference](https://esphome.io/components/external_components.html)
- [Source for ESPHome's Climate Component](https://github.com/esphome/esphome/tree/master/esphome/components/climate)
