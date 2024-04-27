module("luci.controller.network.poe",package.seeall)

function index()
  entry({"admin", "network", "poe"},cbi("network/cbi_poe"),translate("PoE"),20)
end