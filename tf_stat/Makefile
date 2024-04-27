
#
# Copyright (C) 2024 TFortis
#

include $(TOPDIR)/rules.mk

PKG_NAME:=tf_stat
PKG_VERSION:=0.0
PKG_RELEASE:=6
PKG_MAINTAINER:=Sheverdin Konstantin <sheverdin@fort-telecom.ru>

include $(INCLUDE_DIR)/package.mk

define Package/tf_stat
	SECTION:=tfortis
	DEFAULT:=y
	CATEGORY:=TFortis
	TITLE:= collect statistics from perepherial
	URL:=https://www.tfortis.com
endef

define Package/tf_stat/description
	collect statistics from perepherial
endef

define Build/Compile
endef

define Build/Configure
endef

define Build/Prepare
	echo  "=======================  (PKG_BUILD_DIR)"__ $(PKG_BUILD_DIR)
	$(CP) ./files/* $(PKG_BUILD_DIR)/
endef

define Package/tf_stat/install
	echo  "=========install=========  $(1)"__ $(1)
	$(CP) ./files/* $(1)/
endef

$(eval $(call BuildPackage,tf_stat))

