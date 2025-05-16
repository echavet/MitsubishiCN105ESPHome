import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components.uptime.sensor import UptimeSecondsSensor

from esphome.const import ENTITY_CATEGORY_DIAGNOSTIC, ICON_TIMER


from esphome.components import (
    climate,
    uart,
    select,
    sensor,
    button,
    binary_sensor,
    text_sensor,
    uptime,
    number,
)

from esphome.components.logger import HARDWARE_UART_TO_SERIAL
from esphome.components.uart import UARTParityOptions

from esphome.const import (
    CONF_ID,
    CONF_UPDATE_INTERVAL,
    CONF_MODE,
    CONF_FAN_MODE,
    CONF_SWING_MODE,
    CONF_UART_ID,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_SECOND,
    ICON_TIMER,
    DEVICE_CLASS_DURATION,
    CONF_TX_PIN,
    CONF_RX_PIN,
)


from esphome.core import CORE, coroutine

AUTO_LOAD = [
    "climate",
    "sensor",
    "select",
    "binary_sensor",
    "button",
    "text_sensor",
    "uart",
    "uptime",
    "number",
]

DEPENDENCIES = ["uptime"]

CONF_SUPPORTS = "supports"
# from https://github.com/wrouesnel/esphome-mitsubishiheatpump/blob/master/components/mitsubishi_heatpump/climate.py
CONF_HORIZONTAL_SWING_SELECT = "horizontal_vane_select"
CONF_VERTICAL_SWING_SELECT = "vertical_vane_select"
CONF_COMPRESSOR_FREQUENCY_SENSOR = "compressor_frequency_sensor"
CONF_INPUT_POWER_SENSOR = "input_power_sensor"
CONF_KWH_SENSOR = "kwh_sensor"
CONF_RUNTIME_HOURS_SENSOR = "runtime_hours_sensor"
CONF_OUTSIDE_AIR_TEMPERATURE_SENSOR = "outside_air_temperature_sensor"
CONF_ISEE_SENSOR = "isee_sensor"
CONF_FUNCTIONS_SENSOR = "functions_sensor"
CONF_FUNCTIONS_BUTTON = "functions_get_button"
CONF_FUNCTIONS_SET_BUTTON = "functions_set_button"
CONF_FUNCTIONS_SET_CODE = "functions_set_code"
CONF_FUNCTIONS_SET_VALUE = "functions_set_value"
CONF_STAGE_SENSOR = "stage_sensor"
CONF_SUB_MODE_SENSOR = "sub_mode_sensor"
CONF_AUTO_SUB_MODE_SENSOR = "auto_sub_mode_sensor"
CONF_HP_UP_TIME_CONNECTION_SENSOR = "hp_uptime_connection_sensor"

DEFAULT_CLIMATE_MODES = ["AUTO", "COOL", "HEAT", "DRY", "FAN_ONLY"]
DEFAULT_FAN_MODES = ["AUTO", "MIDDLE", "QUIET", "LOW", "MEDIUM", "HIGH"]
DEFAULT_SWING_MODES = ["OFF", "VERTICAL", "HORIZONTAL", "BOTH"]


CN105Climate = cg.global_ns.class_("CN105Climate", climate.Climate, cg.Component)

CONF_REMOTE_TEMP_TIMEOUT = "remote_temperature_timeout"
CONF_DEBOUNCE_DELAY = "debounce_delay"

VaneOrientationSelect = cg.global_ns.class_(
    "VaneOrientationSelect", select.Select, cg.Component
)

CompressorFrequencySensor = cg.global_ns.class_(
    "CompressorFrequencySensor", sensor.Sensor, cg.Component
)

InputPowerSensor = cg.global_ns.class_("InputPowerSensor", sensor.Sensor, cg.Component)

kWhSensor = cg.global_ns.class_("kWhSensor", sensor.Sensor, cg.Component)

RuntimeHoursSensor = cg.global_ns.class_(
    "RuntimeHoursSensor", sensor.Sensor, cg.Component
)

