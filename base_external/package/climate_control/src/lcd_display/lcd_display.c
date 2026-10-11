/**
 * @file lcd_display.c
 * @author Bhumika Sood
 * @brief Functions and data related to the lcd display driver implementation
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

#include "lcd_display.h"
#include "lcd_ioctl.h"

// Wiring of LCD display to Raspberry Pi 3
#define LCD_GPIO_RS 12 
#define LCD_GPIO_E  13
#define LCD_GPIO_D4 19 
#define LCD_GPIO_D5 16 
#define LCD_GPIO_D6 26 
#define LCD_GPIO_D7 20 

// Instructions per Hitachi datasheet
#define CLEAR_LCD    0x01
#define ENTRY_MODE   0x06 
#define DISPLAY_OFF  0x08
#define DISPLAY_ON   0x0C 
#define FUNCTION_SET 0x28 
#define SET_DDRAM    0x80

// Memory address per Hitachi datasheet
#define ADDR_ROW_UPPER 0x00
#define ADDR_ROW_LOWER 0x40

// Function prototypes
static int lcd_open(struct inode *inode, struct file *filp);
static int lcd_release(struct inode *inode, struct file *filp);
static long lcd_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
static int lcd_setup_cdev(struct lcd_display_dev *dev);
static ssize_t lcd_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos);
static void lcd_write_nibble(uint8_t nibble);
static void lcd_display_send(uint8_t value, int mode);
static void lcd_display_clear(void);
static void lcd_hw_init(void);
static int lcd_init_module(void);
static void lcd_cleanup_module(void);

static int lcd_major = 0;
static int lcd_minor = 0;

MODULE_AUTHOR("Bhumika Sood");
MODULE_LICENSE("Dual BSD/GPL");

static struct lcd_display_dev lcd_device;

// Wiring of LCD display to Raspberry Pi 3
static const struct gpio lcd_gpio[] = 
{
    {12, GPIOF_OUT_INIT_LOW, "lcd-rs"},
    {13, GPIOF_OUT_INIT_LOW, "lcd-e"},
    {19, GPIOF_OUT_INIT_LOW, "lcd-d4"},
    {16, GPIOF_OUT_INIT_LOW, "lcd-d5"},
    {26, GPIOF_OUT_INIT_LOW, "lcd-d6"},
    {20, GPIOF_OUT_INIT_LOW, "lcd-d7"},
};

static int lcd_open(struct inode *inode, struct file *filp)
{
	PDEBUG("Open LCD Display Device");

	// Locate device structure
	struct lcd_display_dev *dev;
    dev = container_of(inode->i_cdev, struct lcd_display_dev, cdev);
    // Point to device data
    filp->private_data = dev;

    return 0;
}

static int lcd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("Release LCD Display Device");
    return 0;
}

// Write 4 bits to GPIO pins
static void lcd_write_nibble(uint8_t nibble)
{
    // Set lower 4 bits
    gpio_set_value(LCD_GPIO_D4, (nibble >> 0) & 1);
    gpio_set_value(LCD_GPIO_D5, (nibble >> 1) & 1);
    gpio_set_value(LCD_GPIO_D6, (nibble >> 2) & 1);
    gpio_set_value(LCD_GPIO_D7, (nibble >> 3) & 1);
    udelay(1);

    // Toggle enable pin to store nibble
    gpio_set_value(LCD_GPIO_E, 1);
    udelay(1);
    gpio_set_value(LCD_GPIO_E, 0);
    udelay(1);
}

// Send command or data to LCD display
static void lcd_display_send(uint8_t value, int mode)
{
    // LOW = command and HIGH = data
    gpio_set_value(LCD_GPIO_RS, mode ? 1 : 0);

    // Write high nibble then low nibble to LCD
    lcd_write_nibble(value >> 4);
    lcd_write_nibble(value & 0x0F);
    udelay(50); 
}

// Clear LCD display screen
static void lcd_display_clear(void)
{
    lcd_display_send(CLEAR_LCD, 0);
    usleep_range(2000, 3000);
}

// Write message from userspace to LCD display
static ssize_t lcd_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
    struct lcd_display_dev *dev = filp->private_data;

    // Copy the message from user space, ignore anything larger than the input buffer
    char input_buffer[34];
    size_t length = (count < sizeof(input_buffer)) ? count : sizeof(input_buffer);
    if (copy_from_user(input_buffer, buf, length))
    {
        return -EFAULT;
    }

    // Copy characters into correct format for LCD display, skip any special characters
    char lcd[LCD_ROWS][LCD_COLUMNS];
    memset(lcd, ' ', sizeof(lcd));
    int row, column = 0;
    size_t i = 0;
    for (i = 0; (i < length) && (row < LCD_ROWS); i++)
    {
        // Move to next row if new line character is received
        if (input_buffer[i] == '\n')
        {
            row++;
            column = 0;
        }
        else if (((unsigned char)input_buffer[i] >= 0x20) && (column < LCD_COLUMNS))
        {
            lcd[row][column++] = input_buffer[i];
        }
    }
 
    // Lock with mutex
    if (mutex_lock_interruptible(&dev->lock))
    {
        return -ERESTARTSYS;
    }
    
    int r, c;
    for (r = 0; r < LCD_ROWS; r++)
    {
        // Select destination address for row
        uint32_t row_addr = (r == 0) ? ADDR_ROW_UPPER : ADDR_ROW_LOWER;
        // Send command to move cursor
        lcd_display_send(SET_DDRAM | row_addr, 0);
        // Write data to LCD display
        for (c = 0; c < LCD_COLUMNS; c++)
        {
            lcd_display_send(lcd[r][c], 1);
        }
    }
    
    // Release mutex
    mutex_unlock(&dev->lock);
    return count;
}

static long lcd_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    long retval = 0;
    struct lcd_display_dev *dev = filp->private_data;

    if (_IOC_TYPE(cmd) != LCD_IOC_MAGIC)
    {
        return -ENOTTY;
    }

    switch (cmd)
    {
        case LCD_IOC_CLEAR:
        {
            // Lock with mutex
            if (mutex_lock_interruptible(&dev->lock))
            {
                return -ERESTARTSYS;
            }

            // CLear display
            lcd_display_clear();
            // Release mutex
            mutex_unlock(&dev->lock);
            break;
        }
        default:
            return -ENOTTY;
    }

    return retval;
}

static const struct file_operations lcd_fops = 
{
    .owner =          THIS_MODULE,
    .open =           lcd_open,
    .release =        lcd_release,
    .write =          lcd_write,
    .unlocked_ioctl = lcd_ioctl,
};

static int lcd_setup_cdev(struct lcd_display_dev *dev)
{
    int err, devno = MKDEV(lcd_major, lcd_minor);

    cdev_init(&dev->cdev, &lcd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &lcd_fops;
    err = cdev_add(&dev->cdev, devno, 1);
    if (err)
    {
        printk(KERN_ERR "Error %d adding LCD1602 display cdev\n", err);
    }
    return err;
}

// Initialization sequence per the Hitachi datasheet
static void lcd_hw_init(void)
{
    msleep(50);

    // Set control
    gpio_set_value(LCD_GPIO_RS, 0);
    gpio_set_value(LCD_GPIO_E, 0);
 
    // Write 3 and wait
    lcd_write_nibble(0x3);
    msleep(5);
    // Write 3 and wait
    lcd_write_nibble(0x3);
    udelay(150); 
    // Write 3 and wait
    lcd_write_nibble(0x3);
    udelay(150);
    // Set interface to be 4-bits
    lcd_write_nibble(0x2);
    udelay(150);

    // Specify the number of display lines and character font
    lcd_display_send(FUNCTION_SET, 0);
    // Displaty off
    lcd_display_send(DISPLAY_OFF, 0);
    // Clear display
    lcd_display_clear();
    // Set entry mode
    lcd_display_send(ENTRY_MODE, 0);
    // Display on
    lcd_display_send(DISPLAY_ON, 0);
}

static int lcd_init_module(void)
{
    dev_t dev = 0;
    int result;

    // Claim the GPIO pins as outputs and set LOW
    result = gpio_request_array(lcd_gpio, ARRAY_SIZE(lcd_gpio));
    if (result)
    {
        printk(KERN_WARNING "Can't request LCD GPIOs: %d\n", result);
        return result;
    }

    // Start up LCD display
    lcd_hw_init();

    // Get device number
    result = alloc_chrdev_region(&dev, lcd_minor, 1, LCD_DEVICE_NAME);
    lcd_major = MAJOR(dev);
    if (result < 0)
    {
        printk(KERN_WARNING "Can't get major %d\n", lcd_major);
        goto free_gpios;
    }

    // Initialize device to zero
    memset(&lcd_device, 0, sizeof(struct lcd_display_dev));
    mutex_init(&lcd_device.lock);

    // Register character device
    result = lcd_setup_cdev(&lcd_device);
    if (result)
    {
        unregister_chrdev_region(dev, 1);
        goto free_gpios;
    }

    PDEBUG("Loaded, major %d", lcd_major);
    return 0;

free_gpios:
    gpio_free_array(lcd_gpio, ARRAY_SIZE(lcd_gpio));
    return result;

}

static void lcd_cleanup_module(void)
{
    dev_t devno = MKDEV(lcd_major, lcd_minor);

    cdev_del(&lcd_device.cdev);
    unregister_chrdev_region(devno, 1);
    lcd_display_clear();
    gpio_free_array(lcd_gpio, ARRAY_SIZE(lcd_gpio));

    PDEBUG("Unloaded LCD Display Device");
}

module_init(lcd_init_module);
module_exit(lcd_cleanup_module);