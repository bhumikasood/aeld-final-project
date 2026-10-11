/*
 * @file lcd_ioctl.h
 * @author Bhumika Sood 
 * @brief Definitions for a LCD 1602 driver.
 */

#ifndef LCD_IOCTL_H
#define LCD_IOCTL_H

#ifdef __KERNEL__
#include <linux/ioctl.h>
#else
#include <sys/ioctl.h>
#endif


#define LCD_DEVICE_NAME   "lcd_display" // Device name as it appears in /dev/lcd_display
#define LCD_ROWS          2  // Numbers of lines in the LCD display
#define LCD_COLUMNS       16  // Number of characters that can fit in a line
#define LCD_IOC_MAGIC     0x19 // LCD display IOCTL magic number

// Clear the LCD display
#define LCD_IOC_CLEAR _IO(LCD_IOC_MAGIC, 1)

#endif /* LCD_IOCTL_H */