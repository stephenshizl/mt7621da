#
# Copyright (C) 2006-2016 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#
include $(TOPDIR)/rules.mk

PKG_NAME:=findutils
PKG_VERSION:=4.6.0

HOST_BUILD_DIR:=$(BUILD_DIR_HOST)/findutils-$(PKG_VERSION)

include $(INCLUDE_DIR)/host-build.mk

define Host/Prepare
	mkdir -p $(HOST_BUILD_DIR)
endef

define Host/Configure
endef

define Host/Compile
endef

define Host/Install
	$(INSTALL_BIN) /usr/bin/find $(STAGING_DIR_HOST)/bin/find
	$(INSTALL_BIN) /usr/bin/xargs $(STAGING_DIR_HOST)/bin/xargs
endef

define Host/Clean
	rm -f $(STAGING_DIR_HOST)/bin/find $(STAGING_DIR_HOST)/bin/xargs
	$(call Host/Clean/Default)
endef

$(eval $(call HostBuild))
