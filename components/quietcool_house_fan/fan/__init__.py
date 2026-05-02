import esphome.codegen as cg
import esphome.config_validation as cv

from esphome.const import (
    CONF_OUTPUT_ID
)

from esphome.components import fan

from .. import quietcool_house_fan_ns

CONF_CS_PIN = "cs_pin"
CONF_CLK_PIN = "clk_pin"
CONF_MISO_PIN = "miso_pin"
CONF_MOSI_PIN = "mosi_pin"
CONF_GDO0_PIN = "gdo0_pin"
CONF_GDO2_PIN = "gdo2_pin"
CONF_REMOTE_ID = "remote_id"
CONF_FREQ_MHZ = "center_freq_mhz"
CONF_DEVIATION_KHZ = "deviation_khz"

QuietcoolHouseFan = quietcool_house_fan_ns.class_("QuietcoolHouseFan", cg.Component, fan.Fan)

CONFIG_SCHEMA = fan.fan_schema(QuietcoolHouseFan).extend(
    {
        cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(QuietcoolHouseFan),
        cv.Required(CONF_CS_PIN                       ): cv.uint8_t,
        cv.Required(CONF_CLK_PIN                      ): cv.uint8_t,
        cv.Required(CONF_MOSI_PIN                     ): cv.uint8_t,
        cv.Required(CONF_MISO_PIN                     ): cv.uint8_t,
        cv.Optional(CONF_GDO0_PIN,      default=0     ): cv.uint8_t,
        cv.Optional(CONF_GDO2_PIN,      default=0     ): cv.uint8_t,
        cv.Required(CONF_REMOTE_ID                    ): cv.ensure_list(cv.hex_uint8_t),
        cv.Optional(CONF_FREQ_MHZ     , default=433.92): cv.float_,
        cv.Optional(CONF_DEVIATION_KHZ, default=10.0  ): cv.float_
    }
).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])

    await fan.register_fan(var, config)
    await cg.register_component(var, config)


    cg.add(var.set_pins(
        config[CONF_CS_PIN],
        config[CONF_CLK_PIN],
        config[CONF_MOSI_PIN],
        config[CONF_MISO_PIN],
        config[CONF_GDO0_PIN],
        config[CONF_GDO2_PIN]))

    cg.add(var.set_remote_id(config[CONF_REMOTE_ID]))

    cg.add(var.set_frequencies(
        config[CONF_FREQ_MHZ],
        config[CONF_DEVIATION_KHZ]))