OutsideAirTemperatureSensor = cg.global_ns.class_(
    "OutsideAirTemperatureSensor", sensor.Sensor, cg.Component
)

ISeeSensor = cg.global_ns.class_("ISeeSensor", binary_sensor.BinarySensor, cg.Component)
StageSensor = cg.global_ns.class_("StageSensor", text_sensor.TextSensor, cg.Component)
FunctionsSensor = cg.global_ns.class_(
    "FunctionsSensor", text_sensor.TextSensor, cg.Component
)
FunctionsButton = cg.global_ns.class_("FunctionsButton", button.Button, cg.Component)
FunctionsNumber = cg.global_ns.class_("FunctionsNumber", number.Number, cg.Component)
SubModSensor = cg.global_ns.class_("SubModSensor", text_sensor.TextSensor, cg.Component)
AutoSubModSensor = cg.global_ns.class_(
    "AutoSubModSensor", text_sensor.TextSensor, cg.Component
)

uptime_ns = cg.esphome_ns.namespace("uptime")
uptime = uptime_ns.class_("UptimeSecondsSensor", sensor.Sensor, cg.Component)
HpUpTimeConnectionSensor = uptime_ns.class_(
    "HpUpTimeConnectionSensor", sensor.Sensor, cg.PollingComponent
)


# --- help function to retreive tx and rx pin settings from UART section ---
def get_uart_pins_from_config(core_config, target_uart_id_str):
    """
    Récupère les numéros de broches TX et RX pour un UART_ID donné
    à partir de la configuration globale d'ESPHome.
    """
    tx_pin_num = -1
    rx_pin_num = -1
    uart_config_found = {}  # Renommé pour plus de clarté

    # Recherche de la configuration de l'UART correspondant
    for uart_conf_item in core_config.get("uart", []):
        # uart_conf_item[CONF_ID] est un objet esphome.core.ID
        # target_uart_id_str est la représentation en chaîne de l'ID que nous cherchons
        if str(uart_conf_item[CONF_ID]) == target_uart_id_str:  # MODIFICATION ICI
            uart_config_found = uart_conf_item
            break

    if uart_config_found:  # Vérifie si le dictionnaire n'est pas vide
        tx_pin_schema = uart_config_found.get(CONF_TX_PIN)
        if tx_pin_schema:
            if isinstance(tx_pin_schema, dict) and "number" in tx_pin_schema:
                tx_pin_num = tx_pin_schema["number"]
            elif isinstance(tx_pin_schema, int):
                tx_pin_num = tx_pin_schema

        rx_pin_schema = uart_config_found.get(CONF_RX_PIN)
        if rx_pin_schema:
            if isinstance(rx_pin_schema, dict) and "number" in rx_pin_schema:
                rx_pin_num = rx_pin_schema["number"]
            elif isinstance(rx_pin_schema, int):
                rx_pin_num = rx_pin_schema

    return tx_pin_num, rx_pin_num


# --- END help function ---


def valid_uart(uart):
    if CORE.is_esp8266:
        uarts = ["UART0"]  # UART1 is tx-only
    elif CORE.is_esp32:
        uarts = ["UART0", "UART1", "UART2"]
    else:
        raise NotImplementedError

    return cv.one_of(*uarts, upper=True)(uart)


SELECT_SCHEMA = select.SELECT_SCHEMA.extend(
    {cv.GenerateID(CONF_ID): cv.declare_id(VaneOrientationSelect)}
)

COMPRESSOR_FREQUENCY_SENSOR_SCHEMA = sensor.SENSOR_SCHEMA.extend(
    {cv.GenerateID(CONF_ID): cv.declare_id(CompressorFrequencySensor)}
)

INPUT_POWER_SENSOR_SCHEMA = sensor.SENSOR_SCHEMA.extend(
    {cv.GenerateID(CONF_ID): cv.declare_id(InputPowerSensor)}
)

