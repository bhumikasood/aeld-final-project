/**
 * @file rotary_switch.c
 * @author Bhumika Sood
 * @brief Functions and data related to the rotary switch driver implementation
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

#include "rotary_switch.h"
#include "rotary_ioctl.h"

#define DEBOUNCE_SAMPLES 3
#define DEBOUNCE_MS      20

// Function prototypes
static int rotary_open(struct inode *inode, struct file *filp);
static int rotary_release(struct inode *inode, struct file *filp);
static long rotary_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
static int rotary_setup_cdev(struct rotary_dev *dev);
static uint32_t rotary_read_mask(void);
static int rotary_read_position(void);
static int rotary_init_module(void);
static void rotary_cleanup_module(void);

static int rotary_major = 0;
static int rotary_minor = 0;

MODULE_AUTHOR("Bhumika Sood");
MODULE_LICENSE("Dual BSD/GPL");

static struct rotary_dev rotary_device;

// Wiring of rotary switch to Raspberry Pi 3
static const struct gpio rotary_gpio[ROTARY_NUM_POSITIONS] = 
{
    {4, GPIOF_IN, "rotary-pos0"},
    {17, GPIOF_IN, "rotary-pos1"},
    {27, GPIOF_IN, "rotary-pos2"},
    {22, GPIOF_IN, "rotary-pos3"},
    {23, GPIOF_IN, "rotary-pos4"},
    {24, GPIOF_IN, "rotary-pos5"},
    {25, GPIOF_IN, "rotary-pos6"},
    {5, GPIOF_IN, "rotary-pos7"},
};

static int rotary_open(struct inode *inode, struct file *filp)
{
	PDEBUG("Open Rotary Device");

	// Locate device structure
	struct rotary_dev *dev;
    dev = container_of(inode->i_cdev, struct rotary_dev, cdev);
    // Point to device data
    filp->private_data = dev;

    return 0;
}

static int rotary_release(struct inode *inode, struct file *filp)
{
    PDEBUG("Release Rotary Device");
    return 0;
}

// Read rotary switch and return a snapshot of the current position
static uint32_t rotary_read_mask(void)
{
    uint32_t mask = 0;
	int i;
    for (i = 0; i < ROTARY_NUM_POSITIONS; i++)
    {
        if (!gpio_get_value(rotary_gpio[i].gpio))
        {
            mask |= BIT(i);
        }
    }

    return mask;
}

// Read debounced rotary switch position
static int rotary_read_position(void)
{
	// Take snapshot of current rotary switch position
    uint32_t first_read = rotary_read_mask();

	int i;
    for (i = 1; i < DEBOUNCE_SAMPLES; i++)
    {
        usleep_range(DEBOUNCE_MS * 1000, DEBOUNCE_MS * 1000 + 1000);
        if (rotary_read_mask() != first_read)
        {
            return ROTARY_POS_UNKNOWN;
        }
    }

	for (i = 0; i < ROTARY_NUM_POSITIONS; i++)
    {
        if (first_read == BIT(i))
        {
            return i;
        }
    }

    return ROTARY_POS_UNKNOWN;
}

static long rotary_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    long retval = 0;

    if (_IOC_TYPE(cmd) != ROTARY_IOC_MAGIC)
    {
        return -ENOTTY;
    }

    switch (cmd)
    {
        case ROTARY_IOC_GET_POSITION:
        {
            int position = rotary_read_position();

            // Copy position to user space buffer
            if (copy_to_user((int __user *)arg, &position, sizeof(position)))
            {
                retval = -EFAULT;
            }
            break;
        }
        case ROTARY_GET_PIN_MASK:
        {
            unsigned int mask = rotary_read_mask();

            // Copy pin mask to user space buffer
            if (copy_to_user((unsigned int __user *)arg, &mask, sizeof(mask)))
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

static const struct file_operations rotary_fops = 
{
    .owner =          THIS_MODULE,
    .open =           rotary_open,
    .release =        rotary_release,
    .unlocked_ioctl = rotary_ioctl,
};

static int rotary_setup_cdev(struct rotary_dev *dev)
{
    int err, devno = MKDEV(rotary_major, rotary_minor);

    cdev_init(&dev->cdev, &rotary_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &rotary_fops;
    err = cdev_add(&dev->cdev, devno, 1);
    if (err)
    {
        printk(KERN_ERR "Error %d adding rotary cdev\n", err);
    }
    return err;
}

static int rotary_init_module(void)
{
    dev_t dev = 0;
    int result;

    // Claim the GPIO pins as inputs
    result = gpio_request_array(rotary_gpio, ARRAY_SIZE(rotary_gpio));
    if (result)
    {
        printk(KERN_WARNING "Can't request rotary GPIOs: %d\n", result);
        return result;
    }

    // Enable the internal pull-ups
	int i;
    for (i = 0; i < ROTARY_NUM_POSITIONS; i++)
    {
        result = gpiod_set_config(gpio_to_desc(rotary_gpio[i].gpio),
                                  PIN_CONF_PACKED(PIN_CONFIG_BIAS_PULL_UP, 1));
        if (result)
        {
            printk(KERN_WARNING "GPIO%u: pull-up not enabled (%d)\n",
                   rotary_gpio[i].gpio, result);
        }
    }
    usleep_range(1000, 2000); 

    result = alloc_chrdev_region(&dev, rotary_minor, 1, ROTARY_DEVICE_NAME);
    rotary_major = MAJOR(dev);

    if (result < 0)
    {
        printk(KERN_WARNING "Can't get major %d\n", rotary_major);
        goto free_gpios;
    }

    // Initialize device to zero
    memset(&rotary_device, 0, sizeof(struct rotary_dev));

    // Register character device
    result = rotary_setup_cdev(&rotary_device);

    if (result)
    {
        unregister_chrdev_region(dev, 1);
        goto free_gpios;
    }

    PDEBUG("Loaded, major %d", rotary_major);
    return 0;

free_gpios:
    gpio_free_array(rotary_gpio, ARRAY_SIZE(rotary_gpio));
    return result;
}

static void rotary_cleanup_module(void)
{
    dev_t devno = MKDEV(rotary_major, rotary_minor);

    cdev_del(&rotary_device.cdev);
    unregister_chrdev_region(devno, 1);
    gpio_free_array(rotary_gpio, ARRAY_SIZE(rotary_gpio));

    PDEBUG("Unloaded Rotary Device");
}

module_init(rotary_init_module);
module_exit(rotary_cleanup_module);