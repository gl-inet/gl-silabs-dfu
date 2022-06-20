 ###################################################################
 # Copyright 2020 GL-iNet. https://www.gl-inet.com/
 # 
 # Licensed under the Apache License, Version 2.0 (the "License");
 # you may not use this file except in compliance with the License.
 # You may obtain a copy of the License at
 #
 # http://www.apache.org/licenses/LICENSE-2.0
 # 
 # Unless required by applicable law or agreed to in writing, software
 # distributed under the License is distributed on an "AS IS" BASIS,
 # WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 # See the License for the specific language governing permissions and
 # limitations under the License.
 ####################################################################
include $(TOPDIR)/rules.mk

PKG_NAME:=gl-silabs-dfu
PKG_VERSION:=1.2.3


include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk

define Package/gl-silabs-dfu
	SECTION:=base
	CATEGORY:=gl-inet-iot
	TITLE:=GL inet dfu silabs firmware
	DEPENDS:= 
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Build/InstallDev

endef

define Package/gl-silabs-dfu/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/bin/gl-silabs-dfu $(1)/usr/bin/
endef

$(eval $(call BuildPackage,gl-silabs-dfu))
