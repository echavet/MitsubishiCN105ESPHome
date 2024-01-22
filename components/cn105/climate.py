import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate, uart
from esphome.components import select
from esphome.components.logger import HARDWARE_UART_TO_SERIAL

from esphome.const import (
    CONF_ID,
    CONF_HARDWARE_UART,
    CONF_BAUD_RATE,
    CONF_UPDATE_INTERVAL,
    CONF_MODE,
    CONF_FAN_MODE,
    CONF_SWING_MODE,
)
from esphome.core import CORE, coroutine

AUTO_LOAD = ["climate", "sensor", "select", "binary_sensor", "text_sensor"]

CONF_SUPPORTS = "supports"
DEFAULT_CLIMATE_MODES = ["HEAT_COOL", "COOL", "HEAT", "DRY", "FAN_ONLY"]
DEFAULT_FAN_MODES = ["AUTO", "QUIET", "LOW", "MEDIUM", "HIGH"]
DEFAULT_SWING_MODES = ["OFF", "VERTICAL", "HORIZONTAL", "BOTH"]

# Déclaration des constantes pour TX et RX pin
CONF_TX_PIN = "tx_pin"
CONF_RX_PIN = "rx_pin"

CN105Climate = cg.global_ns.class_("CN105Climate", climate.Climate, cg.PollingComponent)


def valid_uart(uart):
    if CORE.is_esp8266:
        uarts = ["UART0"]  # UART1 is tx-only
    elif CORE.is_esp32:
        uarts = ["UART0", "UART1", "UART2"]
    else:
        raise NotImplementedError

    return cv.one_of(*uarts, upper=True)(uart)


CONFIG_SCHEMA = climate.CLIMATE_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(CN105Climate),
        cv.Optional(CONF_HARDWARE_UART, default="UART0"): valid_uart,
        cv.Optional(CONF_BAUD_RATE): cv.positive_int,
        cv.Optional(CONF_TX_PIN): cv.positive_int,
        cv.Optional(CONF_RX_PIN): cv.positive_int,
        cv.Optional(CONF_UPDATE_INTERVAL, default="0ms"): cv.All(cv.update_interval),
        # Optionally override the supported ClimateTraits.
        cv.Optional(CONF_SUPPORTS, default={}): cv.Schema(
            {
                cv.Optional(CONF_MODE, default=DEFAULT_CLIMATE_MODES): cv.ensure_list(
                    climate.validate_climate_mode
                ),
                cv.Optional(CONF_FAN_MODE, default=DEFAULT_FAN_MODES): cv.ensure_list(
                    climate.validate_climate_fan_mode
                ),
                cv.Optional(
                    CONF_SWING_MODE, default=DEFAULT_SWING_MODES
                ): cv.ensure_list(climate.validate_climate_swing_mode),
            }
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


@coroutine
def to_code(config):
    serial = HARDWARE_UART_TO_SERIAL[config[CONF_HARDWARE_UART]]
    var = cg.new_Pvariable(config[CONF_ID], cg.RawExpression(f"&{serial}"))

    if CONF_BAUD_RATE in config:
        cg.add(var.set_baud_rate(config[CONF_BAUD_RATE]))

    # Traitement des configurations de broches TX et RX
    if CONF_TX_PIN in config and CONF_RX_PIN in config:
        tx_pin = config[CONF_TX_PIN]
        rx_pin = config[CONF_RX_PIN]
        cg.add(var.set_tx_rx_pins(tx_pin, rx_pin))

    supports = config[CONF_SUPPORTS]
    traits = var.config_traits()

    for mode in supports[CONF_MODE]:
        if mode == "OFF":
            continue
        cg.add(traits.add_supported_mode(climate.CLIMATE_MODES[mode]))

    for mode in supports[CONF_FAN_MODE]:
        cg.add(traits.add_supported_fan_mode(climate.CLIMATE_FAN_MODES[mode]))

    for mode in supports[CONF_SWING_MODE]:
        cg.add(traits.add_supported_swing_mode(climate.CLIMATE_SWING_MODES[mode]))

    yield cg.register_component(var, config)
    yield climate.register_climate(var, config)

    ## cg.add_library(
    ##    name="HeatPump",
    ##    repository="https://github.com/SwiCago/HeatPump",
    ##    version="cea90c5ed48d24a904835f8918bd88cbc84cb1be",
    ## )
