# Example configuration for a Seeed Studios Xiao ESP32S3 device

This example utilizes a number of advanced ESPHome features, and includes experimental features.

- Splits features into "packages" stored in the /common folder for easier configuration adjustments.
- Requires a `secrets.yaml` file to populate fields like your OTA password and wifi credentials.
- Includes the basic provisions for a HomeAssistant Bluetooth Proxy.
- Includes wiring for two DS18B20 temperature sensors to measure intake and outlet temperature.
- Attempts to guess the fan CFM based on the indoor unit model and a lookup table with values from the service manual.
- If it has both, calculates actual thermal power transfer.

For more discussion on the measurement aspects, see this discussion: https://github.com/echavet/MitsubishiCN105ESPHome/discussions/228

To use in a more simple configuration, simply remove the lines for the `heatpump-intake-sensors.yaml` and `heatpump-fanspeeds.yaml` packages.

## Wiring configuration

TODO: Add photo/graphic

CN105 connector to GPIO43 and GPIO44

Temperature sensors connected to GPIO3 as a one-wire bus

![408890061-c33e45d2-8174-49f0-bfdc-94b867311022](https://github.com/user-attachments/assets/a6b2fe08-1ab0-49f1-ad36-d51e5db8e9d4)
