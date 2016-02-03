################################################################################
#
# ps2bt
#
################################################################################

PS2BT_VERSION = 2.0
PS2BT_SITE = $(BR2_DL_DIR)/ps2bt-src
PS2BT_SITE_METHOD = local
PS2BT_LICENSE = GPLv2+

define PS2BT_BUILD_CMDS
	$(MAKE) CC="$(TARGET_CC)" -C $(@D)
endef

define PS2BT_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/bin/ps2bt $(TARGET_DIR)/ps2bt/ps2bt
endef

$(eval $(generic-package))
