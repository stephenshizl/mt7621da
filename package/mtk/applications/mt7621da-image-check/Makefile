include $(TOPDIR)/rules.mk

PKG_NAME:=mt7621da-image-check
PKG_RELEASE:=1
PKG_LICENSE:=GPL-2.0-only

include $(INCLUDE_DIR)/package.mk

define Package/mt7621da-image-check
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=MT7621DA 8MiB FIT and partition check
  DEPENDS:=+libfdt +libopenssl
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
	$(TARGET_CC) $(TARGET_CFLAGS) $(TARGET_CPPFLAGS) -Wall -Wextra -o $(PKG_BUILD_DIR)/mt7621da-image-check $(PKG_BUILD_DIR)/mt7621da-image-check.c $(TARGET_LDFLAGS) -lfdt -lcrypto
endef

define Package/mt7621da-image-check/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/mt7621da-image-check $(1)/usr/sbin/
endef

$(eval $(call BuildPackage,mt7621da-image-check))
