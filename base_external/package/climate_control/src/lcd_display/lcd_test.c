/**
 * @file lcd_test.c
 * @author Bhumika Sood
 * @brief Prints the data to the LCD1602 display to exercise the driver code
 */

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>

#include "lcd_ioctl.h"
 
int main(void)
{
    int fd = open("/dev/" LCD_DEVICE_NAME, O_WRONLY);
    if (fd < 0)
    {
        perror("open /dev/" LCD_DEVICE_NAME);
        return 1;
    }
    
    // Write messsage to LCD 
    write(fd, "Climate Control\nLCD test", strlen("Climate Control\nLCD test"));
    sleep(3);
 
    // Write message to LCD with dummy data
    write(fd, "Temp: 72.5\337F\nMode: Heat", strlen("Temp: 72.5\337F\nMode: Heat"));
    sleep(3);
 
    if (ioctl(fd, LCD_IOC_CLEAR) < 0)
    {
        perror("ioctl call");
    }
 
    close(fd);
    return 0;
}