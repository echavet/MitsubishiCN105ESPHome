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

### Solution 2: Automatic Workaround (Since v1.3.5)

**GOOD NEWS**: Starting from version 1.3.5 of the CN105 component, an automatic workaround has been integrated into the code. The component automatically resets UART GPIO pins during initialization to prevent cold boot issues.

You no longer need to manually add the workaround in your YAML.

### Solution 3: Manual Workaround (Legacy Method)

If you are using an earlier version of the component, you can add this workaround in your YAML:

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

The automatic workaround has been implemented in `components/cn105/componentEntries.cpp`:

```cpp
void CN105Climate::setup() {
    ESP_LOGD(TAG, "Component initialization: setup call");

    // Workaround for ESP-IDF 5.4.1 GPIO regression
    // Reset GPIO pins to ensure proper UART initialization after cold boot
    if (this->tx_pin_ >= 0) {
        gpio_reset_pin((gpio_num_t)this->tx_pin_);
        ESP_LOGI(TAG, "Reset TX pin %d for ESP-IDF 5.4.1 workaround", this->tx_pin_);
    }
    if (this->rx_pin_ >= 0) {
        gpio_reset_pin((gpio_num_t)this->rx_pin_);
        ESP_LOGI(TAG, "Reset RX pin %d for ESP-IDF 5.4.1 workaround", this->rx_pin_);
    }

    // ... rest of initialization code
}
```

## Migration

If you had already added the manual workaround in your YAML, you can now safely remove it. The component automatically handles this reset.

## References

- [ESP-IDF 5.4.2 Release Notes](https://github.com/espressif/esp-idf/releases/tag/v5.4.2)
- [ESPHome 2025.8.0 Changes](https://github.com/esphome/esphome/releases/tag/2025.8.0)
