import logging
from homeassistant.core import HomeAssistant
from homeassistant.config_entries import ConfigEntry
from homeassistant.helpers.typing import ConfigType

_LOGGER = logging.getLogger(__name__)

DOMAIN = "mitsubishi_climate_proxy"

async def async_setup(hass: HomeAssistant, config: ConfigType) -> bool:
    """Set up the Mitsubishi Hybrid Climate component via YAML (legacy)."""
    # Return true to allow platform setup (which is handled in climate.py)
    # This allows keeping the old configuration method working if needed,
    # or we can fully deprecate it. For now, we return True.
    return True

async def async_setup_entry(hass: HomeAssistant, entry: ConfigEntry) -> bool:
    """Set up Mitsubishi Hybrid Climate from a config entry."""
    hass.data.setdefault(DOMAIN, {})

    # Reload the entry when its options change (e.g. toggling coordinator single-target
    # mode via the Options flow) so the new presentation takes effect live without a
    # delete/recreate — keeping the HomeKit accessory ID stable (no re-pair).
    entry.async_on_unload(entry.add_update_listener(_async_update_listener))

    # Forward the setup to the climate platform
    await hass.config_entries.async_forward_entry_setups(entry, ["climate"])

    return True


async def _async_update_listener(hass: HomeAssistant, entry: ConfigEntry) -> None:
    """Reload the config entry when its options are updated."""
    await hass.config_entries.async_reload(entry.entry_id)

async def async_unload_entry(hass: HomeAssistant, entry: ConfigEntry) -> bool:
    """Unload a config entry."""
    return await hass.config_entries.async_unload_platforms(entry, ["climate"])


async def async_migrate_entry(hass: HomeAssistant, config_entry: ConfigEntry) -> bool:
    """Migrate a config entry to ConfigFlow.VERSION.

    Version 2 only adds optional keys. Entries created by 2026.5.x keep their
    data and options; the climate platform applies defaults for anything
    absent. Home Assistant invokes this once and does not loop, so a version
    1 entry has to land on version 2 in this call.

    Downgrading the integration after this runs leaves the stored entry at
    version 2. A package whose config flow is still version 1 then refuses to
    set up. See the proxy README.
    """
    if config_entry.version == 1:
        hass.config_entries.async_update_entry(config_entry, version=2)
    elif config_entry.version != 2:
        _LOGGER.error(
            "Cannot migrate %s from unsupported version %s",
            config_entry.title,
            config_entry.version,
        )
        return False
    return True
