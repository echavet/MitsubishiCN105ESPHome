from homeassistant.core import HomeAssistant
from homeassistant.helpers.typing import ConfigType

DOMAIN = "mitsubishi_hybrid"

async def async_setup(hass: HomeAssistant, config: ConfigType) -> bool:
    """Set up the Mitsubishi Hybrid component."""
    return True
