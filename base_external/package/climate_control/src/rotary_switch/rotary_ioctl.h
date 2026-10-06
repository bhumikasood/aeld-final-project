/*
 * @file rotary_ioctl.h
 * @author Bhumika Sood 
 * @brief Definitions for a rotary switch driver.
 */

#ifndef ROTARY_IOCTL_H
#define ROTARY_IOCTL_H

#ifdef __KERNEL__
#include <linux/ioctl.h>
#else
#include <sys/ioctl.h>
#endif


#define ROTARY_DEVICE_NAME   "rotary_switch" // Device name as it appears in /dev/rotary_switch
#define ROTARY_NUM_POSITIONS 8 // Number of switch position in rotary switch
#define ROTARY_POS_UNKNOWN   (-1) // Switch value when position is unknown
#define ROTARY_IOC_MAGIC     0x16 // Rotary switch IOCTL magic number

// Get position of rotary switch 0-7, or return ROTARY_POS_UNKNOWN.
#define ROTARY_IOC_GET_POSITION _IOR(ROTARY_IOC_MAGIC, 1, int)

// Get debounced pin state on start-up.
#define ROTARY_GET_PIN_MASK _IOR(ROTARY_IOC_MAGIC, 2, unsigned int)

#endif /* ROTARY_IOCTL_H */