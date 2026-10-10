/*
 * @file temp_ioctl.h
 * @author Bhumika Sood 
 * @brief Definitions for a BME280 temperature sensor driver.
 */

#ifndef TEMP_IOCTL_H
#define TEMP_IOCTL_H

#ifdef __KERNEL__
#include <linux/ioctl.h>
#else
#include <sys/ioctl.h>
#endif


#define TEMP_DEVICE_NAME   "temp_sensor" // Device name as it appears in /dev/temp_sensor
#define TEMP_IOC_MAGIC     0x18 // Temperature sensor IOCTL magic number

// Get temperature in Celcius
#define TEMP_IOC_GET_TEMP _IOR(TEMP_IOC_MAGIC, 1, int)

#endif /* TEMP_IOCTL_H */