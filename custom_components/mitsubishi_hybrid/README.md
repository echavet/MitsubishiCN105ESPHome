# Mitsubishi Hybrid Climate

A Home Assistant custom component that acts as a wrapper for the Mitsubishi CN105 ESPHome entity.

## Problem Solved
The standard Home Assistant UI does not dynamically update the number of temperature sliders (Single vs Dual) when the ESPHome entity changes traits (e.g. from Heat to Heat/Cool).
This component wraps the ESPHome entity and provides a "Hybrid" entity that:
*   Shows **1 Slider** in **Heat**, **Cool**, and **Auto** modes.
*   Shows **2 Sliders** in **Heat/Cool** mode.
*   Intelligently maps single-setpoint adjustments to the underlying dual-setpoint ESPHome entity.

## Installation via HACS

1.  Add this repository to HACS as a Custom Repository.
2.  Install "Mitsubishi Hybrid Climate".
3.  Restart Home Assistant.

## Configuration

Add the following to your `configuration.yaml`:

```yaml
climate:
  - platform: mitsubishi_hybrid
    source_entity: climate.bedroom_climate  # The ID of your real ESPHome entity
    name: Bedroom Hybrid                # Name for the new wrapper entity
```

## How it works
*   **Heat Mode**: Controls `target_temp_low`.
*   **Cool Mode**: Controls `target_temp_high`.
*   **Auto Mode**: Controls the midpoint of the range (moving both low and high to maintain the spread).
*   **Heat/Cool Mode**: Exposes both Low and High setpoints.
