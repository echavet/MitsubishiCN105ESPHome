"""Migration of mitsubishi_climate_proxy config entries on Home Assistant 2026.11.0.

These tests call the integration's async_migrate_entry and, where noted,
Home Assistant's own config-entry setup. They do not reimplement the
version predicate.
"""

from __future__ import annotations

from datetime import UTC, datetime
import json
from pathlib import Path
from types import MappingProxyType

from homeassistant.components.climate import (
    ClimateEntity,
    ClimateEntityFeature,
    HVACMode,
)
from homeassistant.components.climate.const import DATA_COMPONENT
from homeassistant.config_entries import ConfigEntry, ConfigEntryState
from homeassistant.const import ATTR_TEMPERATURE, UnitOfTemperature
from homeassistant.helpers import entity_registry as er
from homeassistant.loader import async_get_integration

from custom_components.mitsubishi_climate_proxy import async_migrate_entry
from custom_components.mitsubishi_climate_proxy.config_flow import (
    MitsubishiHybridConfigFlow,
)

REPO_MANIFEST = (
    Path(__file__).resolve().parents[2]
    / "custom_components"
    / "mitsubishi_climate_proxy"
    / "manifest.json"
)

SOURCE = "climate.room_esphome"
V1_DATA = {"source": SOURCE, "name": "Room"}
V1_OPTIONS: dict = {}
STAMP = datetime(2026, 5, 2, 8, 0, tzinfo=UTC)

# Imported for its side effect: ConfigFlow.__init_subclass__ registers the handler.
assert MitsubishiHybridConfigFlow.VERSION == 2


def _entry(
    hass,
    *,
    version: int,
    data: dict | None = None,
    options: dict | None = None,
    title: str = "Room",
    entry_id: str = "proxy-entry-1",
) -> ConfigEntry:
    """Insert a real ConfigEntry without setting it up."""
    entry = ConfigEntry(
        created_at=STAMP,
        data=dict(V1_DATA if data is None else data),
        discovery_keys=MappingProxyType({}),
        domain="mitsubishi_climate_proxy",
        entry_id=entry_id,
        minor_version=1,
        modified_at=STAMP,
        options=dict(V1_OPTIONS if options is None else options),
        source="user",
        subentries_data=(),
        title=title,
        unique_id=f"{SOURCE}_hybrid",
        version=version,
    )
    hass.config_entries._entries[entry.entry_id] = entry
    return entry


def _snapshot(entry: ConfigEntry) -> dict:
    return {
        "entry_id": entry.entry_id,
        "unique_id": entry.unique_id,
        "title": entry.title,
        "domain": entry.domain,
        "minor_version": entry.minor_version,
        "version": entry.version,
        "data": dict(entry.data),
        "options": dict(entry.options),
        "modified_at": entry.modified_at,
    }


async def test_manifest_version_is_next_stable_release() -> None:
    """The HACS-visible manifest matches the unpublished 2026.11.1 fix release."""
    manifest = json.loads(REPO_MANIFEST.read_text())
    assert manifest["version"] == "2026.11.1"
    assert manifest["domain"] == "mitsubishi_climate_proxy"
    assert MitsubishiHybridConfigFlow.VERSION == 2


async def test_v1_handler_changes_only_version_metadata(hass) -> None:
    """Version 1 keeps data, options and identity; Core bumps modified_at."""
    options = {"restore_last_mode": False}
    entry = _entry(hass, version=1, options=options)
    before = _snapshot(entry)

    assert await async_migrate_entry(hass, entry) is True

    assert entry.version == 2
    assert entry.minor_version == before["minor_version"]
    assert entry.entry_id == before["entry_id"]
    assert entry.unique_id == before["unique_id"]
    assert entry.title == before["title"]
    assert dict(entry.data) == before["data"] == V1_DATA
    assert dict(entry.options) == before["options"] == options
    assert entry.modified_at > before["modified_at"]


