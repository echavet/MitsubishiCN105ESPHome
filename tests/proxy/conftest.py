"""Home Assistant 2026.11.0 fixture for the climate proxy migration tests."""

from __future__ import annotations

from collections.abc import AsyncIterator
from pathlib import Path
import sys

import pytest

REPO_ROOT = Path(__file__).resolve().parents[2]
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from homeassistant.bootstrap import async_load_base_functionality
from homeassistant.config_entries import ConfigEntries
from homeassistant.core import CoreState, HomeAssistant
from homeassistant.loader import async_get_custom_components, async_setup as async_setup_loader
from homeassistant.setup import async_setup_component


@pytest.fixture
async def hass(tmp_path: Path) -> AsyncIterator[HomeAssistant]:
    """Start a real Home Assistant core with this repo's custom component mounted.

    Registries and restore-state are loaded the same way bootstrap does, so
    config-entry setup and the climate platform run inside Core 2026.11.0
    rather than a reimplementation of its migration predicate.
    """
    config_dir = tmp_path / "config"
    custom_components = config_dir / "custom_components"
    custom_components.mkdir(parents=True)
    (custom_components / "mitsubishi_climate_proxy").symlink_to(
        REPO_ROOT / "custom_components" / "mitsubishi_climate_proxy",
        target_is_directory=True,
    )

    hass = HomeAssistant(str(config_dir))
    async_setup_loader(hass)
    hass.config_entries = ConfigEntries(hass, {})
    hass.state = CoreState.running
    assert await async_load_base_functionality(hass)
    assert await async_get_custom_components(hass)
    assert await async_setup_component(hass, "climate", {})
    await hass.async_block_till_done()

    try:
        yield hass
    finally:
        await hass.async_stop()
        await hass.async_block_till_done()
