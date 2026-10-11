/*
 * @file bme280_ioctl.h
 * @author Bhumika Sood 
 * @brief Definitions for a BME280 temperature and humidity sensor driver.
 */

#ifndef BME280_IOCTL_H
#define BME280_IOCTL_H

#ifdef __KERNEL__
#include <linux/ioctl.h>
#else
#include <sys/ioctl.h>
#endif

#define BME280_DEVICE_NAME   "bme280" // Device name as it appears in /dev/bme280
#define BME280_IOC_MAGIC     0x18 // BME280 sensor IOCTL magic number

// Get temperature in Celcius
#define BME280_IOC_GET_TEMP _IOR(BME280_IOC_MAGIC, 1, int)

// Get relative humidity
#define BME280_IOC_GET_HUMIDITY _IOR(BME280_IOC_MAGIC, 2, int)

#endif /* BME280_IOCTL_H */