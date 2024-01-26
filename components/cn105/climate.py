import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate, uart
from esphome.components import select
from esphome.components.logger import HARDWARE_UART_TO_SERIAL
from esphome.components.uart import UARTParityOptions

from esphome.const import (
    CONF_ID,
    CONF_UPDATE_INTERVAL,
    CONF_MODE,
    CONF_FAN_MODE,
    CONF_SWING_MODE,
    CONF_UART_ID,
)
from esphome.core import CORE, coroutine

AUTO_LOAD = ["climate", "sensor", "select", "binary_sensor", "text_sensor", "uart"]

CONF_SUPPORTS = "supports"
DEFAULT_CLIMATE_MODES = ["HEAT_COOL", "COOL", "HEAT", "DRY", "FAN_ONLY"]
DEFAULT_FAN_MODES = ["AUTO", "QUIET", "LOW", "MEDIUM", "HIGH"]
DEFAULT_SWING_MODES = ["OFF", "VERTICAL", "HORIZONTAL", "BOTH"]

# from https://github.com/wrouesnel/esphome-mitsubishiheatpump/blob/master/components/mitsubishi_heatpump/climate.py
# HORIZONTAL_SWING_OPTIONS = [
#     "AUTO",
#     "SWING",
#     "LEFT",
#     "LEFT_CENTER",
#     "CENTER",
#     "RIGHT_CENTER",
#     "RIGHT",
# ]


# Déclaration des constantes pour TX et RX pin
CONF_TX_PIN = "tx_pin"
CONF_RX_PIN = "rx_pin"

CN105Climate = cg.global_ns.class_("CN105Climate", climate.Climate, cg.Component)


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
        cv.GenerateID(CONF_UART_ID): cv.use_id(uart.UARTComponent),
        cv.Optional("baud_rate"): cv.invalid(
            "baud_rate' option is not supported anymore. Please add a separate UART component with baud_rate configured."
        ),
        cv.Optional("hardware_uart"): cv.invalid(
            "'hardware_uart' options is not supported anymore. Please add a separate UART component with the correct rx and tx pin."
        ),
        # cv.Optional(CONF_HARDWARE_UART, default="UART0"): valid_uart,
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
    # serial = HARDWARE_UART_TO_SERIAL[config[CONF_HARDWARE_UART]]
    # var = cg.new_Pvariable(config[CONF_ID], cg.RawExpression(f"&{serial}"))

    uart_var = yield cg.get_variable(config["uart_id"])

    var = cg.new_Pvariable(config[CONF_ID], uart_var)

    cg.add(uart_var.set_data_bits(8))
    cg.add(uart_var.set_parity(UARTParityOptions.UART_CONFIG_PARITY_EVEN))
    cg.add(uart_var.set_stop_bits(1))

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
