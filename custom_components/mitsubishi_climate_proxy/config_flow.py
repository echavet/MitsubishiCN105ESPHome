## config_flow.py — Config Flow for Mitsubishi Climate Proxy
## Role: UI-based configuration wizard for adding proxy instances via HA integrations page.
## Deps: homeassistant.config_entries, homeassistant.components.climate, homeassistant.components.select

import voluptuous as vol
from homeassistant import config_entries
from homeassistant.core import callback
from homeassistant.const import CONF_NAME, CONF_SOURCE
import homeassistant.helpers.config_validation as cv
from homeassistant.components.climate import DOMAIN as CLIMATE_DOMAIN
from homeassistant.components.select import DOMAIN as SELECT_DOMAIN

from . import DOMAIN

CONF_HORIZONTAL_VANE_ENTITY = "horizontal_vane_entity"


class MitsubishiHybridConfigFlow(config_entries.ConfigFlow, domain=DOMAIN):
    """Handle a config flow for Mitsubishi Hybrid Climate."""

    VERSION = 2

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

        # Get list of select entities (for horizontal vane)
        select_entities = [
            ent.entity_id for ent in self.hass.states.async_all(SELECT_DOMAIN)
        ]
        # Add empty option for "no horizontal vane"
        select_entities_with_none = [""] + select_entities

        data_schema = vol.Schema({
            vol.Required(CONF_SOURCE): vol.In(climate_entities),
            vol.Optional(CONF_NAME): str,
            vol.Optional(CONF_HORIZONTAL_VANE_ENTITY): vol.In(
                select_entities_with_none
            ),
        })

        return self.async_show_form(
            step_id="user",
            data_schema=data_schema,
            errors=errors
        )
