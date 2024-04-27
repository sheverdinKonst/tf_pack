
#
# Copyright (C) 2024 TFortis
#

include $(TOPDIR)/rules.mk

PKG_NAME:=tf_clish
PKG_VERSION:=1.0
PKG_RELEASE:=1
PKG_MAINTAINER:=Sheverdin Konstantin <sheverdin@fort-telecom.ru>

include $(INCLUDE_DIR)/package.mk

define Package/tf_clish
	SECTION:=tfortis
	DEFAULT:=y
	CATEGORY:=TFortis
	TITLE:=config files for tf_clish
	URL:=https://www.tfortis.com
endef

define Package/tf_clish/description
	config files for tf_clish
endef

define Build/Compile
endef

define Build/Configure
endef

define Build/Prepare
	echo  "=======================  (PKG_BUILD_DIR)"__ $(PKG_BUILD_DIR)
	$(CP) ./* $(PKG_BUILD_DIR)/
endef

define Package/tf_clish/install
	echo  "=========install=========  $(1)"__ $(1)
	$(CP) ./* $(1)
endef

$(eval $(call BuildPackage,tf_clish))

