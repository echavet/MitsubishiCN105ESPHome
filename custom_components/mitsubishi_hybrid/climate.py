
import logging
from typing import Any, List, Optional
import voluptuous as vol

from homeassistant.components.climate import (
    ClimateEntity,
    PLATFORM_SCHEMA,
    ClimateEntityFeature,
    HVACMode,
)
from homeassistant.const import (
    ATTR_ENTITY_ID,
    ATTR_TEMPERATURE,
    CONF_NAME,
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
    """Set up the Mitsubishi Hybrid Climate platform."""
    source_entity_id = config[CONF_SOURCE_ENTITY]
    name = config.get(CONF_NAME)

    async_add_entities(
        [MitsubishiHybridClimate(hass, name, source_entity_id)],
        True,
    )

class MitsubishiHybridClimate(ClimateEntity):
    """Representation of a Mitsubishi Hybrid Climate device."""

    def __init__(self, hass: HomeAssistant, name: str, source_entity_id: str) -> None:
        """Initialize the climate device."""
        self._hass = hass
        self._name = name or source_entity_id
        self._source_entity_id = source_entity_id
        self._source_state = None
        self._attr_should_poll = False
        
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
        return f"{self._source_entity_id}_hybrid"

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
        if self.hvac_mode == HVACMode.HEAT_COOL:
            features |= ClimateEntityFeature.TARGET_TEMPERATURE_RANGE
        else:
            features |= ClimateEntityFeature.TARGET_TEMPERATURE
            
        return ClimateEntityFeature(features)

    @property
    def temperature_unit(self) -> str:
        """Return the unit of measurement."""
        if self._source_state:
            return self._source_state.attributes.get("unit_of_measurement", UnitOfTemperature.CELSIUS)
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
            if state in HVACMode:
                return HVACMode(state)
        return HVACMode.OFF

    @property
    def hvac_modes(self) -> List[HVACMode]:
        """Return the list of available hvac operation modes."""
        if self._source_state:
            return self._source_state.attributes.get("hvac_modes", [])
        return []

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
        service_data = {"entity_id": self._source_entity_id}
        
        # Determine effective mode (target mode if changing, else current)
        mode = kwargs.get("hvac_mode", self.hvac_mode)
        
        if "target_temp_low" in kwargs or "target_temp_high" in kwargs:
             # Direct dual control
             if "target_temp_low" in kwargs: service_data["target_temp_low"] = kwargs["target_temp_low"]
             if "target_temp_high" in kwargs: service_data["target_temp_high"] = kwargs["target_temp_high"]
        elif "temperature" in kwargs:
             t = kwargs["temperature"]
             
             if mode == HVACMode.HEAT:
                 service_data["target_temp_low"] = t
                 # Ensure high >= low to avoid errors if source validates it
                 curr_high = self.target_temperature_high
                 if curr_high is not None and t > curr_high:
                      service_data["target_temp_high"] = t
                      
             elif mode == HVACMode.COOL:
                 service_data["target_temp_high"] = t
                 # Ensure low <= high
                 curr_low = self.target_temperature_low
                 if curr_low is not None and t < curr_low:
                      service_data["target_temp_low"] = t
                      
             elif mode == HVACMode.AUTO:
                 # Move range, keeping spread
                 curr_low = self.target_temperature_low
                 curr_high = self.target_temperature_high
                 
                 # Default spread if unknown
                 if curr_low is None: curr_low = t - 2
                 if curr_high is None: curr_high = t + 2
                 
                 spread = curr_high - curr_low
                 
                 service_data["target_temp_low"] = t - (spread / 2.0)
                 service_data["target_temp_high"] = t + (spread / 2.0)
             else:
                 # Dry, Fan_only, etc.
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
