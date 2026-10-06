/**
 * @file rotary_test.c
 * @author Bhumika Sood
 * @brief Prints the rotary switch position each time it changes
 */

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "rotary_ioctl.h"

int main(void)
{
    int position;
    int last = -2; 

    int fd = open("/dev/" ROTARY_DEVICE_NAME, O_RDONLY);
    if (fd < 0)
    {
        perror("open /dev/" ROTARY_DEVICE_NAME);
        return 1;
    }

    while (1)
    {
        if (ioctl(fd, ROTARY_IOC_GET_POSITION, &position) < 0)
        {
            perror("ioctl call");
            return 1;
        }

        // Only print when the position changes 
        if (position != last)
        {
            printf("position: %d\n", position);
            last = position;
        }

        usleep(50000); // Check 20 times a second
    }
}