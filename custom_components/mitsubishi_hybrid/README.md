# Mitsubishi Hybrid Climate

A Home Assistant custom component that acts as a wrapper for the Mitsubishi CN105 ESPHome entity.

## Problem Solved
The standard Home Assistant UI does not dynamically update the number of temperature sliders (Single vs Dual) when the ESPHome entity changes traits (e.g. from Heat to Heat/Cool).
This component wraps the ESPHome entity and provides a "Hybrid" entity that:
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
2.  Install "Mitsubishi Hybrid Climate".
3.  Restart Home Assistant.

## Configuration in Home Assistant

⚠️ **Important Distinction**:
The code below belongs in your **Home Assistant's** `configuration.yaml` file (located in your `/config` folder), **NOT** in your ESPHome node configuration.

This configuration creates a new "Hybrid" entity in Home Assistant. You will then use this new entity in your Dashboards instead of the original one.

**Step 1: Define the entity**
Add this to your Home Assistant `configuration.yaml`:

```yaml
# /config/configuration.yaml

climate:
  - platform: mitsubishi_hybrid
    source_entity: climate.chambre_esphome  # The ID of your real ESPHome entity
    name: Chambre Hybrid                    # The name of the new entity to use in your Dashboard
```

**Step 2: Restart Home Assistant**
The new entity (e.g., `climate.chambre_hybrid`) will appear after a restart.

**Step 3: Update your Dashboard (Lovelace)**
Edit your Thermostat card to point to this new hybrid entity:

```yaml
type: thermostat
entity: climate.chambre_hybrid # Use the new hybrid entity here
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
