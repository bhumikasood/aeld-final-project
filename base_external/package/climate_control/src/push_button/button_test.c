/**
 * @file button_test.c
 * @author Bhumika Sood
 * @brief Prints the climate system response to a button press
 */

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "button_ioctl.h"

int main(void)
{
    const char *climate_modes[] = {"HEAT", "COOL", "OFF"};
    int current_mode = 2; // Set initial mode to off
    int previous_state = BUTTON_RELEASED; // Set initial button position
    int current_state = BUTTON_UNKNOWN;

    int fd = open("/dev/" BUTTON_DEVICE_NAME, O_RDONLY);
    if (fd < 0)
    {
        perror("open /dev/" BUTTON_DEVICE_NAME);
        return 1;
    }

    while (1)
    {
        if (ioctl(fd, BUTTON_IOC_GET_STATE, &current_state) < 0)
        {
            perror("ioctl call");
            break;
        }

        // Only change mode when the state changes from released to pressed
        if ((current_state == BUTTON_PRESSED) && (previous_state == BUTTON_RELEASED))
        {
            current_mode = (current_mode + 1) % 3; // Cycle mode
            printf("mode: %s\n", climate_modes[current_mode]);
        }

        // Update last state
        if (current_state != BUTTON_UNKNOWN)
        {
            previous_state = current_state;
        }

        usleep(50000); // Check 20 times a second
    }

    close(fd);
    return 0;
}