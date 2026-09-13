# Mitsubishi Climate Proxy

![Mitsubishi Electric Logo](https://upload.wikimedia.org/wikipedia/commons/thumb/b/b7/Mitsubishi_Electric_logo.svg/320px-Mitsubishi_Electric_logo.svg.png)

A Home Assistant custom component that acts as a wrapper for the Mitsubishi CN105 ESPHome entity.

## Problem Solved

The standard Home Assistant UI does not dynamically update the number of temperature sliders (Single vs Dual) when the ESPHome entity changes traits (e.g. from Heat to Heat/Cool).
This component wraps the ESPHome entity and provides a "Proxy" entity that:

*   Preserves native modes and **single-temperature controls** for single-setpoint sources.
*   Shows **1 Slider** in **Heat** and **Cool**, and **2 Sliders** in **Heat/Cool**, for dual-setpoint sources.
*   Intelligently maps single-setpoint adjustments to the underlying dual-setpoint ESPHome entity.
*   Exposes **independent horizontal vane (WideVane)** control via the HA-native `swing_horizontal_mode` API (requires HA 2024.12+).

## Prerequisites

Both single-setpoint and dual-setpoint ESPHome climate sources are supported.
**You do not need to enable `dual_setpoint` to use the proxy or mode restoration.**
Single-setpoint sources keep their native `auto` mode and forward one target temperature.
The proxy only lists modes advertised by the source.

If you want two target temperatures in `heat_cool`, configure the source to support
both `HEAT_COOL` and `dual_setpoint`:

```yaml
climate:
  - platform: cn105
    # ... other settings
    supports:
      mode: [COOL, HEAT, FAN_ONLY, DRY, AUTO, HEAT_COOL] # Add HEAT_COOL
      dual_setpoint: true   # Required for the two-temperature HEAT_COOL configuration
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
    *   *(Optional)* Select the horizontal vane select entity (e.g., `select.living_room_horizontal_vane`) to enable WideVane control in the climate card.
5.  Click **Submit**.

Your new entity will be created immediately.

## Turn-on mode

The proxy handles `climate.turn_on` by restoring its last observed active mode.
For example, **cool → off → turn on** returns to **cool**. It also remembers mode
changes made directly on the source entity or with the AC remote once ESPHome
reports them. The saved mode survives normal Home Assistant restarts and integration
reloads, including when the unit is off. Off, unknown and unavailable states do not
erase it. An abrupt shutdown can lose changes since Home Assistant's last state save.

Configure these settings when adding the integration, or open its **Configure/options**
dialog under **Settings → Devices & Services** for an existing proxy:

| Option | Default | Behavior |
|---|---|---|
| `restore_last_mode` | `true` | Restore the last observed active mode when supported by the source. Disable to always use the configured default. |
| `default_turn_on_mode` | `default` | Used when restoration is disabled or no supported saved mode is available. Choices: `default`, `heat_cool`, `auto`, `heat`, `cool`, `dry`, `fan_only`. |

With `default`, the proxy selects the first mode actually supported by the source in
this order: **heat_cool → auto → heat → cool → dry → fan_only**. This extends Home
Assistant's fallback preference by including native `auto` before explicit heating.
It reports an error if the source has no supported active mode.

Alternatively, select a fixed mode supported by your source climate entity. If neither
the saved mode nor that fixed mode is supported, turn-on reports an error instead of
selecting another mode.
Single-setpoint sources remember and restore native `auto` unchanged. For dual-setpoint
sources that also advertise `heat_cool`, the existing HomeKit workaround still presents
and remembers `auto` as `heat_cool`.

Calling turn-on while already running leaves the current mode unchanged. Explicit
`climate.set_hvac_mode` commands still select the requested mode. In coordinator
single-target mode, turn-on enables the room and leaves mode selection to the coordinator;
these two settings do not override it.

**Target the proxy entity** in your dashboards, automations and voice assistant exposure.
Commands targeting the original ESPHome entity keep Home Assistant's original behavior.
Updating this custom integration requires a Home Assistant restart; no firmware flash is needed.

## Configuration (YAML Method - Legacy)

If you prefer to define your entities in YAML, you can still add this to your `configuration.yaml`.

```yaml
# /config/configuration.yaml

climate:
  - platform: mitsubishi_climate_proxy
    source_entity: climate.chambre_esphome     # The ID of your real ESPHome entity
    name: Chambre Hybrid                       # The name of the new entity to use in your Dashboard
    restore_last_mode: true                     # Resume the last active mode
    default_turn_on_mode: default               # Pick the first supported mode, or specify a fixed mode
    horizontal_vane_entity: select.chambre_horizontal_vane  # (Optional) WideVane select entity
```

## Dashboard Setup

Edit your Thermostat card to point to this new proxy entity instead of the original ESPHome entity:

```yaml
type: thermostat
entity: climate.living_room_climate # Use the new proxy entity here
```

## How it works

### Temperature Setpoints

For single-setpoint sources, the proxy reads and forwards `temperature` directly
(with unit conversion when needed), including in native `auto`. It never advertises
temperature-range controls for these sources and rejects range commands.

For dual-setpoint sources:

*   **Heat Mode**: Controls `target_temp_low`.
*   **Cool Mode**: Controls `target_temp_high`.
*   **Auto Mode**: When the source also advertises `heat_cool`, presents it as `heat_cool` to preserve the existing HomeKit range display. Otherwise, preserves `auto` and controls the midpoint of the range.
*   **Heat/Cool Mode**: Exposes both Low and High setpoints.

### Horizontal Vane (WideVane)
When a `horizontal_vane_entity` is configured, the proxy:
*   Reads the current vane position from the ESPHome `select` entity
*   Exposes it as `swing_horizontal_mode` (HA-native, requires HA **2024.12+**)
*   Forwards position changes via the `select.select_option` service
*   Updates in real-time when the vane position changes (state tracking)

This allows the WideVane to appear directly in the standard climate card alongside the vertical swing.

### Coordinator Single-Target Mode (optional, off by default)
A multi-zone Mitsubishi MXZ outdoor unit can only run **one mode at a time**, so running each indoor
head in hardware AUTO causes the well-known "idle head starves the other" standby deadlock. The usual
fix is an external **coordinator** that keeps every head in one explicit shared mode and drives each
room to a single target. This mode turns the proxy into the thermostat *surface* for such a
coordinator-owned head:

*   Presents a **single target temperature** (OFF / HEAT / COOL only — no `heat_cool`, no dual range).
*   **Reports the coordinator's current shared mode** and masks the firmware's `fan_only`/`idle` so
    HomeKit/Google show a clean "to X°, idle" instead of a scary `fan_only` tile.
*   **Redirects writes to the coordinator's helpers** (never the firmware directly): setting the
    temperature writes the room's `input_number` target and enables the room; OFF clears the room
    enable; both fire a recompute event for the coordinator. Toggle it live via the **Options flow** —
    the entry reloads in place, so the **HomeKit accessory ID stays stable** (no re-pair).

Enable it with `coordinator_single_target: true` plus a `room_key`. The helper names are fully
configurable (defaults shown):

| Option | Default | Purpose |
|---|---|---|
| `room_key` | *(required)* | zone key `K`, e.g. `primary` |
| `helper_prefix` | `hvac` | builds `input_number.<prefix>_<K>_target` / `input_boolean.<prefix>_<K>_enable` |
| `shared_mode_entity` | `input_select.hvac_shared_mode` | the coordinator's current mode (cool/heat) |
| `season_entity` | `input_select.hvac_season` | season fallback (cooling/heating) |
| `recompute_event` | `mxz_recompute` | event fired after a write to nudge the coordinator |
| `comfort_offset` | `6.0` | °F applied to the far band edge in non-coordinator dual-setpoint writes |

**Default is off**, in which case behavior is identical to a plain proxy. The companion coordinator,
**[ha-mxz-coordinator](https://github.com/dkpnw/ha-mxz-coordinator)**, is now a **one-click HACS
integration** (config-flow): add the repo to HACS, download it, then add the integration from the UI
and pick your heads and temperature sensors — no YAML editing.

> **Which coordinator install to pair with:** the `input_number.*` / `input_boolean.*` /
> `input_select.*` writes documented above match ha-mxz-coordinator's **legacy YAML package**. Its
> **v2.0.0 HACS integration** instead owns its helpers as **`number.*` / `switch.*` / `select.*`**
> entities, which this proxy mode does not write to yet — so pair the proxy's single-target mode with
> the **YAML package** for now. The `mxz_recompute` event nudge works with either.

## Under the Hood: How it solves the UI Glitch

### The Problem
Native ESPHome entities declare their capabilities (Traits) statically. If an entity supports "Dual Setpoints" for *one* mode, Home Assistant forces the Dual Setpoint UI (two sliders) for *all* modes.

### The Solution: Dynamic Feature Masking
This component uses the **Proxy Pattern**. It mirrors the state of your real ESPHome device but intercepts the `supported_features` flag before sending it to Home Assistant's frontend.

*   **When in `HEAT` or `COOL` mode:** A dual-setpoint source is presented with one target control, mapped to the relevant range boundary.
*   **When in `HEAT_COOL` mode:** A source supporting temperature ranges is presented with two target controls.
*   **When a horizontal vane entity is configured:** The component adds `SWING_HORIZONTAL_MODE` to the features, making the horizontal swing selector appear in the climate UI.

### Fahrenheit Compatibility

If you use `fahrenheit_compatibility: "standard"` in your ESPHome YAML, the source climate entity
already exposes all temperatures in **°F**. Without correction, Home Assistant would apply a
*second* °F conversion on top, resulting in values ~2.26× too high (e.g. 69 °F → 156 °F).

The proxy automatically detects the source unit and normalises all values to **°C** internally,
so Home Assistant always receives and sends Celsius regardless of the source's unit setting.
**No extra configuration is required** — the fix is transparent.

### Maintenance & Stability
This component is designed as a **"Thin Wrapper"**.
*   It contains **no network code**. It does not talk to the device directly.
*   It relies on the official ESPHome integration to handle connection, protocol (API/MQTT), and state updates.
*   This makes it highly resistant to updates. As long as the underlying entity remains a valid `climate` entity in Home Assistant, this wrapper will work.

## Disclaimer

This project is not affiliated with, endorsed by, or associated with Mitsubishi Electric Corporation. "Mitsubishi Electric" and the three-diamond logo are registered trademarks of Mitsubishi Electric Corporation. The use of these trademarks in this project is for identification purposes only, to indicate compatibility with their products.
