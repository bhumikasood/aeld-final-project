################################################################################
#
# climate-control
#
################################################################################

CLIMATE_CONTROL_VERSION = 1.0
CLIMATE_CONTROL_SITE = $(CLIMATE_CONTROL_PKGDIR)/src
CLIMATE_CONTROL_SITE_METHOD = local
CLIMATE_CONTROL_LICENSE = GPL-2.0
CLIMATE_CONTROL_MODULE_SUBDIRS = rotary_switch push_button bme280_sensor

# Build
define CLIMATE_CONTROL_BUILD_CMDS
	$(TARGET_CC) $(TARGET_CFLAGS) $(TARGET_LDFLAGS) \
		-o $(@D)/rotary_switch/rotary_test \
		$(@D)/rotary_switch/rotary_test.c

	$(TARGET_CC) $(TARGET_CFLAGS) $(TARGET_LDFLAGS) \
		-o $(@D)/push_button/button_test \
		$(@D)/push_button/button_test.c

	$(TARGET_CC) $(TARGET_CFLAGS) $(TARGET_LDFLAGS) \
		-o $(@D)/bme280_sensor/temp_sensor_test \
		$(@D)/bme280_sensor/temp_sensor_test.c
endef

# Install
define CLIMATE_CONTROL_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/rotary_switch/rotary_test $(TARGET_DIR)/usr/bin/rotary_test
	$(INSTALL) -D -m 0755 $(@D)/rotary_switch/rotary_load $(TARGET_DIR)/usr/bin/rotary_load
	$(INSTALL) -D -m 0755 $(@D)/rotary_switch/rotary_unload $(TARGET_DIR)/usr/bin/rotary_unload

	$(INSTALL) -D -m 0755 $(@D)/push_button/button_test $(TARGET_DIR)/usr/bin/button_test
	$(INSTALL) -D -m 0755 $(@D)/push_button/button_load $(TARGET_DIR)/usr/bin/button_load
	$(INSTALL) -D -m 0755 $(@D)/push_button/button_unload $(TARGET_DIR)/usr/bin/button_unload

	$(INSTALL) -D -m 0755 $(@D)/bme280_sensor/temp_sensor_test $(TARGET_DIR)/usr/bin/temp_sensor_test
	$(INSTALL) -D -m 0755 $(@D)/bme280_sensor/temp_sensor_load $(TARGET_DIR)/usr/bin/temp_sensor_load
	$(INSTALL) -D -m 0755 $(@D)/bme280_sensor/temp_sensor_unload $(TARGET_DIR)/usr/bin/temp_sensor_unload
endef

$(eval $(kernel-module))
$(eval $(generic-package))