KWH_SENSOR_SCHEMA = sensor.SENSOR_SCHEMA.extend(
    {cv.GenerateID(CONF_ID): cv.declare_id(kWhSensor)}
)

RUNTIME_HOURS_SENSOR_SCHEMA = sensor.SENSOR_SCHEMA.extend(
    {cv.GenerateID(CONF_ID): cv.declare_id(RuntimeHoursSensor)}
)

OUTSIDE_AIR_TEMPERATURE_SENSOR_SCHEMA = sensor.SENSOR_SCHEMA.extend(
    {cv.GenerateID(CONF_ID): cv.declare_id(OutsideAirTemperatureSensor)}
)

ISEE_SENSOR_SCHEMA = binary_sensor.BINARY_SENSOR_SCHEMA.extend(
    {cv.GenerateID(CONF_ID): cv.declare_id(ISeeSensor)}
)

FUNCTIONS_SENSOR_SCHEMA = text_sensor.TEXT_SENSOR_SCHEMA.extend(
    {cv.GenerateID(CONF_ID): cv.declare_id(FunctionsSensor)}
)

FUNCTIONS_BUTTON_SCHEMA = button.BUTTON_SCHEMA.extend(
    {cv.GenerateID(CONF_ID): cv.declare_id(FunctionsButton)}
)

FUNCTIONS_NUMBER_SCHEMA = number.NUMBER_SCHEMA.extend(
    {cv.GenerateID(CONF_ID): cv.declare_id(FunctionsNumber)}
)

STAGE_SENSOR_SCHEMA = text_sensor.TEXT_SENSOR_SCHEMA.extend(
    {cv.GenerateID(CONF_ID): cv.declare_id(StageSensor)}
)

SUB_MODE_SENSOR_SCHEMA = text_sensor.TEXT_SENSOR_SCHEMA.extend(
    {cv.GenerateID(CONF_ID): cv.declare_id(SubModSensor)}
)

AUTO_SUB_MODE_SENSOR_SCHEMA = text_sensor.TEXT_SENSOR_SCHEMA.extend(
    {cv.GenerateID(CONF_ID): cv.declare_id(AutoSubModSensor)}
)


