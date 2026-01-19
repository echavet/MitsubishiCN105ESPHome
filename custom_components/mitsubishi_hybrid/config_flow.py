import voluptuous as vol
from homeassistant import config_entries
from homeassistant.core import callback
from homeassistant.const import CONF_NAME, CONF_SOURCE
import homeassistant.helpers.config_validation as cv
from homeassistant.components.climate import DOMAIN as CLIMATE_DOMAIN

from . import DOMAIN

class MitsubishiHybridConfigFlow(config_entries.ConfigFlow, domain=DOMAIN):
    """Handle a config flow for Mitsubishi Hybrid Climate."""

    VERSION = 1

    async def async_step_user(self, user_input=None):
        """Handle the initial step."""
        errors = {}

        if user_input is not None:
            # Validate input
            await self.async_set_unique_id(f"{user_input[CONF_SOURCE]}_hybrid")
            self._abort_if_unique_id_configured()

            return self.async_create_entry(
                title=user_input.get(CONF_NAME, user_input[CONF_SOURCE]),
                data=user_input
            )

        # Get list of climate entities to offer in dropdown
        climate_entities = [
            ent.entity_id for ent in self.hass.states.async_all(CLIMATE_DOMAIN)
        ]

        data_schema = vol.Schema({
            vol.Required(CONF_SOURCE): vol.In(climate_entities),
            vol.Optional(CONF_NAME): str,
        })

        return self.async_show_form(
            step_id="user",
            data_schema=data_schema,
            errors=errors
        )
