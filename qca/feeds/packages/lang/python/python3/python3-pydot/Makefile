include $(TOPDIR)/rules.mk

PKG_NAME:=python3-pydot
PKG_VERSION:=1.4.2
PKG_RELEASE:=1

PYPI_SOURCE_NAME:=pydot

PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).tar.gz
PKG_SOURCE_URL:=https://github.com/pydot/pydot
PKG_SOURCE_PROTO:=git
PKG_SOURCE_VERSION:=cb7c3845bb899acbe6547b3b1871580463cd7b3b
PKG_HASH := 90bf973371d70879041f520b3b58fbe51402eaba0c904ce0b3ac104b88ffdfce
PKG_SOURCE_SUBMODULES:=skip
HOST_BUILD_DEPENDS:=python3/host python-build/host python-installer/host python-wheel/host

PKG_LICENSE:=MIT

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/host-build.mk
include $(TOPDIR)/feeds/packages/lang/python/python3-package.mk
include $(TOPDIR)/feeds/packages/lang/python/python3-host-build.mk

define Package/python3-pydot
    SECTION:=language-python
    CATEGORY:=Languages
    SUBMENU:=Python
    TITLE:=python3-pydot
    URL:=https://github.com/pydot/pydot
endef

$(eval $(call HostBuild))
$(eval $(call Py3Package,python3-pydot))
$(eval $(call BuildPackage,python3-pydot))
