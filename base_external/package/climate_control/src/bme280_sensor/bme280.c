/**
 * @file bme280.c
 * @author Bhumika Sood
 * @brief Functions and data related to the BME280 sensor driver implementation
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/cdev.h>
#include <linux/fs.h> 
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/pinctrl/pinconf-generic.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/bitops.h>
#include <linux/i2c.h>
#include <linux/err.h> 

#include "bme280.h"
#include "bme280_ioctl.h"

// I2C connection information
#define BME280_I2C_BUS      1   
#define BME280_I2C_ADDR     0x77

// Register values
#define BME280_TEMP_SKIPPED       0x80000
#define BME280_HUMID_SKIPPED      0x8000    
#define BME280_RESET_VALUE        0xB6
#define BME280_TEMP_CTRL_MEAS     0x23
#define BME280_HUMID_CTRL_MEAS    0x01

// Register mapping defined in the BME280 datasheet
enum 
{
    BME280_REGISTER_DIG_T1 = 0x88,
    BME280_REGISTER_DIG_T2 = 0x8A,
    BME280_REGISTER_DIG_T3 = 0x8C,

    BME280_REGISTER_DIG_P1 = 0x8E,
    BME280_REGISTER_DIG_P2 = 0x90,
    BME280_REGISTER_DIG_P3 = 0x92,
    BME280_REGISTER_DIG_P4 = 0x94,
    BME280_REGISTER_DIG_P5 = 0x96,
    BME280_REGISTER_DIG_P6 = 0x98,
    BME280_REGISTER_DIG_P7 = 0x9A,
    BME280_REGISTER_DIG_P8 = 0x9C,
    BME280_REGISTER_DIG_P9 = 0x9E,

    BME280_REGISTER_DIG_H1 = 0xA1,
    BME280_REGISTER_DIG_H2 = 0xE1,
    BME280_REGISTER_DIG_H3 = 0xE3,
    BME280_REGISTER_DIG_H4 = 0xE4,
    BME280_REGISTER_DIG_H5 = 0xE5,
    BME280_REGISTER_DIG_H6 = 0xE7,

    BME280_REGISTER_CHIPID = 0xD0,
    BME280_REGISTER_VERSION = 0xD1,
    BME280_REGISTER_SOFTRESET = 0xE0,

    BME280_REGISTER_CAL26 = 0xE1, 
    BME280_REGISTER_CONTROLHUMID = 0xF2,
    BME280_REGISTER_STATUS = 0XF3,
    BME280_REGISTER_CONTROL = 0xF4,
    BME280_REGISTER_CONFIG = 0xF5,
    BME280_REGISTER_PRESSUREDATA = 0xF7,
    BME280_REGISTER_TEMPDATA = 0xFA,
    BME280_REGISTER_HUMIDDATA = 0xFD
};

// Function prototypes
static int bme280_open(struct inode *inode, struct file *filp);
static int bme280_release(struct inode *inode, struct file *filp);
static long bme280_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
static int bme280_setup_cdev(struct bme280_dev *dev);
static int bme280_hw_init(struct bme280_dev *dev);
static int bme280_read_measurements(struct bme280_dev *dev, int *temp, int *humidity);
static int bme280_compensate_temp(struct bme280_dev *dev, int raw_temp);
static int bme280_compensate_humidity(struct bme280_dev *dev, int t_fine, int raw_humidity);
static int bme280_init_module(void);
static void bme280_cleanup_module(void);

static int bme280_major = 0;
static int bme280_minor = 0;

MODULE_AUTHOR("Bhumika Sood");
MODULE_LICENSE("Dual BSD/GPL");

static struct bme280_dev bme280_device;

static int bme280_open(struct inode *inode, struct file *filp)
{
	PDEBUG("Open BME280 Sensor Device");

	// Locate device structure
	struct bme280_dev *dev;
    dev = container_of(inode->i_cdev, struct bme280_dev, cdev);
    // Point to device data
    filp->private_data = dev;

    return 0;
}

static int bme280_release(struct inode *inode, struct file *filp)
{
    PDEBUG("Release BME280 Sensor Device");
    return 0;
}

// Temperature compensation equations from BME280 datasheet
static int bme280_compensate_temp(struct bme280_dev *dev, int raw_temp)
{
    int var_1 = (((raw_temp >> 3) - ((int)dev->calibration.dig_T1 << 1)) 
                * dev->calibration.dig_T2) >> 11;
    int var_2 = ((((raw_temp >> 4) - dev->calibration.dig_T1)
                * ((raw_temp >> 4) - dev->calibration.dig_T1) >> 12)
                * dev->calibration.dig_T3) >> 14;
    int t_fine = var_1 + var_2;

    return t_fine; 
}

// Humidity compensation equations from BME280 datasheet
static int bme280_compensate_humidity(struct bme280_dev *dev, int t_fine, int raw_humidity)
{
    int humidity = t_fine - 76800;
    humidity = (((((raw_humidity << 14) - ((int)dev->calibration.dig_H4 << 20)
               - ((int)dev->calibration.dig_H5 * humidity)) + 16384) >> 15)
               * (((((((humidity * (int)dev->calibration.dig_H6) >> 10)
               * (((humidity * (int)dev->calibration.dig_H3) >> 11) + 32768)) >> 10)
               + 2097152) * (int)dev->calibration.dig_H2 + 8192) >> 14));
    humidity = humidity - (((((humidity >> 15) * (humidity >> 15)) >> 7)
               * (int)dev->calibration.dig_H1) >> 4);
               
    if (humidity < 0)
    {
        humidity = 0;
    }
    if (humidity > 419430400)
    {
        humidity = 419430400;
    }

    return (((humidity >> 12) * 100) >> 10);
}

// Get measurement readings from sensor
static int bme280_read_measurements(struct bme280_dev *dev, int *temp, int *humidity)
{
    // Get temperature and humidity readings
    uint8_t buffer[5];
    int result = i2c_smbus_read_i2c_block_data(dev->client, BME280_REGISTER_TEMPDATA, 5, buffer);
    if (result != 5)
    {
        // Return error code
        return (result < 0) ? result : -EIO; 
    }

    // Temperature readout per BME280 data sheet
    int raw_temp = (buffer[0] << 12) | // [19:12]
                   (buffer[1] << 4) |  // [11:4]
                   (buffer[2] >> 4);   // [3:0]
    // Humidity readout per BME280 data sheet
    int raw_humidity = (buffer[3] << 8) | // [15:8]
                       (buffer[4]);       // [7:0]
    // Check if either measurement was skipped
    if ((raw_temp == BME280_TEMP_SKIPPED) || (raw_humidity == BME280_HUMID_SKIPPED))
    {
        return -EAGAIN;
    }

    // Run compensenation equations for temperature readings
    int t_fine = bme280_compensate_temp(dev, raw_temp);
    *temp = ((t_fine * 5) + 128) >> 8;
    // Run compensation equations for humdity readings
    *humidity = bme280_compensate_humidity(dev, t_fine, raw_humidity);

    return 0;
}

static long bme280_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct bme280_dev *dev = filp->private_data;
    long retval = 0;

    if (_IOC_TYPE(cmd) != BME280_IOC_MAGIC)
    {
        return -ENOTTY;
    }

    switch (cmd)
    {
        case BME280_IOC_GET_TEMP:
        case BME280_IOC_GET_HUMIDITY:
        {
            int temp, humidity = 0;
            retval = bme280_read_measurements(dev, &temp, &humidity);
            if (retval)
            {
                break;
            }

            // Copy state to user space buffer
            int request = (cmd == BME280_IOC_GET_TEMP) ? temp : humidity;
            if (copy_to_user((int __user *)arg, &request, sizeof(request)))
            {
                retval = -EFAULT;
            }
            break;
        }
        default:
            return -ENOTTY;
    }

    return retval;
}

static const struct file_operations bme280_fops = 
{
    .owner =          THIS_MODULE,
    .open =           bme280_open,
    .release =        bme280_release,
    .unlocked_ioctl = bme280_ioctl,
};

static int bme280_setup_cdev(struct bme280_dev *dev)
{
    int err, devno = MKDEV(bme280_major, bme280_minor);

    cdev_init(&dev->cdev, &bme280_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &bme280_fops;
    err = cdev_add(&dev->cdev, devno, 1);
    if (err)
    {
        printk(KERN_ERR "Error %d adding BME280 sensor cdev\n", err);
    }
    return err;
}

static int bme280_hw_init(struct bme280_dev *dev)
{
    // Find the I2C bus the sensor is connected to
    int result;
    struct i2c_adapter *adapter = i2c_get_adapter(BME280_I2C_BUS);
    if (!adapter)
    {
        printk(KERN_WARNING "I2C bus %d not found\n", BME280_I2C_BUS);
        return -ENODEV;
    }

    // Create the I2C client for the sensor
    dev->client = i2c_new_dummy_device(adapter, BME280_I2C_ADDR);
    i2c_put_adapter(adapter);
    if (IS_ERR(dev->client))
    {
        result = PTR_ERR(dev->client);
        return result;
    }

    // Read chip register ID
    result = i2c_smbus_read_byte_data(dev->client, BME280_REGISTER_CHIPID);
    if (result < 0)
    {
        // Unregister device if sensor does not respond
        printk(KERN_WARNING "Unexpected result: %d\n", result);
        goto unregister_device;
    }

    // Reset sensor before reading data
    result = i2c_smbus_write_byte_data(dev->client, BME280_REGISTER_SOFTRESET, BME280_RESET_VALUE);
    if (result)
    {
        goto unregister_device;
    }
    usleep_range(5000, 6000);

    // Read temperature calibration data
    uint8_t temp_calibration[6];
    result = i2c_smbus_read_i2c_block_data(dev->client, BME280_REGISTER_DIG_T1, 6, temp_calibration);
    if (result != 6)
    {
        printk(KERN_WARNING "Error reading temperature calibration data: %d\n", result);
        result = (result < 0) ? result : -EIO;
        goto unregister_device;
    }
    // Convert and store temperature calibration data per BME280 datasheet
    dev->calibration.dig_T1 = (uint16_t)(temp_calibration[0] | (temp_calibration[1] << 8));
    dev->calibration.dig_T2 = (int16_t)(temp_calibration[2] | (temp_calibration[3] << 8));
    dev->calibration.dig_T3 = (int16_t)(temp_calibration[4] | (temp_calibration[5] << 8));

    // Read humidity calibration data
    result = i2c_smbus_read_byte_data(dev->client, BME280_REGISTER_DIG_H1);
    if (result < 0)
    {
        printk(KERN_WARNING "Error reading humidity calibration data: %d\n", result);
        goto unregister_device;
    }
    dev->calibration.dig_H1 = (uint8_t)result;

    uint8_t humidity_calibration[7];
    result = i2c_smbus_read_i2c_block_data(dev->client, BME280_REGISTER_DIG_H2, 7, humidity_calibration);
    if (result != 7)
    {
        printk(KERN_WARNING "Error reading humidity calibration data: %d\n", result);
        result = (result < 0) ? result : -EIO;
        goto unregister_device;
    }
    // Convert and store humidity calibration data per BME280 datasheet
    dev->calibration.dig_H2 = (int16_t)(humidity_calibration[0] | (humidity_calibration[1] << 8));
    dev->calibration.dig_H3 = (uint8_t)humidity_calibration[2];
    dev->calibration.dig_H4 = (int16_t)((((int8_t)humidity_calibration[3] << 4) | (humidity_calibration[4] & 0x0F)));
    dev->calibration.dig_H5 = (int16_t)((((int8_t)humidity_calibration[5] << 4) | (humidity_calibration[4] >> 4)));
    dev->calibration.dig_H6 = (int8_t)humidity_calibration[6];

    // Write humidity measurement command to chip
    result = i2c_smbus_write_byte_data(dev->client, BME280_REGISTER_CONTROLHUMID, BME280_HUMID_CTRL_MEAS);
    if (result)
    {
        goto unregister_device;
    }
    usleep_range(10000, 11000); 

    // Write temperature measurement command to chip
    result = i2c_smbus_write_byte_data(dev->client, BME280_REGISTER_CONTROL, BME280_TEMP_CTRL_MEAS);
    if (result)
    {
        goto unregister_device;
    }
    usleep_range(10000, 11000); 

    return 0;

unregister_device:
    i2c_unregister_device(dev->client);
    return result;
}

static int bme280_init_module(void)
{
    dev_t dev = 0;
    int result;

    result = alloc_chrdev_region(&dev, bme280_minor, 1, BME280_DEVICE_NAME);
    bme280_major = MAJOR(dev);

    if (result < 0)
    {
        printk(KERN_WARNING "Can't get major %d\n", bme280_major);
        return result;
    }

    // Initialize device to zero
    memset(&bme280_device, 0, sizeof(struct bme280_dev));

    // Setup device
    result = bme280_hw_init(&bme280_device);
    if (result)
    {
        goto unregister_region;
    }

    // Register character device
    result = bme280_setup_cdev(&bme280_device);
    if (result)
    {
        i2c_unregister_device(bme280_device.client);        
        goto unregister_region;
    }

    PDEBUG("Loaded, major %d", bme280_major);
    return 0;

unregister_region:
    unregister_chrdev_region(dev, 1);
    return result;
}

static void bme280_cleanup_module(void)
{
    dev_t devno = MKDEV(bme280_major, bme280_minor);

    cdev_del(&bme280_device.cdev);
    i2c_unregister_device(bme280_device.client);
    unregister_chrdev_region(devno, 1);

    PDEBUG("Unloaded BME280 Sensor Device");
}

module_init(bme280_init_module);
module_exit(bme280_cleanup_module);