HP_UP_TIME_CONNECTION_SENSOR_SCHEMA = sensor.sensor_schema(
    HpUpTimeConnectionSensor,
    unit_of_measurement=UNIT_SECOND,
    icon=ICON_TIMER,
    accuracy_decimals=0,
    state_class=STATE_CLASS_TOTAL_INCREASING,
    device_class=DEVICE_CLASS_DURATION,
    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
).extend(cv.polling_component_schema("60s"))

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
        cv.Optional(CONF_UPDATE_INTERVAL, default="2s"): cv.All(cv.update_interval),
        cv.Optional(CONF_HORIZONTAL_SWING_SELECT): SELECT_SCHEMA,
        cv.Optional(CONF_VERTICAL_SWING_SELECT): SELECT_SCHEMA,
        cv.Optional(
            CONF_COMPRESSOR_FREQUENCY_SENSOR
        ): COMPRESSOR_FREQUENCY_SENSOR_SCHEMA,
        cv.Optional(CONF_INPUT_POWER_SENSOR): INPUT_POWER_SENSOR_SCHEMA,
        cv.Optional(CONF_KWH_SENSOR): KWH_SENSOR_SCHEMA,
        cv.Optional(CONF_RUNTIME_HOURS_SENSOR): RUNTIME_HOURS_SENSOR_SCHEMA,
        cv.Optional(
            CONF_OUTSIDE_AIR_TEMPERATURE_SENSOR
        ): OUTSIDE_AIR_TEMPERATURE_SENSOR_SCHEMA,
        cv.Optional(CONF_ISEE_SENSOR): ISEE_SENSOR_SCHEMA,
        cv.Optional(CONF_FUNCTIONS_SENSOR): FUNCTIONS_SENSOR_SCHEMA,
        cv.Optional(CONF_FUNCTIONS_BUTTON): FUNCTIONS_BUTTON_SCHEMA,
        cv.Optional(CONF_FUNCTIONS_SET_BUTTON): FUNCTIONS_BUTTON_SCHEMA,
        cv.Optional(CONF_FUNCTIONS_SET_CODE): FUNCTIONS_NUMBER_SCHEMA,
        cv.Optional(CONF_FUNCTIONS_SET_VALUE): FUNCTIONS_NUMBER_SCHEMA,
        cv.Optional(CONF_STAGE_SENSOR): STAGE_SENSOR_SCHEMA,
        cv.Optional(CONF_SUB_MODE_SENSOR): SUB_MODE_SENSOR_SCHEMA,
        cv.Optional(CONF_AUTO_SUB_MODE_SENSOR): AUTO_SUB_MODE_SENSOR_SCHEMA,
        cv.Optional(CONF_REMOTE_TEMP_TIMEOUT, default="never"): cv.All(
            cv.update_interval
        ),
        cv.Optional(CONF_DEBOUNCE_DELAY, default="100ms"): cv.All(cv.update_interval),
        cv.Optional(
            CONF_HP_UP_TIME_CONNECTION_SENSOR
        ): HP_UP_TIME_CONNECTION_SENSOR_SCHEMA,
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

    uart_id_object = config[CONF_UART_ID]
    uart_var = yield cg.get_variable(uart_id_object)
    var = cg.new_Pvariable(config[CONF_ID], uart_var)

    cg.add(uart_var.set_data_bits(8))
    cg.add(uart_var.set_parity(UARTParityOptions.UART_CONFIG_PARITY_EVEN))
    cg.add(uart_var.set_stop_bits(1))

    uart_id_str_for_lookup = str(uart_id_object)
    tx_pin, rx_pin = get_uart_pins_from_config(CORE.config, uart_id_str_for_lookup)

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

    cg.add(var.set_remote_temp_timeout(config[CONF_REMOTE_TEMP_TIMEOUT]))

    cg.add(var.set_debounce_delay(config[CONF_DEBOUNCE_DELAY]))

    if CONF_HORIZONTAL_SWING_SELECT in config:
        conf = config[CONF_HORIZONTAL_SWING_SELECT]
        swing_select = yield select.new_select(conf, options=[])
        yield cg.register_component(swing_select, conf)
        cg.add(var.set_horizontal_vane_select(swing_select))

    if CONF_VERTICAL_SWING_SELECT in config:
        conf = config[CONF_VERTICAL_SWING_SELECT]
        swing_select = yield select.new_select(conf, options=[])
        yield cg.register_component(swing_select, conf)
        cg.add(var.set_vertical_vane_select(swing_select))

    if CONF_COMPRESSOR_FREQUENCY_SENSOR in config:
        conf = config[CONF_COMPRESSOR_FREQUENCY_SENSOR]
        conf["force_update"] = False
        sensor_ = yield sensor.new_sensor(conf)
        yield cg.register_component(sensor_, conf)
        cg.add(var.set_compressor_frequency_sensor(sensor_))

    if CONF_INPUT_POWER_SENSOR in config:
        conf = config[CONF_INPUT_POWER_SENSOR]
        conf["force_update"] = False
        sensor_ = yield sensor.new_sensor(conf)
        yield cg.register_component(sensor_, conf)
        cg.add(var.set_input_power_sensor(sensor_))

    if CONF_KWH_SENSOR in config:
        conf = config[CONF_KWH_SENSOR]
        conf["force_update"] = False
        sensor_ = yield sensor.new_sensor(conf)
        yield cg.register_component(sensor_, conf)
        cg.add(var.set_kwh_sensor(sensor_))

    if CONF_RUNTIME_HOURS_SENSOR in config:
        conf = config[CONF_RUNTIME_HOURS_SENSOR]
        conf["force_update"] = False
        sensor_ = yield sensor.new_sensor(conf)
        yield cg.register_component(sensor_, conf)
        cg.add(var.set_runtime_hours_sensor(sensor_))

    if CONF_OUTSIDE_AIR_TEMPERATURE_SENSOR in config:
        conf = config[CONF_OUTSIDE_AIR_TEMPERATURE_SENSOR]
        conf["force_update"] = False
        sensor_ = yield sensor.new_sensor(conf)
        yield cg.register_component(sensor_, conf)
        cg.add(var.set_outside_air_temperature_sensor(sensor_))

    if CONF_ISEE_SENSOR in config:
        conf = config[CONF_ISEE_SENSOR]
        bsensor_ = yield binary_sensor.new_binary_sensor(conf)
        yield cg.register_component(bsensor_, conf)
        cg.add(var.set_isee_sensor(bsensor_))

    if CONF_FUNCTIONS_SENSOR in config:
        conf = config[CONF_FUNCTIONS_SENSOR]
        tsensor_ = yield text_sensor.new_text_sensor(conf)
        yield cg.register_component(tsensor_, conf)
        cg.add(var.set_functions_sensor(tsensor_))

    if CONF_FUNCTIONS_BUTTON in config:
        conf = config[CONF_FUNCTIONS_BUTTON]
        button_ = yield button.new_button(conf)
        yield cg.register_component(button_, conf)
        cg.add(var.set_functions_get_button(button_))

    if CONF_FUNCTIONS_SET_BUTTON in config:
        conf = config[CONF_FUNCTIONS_SET_BUTTON]
        button_ = yield button.new_button(conf)
        yield cg.register_component(button_, conf)
        cg.add(var.set_functions_set_button(button_))

    if CONF_FUNCTIONS_SET_CODE in config:
        conf = config[CONF_FUNCTIONS_SET_CODE]
        number_ = yield number.new_number(conf, min_value=100, max_value=128, step=1)
        yield cg.register_component(number_, conf)
        cg.add(var.set_functions_set_code(number_))

    if CONF_FUNCTIONS_SET_VALUE in config:
        conf = config[CONF_FUNCTIONS_SET_VALUE]
        number_ = yield number.new_number(conf, min_value=1, max_value=3, step=1)
        yield cg.register_component(number_, conf)
        cg.add(var.set_functions_set_value(number_))

    if CONF_STAGE_SENSOR in config:
        conf = config[CONF_STAGE_SENSOR]
        tsensor_ = yield text_sensor.new_text_sensor(conf)
        yield cg.register_component(tsensor_, conf)
        cg.add(var.set_stage_sensor(tsensor_))

    if CONF_SUB_MODE_SENSOR in config:
        conf = config[CONF_SUB_MODE_SENSOR]
        tsensor_ = yield text_sensor.new_text_sensor(conf)
        yield cg.register_component(tsensor_, conf)
        cg.add(var.set_sub_mode_sensor(tsensor_))

    if CONF_AUTO_SUB_MODE_SENSOR in config:
        conf = config[CONF_AUTO_SUB_MODE_SENSOR]
        tsensor_ = yield text_sensor.new_text_sensor(conf)
        yield cg.register_component(tsensor_, conf)
        cg.add(var.set_auto_sub_mode_sensor(tsensor_))

    if CONF_HP_UP_TIME_CONNECTION_SENSOR in config:
        conf = config[CONF_HP_UP_TIME_CONNECTION_SENSOR]
        hp_connection_sensor_ = yield sensor.new_sensor(conf)
        yield cg.register_component(hp_connection_sensor_, conf)
        cg.add(var.set_hp_uptime_connection_sensor(hp_connection_sensor_))

    yield cg.register_component(var, config)
    yield climate.register_climate(var, config)


## cg.add_library(
##    name="HeatPump",
##    repository="https://github.com/SwiCago/HeatPump",
##    version="cea90c5ed48d24a904835f8918bd88cbc84cb1be",
## )
