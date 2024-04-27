#
# This software is licensed under the Public Domain.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=hwsys
PKG_VERSION:=0.1
PKG_RELEASE:=1

PKG_MAINTAINER:=Belyaev Alex <abelyaev@fort-telecom.ru>
PKG_LICENSE:=CC0-1.0

include $(INCLUDE_DIR)/package.mk

define Package/hwsys
	SECTION:=tfortis
	# Select package by default
	DEFAULT:=y
	CATEGORY:=TFortis
	DEPENDS:=+libgpiod
	TITLE:=App for communicate with periphery (Sensors, UPS, SFP), includes Daemon and Controller
	URL:=https://www.tfortis.com
endef

define Package/hwsys/description
	App for communicate with periphery (Sensors, UPS, SFP), includes Daemon and Controller
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Package/hwsys/install
	$(CP) ./files/* $(1)/
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/hwsysd $(1)/usr/bin/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/hwsys_ctrl $(1)/usr/bin/
endef

$(eval $(call BuildPackage,hwsys))
