# Mitsubishi Climate Proxy

![Mitsubishi Electric Logo](https://upload.wikimedia.org/wikipedia/commons/thumb/b/b7/Mitsubishi_Electric_logo.svg/320px-Mitsubishi_Electric_logo.svg.png)

A Home Assistant custom component that acts as a wrapper for the Mitsubishi CN105 ESPHome entity.

## Problem Solved

The standard Home Assistant UI does not dynamically update the number of temperature sliders (Single vs Dual) when the ESPHome entity changes traits (e.g. from Heat to Heat/Cool).
This component wraps the ESPHome entity and provides a "Proxy" entity that:

*   Shows **1 Slider** in **Heat**, **Cool**, and **Auto** modes.
*   Shows **2 Sliders** in **Heat/Cool** mode.
*   Intelligently maps single-setpoint adjustments to the underlying dual-setpoint ESPHome entity.

## Prerequisites

**On your ESPHome Configuration (YAML):**
You **MUST** enable `dual_setpoint` support in your climate configuration for the underlying entity to support the `HEAT_COOL` mode correctly.

```yaml
climate:
  - platform: cn105
    # ... other settings
    supports:
      mode: [COOL, HEAT, FAN_ONLY, DRY, AUTO, HEAT_COOL] # Add HEAT_COOL
      dual_setpoint: true   # <--- CRITICAL: Must be set to true
```

## Installation via HACS

1.  Add this repository to HACS as a Custom Repository.
2.  Install "Mitsubishi Climate Proxy".
3.  Restart Home Assistant.

## Configuration (UI Method - Recommended)

This integration now supports configuration directly via the Home Assistant user interface.

1.  Navigate to **Settings** > **Devices & Services**.
2.  Click the **+ ADD INTEGRATION** button at the bottom right.
3.  Search for **Mitsubishi Climate Proxy**.
4.  Follow the on-screen instructions:
    *   Select the source ESPHome entity (e.g., `climate.living_room_esphome`).
    *   Give your new proxy entity a name (e.g., `Living Room Climate`).
5.  Click **Submit**.

Your new entity will be created immediately.

## Configuration (YAML Method - Legacy)

If you prefer to define your entities in YAML, you can still add this to your `configuration.yaml`.

```yaml
# /config/configuration.yaml

climate:
  - platform: mitsubishi_climate_proxy
    source_entity: climate.chambre_esphome  # The ID of your real ESPHome entity
    name: Chambre Hybrid                    # The name of the new entity to use in your Dashboard
```

## Dashboard Setup

Edit your Thermostat card to point to this new proxy entity instead of the original ESPHome entity:

```yaml
type: thermostat
entity: climate.living_room_climate # Use the new proxy entity here
```

## How it works

*   **Heat Mode**: Controls `target_temp_low`.
*   **Cool Mode**: Controls `target_temp_high`.
*   **Auto Mode**: Controls the midpoint of the range (moving both low and high to maintain the spread).
*   **Heat/Cool Mode**: Exposes both Low and High setpoints.

## Under the Hood: How it solves the UI Glitch

### The Problem
Native ESPHome entities declare their capabilities (Traits) statically. If an entity supports "Dual Setpoints" for *one* mode, Home Assistant forces the Dual Setpoint UI (two sliders) for *all* modes.

### The Solution: Dynamic Feature Masking
This component uses the **Proxy Pattern**. It mirrors the state of your real ESPHome device but intercepts the `supported_features` flag before sending it to Home Assistant's frontend.

*   **When in `HEAT` or `AUTO` mode:** The component masks the "Dual Setpoint" capability. Home Assistant believes the device only supports a single target and renders **one slider**.
*   **When in `HEAT_COOL` mode:** The component reveals the "Dual Setpoint" capability. Home Assistant renders **two sliders**.

### Maintenance & Stability
This component is designed as a **"Thin Wrapper"**.
*   It contains **no network code**. It does not talk to the device directly.
*   It relies on the official ESPHome integration to handle connection, protocol (API/MQTT), and state updates.
*   This makes it highly resistant to updates. As long as the underlying entity remains a valid `climate` entity in Home Assistant, this wrapper will work.

## Disclaimer

This project is not affiliated with, endorsed by, or associated with Mitsubishi Electric Corporation. "Mitsubishi Electric" and the three-diamond logo are registered trademarks of Mitsubishi Electric Corporation. The use of these trademarks in this project is for identification purposes only, to indicate compatibility with their products.
