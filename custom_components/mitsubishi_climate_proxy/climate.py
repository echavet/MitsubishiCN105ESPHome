## climate.py — Mitsubishi Climate Proxy
## Role: Wraps an ESPHome CN105 climate entity to fix dual setpoint, F/C conversion,
##        and expose Mitsubishi-specific features (horizontal vane) via standard HA APIs.
## Deps: homeassistant.components.climate, homeassistant.helpers.event

import logging
from typing import Any, List, Optional
import voluptuous as vol

from homeassistant.components.climate import (
    ClimateEntity,
    PLATFORM_SCHEMA,
    ClimateEntityFeature,
    HVACMode,
)

try:
    # HA versions vary: `HVACAction` moved in some releases.
    from homeassistant.components.climate import HVACAction
except ImportError:  # pragma: no cover
    from homeassistant.components.climate.const import HVACAction
from homeassistant.const import (
    CONF_NAME,
    CONF_SOURCE,
    UnitOfTemperature,
    STATE_UNAVAILABLE,
    STATE_UNKNOWN,
)
import homeassistant.helpers.config_validation as cv
from homeassistant.core import Event, HomeAssistant, callback
from homeassistant.helpers.event import async_track_state_change_event
from homeassistant.helpers.typing import ConfigType, DiscoveryInfoType

_LOGGER = logging.getLogger(__name__)

CONF_SOURCE_ENTITY = "source_entity"
CONF_HORIZONTAL_VANE_ENTITY = "horizontal_vane_entity"
# Vertical vane positions exposed as swing modes. Mirrors the horizontal_vane pattern
# for units whose only powered vane is the up/down flap. When configured, the proxy's
# swing selector presents the vane position list (AUTO/up/down/SWING) instead of the
# firmware's off/vertical.
CONF_VERTICAL_VANE_ENTITY = "vertical_vane_entity"

# "coordinator single-target" mode. When enabled, this proxy stops being a thin firmware
# wrapper and becomes the Tesla-style single-target surface for a head owned by an external
# multi-zone coordinator (see the companion ha-mxz-coordinator package): it presents a
# SINGLE target temperature (no dual range, no heat_cool), masks the firmware's fan_only/
# idle state so HomeKit/Google never show a scary mode, and REDIRECTS writes to the
# coordinator's per-room helpers (never the firmware directly — the coordinator owns it).
# Mode follows the coordinator's shared mode; the seasonal lock + hysteresis in the
# coordinator prevent the heat/cool flip-flop of native AUTO. Default off => behavior
# identical to a plain proxy.
#
# Helper-name contract (all configurable): with helper_prefix=P and room_key=K the proxy
# reads/writes input_number.P_K_target and input_boolean.P_K_enable, and reads the
# coordinator's shared mode from shared_mode_entity. Defaults match ha-mxz-coordinator.
CONF_COORDINATOR_SINGLE_TARGET = "coordinator_single_target"
CONF_ROOM_KEY = "room_key"  # any zone key, e.g. "primary" -> input_number.<prefix>_<key>_target
CONF_HELPER_PREFIX = "helper_prefix"
CONF_SEASON_ENTITY = "season_entity"
CONF_SHARED_MODE_ENTITY = "shared_mode_entity"
CONF_RECOMPUTE_EVENT = "recompute_event"
CONF_COMFORT_OFFSET = "comfort_offset"
DEFAULT_HELPER_PREFIX = "hvac"
DEFAULT_SEASON_ENTITY = "input_select.hvac_season"
DEFAULT_SHARED_MODE_ENTITY = "input_select.hvac_shared_mode"
DEFAULT_RECOMPUTE_EVENT = "mxz_recompute"
DEFAULT_COMFORT_OFFSET = 6.0

PLATFORM_SCHEMA = PLATFORM_SCHEMA.extend(
    {
        vol.Required(CONF_SOURCE_ENTITY): cv.entity_id,
        vol.Optional(CONF_NAME): cv.string,
        vol.Optional(CONF_HORIZONTAL_VANE_ENTITY): cv.entity_id,
        vol.Optional(CONF_VERTICAL_VANE_ENTITY): cv.entity_id,
        vol.Optional(CONF_COORDINATOR_SINGLE_TARGET, default=False): cv.boolean,
        vol.Optional(CONF_ROOM_KEY): cv.string,
        vol.Optional(CONF_HELPER_PREFIX, default=DEFAULT_HELPER_PREFIX): cv.string,
        vol.Optional(CONF_SEASON_ENTITY, default=DEFAULT_SEASON_ENTITY): cv.string,
        vol.Optional(CONF_SHARED_MODE_ENTITY, default=DEFAULT_SHARED_MODE_ENTITY): cv.string,
        vol.Optional(CONF_RECOMPUTE_EVENT, default=DEFAULT_RECOMPUTE_EVENT): cv.string,
        vol.Optional(CONF_COMFORT_OFFSET, default=DEFAULT_COMFORT_OFFSET): vol.Coerce(float),
    }
)


