import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome import pins
from esphome.const import (
    CONF_ID,
    CONF_UPDATE_INTERVAL,
    CONF_FLOW_CONTROL_PIN,
)

CODEOWNERS = ["@rwagoner"]
DEPENDENCIES = ["uart"]
AUTO_LOAD = []
MULTI_CONF = False

CONF_WATERFURNACE_ID = "waterfurnace_id"

waterfurnace_ns = cg.esphome_ns.namespace("waterfurnace")
WaterFurnace = waterfurnace_ns.class_(
    "WaterFurnace", cg.PollingComponent, uart.UARTDevice
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(WaterFurnace),
            cv.Optional(CONF_FLOW_CONTROL_PIN): pins.gpio_output_pin_schema,
        }
    )
    .extend(cv.polling_component_schema("10s"))
    .extend(uart.UART_DEVICE_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    if CONF_FLOW_CONTROL_PIN in config:
        pin = await cg.gpio_pin_expression(config[CONF_FLOW_CONTROL_PIN])
        cg.add(var.set_flow_control_pin(pin))