async def test_v2_handler_is_idempotent(hass) -> None:
    """An entry already at version 2 is accepted and not rewritten."""
    data = {
        "source": SOURCE,
        "name": "Room",
        "coordinator_single_target": False,
        "restore_last_mode": True,
    }
    options = {"comfort_offset": 6.0, "default_turn_on_mode": "heat"}
    entry = _entry(hass, version=2, data=data, options=options)
    before = _snapshot(entry)

    assert await async_migrate_entry(hass, entry) is True
    assert await async_migrate_entry(hass, entry) is True

    assert _snapshot(entry) == before


async def test_unexpected_version_is_rejected(hass) -> None:
    """Versions other than 1 and 2 fail closed without touching the entry."""
    for version in (0, 3, 99):
        entry = _entry(
            hass,
            version=version,
            entry_id=f"proxy-entry-v{version}",
        )
        before = _snapshot(entry)

        assert await async_migrate_entry(hass, entry) is False

        assert _snapshot(entry) == before


async def test_core_setup_migrates_v1_and_preserves_storage(hass, caplog) -> None:
    """Core setup loads the component handler and writes version 2 only."""
    entry = _entry(hass, version=1)
    entry_id = entry.entry_id
    data_before = dict(entry.data)
    options_before = dict(entry.options)

    _publish_source(hass)
    assert await hass.config_entries.async_setup(entry_id)
    await hass.async_block_till_done()

    entry = hass.config_entries.async_get_entry(entry_id)
    assert entry.state is ConfigEntryState.LOADED
    assert entry.version == 2
    assert dict(entry.data) == data_before
    assert dict(entry.options) == options_before
    assert entry.entry_id == entry_id
    assert entry.unique_id == f"{SOURCE}_hybrid"
    assert "Migration handler not found" not in caplog.text
    assert "Cannot migrate" not in caplog.text

    integration = await async_get_integration(hass, "mitsubishi_climate_proxy")
    component = await integration.async_get_component()
    assert component.async_migrate_entry is async_migrate_entry

    registry = er.async_get(hass)
    entities = er.async_entries_for_config_entry(registry, entry_id)
    assert len(entities) == 1
    assert entities[0].config_entry_id == entry_id
    assert entities[0].unique_id == entry_id
    state = hass.states.get(entities[0].entity_id)
    assert state is not None
    assert state.state == HVACMode.HEAT

    await hass.config_entries._store._async_callback_delayed_write()
    stored = json.loads(
        (Path(hass.config.config_dir) / ".storage" / "core.config_entries").read_text()
    )
    saved = next(item for item in stored["data"]["entries"] if item["entry_id"] == entry_id)
    assert saved["version"] == 2
    assert saved["data"] == data_before
    assert saved["options"] == options_before
    assert saved["unique_id"] == f"{SOURCE}_hybrid"
    assert saved["minor_version"] == 1


async def test_core_setup_leaves_v2_entry_unchanged(hass) -> None:
    """Core does not call a rewrite when the stored version is already 2."""
    data = {**V1_DATA, "horizontal_vane_entity": "select.room_vane"}
    entry = _entry(hass, version=2, data=data, entry_id="proxy-entry-v2")
    before = _snapshot(entry)
    _publish_source(hass)

    assert await hass.config_entries.async_setup(entry.entry_id)
    await hass.async_block_till_done()

    assert entry.state is ConfigEntryState.LOADED
    assert _snapshot(entry) == before


async def test_core_refuses_newer_entry_without_rewriting_it(hass, caplog) -> None:
    """An entry newer than ConfigFlow.VERSION is the downgrade failure mode."""
    entry = _entry(hass, version=3, entry_id="proxy-entry-newer")
    before = _snapshot(entry)

    assert not await hass.config_entries.async_setup(entry.entry_id)
    await hass.async_block_till_done()

    assert entry.state is ConfigEntryState.MIGRATION_ERROR
    assert _snapshot(entry) == before
    assert "higher than the current version" in caplog.text


async def test_core_refuses_version_zero_via_the_handler(hass, caplog) -> None:
    """A stored version Core actually hands to the handler is rejected."""
    entry = _entry(hass, version=0, entry_id="proxy-entry-zero")
    before = _snapshot(entry)

    assert not await hass.config_entries.async_setup(entry.entry_id)
    await hass.async_block_till_done()

    assert entry.state is ConfigEntryState.MIGRATION_ERROR
    assert _snapshot(entry) == before
    assert "Cannot migrate" in caplog.text


