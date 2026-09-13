## config_flow.py — Config Flow for Mitsubishi Climate Proxy
## Role: UI-based configuration wizard for adding proxy instances via HA integrations page,
##       plus an Options flow to toggle the coordinator single-target mode LIVE (no
##       delete/recreate — preserves the HomeKit accessory ID).
## Deps: homeassistant.config_entries, homeassistant.components.climate, homeassistant.components.select

import voluptuous as vol
from homeassistant import config_entries
from homeassistant.core import callback
from homeassistant.const import CONF_NAME, CONF_SOURCE
import homeassistant.helpers.config_validation as cv
from homeassistant.components.climate import DOMAIN as CLIMATE_DOMAIN
from homeassistant.components.select import DOMAIN as SELECT_DOMAIN

from . import DOMAIN

CONF_RESTORE_LAST_MODE = "restore_last_mode"
CONF_DEFAULT_TURN_ON_MODE = "default_turn_on_mode"
DEFAULT_RESTORE_LAST_MODE = True
DEFAULT_TURN_ON_MODE = "default"
TURN_ON_MODES = ("default", "heat_cool", "auto", "heat", "cool", "dry", "fan_only")

CONF_HORIZONTAL_VANE_ENTITY = "horizontal_vane_entity"
# Vertical vane positions as swing modes
CONF_VERTICAL_VANE_ENTITY = "vertical_vane_entity"
# Coordinator single-target (Tesla-style) mode + its helper-name contract
CONF_COORDINATOR_SINGLE_TARGET = "coordinator_single_target"
CONF_ROOM_KEY = "room_key"
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
            vol.Optional(CONF_RESTORE_LAST_MODE, default=DEFAULT_RESTORE_LAST_MODE): bool,
            vol.Optional(CONF_DEFAULT_TURN_ON_MODE, default=DEFAULT_TURN_ON_MODE): vol.In(TURN_ON_MODES),
            vol.Optional(CONF_NAME): str,
            vol.Optional(CONF_HORIZONTAL_VANE_ENTITY): vol.In(
                select_entities_with_none
            ),
            vol.Optional(CONF_VERTICAL_VANE_ENTITY): vol.In(
                select_entities_with_none
            ),
            vol.Optional(CONF_COORDINATOR_SINGLE_TARGET, default=False): bool,
            vol.Optional(CONF_ROOM_KEY, default=""): str,
            vol.Optional(CONF_HELPER_PREFIX, default=DEFAULT_HELPER_PREFIX): str,
            vol.Optional(CONF_SEASON_ENTITY, default=DEFAULT_SEASON_ENTITY): str,
            vol.Optional(CONF_SHARED_MODE_ENTITY, default=DEFAULT_SHARED_MODE_ENTITY): str,
            vol.Optional(CONF_RECOMPUTE_EVENT, default=DEFAULT_RECOMPUTE_EVENT): str,
            vol.Optional(CONF_COMFORT_OFFSET, default=DEFAULT_COMFORT_OFFSET): vol.Coerce(float),
        })

        return self.async_show_form(
            step_id="user",
            data_schema=data_schema,
            errors=errors
        )

    @staticmethod
    @callback
    def async_get_options_flow(config_entry):
        """Get the options flow for this handler (live toggle of single-target mode)."""
        return MitsubishiHybridOptionsFlow()


class MitsubishiHybridOptionsFlow(config_entries.OptionsFlow):
    """Toggle the coordinator single-target mode (and its params) on an existing entry.

    Editing options reloads the entry in place (via the update listener in __init__.py)
    so the HomeKit accessory ID is preserved — Apple Home updates the tile, no re-pair.
    """

    async def async_step_init(self, user_input=None):
        """Manage the options."""
        if user_input is not None:
            return self.async_create_entry(title="", data=user_input)

        cur = {**self.config_entry.data, **self.config_entry.options}
        schema = vol.Schema({
            vol.Optional(
                CONF_RESTORE_LAST_MODE,
                default=cur.get(CONF_RESTORE_LAST_MODE, DEFAULT_RESTORE_LAST_MODE),
            ): bool,
            vol.Optional(
                CONF_DEFAULT_TURN_ON_MODE,
                default=cur.get(CONF_DEFAULT_TURN_ON_MODE, DEFAULT_TURN_ON_MODE),
            ): vol.In(TURN_ON_MODES),
            vol.Optional(
                CONF_COORDINATOR_SINGLE_TARGET,
                default=cur.get(CONF_COORDINATOR_SINGLE_TARGET, False),
            ): bool,
            vol.Optional(
                CONF_ROOM_KEY,
                default=cur.get(CONF_ROOM_KEY, ""),
            ): str,
            vol.Optional(
                CONF_HELPER_PREFIX,
                default=cur.get(CONF_HELPER_PREFIX, DEFAULT_HELPER_PREFIX),
            ): str,
            vol.Optional(
                CONF_SEASON_ENTITY,
                default=cur.get(CONF_SEASON_ENTITY, DEFAULT_SEASON_ENTITY),
            ): str,
            vol.Optional(
                CONF_SHARED_MODE_ENTITY,
                default=cur.get(CONF_SHARED_MODE_ENTITY, DEFAULT_SHARED_MODE_ENTITY),
            ): str,
            vol.Optional(
                CONF_RECOMPUTE_EVENT,
                default=cur.get(CONF_RECOMPUTE_EVENT, DEFAULT_RECOMPUTE_EVENT),
            ): str,
            vol.Optional(
                CONF_COMFORT_OFFSET,
                default=cur.get(CONF_COMFORT_OFFSET, DEFAULT_COMFORT_OFFSET),
            ): vol.Coerce(float),
        })

        return self.async_show_form(step_id="init", data_schema=schema)
