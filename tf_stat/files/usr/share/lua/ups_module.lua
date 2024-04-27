---
--- Generated by EmmyLua(https://github.com/EmmyLua)
--- Created by sheverdin.
--- DateTime: 3/15/24 3:11 PM
---

local tf = require "tf_module"

local ups_module = {}

ups_module.upsEnum = {
    {"upsModeAvalable",     "RPS_CONNECTED"     },
    {"upsPwrSource",        "RPS_VAC"           },
    {"upsBatteryVoltage",   "RPS_BAT_VOLTAGE"   },
    {"upsBatteryTime",      "RPS_REMAIN_TIME"   },
    --{ "ups_test_mode",      "RPS_TEST_MODE"     },
    --{ "ups_sw_version",     "RPS_SW_VERS"       },
    --{ "ups_but_current",    "RPS_BAT_CURRENT"   },
    --{ "ups_board_temper",   "RPS_TEMPER"        },
    --{ "ups_led_state",      "RPS_LED_STATE"     },
    --{ "ups_bat_key",        "RPS_BAT_KEY"       },
    --{ "ups_chrg_key",       "RPS_CHRG_KEY"      },
    --{ "ups_relay_state",    "RPS_REL_STATE"     },
    --{ "ups_ltC4151_ok",     "RPS_LTC4151_OK"    },
    --{ "ups_adc_bat_curr",   "RPS_ADC_BAT_CURR"  },
    --{ "ups_hw_version",     "RPS_HW_VERS"       },
    --{ "ups_chrg_voltage",   "RPS_CHRG_VOLTAGE"  },
    --{ "ups_min_voltage",    "RPS_MIN_VOLTAGE"   },
    --{ "ups_disch_votage",   "RPS_DISCH_VOLTAGE" },
    --{ "ups_test_ok",        "RPS_TEST_OK"       },
    --{ "ups_cpu_id",         "RPS_CPU_ID"        },
    --{ "ups_adc_bat_volt",   "RPS_ADC_BAT_VOLT"  },
}

local jsonInfo  = {}

local function getUbusData()
    jsonInfo = tf.collectJsonTable("ubus call ups_control getStatus")
end

function getUpsModeAvailable(index)
    getUbusData()
    local upsData = ""
    local key = ups_module.upsEnum[index][1]
    if jsonInfo.ups_control[key] == "0" then
        upsData = "1"
    elseif jsonInfo.ups_control[key] == "1" then
        upsData = "2"
    end
    return upsData
end

function getUpsPwrSource(index)
    getUbusData()
    local upsData = ""
    local key = ups_module.upsEnum[index][1]
    if jsonInfo.ups_control[key] == "0" then
        upsData = "1"
    elseif jsonInfo.ups_control[key] == "1" then
        upsData = "2"
    end
    return upsData
end

function getUpsBatteryVoltage(index)
    getUbusData()
    local upsData = ""
    local key = ups_module.upsEnum[index][1]
    upsData = jsonInfo.ups_control[key]
    return upsData
end
function getUps_test_mode(index)
    getUbusData()
    local upsData = ""
    local key = ups_module.upsEnum[index][1]
    upsData = jsonInfo.ups_control[key]
    return upsData
end

local upsFunc = {
    getUpsModeAvailable,
    getUpsPwrSource,
    getUpsBatteryVoltage,
    getUps_test_mode,
}

function ups_module.getUpsData(index)
    getUbusData()
    local upsData = ""
    index = tonumber(index)
    local key = ups_module.upsEnum[index][1]
    upsData = upsFunc[index](index)
    return upsData
end

return ups_module
