#
# This software is licensed under the Public Domain.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=tf_autorestart
PKG_VERSION:=3.0
PKG_RELEASE:=8

PKG_MAINTAINER:=Sheverdin Konstantin <sheverdin@fort-telecom.ru>
PKG_LICENSE:=CC0-1.0

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk

define Package/tf_autorestart
	SECTION:=tfortis
	DEFAULT:=y
	CATEGORY:=TFortis
	TITLE:=Camera Autorestart Deamon
	URL:=https://www.tfortis.com
endef

define Package/tf_autorestart/description
	Daemon for autorestart Cameras
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Package/tf_autorestart/install
	$(CP) ./files/* $(1)/
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/tf_autorestart  $(1)/usr/bin/
endef

$(eval $(call BuildPackage,tf_autorestart))