async def async_setup_platform(
    hass: HomeAssistant,
    config: ConfigType,
    async_add_entities: Any,
    discovery_info: DiscoveryInfoType | None = None,
) -> None:
    """Set up the Mitsubishi Hybrid Climate platform via YAML."""
    source_entity_id = config[CONF_SOURCE_ENTITY]
    name = config.get(CONF_NAME)
    horizontal_vane_entity_id = config.get(CONF_HORIZONTAL_VANE_ENTITY)
    vertical_vane_entity_id = config.get(CONF_VERTICAL_VANE_ENTITY)

    async_add_entities(
        [MitsubishiHybridClimate(
            hass, name, source_entity_id,
            horizontal_vane_entity_id=horizontal_vane_entity_id,
            vertical_vane_entity_id=vertical_vane_entity_id,
            coordinator_single_target=config.get(CONF_COORDINATOR_SINGLE_TARGET, False),
            room_key=config.get(CONF_ROOM_KEY),
            helper_prefix=config.get(CONF_HELPER_PREFIX, DEFAULT_HELPER_PREFIX),
            season_entity=config.get(CONF_SEASON_ENTITY, DEFAULT_SEASON_ENTITY),
            shared_mode_entity=config.get(CONF_SHARED_MODE_ENTITY, DEFAULT_SHARED_MODE_ENTITY),
            recompute_event=config.get(CONF_RECOMPUTE_EVENT, DEFAULT_RECOMPUTE_EVENT),
            comfort_offset=config.get(CONF_COMFORT_OFFSET, DEFAULT_COMFORT_OFFSET),
        )],
        True,
    )


async def async_setup_entry(
    hass: HomeAssistant,
    entry: ConfigType,
    async_add_entities: Any,
) -> None:
    """Set up the Mitsubishi Hybrid Climate platform via Config Flow."""
    # Handle key difference between YAML and Config Flow
    if CONF_SOURCE in entry.data:
        source_entity_id = entry.data[CONF_SOURCE]
    else:
        source_entity_id = entry.data.get(CONF_SOURCE_ENTITY)

    name = entry.data.get(CONF_NAME)
    horizontal_vane_entity_id = entry.data.get(CONF_HORIZONTAL_VANE_ENTITY)
    vertical_vane_entity_id = entry.data.get(CONF_VERTICAL_VANE_ENTITY)

    # Options take precedence over data so the single-target mode can be toggled live
    # via the Options flow without delete/recreate (which would re-key the HomeKit AID).
    def _opt(key, default=None):
        if key in entry.options:
            return entry.options[key]
        return entry.data.get(key, default)

    async_add_entities(
        [MitsubishiHybridClimate(
            hass, name, source_entity_id, entry.entry_id,
            horizontal_vane_entity_id=horizontal_vane_entity_id,
            vertical_vane_entity_id=vertical_vane_entity_id,
            coordinator_single_target=_opt(CONF_COORDINATOR_SINGLE_TARGET, False),
            room_key=_opt(CONF_ROOM_KEY),
            helper_prefix=_opt(CONF_HELPER_PREFIX, DEFAULT_HELPER_PREFIX),
            season_entity=_opt(CONF_SEASON_ENTITY, DEFAULT_SEASON_ENTITY),
            shared_mode_entity=_opt(CONF_SHARED_MODE_ENTITY, DEFAULT_SHARED_MODE_ENTITY),
            recompute_event=_opt(CONF_RECOMPUTE_EVENT, DEFAULT_RECOMPUTE_EVENT),
            comfort_offset=_opt(CONF_COMFORT_OFFSET, DEFAULT_COMFORT_OFFSET),
        )],
        True,
    )


