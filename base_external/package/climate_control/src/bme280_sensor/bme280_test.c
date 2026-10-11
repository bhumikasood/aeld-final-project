/**
 * @file bme280_test.c
 * @author Bhumika Sood
 * @brief Prints the temperature reading from the BME280 sensor
 */

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "bme280_ioctl.h"

int main(void)
{
    int fd = open("/dev/" BME280_DEVICE_NAME, O_RDONLY);
    if (fd < 0)
    {
        perror("open /dev/" BME280_DEVICE_NAME);
        return 1;
    }

    int temperature;
    while (1)
    {
        if (ioctl(fd, BME280_IOC_GET_TEMP, &temperature) < 0)
        {
            perror("ioctl call");
            break;
        }

        printf("temperature: %.2f C\n", temperature / 100.0);
        sleep(1);
    }

    close(fd);
    return 0;
}