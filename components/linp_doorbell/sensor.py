import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import CONF_ID, UNIT_EMPTY

linp_doorbell_ns = cg.esphome_ns.namespace("linp_doorbell")

LinpDoorbellComponent = linp_doorbell_ns.class_(
    "LinpDoorbellComponent", cg.Component
)

CONF_VOLUME = "volume"
CONF_CHIME_PLAYING = "chime_playing"
CONF_USE_OLD_SERVICE_NAMES = "use_old_service_names"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(LinpDoorbellComponent),
            cv.Optional(CONF_VOLUME): sensor.sensor_schema(
                unit_of_measurement=UNIT_EMPTY,
                icon="mdi:volume-high",
                accuracy_decimals=0,
            ),
            cv.Optional(CONF_CHIME_PLAYING): sensor.sensor_schema(
                unit_of_measurement=UNIT_EMPTY,
                icon="mdi:music",
                accuracy_decimals=0,
            ),
            cv.Optional(CONF_USE_OLD_SERVICE_NAMES, default=False): cv.boolean,
        }
    )
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    if CONF_VOLUME in config:
        conf = config[CONF_VOLUME]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_volume_sensor(sens))

    if CONF_CHIME_PLAYING in config:
        conf = config[CONF_CHIME_PLAYING]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_chime_playing_sensor(sens))
    
    cg.add(var.set_use_old_service_names(config[CONF_USE_OLD_SERVICE_NAMES]))

    cg.add_library("ArduinoQueue", "1.2.3")

