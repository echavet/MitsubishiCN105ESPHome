import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components.uptime.sensor import UptimeSecondsSensor

from esphome.components import (
    climate,
    uart,
    select,
    sensor,
    button,
	switch,
    binary_sensor,
    text_sensor,
    uptime,
    number,
)
from esphome.components.uart import UARTParityOptions

from esphome.const import (
    CONF_ID,
    CONF_NAME,
    CONF_ICON,
    CONF_UPDATE_INTERVAL,
    CONF_MODE,
    CONF_FAN_MODE,
    CONF_SWING_MODE,
    CONF_UART_ID,
    CONF_ENTITY_CATEGORY,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_SECOND,
    ICON_TIMER,
    DEVICE_CLASS_DURATION,
    CONF_TX_PIN,
    CONF_RX_PIN,
)
from esphome.components.sensor import (
    CONF_UNIT_OF_MEASUREMENT as SENSOR_CONF_UNIT_OF_MEASUREMENT,
    CONF_ACCURACY_DECIMALS as SENSOR_CONF_ACCURACY_DECIMALS,
    CONF_DEVICE_CLASS as SENSOR_CONF_DEVICE_CLASS,
    CONF_STATE_CLASS as SENSOR_CONF_STATE_CLASS,
)
from esphome.core import CORE, coroutine

# ... (AUTO_LOAD, DEPENDENCIES, toutes les constantes CONF_XXX_SENSOR, DEFAULT_MODES - identiques à votre version) ...
AUTO_LOAD = [
    "climate",
    "sensor",
    "select",
    "binary_sensor",
    "button",
    "switch",
    "text_sensor",
    "uart",
    "uptime",
    "number",
]
DEPENDENCIES = ["uart", "uptime"]  # Garder uart ici aussi

CONF_SUPPORTS = "supports"
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
CONF_USE_AS_OPERATING_FALLBACK = "use_as_operating_fallback"  # Nouvelle constante
CONF_FAHRENHEIT_SUPPORT_MODE = "fahrenheit_compatibility"
CONF_AIRFLOW_CONTROL_SELECT = "airflow_control_select"
CONF_AIR_PURIFIER_SWITCH = "air_purifier_switch"
CONF_NIGHT_MODE_SWITCH = "night_mode_switch"
CONF_CIRCULATOR_SWITCH = "circulator_switch"

DEFAULT_CLIMATE_MODES = ["AUTO", "COOL", "HEAT", "DRY", "FAN_ONLY"]
DEFAULT_FAN_MODES = ["AUTO", "MIDDLE", "QUIET", "LOW", "MEDIUM", "HIGH"]
DEFAULT_SWING_MODES = ["OFF", "VERTICAL", "HORIZONTAL", "BOTH"]


CN105Climate = cg.global_ns.class_(
    "CN105Climate", climate.Climate, cg.Component, uart.UARTDevice
)
CONF_REMOTE_TEMP_TIMEOUT = "remote_temperature_timeout"
CONF_DEBOUNCE_DELAY = "debounce_delay"

# Définitions des classes C++ (identiques à votre version)
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
uptime_ns = cg.esphome_ns.namespace("esphome").namespace("uptime")
HpUpTimeConnectionSensor = uptime_ns.class_(
    "HpUpTimeConnectionSensor", sensor.Sensor, cg.PollingComponent
)
FlowControlSensor = cg.global_ns.class_("FlowControlSensor", text_sensor.TextSensor, cg.Component)
HVACOptionSwitch = cg.global_ns.class_("HVACOptionSwitch", switch.Switch, cg.Component)


# --- Fonction d'aide pour récupérer les pins TX/RX (identique à votre version corrigée) ---
def get_uart_pins_from_config(core_config, target_uart_id_str):
    tx_pin_num = -1
    rx_pin_num = -1
    uart_config_found = {}
    for uart_conf_item in core_config.get("uart", []):
        if str(uart_conf_item[CONF_ID]) == target_uart_id_str:
            uart_config_found = uart_conf_item
            break
    if uart_config_found:
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


# --- FIN de la fonction d'aide ---

