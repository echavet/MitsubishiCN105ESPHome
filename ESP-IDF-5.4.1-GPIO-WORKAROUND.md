# ESP-IDF 5.4.1 GPIO Regression Workaround

## Problem

A regression was introduced in ESP-IDF 5.4.1 that causes UART connection issues after a cold boot. This regression particularly affects components that use UART communication, such as the CN105 component for Mitsubishi heat pumps.

## Symptoms

- UART connection loss after a cold boot restart
- The CN105 component can no longer communicate with the heat pump
- OTA updates may work but the connection is lost after a restart

## Solutions

### Solution 1: Force ESP-IDF 5.4.0 (Recommended)

In your YAML file, force the ESP-IDF version to 5.4.0:

```yaml
esp32:
  board: esp32-s3-devkitc-1
  framework:
    type: esp-idf
    version: 5.4.0 # Force version 5.4.0
  variant: esp32s3
  flash_size: 8MB
```

### Solution 2: Manual Workaround (Required)

**IMPORTANT**: Due to the nature of ESP boot sequence, the GPIO reset must be performed before UART initialization. This requires adding a manual workaround in your YAML configuration.

The CN105 component cannot automatically add this workaround because it would need to modify the boot sequence before the component itself is initialized.

### Solution 3: Manual Workaround Implementation

Add this workaround in your YAML configuration:

```yaml
esphome:
  on_boot:
    - priority: 1001
      then:
        - lambda: |-
            gpio_reset_pin((gpio_num_t)00);  # Replace with your TX pin
            gpio_reset_pin((gpio_num_t)04);  # Replace with your RX pin
```

**Note**: Replace `00` and `04` with your actual TX and RX pin numbers.

## Technical Implementation

The workaround must be implemented at the ESPHome configuration level using the `on_boot` mechanism. This is because:

1. **Boot Sequence Timing**: The GPIO reset must happen before UART initialization
2. **Component Lifecycle**: The CN105 component is initialized after the UART, so it cannot modify the boot sequence
3. **ESPHome Architecture**: Only the `on_boot` mechanism can reliably execute code before component initialization

## Why Automatic Implementation is Not Possible

The CN105 component cannot automatically add this workaround because:

- The component is initialized after the UART configuration
- The `on_boot` sequence must be defined at the ESPHome configuration level
- Modifying the boot sequence from within a component would require complex workarounds that could break other components

## References

- [ESP-IDF 5.4.2 Release Notes](https://github.com/espressif/esp-idf/releases/tag/v5.4.2)
- [ESPHome 2025.8.0 Changes](https://github.com/esphome/esphome/releases/tag/2025.8.0)
