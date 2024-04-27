module("luci.controller.special.ups",package.seeall)

function index()
  entry({"admin","special"},firstchild(),translate("Special"),60).dependent=false
  entry({"admin", "special", "ups"},template("special/view_ups"),translate("UPS"), 10).acl_depends={ "luci-app-tfortis-ups" }
end