# Schémas pour les entités optionnelles (identiques à votre version)
SELECT_SCHEMA = select.select_schema(VaneOrientationSelect).extend(
    {cv.GenerateID(CONF_ID): cv.declare_id(VaneOrientationSelect)}
)
COMPRESSOR_FREQUENCY_SENSOR_SCHEMA = sensor.sensor_schema(CompressorFrequencySensor).extend(
    {cv.GenerateID(CONF_ID): cv.declare_id(CompressorFrequencySensor)}
)
INPUT_POWER_SENSOR_SCHEMA = sensor.sensor_schema(InputPowerSensor).extend(
    {cv.GenerateID(CONF_ID): cv.declare_id(InputPowerSensor)}
)
KWH_SENSOR_SCHEMA = sensor.sensor_schema(kWhSensor).extend(
    {cv.GenerateID(CONF_ID): cv.declare_id(kWhSensor)}
)
RUNTIME_HOURS_SENSOR_SCHEMA = sensor.sensor_schema(RuntimeHoursSensor).extend(
    {cv.GenerateID(CONF_ID): cv.declare_id(RuntimeHoursSensor)}
)
OUTSIDE_AIR_TEMPERATURE_SENSOR_SCHEMA = sensor.sensor_schema(OutsideAirTemperatureSensor).extend(
    {cv.GenerateID(CONF_ID): cv.declare_id(OutsideAirTemperatureSensor)}
)
ISEE_SENSOR_SCHEMA = binary_sensor.binary_sensor_schema(ISeeSensor).extend(
    {cv.GenerateID(CONF_ID): cv.declare_id(ISeeSensor)}
)
FUNCTIONS_SENSOR_SCHEMA = text_sensor.text_sensor_schema(FunctionsSensor).extend(
    {cv.GenerateID(CONF_ID): cv.declare_id(FunctionsSensor)}
)
FUNCTIONS_BUTTON_SCHEMA = button.button_schema(FunctionsButton).extend(
    {cv.GenerateID(CONF_ID): cv.declare_id(FunctionsButton)}
)
FUNCTIONS_NUMBER_SCHEMA = number.number_schema(FunctionsNumber).extend(
    {cv.GenerateID(CONF_ID): cv.declare_id(FunctionsNumber)}
)
SUB_MODE_SENSOR_SCHEMA = text_sensor.text_sensor_schema(SubModSensor).extend(
    {cv.GenerateID(CONF_ID): cv.declare_id(SubModSensor)}
)
AUTO_SUB_MODE_SENSOR_SCHEMA = text_sensor.text_sensor_schema(AutoSubModSensor).extend(
    {cv.GenerateID(CONF_ID): cv.declare_id(AutoSubModSensor)}
)

# Schéma pour STAGE_SENSOR (qui est un text_sensor) AVEC la nouvelle sous-option
STAGE_SENSOR_CONFIG_SCHEMA = text_sensor.text_sensor_schema(StageSensor).extend(
    {
        # L'ID pour l'objet StageSensor C++ est géré par text_sensor.TEXT_SENSOR_SCHEMA (via CONF_ID)
        cv.Optional(CONF_USE_AS_OPERATING_FALLBACK, default=False): cv.boolean,
    }
)

# Schéma pour HP_UP_TIME_CONNECTION_SENSOR (identique à votre version)
HP_UP_TIME_CONNECTION_SENSOR_SCHEMA = sensor.sensor_schema(
    HpUpTimeConnectionSensor,
    unit_of_measurement=UNIT_SECOND,
    icon=ICON_TIMER,
    accuracy_decimals=0,
    state_class=STATE_CLASS_TOTAL_INCREASING,
    device_class=DEVICE_CLASS_DURATION,
    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
).extend(cv.polling_component_schema("60s"))

HVAC_OPTION_SWITCH_SCHEMA = switch.switch_schema(HVACOptionSwitch).extend(
    {cv.GenerateID(CONF_ID): cv.declare_id(HVACOptionSwitch )}
)

