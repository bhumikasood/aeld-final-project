/*
 * @file push_button.h
 * @author Bhumika Sood 
 * @brief Definitions for a push button driver.
 */

#ifndef BUTTON_IOCTL_H
#define BUTTON_IOCTL_H

#ifdef __KERNEL__
#include <linux/ioctl.h>
#else
#include <sys/ioctl.h>
#endif


#define BUTTON_DEVICE_NAME   "push_button" // Device name as it appears in /dev/push_button
#define BUTTON_RELEASED      0  // Button is not pressed
#define BUTTON_PRESSED       1  // Button is pressed
#define BUTTON_UNKNOWN       (-1) // Button is in an in-betweeen state
#define BUTTON_IOC_MAGIC     0x17 // Push button IOCTL magic number

// Get push button state: BUTTON_RELEASED, BUTTON_PRESSED, or, BUTTON_UNKNOWN
#define BUTTON_IOC_GET_STATE _IOR(BUTTON_IOC_MAGIC, 1, int)

#endif /* BUTTON_IOCTL_H */