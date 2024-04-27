#!/usr/bin/lua

---
--- Generated by Luanalysis
--- Created by sheverdin.
--- DateTime: 2/14/24 5:35 PM
---

local tf = require "tf_module"

function showChanges()
    local section_list = tf.changes(tf.ubusList)
    if section_list == nil then
        io.write("error: >> get changes\n")
    elseif  #section_list == 0 then
        io.write("\tno changes\n")
    end
end

function revertSection()
    local section_list = tf.changes(tf.ubusList)
    main_revert(section_list, tf.ubusList)
end

function saveChanges()
    tf.executeCommand("uci commit")
    tf.executeCommand("reload_config")
    io.write("new configuration saved\n")
end

local change_cmdList = {
    { cmd = "showchanges",      func = showChanges},
    { cmd = "revertsection",    func = revertSection},
    { cmd = "savechanges",      func = saveChanges},
}

function main_revert(config_type, cmd_list)
    for i, section in pairs(config_type) do
        local revert_cmd = cmd_list.cmd_ubus .. cmd_list.cmd_revert .. cmd_list.cmd_prefix .. cmd_list.cmd_config .. section .. cmd_list.cmd_suffix
        tf.executeCommand(revert_cmd)
        io.write("configuration \t*", section, "*US\t was reverted\n")
    end
end

function run_module(cmdList)
    for index, value in pairs(cmdList) do
        if arg[1] == value.cmd then
            value.func(index)
        end
    end
end

function main_changes()
    local errorCode = tf.validateParam(1, arg)
    if errorCode == 0 then
        run_module(change_cmdList)
    end
end

main_changes()
