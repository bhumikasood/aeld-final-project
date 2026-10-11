/**
 * @file lcd_display.h
 * @author Bhumika Sood
 * @brief Device structure for the lcd display driver
 */
 
#ifndef LCD_DISPLAY_H
#define LCD_DISPLAY_H

#include <linux/cdev.h>

#undef PDEBUG
#ifdef __KERNEL__
#define PDEBUG(fmt, args...) printk(KERN_DEBUG "push_button: " fmt "\n", ## args)
#else
#define PDEBUG(fmt, args...) fprintf(stderr, fmt, ## args)
#endif

struct lcd_display_dev
{
    struct cdev cdev;   
    struct mutex lock; 
};

#endif /* LCD_DISPLAY_H */
