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

#define BME280_I2C_BUS      1   
#define BME280_I2C_ADDR     0x77
#define BME280_TEMP_SKIPPED 0x80000  
#define BME280_RESET_VALUE  0xB6
#define BME280_CTRL_MEAS    0x23

// Defined in the BME280 datasheet
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
static int bme280_read_temp(struct bme280_dev *dev, int *temp);
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


// Get a temperature reading from the sensor and convert it to celcius
static int bme280_read_temp(struct bme280_dev *dev, int *temp)
{
    // Get reading of temperature
    uint8_t temp_buffer[3];
    int result = i2c_smbus_read_i2c_block_data(dev->client, BME280_REGISTER_TEMPDATA, 3, temp_buffer);
    if (result != 3)
    {
        // Return error code
        return (result < 0) ? result : -EIO; 
    }

    // Readout and compensation per BME280 data sheet
    int raw_temp = (temp_buffer[0] << 12) | // [19:12]
                   (temp_buffer[1] << 4) |  // [11:4]
                   (temp_buffer[2] >> 4);   // [3:0]
    if (raw_temp == BME280_TEMP_SKIPPED)
    {
        return -EAGAIN;
    }

    // Compensation equations from BME280 datasheet
    int var_1 = (((raw_temp >> 3) - ((int)dev->calibration.dig_T1 << 1)) 
                * dev->calibration.dig_T2) >> 11;
    int var_2 = ((((raw_temp >> 4) - dev->calibration.dig_T1)
                * ((raw_temp >> 4) - dev->calibration.dig_T1) >> 12)
                * dev->calibration.dig_T3) >> 14;
    
    *temp = (((var_1 + var_2) * 5) + 128) >> 8;
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
        {
            int temp = 0;
            retval = bme280_read_temp(dev, &temp);
            if (retval)
            {
                break;
            }

            // Copy state to user space buffer
            if (copy_to_user((int __user *)arg, &temp, sizeof(temp)))
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

    // Reset sensor before reading temperature data
    result = i2c_smbus_write_byte_data(dev->client, BME280_REGISTER_SOFTRESET, BME280_RESET_VALUE);
    if (result)
    {
        goto unregister_device;
    }
    usleep_range(5000, 6000);

    // Read calibration data
    uint8_t calibration_data[6];
    result = i2c_smbus_read_i2c_block_data(dev->client, BME280_REGISTER_DIG_T1, 6, calibration_data);
    if (result != 6)
    {
        printk(KERN_WARNING "Error reading calibration data: %d\n", result);
        result = (result < 0) ? result : -EIO;
        goto unregister_device;
    }

    // Convert and store calibration data per BME280 datasheet
    dev->calibration.dig_T1 = (uint16_t)(calibration_data[0] | (calibration_data[1] << 8));
    dev->calibration.dig_T2 = (int16_t)(calibration_data[2] | (calibration_data[3] << 8));
    dev->calibration.dig_T3 = (int16_t)(calibration_data[4] | (calibration_data[5] << 8));

    // Temperature x1, pressure off, normal mode: the chip keeps measuring
    result = i2c_smbus_write_byte_data(dev->client, BME280_REGISTER_CONTROL, BME280_CTRL_MEAS);
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