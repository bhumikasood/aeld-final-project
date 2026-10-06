/**
 * @file rotary_switch.h
 * @author Bhumika Sood
 * @brief Device structure for the rotary switch driver
 */
 
#ifndef ROTARY_SWITCH_H_
#define ROTARY_SWITCH_H_
 
#include <linux/cdev.h>
 
#define ROTARY_DEBUG 1 
 
#undef PDEBUG            
#ifdef ROTARY_DEBUG
#  define PDEBUG(fmt, args...) printk(KERN_DEBUG "rotary_switch: " fmt "\n", ## args)
#else
#  define PDEBUG(fmt, args...) /* not debugging: nothing */
#endif
 
struct rotary_dev
{
    struct cdev cdev;    
};
 
#endif /* ROTARY_SWITCH_H_ */
