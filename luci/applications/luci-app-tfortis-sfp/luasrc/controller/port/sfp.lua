module("luci.controller.port.sfp",package.seeall)

function index()
  entry({"admin", "port", "sfp"},template("port/view_sfp"),translate("SFP"), 10)
end