async def test_downgrade_to_flow_version_1_is_refused_by_core(hass, caplog) -> None:
    """After migration, loading a VERSION=1 flow refuses the stored entry."""
    entry = _entry(hass, version=2, entry_id="proxy-entry-downgrade")
    before = _snapshot(entry)
    assert MitsubishiHybridConfigFlow.VERSION == 2
    MitsubishiHybridConfigFlow.VERSION = 1
    try:
        migrated = await entry.async_migrate(hass)
    finally:
        MitsubishiHybridConfigFlow.VERSION = 2

    assert migrated is False
    assert _snapshot(entry) == before
    assert MitsubishiHybridConfigFlow.VERSION == 2
    assert "higher than the current version" in caplog.text


class _SourceClimate(ClimateEntity):
    """ESPHome climate stand-in registered with the real climate component."""

    _attr_name = "Room ESPHome"
    _attr_unique_id = "room-esphome-source"
    _attr_hvac_mode = HVACMode.HEAT
    _attr_hvac_modes = [
        HVACMode.OFF,
        HVACMode.HEAT,
        HVACMode.COOL,
        HVACMode.HEAT_COOL,
    ]
    _attr_supported_features = (
        ClimateEntityFeature.TARGET_TEMPERATURE
        | ClimateEntityFeature.TARGET_TEMPERATURE_RANGE
    )
    _attr_target_temperature_low = 20
    _attr_target_temperature_high = 24
    _attr_current_temperature = 21
    _attr_min_temp = 16
    _attr_max_temp = 31
    _attr_temperature_unit = UnitOfTemperature.CELSIUS

    def __init__(self) -> None:
        """Force the entity id the proxy config entry points at."""
        self.entity_id = SOURCE
        self.calls: list[dict] = []

    async def async_set_temperature(self, **kwargs) -> None:
        """Record the service call Home Assistant delivers to the source."""
        self.calls.append(kwargs)


async def test_loaded_proxy_forwards_heat_setpoint_to_source_low(hass) -> None:
    """A migrated proxy forwards a single heat setpoint to target_temp_low."""
    source = _SourceClimate()
    await hass.data[DATA_COMPONENT].async_add_entities([source])
    await hass.async_block_till_done()
    assert hass.states.get(SOURCE) is not None

    entry = _entry(hass, version=1, entry_id="proxy-entry-setpoint")
    assert await hass.config_entries.async_setup(entry.entry_id)
    await hass.async_block_till_done()
    assert entry.version == 2
    assert dict(entry.data) == V1_DATA

    registry = er.async_get(hass)
    proxy_entity_id = er.async_entries_for_config_entry(registry, entry.entry_id)[
        0
    ].entity_id
    await hass.services.async_call(
        "climate",
        "set_temperature",
        {"entity_id": proxy_entity_id, ATTR_TEMPERATURE: 20.5},
        blocking=True,
    )

    assert source.calls == [
        {
            "entity_id": [SOURCE],
            "target_temp_low": 20.5,
            "target_temp_high": 24.0,
        }
    ]
    assert dict(entry.data) == V1_DATA
    assert dict(entry.options) == V1_OPTIONS


def _publish_source(hass) -> None:
    """Publish the ESPHome climate the proxy wraps. It is not another proxy."""
    hass.states.async_set(
        SOURCE,
        HVACMode.HEAT,
        {
            "hvac_modes": [
                HVACMode.OFF,
                HVACMode.HEAT,
                HVACMode.COOL,
                HVACMode.HEAT_COOL,
            ],
            "supported_features": int(
                ClimateEntityFeature.TARGET_TEMPERATURE
                | ClimateEntityFeature.TARGET_TEMPERATURE_RANGE
            ),
            "target_temp_low": 20,
            "target_temp_high": 24,
            "current_temperature": 21,
            "min_temp": 16,
            "max_temp": 31,
        },
    )

