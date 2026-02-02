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

PLATFORM_SCHEMA = PLATFORM_SCHEMA.extend(
    {
        vol.Required(CONF_SOURCE_ENTITY): cv.entity_id,
        vol.Optional(CONF_NAME): cv.string,
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

    async_add_entities(
        [MitsubishiHybridClimate(hass, name, source_entity_id)],
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

    async_add_entities(
        [MitsubishiHybridClimate(hass, name, source_entity_id, entry.entry_id)],
        True,
    )


class MitsubishiHybridClimate(ClimateEntity):
    """Representation of a Mitsubishi Hybrid Climate device."""

    def __init__(
        self,
        hass: HomeAssistant,
        name: str | None,
        source_entity_id: str,
        unique_id: str | None = None,
    ) -> None:
        """Initialize the climate device."""
        super().__init__()
        self._hass = hass
        self._name = name or source_entity_id
        self._source_entity_id = source_entity_id
        self._source_state = None
        self._attr_should_poll = False
        self._attr_unique_id = unique_id or f"{source_entity_id}_hybrid"

    async def async_added_to_hass(self) -> None:
        """Run when entity about to be added to hass."""
        # Get initial state
        self._source_state = self.hass.states.get(self._source_entity_id)

        self.async_on_remove(
            async_track_state_change_event(
                self.hass, [self._source_entity_id], self._async_source_changed
            )
        )

    @callback
    def _async_source_changed(self, event: Event) -> None:
        """Handle source entity state changes."""
        new_state = event.data.get("new_state")
        self._source_state = new_state
        self.async_write_ha_state()

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

        # Mask out the temperature related flags to reset them
        # We start fresh with temperature features
        features = source_features & ~ClimateEntityFeature.TARGET_TEMPERATURE
        features = features & ~ClimateEntityFeature.TARGET_TEMPERATURE_RANGE

        # Dynamically add the flag based on current mode
        # Use RANGE if we are in HEAT_COOL mode, OR if we are in OFF mode and the device supports HEAT_COOL
        # (This ensures OFF mode shows dual setpoints instead of being empty, as OFF typically lacks a single 'temperature' attribute on dual-sp entities)
        if self.hvac_mode == HVACMode.HEAT_COOL or (
            self.hvac_mode == HVACMode.OFF and HVACMode.HEAT_COOL in self.hvac_modes
        ):
            features |= ClimateEntityFeature.TARGET_TEMPERATURE_RANGE
        else:
            features |= ClimateEntityFeature.TARGET_TEMPERATURE

        return ClimateEntityFeature(features)

    @property
    def temperature_unit(self) -> str:
        """Return the unit of measurement."""
        if self._source_state:
            return self._source_state.attributes.get(
                "unit_of_measurement", UnitOfTemperature.CELSIUS
            )
        return UnitOfTemperature.CELSIUS

    @property
    def current_temperature(self) -> Optional[float]:
        """Return the current temperature."""
        if self._source_state:
            return self._source_state.attributes.get("current_temperature")
        return None

    @property
    def target_temperature(self) -> Optional[float]:
        """Return the temperature we try to reach."""
        if not self._source_state:
            return None

        # Try to get direct attribute first
        val = self._source_state.attributes.get("temperature")
        if val is not None:
            return val

        # Fallback to derived values if source is in dual mode but we are presenting single
        low = self._source_state.attributes.get("target_temp_low")
        high = self._source_state.attributes.get("target_temp_high")

        if self.hvac_mode == HVACMode.HEAT:
            return low if low is not None else high
        elif self.hvac_mode == HVACMode.COOL:
            return high if high is not None else low
        elif self.hvac_mode == HVACMode.DRY:
            # Mode DRY uses cooling logic (high setpoint or single)
            return high if high is not None else low
        elif self.hvac_mode == HVACMode.AUTO:
            if low is not None and high is not None:
                return (low + high) / 2.0
            return low if low is not None else high

        return None

    @property
    def target_temperature_high(self) -> Optional[float]:
        """Return the highbound target temperature we try to reach."""
        if self._source_state:
            return self._source_state.attributes.get("target_temp_high")
        return None

    @property
    def target_temperature_low(self) -> Optional[float]:
        """Return the lowbound target temperature we try to reach."""
        if self._source_state:
            return self._source_state.attributes.get("target_temp_low")
        return None

    @property
    def hvac_mode(self) -> HVACMode:
        """Return hvac operation ie. heat, cool mode."""
        if self._source_state:
            state = self._source_state.state
            try:
                return HVACMode(state)
            except ValueError:
                return HVACMode.OFF
        return HVACMode.OFF

    @property
    def hvac_modes(self) -> List[HVACMode]:
        """Return the list of available hvac operation modes."""
        if not self._source_state:
            return []

        raw_modes = self._source_state.attributes.get("hvac_modes", []) or []
        modes: list[HVACMode] = []
        for raw in raw_modes:
            try:
                modes.append(HVACMode(raw))
            except ValueError:
                _LOGGER.debug(
                    "Unsupported hvac_mode from source %s: %r",
                    self._source_entity_id,
                    raw,
                )
        return modes

    @property
    def hvac_action(self) -> HVACAction | None:
        """Return the current HVAC action (heating/cooling/idle...)."""
        if not self._source_state:
            return None

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
        await self.hass.services.async_call(
            "climate",
            "set_hvac_mode",
            {"entity_id": self._source_entity_id, "hvac_mode": hvac_mode},
            blocking=True,
        )

    async def async_set_temperature(self, **kwargs) -> None:
        """Set new target temperature."""
        service_data: dict[str, Any] = {"entity_id": self._source_entity_id}

        # Check source capabilities
        source_features = 0
        if self._source_state:
            source_features = self._source_state.attributes.get("supported_features", 0)

        source_is_dual = source_features & ClimateEntityFeature.TARGET_TEMPERATURE_RANGE

        # Determine effective mode (target mode if changing, else current)
        mode = kwargs.get("hvac_mode", self.hvac_mode)

        if "target_temp_low" in kwargs or "target_temp_high" in kwargs:
            # Direct dual control
            if "target_temp_low" in kwargs:
                service_data["target_temp_low"] = kwargs["target_temp_low"]
            if "target_temp_high" in kwargs:
                service_data["target_temp_high"] = kwargs["target_temp_high"]
        elif "temperature" in kwargs:
            t = kwargs["temperature"]

            # If source is NOT dual (single setpoint), just send temperature directly
            if not source_is_dual:
                service_data["temperature"] = t
            elif mode == HVACMode.HEAT:
                service_data["target_temp_low"] = t
                # Get current high to ensure we send a complete pair
                curr_high = self.target_temperature_high
                if curr_high is None:
                    # Fallback if unknown, usually safe to assume a delta or max
                    curr_high = self.max_temp

                # Ensure high >= low
                if t > curr_high:
                    service_data["target_temp_high"] = t
                else:
                    service_data["target_temp_high"] = curr_high

            elif mode == HVACMode.COOL:
                service_data["target_temp_high"] = t
                # Get current low to ensure we send a complete pair
                curr_low = self.target_temperature_low
                if curr_low is None:
                    curr_low = self.min_temp

                # Ensure low <= high
                if t < curr_low:
                    service_data["target_temp_low"] = t
                else:
                    service_data["target_temp_low"] = curr_low

            elif mode == HVACMode.DRY:
                # Mode DRY treats target as a cooling setpoint
                service_data["target_temp_high"] = t
                # Get current low to ensure we send a complete pair
                curr_low = self.target_temperature_low
                if curr_low is None:
                    curr_low = self.min_temp

                # Ensure low <= high to keep consistency
                if t < curr_low:
                    service_data["target_temp_low"] = t
                else:
                    service_data["target_temp_low"] = curr_low

            elif mode == HVACMode.AUTO:
                # Move range, keeping spread
                curr_low = self.target_temperature_low
                curr_high = self.target_temperature_high

                # Default spread if unknown
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

    @property
    def swing_mode(self) -> Optional[str]:
        """Return the swing setting."""
        if self._source_state:
            return self._source_state.attributes.get("swing_mode")
        return None

    @property
    def swing_modes(self) -> Optional[List[str]]:
        """Return the list of available swing modes."""
        if self._source_state:
            return self._source_state.attributes.get("swing_modes")
        return None

    async def async_set_swing_mode(self, swing_mode: str) -> None:
        """Set new target swing operation."""
        await self.hass.services.async_call(
            "climate",
            "set_swing_mode",
            {"entity_id": self._source_entity_id, "swing_mode": swing_mode},
            blocking=True,
        )

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

    @property
    def min_temp(self) -> float:
        """Return the minimum temperature."""
        if self._source_state:
            return self._source_state.attributes.get("min_temp", 7)
        return 7

    @property
    def max_temp(self) -> float:
        """Return the maximum temperature."""
        if self._source_state:
            return self._source_state.attributes.get("max_temp", 35)
        return 35
