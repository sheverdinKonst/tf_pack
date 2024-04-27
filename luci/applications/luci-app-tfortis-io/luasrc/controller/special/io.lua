module("luci.controller.special.io",package.seeall)


function index()
  entry({"admin","special"},firstchild(),translate("Special"),60).dependent=false
  entry({"admin", "special", "io"},cbi("special/cbi_io"),translate("Inputs/Outputs"),20)
end