import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_TOPIC_PREFIX
from esphome.core import coroutine_with_priority

# Espace de nom C++ de notre composant
cn105_ns = cg.esphome_ns.namespace("cn105")

# Déclarations des classes C++ que nous allons manipuler
CN105MqttMim = cn105_ns.class_("CN105MqttMim", cg.Component)
CN105Climate = cn105_ns.class_("CN105Climate") # Nécessaire pour la liaison

# Schéma de configuration pour le bloc `cn105_mqtt_mim` dans le YAML
MIM_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(CN105MqttMim),
        cv.Optional(CONF_TOPIC_PREFIX, default="hvac/mim"): cv.string,
    }
).extend(cv.COMPONENT_SCHEMA)

# La fonction qui sera appelée par ESPHome pour générer le code C++ de ce composant
@coroutine_with_priority(40.0)
async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    if CONF_TOPIC_PREFIX in config:
        cg.add(var.set_topic_prefix(config[CONF_TOPIC_PREFIX]))