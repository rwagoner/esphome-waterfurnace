import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import CONF_ID
from .. import waterfurnace_ns, WaterFurnace, CONF_WATERFURNACE_ID

DEPENDENCIES = ["waterfurnace"]

WaterFurnaceTextSensor = waterfurnace_ns.class_(
    "WaterFurnaceTextSensor", text_sensor.TextSensor, cg.Component
)

CONF_CURRENT_FAULT = "current_fault"
CONF_MODEL_NUMBER = "model_number"
CONF_SERIAL_NUMBER = "serial_number"
CONF_SYSTEM_MODE = "system_mode"

TEXT_SENSOR_TYPES = {
    CONF_CURRENT_FAULT: "fault",
    CONF_MODEL_NUMBER: "model",
    CONF_SERIAL_NUMBER: "serial",
    CONF_SYSTEM_MODE: "mode",
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_WATERFURNACE_ID): cv.use_id(WaterFurnace),
        **{
            cv.Optional(key): text_sensor.text_sensor_schema().extend(
                {cv.GenerateID(): cv.declare_id(WaterFurnaceTextSensor)}
            )
            for key in TEXT_SENSOR_TYPES
        },
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_WATERFURNACE_ID])

    for key, sensor_type in TEXT_SENSOR_TYPES.items():
        if key not in config:
            continue
        conf = config[key]
        var = cg.new_Pvariable(conf[CONF_ID])
        await cg.register_component(var, conf)
        await text_sensor.register_text_sensor(var, conf)
        cg.add(var.set_parent(parent))
        cg.add(var.set_sensor_type(sensor_type))
