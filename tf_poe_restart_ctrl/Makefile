#
# This software is licensed under the Public Domain.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=tf_poe_restart_ctrl
PKG_VERSION:=2.0
PKG_RELEASE:=4

PKG_MAINTAINER:=Sheverdin Konstantin <sheverdin@fort-telecom.ru>
PKG_LICENSE:=CC0-1.0

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk

define Package/tf_poe_restart_ctrl
	SECTION:=tfortis
	DEFAULT:=y
	CATEGORY:=TFortis
	TITLE:=tf_autorestart demon Control
	URL:=https://www.tfortis.com
endef

define Package/tf_poe_restart_ctrl/description
	Daemon for control PoE autorestart
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Package/tf_poe_restart_ctrl/install
	$(CP) ./files/* $(1)/
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/tf_poe_restart_ctrl  $(1)/usr/bin/
endef

$(eval $(call BuildPackage,tf_poe_restart_ctrl))
