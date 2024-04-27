#
# This software is licensed under the Public Domain.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=dm_client
PKG_VERSION:=1.5
PKG_RELEASE:=40

PKG_MAINTAINER:=Sheverdin Konstantin <sheverdin@fort-telecom.ru>
PKG_LICENSE:=CC0-1.0

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk

define Package/dm_client
	SECTION:=tfortis
	DEFAULT:=y
	CATEGORY:=TFortis
	TITLE:=TFortis Device Monitor client
	URL:=https://www.tfortis.com
endef

define Package/dm_client/description
	Device Manager application for collect and send data about the device..
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Package/dm_client/install
	$(CP) ./files/* $(1)/
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/dm_client  $(1)/usr/bin/
endef

$(eval $(call BuildPackage,dm_client))