CONFIG_SCHEMA = climate.climate_schema(CN105Climate).extend(
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
        cv.Optional(CONF_FAHRENHEIT_SUPPORT_MODE): cv.boolean,
        cv.Optional(
            CONF_STAGE_SENSOR
        ): STAGE_SENSOR_CONFIG_SCHEMA,  # Modifié pour le nouveau schéma
        cv.Optional(CONF_SUB_MODE_SENSOR): SUB_MODE_SENSOR_SCHEMA,
        cv.Optional(CONF_AUTO_SUB_MODE_SENSOR): AUTO_SUB_MODE_SENSOR_SCHEMA,
        cv.Optional(CONF_REMOTE_TEMP_TIMEOUT, default="never"): cv.All(
            cv.update_interval
        ),
        cv.Optional(CONF_DEBOUNCE_DELAY, default="100ms"): cv.All(cv.update_interval),
        cv.Optional(
            CONF_HP_UP_TIME_CONNECTION_SENSOR
        ): HP_UP_TIME_CONNECTION_SENSOR_SCHEMA,
        cv.Optional(CONF_AIRFLOW_CONTROL_SELECT): SELECT_SCHEMA,
        cv.Optional(CONF_AIR_PURIFIER_SWITCH): HVAC_OPTION_SWITCH_SCHEMA,
        cv.Optional(CONF_NIGHT_MODE_SWITCH): HVAC_OPTION_SWITCH_SCHEMA,
        cv.Optional(CONF_CIRCULATOR_SWITCH): HVAC_OPTION_SWITCH_SCHEMA,
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

    if CONF_SUPPORTS in config:
        supports = config[CONF_SUPPORTS]
        traits = var.config_traits()
        for mode_str in supports.get(CONF_MODE, DEFAULT_CLIMATE_MODES):
            if mode_str == "OFF":
                continue
            if mode_str in climate.CLIMATE_MODES:
                cg.add(traits.add_supported_mode(climate.CLIMATE_MODES[mode_str]))
        for fan_mode_str in supports.get(CONF_FAN_MODE, DEFAULT_FAN_MODES):
            if fan_mode_str in climate.CLIMATE_FAN_MODES:
                cg.add(
                    traits.add_supported_fan_mode(
                        climate.CLIMATE_FAN_MODES[fan_mode_str]
                    )
                )
        for swing_mode_str in supports.get(CONF_SWING_MODE, DEFAULT_SWING_MODES):
            if swing_mode_str in climate.CLIMATE_SWING_MODES:
                cg.add(
                    traits.add_supported_swing_mode(
                        climate.CLIMATE_SWING_MODES[swing_mode_str]
                    )
                )

    cg.add(var.set_remote_temp_timeout(config[CONF_REMOTE_TEMP_TIMEOUT]))
    cg.add(var.set_debounce_delay(config[CONF_DEBOUNCE_DELAY]))

    # --- Configuration des entités optionnelles (style original) ---
    if CONF_HORIZONTAL_SWING_SELECT in config:
        conf_item = config[CONF_HORIZONTAL_SWING_SELECT]
        # new_select s'occupe de l'enregistrement. options=[] est important.
        swing_select_var = yield select.new_select(conf_item, options=[])
        cg.add(var.set_horizontal_vane_select(swing_select_var))

    if CONF_VERTICAL_SWING_SELECT in config:
        conf_item = config[CONF_VERTICAL_SWING_SELECT]
        swing_select_var = yield select.new_select(conf_item, options=[])
        cg.add(var.set_vertical_vane_select(swing_select_var))

    if CONF_AIRFLOW_CONTROL_SELECT in config:
        conf_item = config[CONF_AIRFLOW_CONTROL_SELECT]
        control_select_var = yield select.new_select(conf_item, options=[])
        cg.add(var.set_airflow_control_select(control_select_var))

    # Pour les capteurs, text_sensors, etc., utiliser la méthode .new_... standard
    # Ces fonctions s'occupent de l'enregistrement du composant.
    if CONF_COMPRESSOR_FREQUENCY_SENSOR in config:
        # conf = config[CONF_COMPRESSOR_FREQUENCY_SENSOR] # 'conf' est déjà utilisé comme argument de to_code
        # conf["force_update"] = False # Ceci était dans votre code original, le garder si pertinent
        conf_item = config[CONF_COMPRESSOR_FREQUENCY_SENSOR]
        if (
            "force_update" not in conf_item
        ):  # S'assurer de ne pas l'écraser si l'user l'a mis
            conf_item["force_update"] = False
        sensor_var = yield sensor.new_sensor(conf_item)
        cg.add(var.set_compressor_frequency_sensor(sensor_var))

    if CONF_INPUT_POWER_SENSOR in config:
        conf_item = config[CONF_INPUT_POWER_SENSOR]
        if "force_update" not in conf_item:
            conf_item["force_update"] = False
        sensor_var = yield sensor.new_sensor(conf_item)
        cg.add(var.set_input_power_sensor(sensor_var))

    if CONF_KWH_SENSOR in config:
        conf_item = config[CONF_KWH_SENSOR]
        if "force_update" not in conf_item:
            conf_item["force_update"] = False
        sensor_var = yield sensor.new_sensor(conf_item)
        cg.add(var.set_kwh_sensor(sensor_var))

    if CONF_RUNTIME_HOURS_SENSOR in config:
        conf_item = config[CONF_RUNTIME_HOURS_SENSOR]
        if "force_update" not in conf_item:
            conf_item["force_update"] = False
        sensor_var = yield sensor.new_sensor(conf_item)
        cg.add(var.set_runtime_hours_sensor(sensor_var))

    if CONF_OUTSIDE_AIR_TEMPERATURE_SENSOR in config:
        conf_item = config[CONF_OUTSIDE_AIR_TEMPERATURE_SENSOR]
        if "force_update" not in conf_item:
            conf_item["force_update"] = False
        sensor_var = yield sensor.new_sensor(conf_item)
        cg.add(var.set_outside_air_temperature_sensor(sensor_var))

    if CONF_ISEE_SENSOR in config:
        bsensor_var = yield binary_sensor.new_binary_sensor(config[CONF_ISEE_SENSOR])
        cg.add(var.set_isee_sensor(bsensor_var))

    if CONF_FUNCTIONS_SENSOR in config:
        tsensor_var = yield text_sensor.new_text_sensor(config[CONF_FUNCTIONS_SENSOR])
        cg.add(var.set_functions_sensor(tsensor_var))

    if CONF_FUNCTIONS_BUTTON in config:
        button_var = yield button.new_button(config[CONF_FUNCTIONS_BUTTON])
        cg.add(var.set_functions_get_button(button_var))

    if CONF_FUNCTIONS_SET_BUTTON in config:
        button_var = yield button.new_button(config[CONF_FUNCTIONS_SET_BUTTON])
        cg.add(var.set_functions_set_button(button_var))

    if CONF_FUNCTIONS_SET_CODE in config:
        conf_item = config[CONF_FUNCTIONS_SET_CODE]
        number_var = yield number.new_number(
            conf_item, min_value=100.0, max_value=128.0, step=1.0
        )
        cg.add(var.set_functions_set_code(number_var))

    if CONF_FUNCTIONS_SET_VALUE in config:
        conf_item = config[CONF_FUNCTIONS_SET_VALUE]
        number_var = yield number.new_number(
            conf_item, min_value=1.0, max_value=3.0, step=1.0
        )
        cg.add(var.set_functions_set_value(number_var))

    if CONF_AIR_PURIFIER_SWITCH in config:
        switch_var = yield switch.new_switch(config[CONF_AIR_PURIFIER_SWITCH])
        cg.add(var.set_air_purifier_switch(switch_var))

    if CONF_NIGHT_MODE_SWITCH in config:
        switch_var = yield switch.new_switch(config[CONF_NIGHT_MODE_SWITCH])
        cg.add(var.set_night_mode_switch(switch_var))

    if CONF_CIRCULATOR_SWITCH in config:
        switch_var = yield switch.new_switch(config[CONF_CIRCULATOR_SWITCH])
        cg.add(var.set_circulator_switch(switch_var))

    if CONF_FAHRENHEIT_SUPPORT_MODE in config:
        cg.add(var.set_use_fahrenheit_support_mode(config.get(CONF_FAHRENHEIT_SUPPORT_MODE)))

    # --- TRAITEMENT POUR STAGE_SENSOR AVEC LA NOUVELLE OPTION ---
    if CONF_STAGE_SENSOR in config:
        conf_stage_dict = config[CONF_STAGE_SENSOR]
        # new_text_sensor gère la création et l'enregistrement de base du text_sensor
        stage_ts_var = yield text_sensor.new_text_sensor(conf_stage_dict)
        cg.add(var.set_stage_sensor(stage_ts_var))

        # Passer l'option de fallback au C++
        if conf_stage_dict.get(CONF_USE_AS_OPERATING_FALLBACK, False):
            cg.add(var.set_use_stage_for_operating_status(True))
    # --- FIN DU TRAITEMENT POUR STAGE_SENSOR ---

    if CONF_SUB_MODE_SENSOR in config:
        tsensor_var = yield text_sensor.new_text_sensor(config[CONF_SUB_MODE_SENSOR])
        cg.add(var.set_sub_mode_sensor(tsensor_var))

    if CONF_AUTO_SUB_MODE_SENSOR in config:
        tsensor_var = yield text_sensor.new_text_sensor(
            config[CONF_AUTO_SUB_MODE_SENSOR]
        )
        cg.add(var.set_auto_sub_mode_sensor(tsensor_var))

    if CONF_HP_UP_TIME_CONNECTION_SENSOR in config:
        conf = config[CONF_HP_UP_TIME_CONNECTION_SENSOR]
        hp_connection_sensor_ = yield sensor.new_sensor(conf)
        yield cg.register_component(hp_connection_sensor_, conf)
        cg.add(var.set_hp_uptime_connection_sensor(hp_connection_sensor_))

    yield cg.register_component(var, config)
    yield climate.register_climate(var, config)
