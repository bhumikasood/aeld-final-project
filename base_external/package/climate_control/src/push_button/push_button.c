/**
 * @file push_button.c
 * @author Bhumika Sood
 * @brief Functions and data related to the push button driver implementation
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

#include "push_button.h"
#include "button_ioctl.h"

#define PUSH_BUTTON_GPIO 6
#define DEBOUNCE_SAMPLES 3
#define DEBOUNCE_MS      50

// Function prototypes
static int button_open(struct inode *inode, struct file *filp);
static int button_release(struct inode *inode, struct file *filp);
static long button_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
static int button_setup_cdev(struct push_button_dev *dev);
static int button_read_state(void);
static int button_init_module(void);
static void button_cleanup_module(void);

static int button_major = 0;
static int button_minor = 0;

MODULE_AUTHOR("Bhumika Sood");
MODULE_LICENSE("Dual BSD/GPL");

static struct push_button_dev button_device;

static int button_open(struct inode *inode, struct file *filp)
{
	PDEBUG("Open Button Device");

	// Locate device structure
	struct push_button_dev *dev;
    dev = container_of(inode->i_cdev, struct push_button_dev, cdev);
    // Point to device data
    filp->private_data = dev;

    return 0;
}

static int button_release(struct inode *inode, struct file *filp)
{
    PDEBUG("Release Button Device");
    return 0;
}


// Read debounced rotary switch position
static int button_read_state(void)
{
    // Get initial reading of button
    int initial = gpio_get_value(PUSH_BUTTON_GPIO);

    int i;
    for (i = 1; i < DEBOUNCE_SAMPLES; i++)
    {
        usleep_range(DEBOUNCE_MS * 1000, DEBOUNCE_MS * 1000 + 1000);
        if (gpio_get_value(PUSH_BUTTON_GPIO) != initial)
        {
            return BUTTON_UNKNOWN;
        }
    }

    return initial ? BUTTON_RELEASED : BUTTON_PRESSED;
}

static long button_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    long retval = 0;

    if (_IOC_TYPE(cmd) != BUTTON_IOC_MAGIC)
    {
        return -ENOTTY;
    }

    switch (cmd)
    {
        case BUTTON_IOC_GET_STATE:
        {
            int state = button_read_state();

            // Copy state to user space buffer
            if (copy_to_user((int __user *)arg, &state, sizeof(state)))
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

static const struct file_operations button_fops = 
{
    .owner =          THIS_MODULE,
    .open =           button_open,
    .release =        button_release,
    .unlocked_ioctl = button_ioctl,
};

static int button_setup_cdev(struct push_button_dev *dev)
{
    int err, devno = MKDEV(button_major button_minor);

    cdev_init(&dev->cdev, &button_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &button_fops;
    err = cdev_add(&dev->cdev, devno, 1);
    if (err)
    {
        printk(KERN_ERR "Error %d adding button cdev\n", err);
    }
    return err;
}

static int button_init_module(void)
{
    dev_t dev = 0;
    int result;

    // Claim the GPIO pin as an input
    result = gpio_request_one(PUSH_BUTTON_GPIO, GPIOF_IN, "push-button");
    if (result)
    {
        printk(KERN_WARNING "Can't request push button GPIO: %d\n", result);
        return result;
    }

    // Enable the internal pull-up
	result = gpiod_set_config(gpio_to_desc(PUSH_BUTTON_GPIO),
                              PIN_CONF_PACKED(PIN_CONFIG_BIAS_PULL_UP, 1));
    usleep_range(1000, 2000); 

    result = alloc_chrdev_region(&dev, button_minor, 1, BUTTON_DEVICE_NAME);
    button_major = MAJOR(dev);

    if (result < 0)
    {
        printk(KERN_WARNING "Can't get major %d\n", button_major);
        goto free_gpio;
    }

    // Initialize device to zero
    memset(&button_device, 0, sizeof(struct push_button_dev));

    // Register character device
    result = button_setup_cdev(&button_device);

    if (result)
    {
        unregister_chrdev_region(dev, 1);
        goto free_gpio;
    }

    PDEBUG("Loaded, major %d", button_major);
    return 0;

free_gpio:
    gpio_free(PUSH_BUTTON_GPIO);
    return result;
}

static void button_cleanup_module(void)
{
    dev_t devno = MKDEV(button_major, button_minor);

    cdev_del(&button_device.cdev);
    unregister_chrdev_region(devno, 1);
    gpio_free(PUSH_BUTTON_GPIO);

    PDEBUG("Unloaded Button Device");
}

module_init(rotary_init_module);
module_exit(rotary_cleanup_module);