class MitsubishiHybridClimate(ClimateEntity):
    """Representation of a Mitsubishi Hybrid Climate device.

    Wraps an ESPHome CN105 climate entity to provide:
    - Adaptive single/dual setpoint based on current HVAC mode
    - Fahrenheit normalisation (avoids double F→C conversion)
    - Independent horizontal swing via HA 2024.12+ swing_horizontal_mode
    """

    def __init__(
        self,
        hass: HomeAssistant,
        name: str | None,
        source_entity_id: str,
        unique_id: str | None = None,
        horizontal_vane_entity_id: str | None = None,
        vertical_vane_entity_id: str | None = None,
        coordinator_single_target: bool = False,
        room_key: str | None = None,
        helper_prefix: str = DEFAULT_HELPER_PREFIX,
        season_entity: str = DEFAULT_SEASON_ENTITY,
        shared_mode_entity: str = DEFAULT_SHARED_MODE_ENTITY,
        recompute_event: str = DEFAULT_RECOMPUTE_EVENT,
        comfort_offset: float = DEFAULT_COMFORT_OFFSET,
    ) -> None:
        """Initialize the climate device."""
        super().__init__()
        self._hass = hass
        self._name = name or source_entity_id
        self._source_entity_id = source_entity_id
        self._source_state = None
        self._attr_should_poll = False
        self._attr_unique_id = unique_id or f"{source_entity_id}_hybrid"
        # Horizontal vane (WideVane) — optional select entity from ESPHome
        self._horizontal_vane_entity_id = horizontal_vane_entity_id
        self._horizontal_vane_state = None
        # Vertical vane — optional select entity; when set, its positions are presented
        # as this entity's swing modes.
        self._vertical_vane_entity_id = vertical_vane_entity_id
        self._vertical_vane_state = None
        # Coordinator single-target mode (Tesla-style surface).
        self._cst = bool(coordinator_single_target) and bool(room_key)
        self._room_key = room_key
        self._helper_prefix = (helper_prefix or DEFAULT_HELPER_PREFIX).strip("._")
        self._season_entity = season_entity or DEFAULT_SEASON_ENTITY
        try:
            self._comfort_offset = float(comfort_offset)
        except (TypeError, ValueError):
            self._comfort_offset = DEFAULT_COMFORT_OFFSET
        if self._cst:
            # Single-target AUTO model: one comfort target per room; the coordinator owns
            # the firmware band + shared mode. The proxy reads/writes the target helper and
            # reflects the coordinator's actual shared mode. Helper names follow the
            # configurable contract: input_number.<prefix>_<room_key>_target etc.
            self._recompute_event = recompute_event or DEFAULT_RECOMPUTE_EVENT
            self._target_helper = f"input_number.{self._helper_prefix}_{room_key}_target"
            self._enable_helper = f"input_boolean.{self._helper_prefix}_{room_key}_enable"
            self._shared_mode_entity = shared_mode_entity or DEFAULT_SHARED_MODE_ENTITY
        else:
            self._recompute_event = recompute_event or DEFAULT_RECOMPUTE_EVENT
            self._target_helper = self._enable_helper = self._shared_mode_entity = None

    async def async_added_to_hass(self) -> None:
        """Run when entity about to be added to hass."""
        # Get initial state
        self._source_state = self.hass.states.get(self._source_entity_id)

        # Track source climate entity changes
        tracked_entities = [self._source_entity_id]

        # Track horizontal vane select entity changes (if configured)
        if self._horizontal_vane_entity_id:
            self._horizontal_vane_state = self.hass.states.get(
                self._horizontal_vane_entity_id
            )
            tracked_entities.append(self._horizontal_vane_entity_id)

        # track vertical vane select entity changes (if configured)
        if self._vertical_vane_entity_id:
            self._vertical_vane_state = self.hass.states.get(
                self._vertical_vane_entity_id
            )
            tracked_entities.append(self._vertical_vane_entity_id)

        # in coordinator single-target mode the masked mode/target/action are
        # derived from the coordinator helpers + season, so re-render when any of them change.
        if self._cst:
            tracked_entities += [
                self._target_helper,
                self._enable_helper,
                self._shared_mode_entity,
                self._season_entity,
            ]

        self.async_on_remove(
            async_track_state_change_event(
                self.hass, tracked_entities, self._async_source_changed
            )
        )

    @callback
    def _async_source_changed(self, event: Event) -> None:
        """Handle source entity state changes."""
        new_state = event.data.get("new_state")
        entity_id = event.data.get("entity_id")

        if entity_id == self._source_entity_id:
            self._source_state = new_state
        elif entity_id == self._horizontal_vane_entity_id:
            self._horizontal_vane_state = new_state
        elif entity_id == self._vertical_vane_entity_id:
            self._vertical_vane_state = new_state

        self.async_write_ha_state()

    # ════════════════════════════════════════════════════════════════
    # coordinator single-target helpers
    # ════════════════════════════════════════════════════════════════

    def _season(self) -> str:
        """Return the coordinator's season ('cooling' | 'heating'); default cooling."""
        st = self.hass.states.get(self._season_entity) if self._season_entity else None
        if st and st.state in ("cooling", "heating"):
            return st.state
        return "cooling"

    def _season_mode(self) -> HVACMode:
        """Map the season to the explicit shared mode (fallback only)."""
        return HVACMode.COOL if self._season() == "cooling" else HVACMode.HEAT

    def _shared_mode(self) -> HVACMode:
        """The coordinator's CURRENT shared mode (what the unit is actually doing).

        Single-target AUTO: the displayed mode follows the coordinator, not the season,
        so Apple Home shows Heat when it's heating and Cool when cooling. Falls back to
        the season mode if the shared-mode helper is briefly unknown.
        """
        st = self.hass.states.get(self._shared_mode_entity) if self._shared_mode_entity else None
        if st and st.state in ("cool", "heat"):
            return HVACMode.COOL if st.state == "cool" else HVACMode.HEAT
        return self._season_mode()

    def _helper_float(self, entity_id: str | None) -> Optional[float]:
        """Read a helper's numeric state (in °F); None if missing/unparseable."""
        if not entity_id:
            return None
        st = self.hass.states.get(entity_id)
        if not st or st.state in (STATE_UNAVAILABLE, STATE_UNKNOWN, "", None):
            return None
        try:
            return float(st.state)
        except (TypeError, ValueError):
            return None

    def _room_enabled(self) -> bool:
        """True when this room participates (coordinator enable on)."""
        st = self.hass.states.get(self._enable_helper) if self._enable_helper else None
        return bool(st and st.state == "on")

    @property
    def name(self) -> str:
        """Return the name of the entity."""
        return self._name

    @property
    def unique_id(self) -> str:
        """Return the unique ID of the entity."""
        return self._attr_unique_id

    @property
    def available(self) -> bool:
        """Return if entity is available."""
        return (
            self._source_state is not None
            and self._source_state.state != STATE_UNAVAILABLE
            and self._source_state.state != STATE_UNKNOWN
        )

    @property
    def supported_features(self) -> ClimateEntityFeature:
        """Return the list of supported features."""
        if not self._source_state:
            return ClimateEntityFeature(0)

        # Get source features
        source_features = self._source_state.attributes.get("supported_features", 0)

        # coordinator single-target -> always a SINGLE setpoint (never RANGE),
        # so HomeKit renders a single-target thermostat. Keep fan; vane stays as swing_mode;
        # drop the horizontal-swing bit to keep the HomeKit thermostat clean.
        if self._cst:
            feats = source_features
            feats &= ~ClimateEntityFeature.TARGET_TEMPERATURE_RANGE
            feats |= ClimateEntityFeature.TARGET_TEMPERATURE
            feats &= ~ClimateEntityFeature.SWING_HORIZONTAL_MODE
            if self._vertical_vane_entity_id:
                feats |= ClimateEntityFeature.SWING_MODE
            else:
                feats &= ~ClimateEntityFeature.SWING_MODE
            return ClimateEntityFeature(feats)

        # Mask out the temperature related flags to reset them
        # We start fresh with temperature features
        features = source_features & ~ClimateEntityFeature.TARGET_TEMPERATURE
        features = features & ~ClimateEntityFeature.TARGET_TEMPERATURE_RANGE

        # Dynamically expose the target shape based on the current mode and
        # values retained by the source. In OFF mode, ESPHome may retain a
        # single target even when HEAT_COOL is supported; advertising a range
        # in that case leaves the HA control empty because no range exists.
        source_has_single_target = (
            self._source_state.attributes.get("temperature") is not None
        )
        source_has_range_target = (
            self._source_state.attributes.get("target_temp_low") is not None
            and self._source_state.attributes.get("target_temp_high") is not None
        )

        if self.hvac_mode == HVACMode.HEAT_COOL or (
            self.hvac_mode == HVACMode.OFF
            and source_has_range_target
            and not source_has_single_target
        ):
            features |= ClimateEntityFeature.TARGET_TEMPERATURE_RANGE
        else:
            features |= ClimateEntityFeature.TARGET_TEMPERATURE

        # Add independent horizontal swing support when a horizontal vane entity is configured
        if self._horizontal_vane_entity_id:
            features |= ClimateEntityFeature.SWING_HORIZONTAL_MODE

        # vane-backed swing modes still need the SWING_MODE feature bit
        if self._vertical_vane_entity_id:
            features |= ClimateEntityFeature.SWING_MODE

        return ClimateEntityFeature(features)

    # ════════════════════════════════════════════════════════════════
    # Temperature normalisation (F ↔ C)
    # ════════════════════════════════════════════════════════════════

    @property
    def _source_unit(self) -> str:
        """Return the temperature unit used by HA for climate state attributes.

        Climate entities in HA do NOT expose 'unit_of_measurement' in their
        state attributes (unlike sensor entities).  Instead, HA converts all
        climate temperature values to match the user's configured unit system
        before storing them in the state object.

        Therefore the reliable way to know what unit the source values are in
        is to read the HA instance's unit system.
        """
        return self.hass.config.units.temperature_unit

    def _normalize_temp(self, val: Optional[float]) -> Optional[float]:
        """Convert a source temperature to °C if the source advertises °F.

        When fahrenheit_compatibility is active on the ESPHome side, the source
        climate entity already exposes values in Fahrenheit.  If we forward that
        unit to HA unchanged, HA applies a *second* F→display conversion,
        resulting in values ~2.26× too high (e.g. 69 °F → 156 °F).
        By normalising everything to Celsius here we break that double-
        conversion loop without affecting Celsius-only setups.
        """
        if val is None:
            return None
        if self._source_unit == UnitOfTemperature.FAHRENHEIT:
            return (val - 32.0) * 5.0 / 9.0
        return val

    def _denormalize_temp(self, val: Optional[float]) -> Optional[float]:
        """Convert a Celsius setpoint back to °F before forwarding to the source.

        HA sends setpoints in the unit declared by temperature_unit (always °C
        for this proxy).  When the source entity expects °F we must convert back.
        """
        if val is None:
            return None
        if self._source_unit == UnitOfTemperature.FAHRENHEIT:
            return val * 9.0 / 5.0 + 32.0
        return val

    @property
    def temperature_unit(self) -> str:
        """Always report Celsius to prevent HA from applying a second conversion.

        The proxy normalises all temperatures to °C internally via
        _normalize_temp / _denormalize_temp, so HA must be told the unit is
        always °C regardless of what the source entity advertises.
        """
        return UnitOfTemperature.CELSIUS

    # ════════════════════════════════════════════════════════════════
    # Temperature properties
    # ════════════════════════════════════════════════════════════════

    @property
    def current_temperature(self) -> Optional[float]:
        """Return the current temperature normalised to °C."""
        if self._source_state:
            return self._normalize_temp(
                self._source_state.attributes.get("current_temperature")
            )
        return None

    @property
    def target_temperature(self) -> Optional[float]:
        """Return the temperature we try to reach, normalised to °C."""
        # coordinator single-target -> the displayed target is the room's single
        # comfort target helper (NOT the firmware 'temperature', which is None while idling).
        if self._cst:
            val = self._helper_float(self._target_helper)
            return self._normalize_temp(val) if val is not None else None

        if not self._source_state:
            return None

        # Try to get direct attribute first
        val = self._source_state.attributes.get("temperature")
        if val is not None:
            return self._normalize_temp(val)

        # Fallback to derived values if source is in dual mode but we are presenting single
        low = self._source_state.attributes.get("target_temp_low")
        high = self._source_state.attributes.get("target_temp_high")

        if self.hvac_mode == HVACMode.HEAT:
            return self._normalize_temp(low if low is not None else high)
        elif self.hvac_mode == HVACMode.COOL:
            return self._normalize_temp(high if high is not None else low)
        elif self.hvac_mode == HVACMode.DRY:
            # Mode DRY uses cooling logic (high setpoint or single)
            return self._normalize_temp(high if high is not None else low)
        elif self.hvac_mode == HVACMode.AUTO:
            if low is not None and high is not None:
                return self._normalize_temp((low + high) / 2.0)
            return self._normalize_temp(low if low is not None else high)

        return None

    @property
    def target_temperature_high(self) -> Optional[float]:
        """Return the highbound target temperature, normalised to °C."""
        if self._source_state:
            return self._normalize_temp(
                self._source_state.attributes.get("target_temp_high")
            )
        return None

    @property
    def target_temperature_low(self) -> Optional[float]:
        """Return the lowbound target temperature, normalised to °C."""
        if self._source_state:
            return self._normalize_temp(
                self._source_state.attributes.get("target_temp_low")
            )
        return None

    # ════════════════════════════════════════════════════════════════
    # HVAC mode
    # ════════════════════════════════════════════════════════════════

    @property
    def hvac_mode(self) -> HVACMode:
        """Return hvac operation ie. heat, cool mode.

        Legacy plain AUTO is hidden: the source briefly reports ``auto`` after a
        power-cycle (band lost from ESP RAM) before the band is re-applied. We
        present that as HEAT_COOL so HomeKit never sees a single-setpoint ``auto``
        alongside the two-threshold heat_cool — the combination makes the HomeKit
        thermostat render the heat/cool thresholds inverted.
        """
        # coordinator single-target -> report the coordinator's CURRENT shared
        # mode while this room participates (masking the firmware's fan_only/idle so Apple
        # Home shows Heat/Cool, never a scary state); OFF only when the room is disabled.
        if self._cst:
            return self._shared_mode() if self._room_enabled() else HVACMode.OFF

        if self._source_state:
            state = self._source_state.state
            if state == HVACMode.AUTO:
                return HVACMode.HEAT_COOL
            try:
                return HVACMode(state)
            except ValueError:
                return HVACMode.OFF
        return HVACMode.OFF

    @property
    def hvac_modes(self) -> List[HVACMode]:
        """Return the list of available hvac operation modes.

        Drops legacy plain AUTO (see ``hvac_mode``) and guarantees HEAT_COOL is
        offered, so the household only ever sees the dual-setpoint mode and
        HomeKit maps it cleanly to its auto/range thermostat.
        """
        # coordinator single-target AUTO -> offer OFF + HEAT + COOL (single
        # setpoint each, no heat_cool range). The coordinator auto-picks heat vs cool to
        # reach the target; the reported hvac_mode follows it. Both must be listed so the
        # active mode is always representable to HomeKit.
        if self._cst:
            return [HVACMode.OFF, HVACMode.HEAT, HVACMode.COOL]

        if not self._source_state:
            return []

        raw_modes = self._source_state.attributes.get("hvac_modes", []) or []
        modes: list[HVACMode] = []
        for raw in raw_modes:
            if raw == HVACMode.AUTO:
                continue  # hide legacy single-setpoint AUTO
            try:
                modes.append(HVACMode(raw))
            except ValueError:
                _LOGGER.debug(
                    "Unsupported hvac_mode from source %s: %r",
                    self._source_entity_id,
                    raw,
                )
        if HVACMode.HEAT_COOL not in modes:
            modes.append(HVACMode.HEAT_COOL)
        return modes

    @property
    def hvac_action(self) -> HVACAction | None:
        """Return the current HVAC action (heating/cooling/idle...)."""
        if not self._source_state:
            return None

        # coordinator single-target -> normalize the idle states. The firmware
        # reports fan_only/fan/idle while satisfied; surface that as IDLE (or OFF when the
        # room is disabled) so Apple Home shows "to X°, idle", never "fan_only".
        if self._cst:
            raw_cst = self._source_state.attributes.get("hvac_action")
            if raw_cst in ("cooling", "heating"):
                try:
                    return HVACAction(raw_cst)
                except ValueError:
                    pass
            if not self._room_enabled():
                return HVACAction.OFF
            return HVACAction.IDLE

        # Home Assistant uses `hvac_action`. Some integrations may expose `action`.
        raw = self._source_state.attributes.get("hvac_action")
        if raw is None:
            raw = self._source_state.attributes.get("action")

        if raw is None:
            return None

        try:
            return HVACAction(raw)
        except ValueError:
            _LOGGER.debug(
                "Unsupported hvac_action from source %s: %r",
                self._source_entity_id,
                raw,
            )
            return None

    async def async_set_hvac_mode(self, hvac_mode: HVACMode) -> None:
        """Set new target hvac mode."""
        # coordinator single-target -> never forward a mode to the firmware
        # (the coordinator owns mode). OFF disables this room; the season mode enables it.
        if self._cst:
            if hvac_mode == HVACMode.OFF:
                await self.hass.services.async_call(
                    "input_boolean", "turn_off",
                    {"entity_id": self._enable_helper}, blocking=True,
                )
            else:
                await self.hass.services.async_call(
                    "input_boolean", "turn_on",
                    {"entity_id": self._enable_helper}, blocking=True,
                )
            self.hass.bus.async_fire(self._recompute_event)
            return

        await self.hass.services.async_call(
            "climate",
            "set_hvac_mode",
            {"entity_id": self._source_entity_id, "hvac_mode": hvac_mode},
            blocking=True,
        )

    async def async_set_temperature(self, **kwargs) -> None:
        """Set new target temperature."""
        # coordinator single-target AUTO -> REDIRECT to the room's comfort TARGET
        # helper (never the firmware; the coordinator owns the band + mode and would override
        # a direct write). The coordinator reaches this target by heating or cooling. Then
        # ensure the room participates and fire the recompute event.
        if self._cst:
            t_c = kwargs.get("temperature")
            if t_c is None:
                return
            # HA sends °C (our declared unit); convert to °F integer for the helper.
            t = round(self._denormalize_temp(t_c))
            await self.hass.services.async_call(
                "input_number", "set_value",
                {"entity_id": self._target_helper, "value": t}, blocking=True,
            )
            await self.hass.services.async_call(
                "input_boolean", "turn_on",
                {"entity_id": self._enable_helper}, blocking=True,
            )
            self.hass.bus.async_fire(self._recompute_event)
            return

        service_data: dict[str, Any] = {"entity_id": self._source_entity_id}

        # Check source capabilities
        source_features = 0
        if self._source_state:
            source_features = self._source_state.attributes.get("supported_features", 0)

        source_is_dual = source_features & ClimateEntityFeature.TARGET_TEMPERATURE_RANGE

        # Determine effective mode (target mode if changing, else current)
        mode = kwargs.get("hvac_mode", self.hvac_mode)

        if "target_temp_low" in kwargs or "target_temp_high" in kwargs:
            # Direct dual control — HA sends values in °C (our declared unit),
            # convert back to source unit before forwarding.
            if "target_temp_low" in kwargs:
                service_data["target_temp_low"] = self._denormalize_temp(
                    kwargs["target_temp_low"]
                )
            if "target_temp_high" in kwargs:
                service_data["target_temp_high"] = self._denormalize_temp(
                    kwargs["target_temp_high"]
                )
        elif "temperature" in kwargs:
            # HA sends the setpoint in °C (our declared unit); convert to source unit.
            t = self._denormalize_temp(kwargs["temperature"])
            # curr_high / curr_low are already in source unit (raw attributes),
            # so comparisons are safe in the same unit.
            raw_high = self._source_state.attributes.get("target_temp_high") if self._source_state else None
            raw_low = self._source_state.attributes.get("target_temp_low") if self._source_state else None

            # If source is NOT dual (single setpoint), just send temperature directly
            if not source_is_dual:
                service_data["temperature"] = t
            elif mode == HVACMode.HEAT:
                service_data["target_temp_low"] = t
                # Get current high to ensure we send a complete pair
                curr_high = raw_high
                if curr_high is None:
                    # Fallback if unknown, usually safe to assume a delta or max
                    curr_high = self._denormalize_temp(self.max_temp)

                # Ensure high >= low
                if t > curr_high:
                    service_data["target_temp_high"] = t
                else:
                    service_data["target_temp_high"] = curr_high

            elif mode == HVACMode.COOL:
                service_data["target_temp_high"] = t
                # Get current low to ensure we send a complete pair
                curr_low = raw_low
                if curr_low is None:
                    curr_low = self._denormalize_temp(self.min_temp)

                # Ensure low <= high
                if t < curr_low:
                    service_data["target_temp_low"] = t
                else:
                    service_data["target_temp_low"] = curr_low

            elif mode == HVACMode.DRY:
                # Mode DRY treats target as a cooling setpoint
                service_data["target_temp_high"] = t
                # Get current low to ensure we send a complete pair
                curr_low = raw_low
                if curr_low is None:
                    curr_low = self._denormalize_temp(self.min_temp)

                # Ensure low <= high to keep consistency
                if t < curr_low:
                    service_data["target_temp_low"] = t
                else:
                    service_data["target_temp_low"] = curr_low

            elif mode == HVACMode.AUTO:
                # Move range, keeping spread — work in source unit (raw)
                curr_low = raw_low
                curr_high = raw_high

                # Default spread if unknown (2° in source unit)
                if curr_low is None:
                    curr_low = t - 2
                if curr_high is None:
                    curr_high = t + 2

                spread = curr_high - curr_low

                service_data["target_temp_low"] = t - (spread / 2.0)
                service_data["target_temp_high"] = t + (spread / 2.0)
            else:
                # Fan_only, etc.
                service_data["temperature"] = t

        if "hvac_mode" in kwargs:
            service_data["hvac_mode"] = kwargs["hvac_mode"]

        await self.hass.services.async_call(
            "climate",
            "set_temperature",
            service_data,
            blocking=True,
        )

    # ════════════════════════════════════════════════════════════════
    # Fan mode (pass-through)
    # ════════════════════════════════════════════════════════════════

    @property
    def fan_mode(self) -> Optional[str]:
        """Return the fan setting."""
        if self._source_state:
            return self._source_state.attributes.get("fan_mode")
        return None

    @property
    def fan_modes(self) -> Optional[List[str]]:
        """Return the list of available fan modes."""
        if self._source_state:
            return self._source_state.attributes.get("fan_modes")
        return None

    async def async_set_fan_mode(self, fan_mode: str) -> None:
        """Set new target fan mode."""
        await self.hass.services.async_call(
            "climate",
            "set_fan_mode",
            {"entity_id": self._source_entity_id, "fan_mode": fan_mode},
            blocking=True,
        )

    # ════════════════════════════════════════════════════════════════
    # Swing mode — vertical
    # Pass-through from source climate by default. when a
    # vertical_vane_entity is configured, the vane select's positions
    # (AUTO/↑↑/↑/—/↓/↓↓/SWING) ARE the swing modes — restoring the classic
    # Mitsubishi presentation where swing offered positions, not on/off.
    # ════════════════════════════════════════════════════════════════

    @property
    def swing_mode(self) -> Optional[str]:
        """Return the swing setting (vane position when vane-backed)."""
        if self._vertical_vane_entity_id:
            if self._vertical_vane_state is None:
                self._vertical_vane_state = self.hass.states.get(
                    self._vertical_vane_entity_id
                )
            if (
                self._vertical_vane_state is None
                or self._vertical_vane_state.state
                in (STATE_UNAVAILABLE, STATE_UNKNOWN)
            ):
                return None
            return self._vertical_vane_state.state

        if self._source_state:
            return self._source_state.attributes.get("swing_mode")
        return None

    @property
    def swing_modes(self) -> Optional[List[str]]:
        """Return the list of available swing modes (vane positions when vane-backed)."""
        if self._vertical_vane_entity_id:
            if self._vertical_vane_state is None:
                self._vertical_vane_state = self.hass.states.get(
                    self._vertical_vane_entity_id
                )
            if self._vertical_vane_state is not None:
                options = self._vertical_vane_state.attributes.get("options")
                if options:
                    return list(options)
            # Fallback: standard Mitsubishi vertical vane options
            return ["AUTO", "↑↑", "↑", "—", "↓", "↓↓", "SWING"]

        if self._source_state:
            return self._source_state.attributes.get("swing_modes")
        return None

    async def async_set_swing_mode(self, swing_mode: str) -> None:
        """Set new target swing operation (vane position when vane-backed)."""
        if self._vertical_vane_entity_id:
            await self.hass.services.async_call(
                "select",
                "select_option",
                {
                    "entity_id": self._vertical_vane_entity_id,
                    "option": swing_mode,
                },
                blocking=True,
            )
            return

        await self.hass.services.async_call(
            "climate",
            "set_swing_mode",
            {"entity_id": self._source_entity_id, "swing_mode": swing_mode},
            blocking=True,
        )

    # ════════════════════════════════════════════════════════════════
    # Swing horizontal mode — WideVane (Mitsubishi-specific)
    # Maps the ESPHome horizontal_vane_select entity to the HA-native
    # swing_horizontal_mode API (available since HA 2024.12).
    # ════════════════════════════════════════════════════════════════

    @property
    def swing_horizontal_mode(self) -> Optional[str]:
        """Return the current horizontal vane (WideVane) position.

        Reads from the configured ESPHome select entity for horizontal vane.
        Returns None if no horizontal vane entity is configured or unavailable.
        """
        if not self._horizontal_vane_entity_id:
            return None

        if self._horizontal_vane_state is None:
            self._horizontal_vane_state = self.hass.states.get(
                self._horizontal_vane_entity_id
            )

        if (
            self._horizontal_vane_state is None
            or self._horizontal_vane_state.state in (STATE_UNAVAILABLE, STATE_UNKNOWN)
        ):
            return None

        return self._horizontal_vane_state.state

    @property
    def swing_horizontal_modes(self) -> Optional[List[str]]:
        """Return the list of available horizontal vane positions.

        Reads the options from the ESPHome select entity's 'options' attribute.
        Falls back to a default Mitsubishi WideVane set if options are unavailable.
        """
        if not self._horizontal_vane_entity_id:
            return None

        if self._horizontal_vane_state is None:
            self._horizontal_vane_state = self.hass.states.get(
                self._horizontal_vane_entity_id
            )

        if self._horizontal_vane_state is not None:
            options = self._horizontal_vane_state.attributes.get("options")
            if options:
                return list(options)

        # Fallback: standard Mitsubishi WideVane options
        return ["←←", "←", "|", "→", "→→", "←→", "SWING"]

    async def async_set_swing_horizontal_mode(
        self, swing_horizontal_mode: str
    ) -> None:
        """Set new horizontal vane (WideVane) position.

        Forwards the command to the ESPHome select entity via the
        select.select_option service.
        """
        if not self._horizontal_vane_entity_id:
            _LOGGER.warning(
                "Cannot set horizontal swing: no horizontal_vane_entity configured"
            )
            return

        await self.hass.services.async_call(
            "select",
            "select_option",
            {
                "entity_id": self._horizontal_vane_entity_id,
                "option": swing_horizontal_mode,
            },
            blocking=True,
        )

    # ════════════════════════════════════════════════════════════════
    # Preset mode (pass-through)
    # ════════════════════════════════════════════════════════════════

    @property
    def preset_mode(self) -> Optional[str]:
        """Return the current preset mode."""
        if self._source_state:
            return self._source_state.attributes.get("preset_mode")
        return None

    @property
    def preset_modes(self) -> Optional[List[str]]:
        """Return a list of available preset modes."""
        if self._source_state:
            return self._source_state.attributes.get("preset_modes")
        return None

    async def async_set_preset_mode(self, preset_mode: str) -> None:
        """Set new preset mode."""
        await self.hass.services.async_call(
            "climate",
            "set_preset_mode",
            {
                "entity_id": self._source_entity_id,
                "preset_mode": preset_mode,
            },
            blocking=True,
        )

    # ════════════════════════════════════════════════════════════════
    # Temperature bounds
    # ════════════════════════════════════════════════════════════════

    @property
    def min_temp(self) -> float:
        """Return the minimum temperature, normalised to °C."""
        if self._source_state:
            return self._normalize_temp(
                self._source_state.attributes.get("min_temp", 7)
            )
        return 7

    @property
    def max_temp(self) -> float:
        """Return the maximum temperature, normalised to °C."""
        if self._source_state:
            return self._normalize_temp(
                self._source_state.attributes.get("max_temp", 35)
            )
        return 35
