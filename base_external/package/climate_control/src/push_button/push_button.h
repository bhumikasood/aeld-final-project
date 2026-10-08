/**
 * @file push_button.h
 * @author Bhumika Sood
 * @brief Device structure for the push button driver
 */
 
#ifndef PUSH_BUTTON_H
#define PUSH_BUTTON_H

#include <linux/cdev.h>

#undef PDEBUG
#ifdef __KERNEL__
#define PDEBUG(fmt, args...) printk(KERN_DEBUG "push_button: " fmt "\n", ## args)
#else
#define PDEBUG(fmt, args...) fprintf(stderr, fmt, ## args)
#endif

struct push_button_dev
{
    struct cdev cdev; 
};

#endif /* PUSH_BUTTON_H */
