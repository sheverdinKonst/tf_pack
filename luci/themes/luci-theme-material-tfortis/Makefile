#
# Copyright (C) 2008-2014 The LuCI Team <luci@lists.subsignal.org>
#
# This is free software, licensed under the Apache License, Version 2.0 .
#

include $(TOPDIR)/rules.mk

LUCI_TITLE:=Material Theme TFortis
LUCI_DEPENDS:=

define Package/luci-theme-material-tfortis/postrm
#!/bin/sh
[ -n "$${IPKG_INSTROOT}" ] || {
	uci -q delete luci.themes.Material
	uci commit luci
}
endef

#include $(TOPDIR)/feeds/luci/luci.mk
include ../../luci.mk

# call BuildPackage - OpenWrt buildroot signature
