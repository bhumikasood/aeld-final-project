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

    int temperature, humidity;
    while (1)
    {
        if (ioctl(fd, BME280_IOC_GET_TEMP, &temperature) < 0)
        {
            perror("ioctl temperature");
            break;
        }

        if (ioctl(fd, BME280_IOC_GET_HUMIDITY, &humidity) < 0)
        {
            perror("ioctl humidity");
            break;
        }

        printf("temperature: %.2f C \t humidity: %.2f %%\n", temperature / 100.0, humidity / 100.0);
        sleep(1);
    }

    close(fd);
    return 0;
}