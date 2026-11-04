# 
# Copyright (C) 2008-2013 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#
include $(TOPDIR)/rules.mk

PKG_NAME:=m4
PKG_VERSION:=1.4.17

HOST_BUILD_DIR:=$(BUILD_DIR_HOST)/m4-$(PKG_VERSION)

include $(INCLUDE_DIR)/host-build.mk

define Host/Prepare
	mkdir -p $(HOST_BUILD_DIR)
endef

define Host/Configure
endef

define Host/Compile
endef

define Host/Install
	$(INSTALL_BIN) /usr/bin/m4 $(STAGING_DIR_HOST)/bin/m4
endef

define Host/Clean
	rm -f $(STAGING_DIR_HOST)/bin/m4
	$(call Host/Clean/Default)
endef

$(eval $(call HostBuild))
