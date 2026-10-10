/**
 * @file temp_sensor.c
 * @author Bhumika Sood
 * @brief Reads inputs from the climate control devices and prints results to console.
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <time.h>

#include "rotary_ioctl.h"
#include "button_ioctl.h"
#include "temp_ioctl.h"

#define POLL_INTERVAL_US 20000
#define PRINT_INTERVAL_S 2  

const char *climate_modes[] = {"HEAT", "COOL", "OFF"};

// Signal flag
static volatile sig_atomic_t running = 1;

// Function prototypes
static void signal_handler(int signal_num);
static int open_device(const char *node);
static float celcius_to_fahrenheit(int temp_c);

static void signal_handler(int signal_num)
{
    (void)signal_num;
    running = 0;
}

// Open device
static int open_device(const char *node)
{
    int fd = open(node, O_RDONLY);
    if (fd < 0)
    {
        perror(node);
    }
    return fd;
}

// Convert from celcius to fahrenheit
static float celcius_to_fahrenheit(int temp_c)
{
    float temp_celcius = temp_c / 100.0;
    float temp_fahrenheit = (temp_celcius * 1.8) + 32;
    return temp_fahrenheit;
}

int main(void)
{
    // Initial values for rotary switch
    int rotary_fd;
    int previous_rotary_position = ROTARY_POS_UNKNOWN;
    int current_rotary_position = ROTARY_POS_UNKNOWN; 
    // Initial values for push button
    int button_fd;
    int current_mode = 2; 
    int previous_button_state = BUTTON_RELEASED;
    int current_button_state = BUTTON_UNKNOWN;
    // Initial values for BME280 sensor
    int temp_fd;
    int temp_c, temp_status = 0;
    float temp_f;

    // Timer values
    struct timespec current_time, previous_time;
    clock_gettime(CLOCK_MONOTONIC, &previous_time);
    // Set previous time behind by 2 seconds
    previous_time.tv_sec -= PRINT_INTERVAL_S;


    // Handle signals
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Open devices
    rotary_fd = open_device("/dev/" ROTARY_DEVICE_NAME);
    button_fd = open_device("/dev/" BUTTON_DEVICE_NAME);
    temp_fd = open_device("/dev/" TEMP_DEVICE_NAME);

    printf("Climate Control System Started\n");

    while (running)
    {
        // Rotary switch
        if (rotary_fd >= 0)
        {
            if (ioctl(rotary_fd, ROTARY_IOC_GET_POSITION, &current_rotary_position) < 0)
            {
                if (running)
                {
                    perror("rotary ioctl");
                }
                close(rotary_fd);
                rotary_fd = -1;
            }
            else if ((current_rotary_position != ROTARY_POS_UNKNOWN) && 
                    (current_rotary_position != previous_rotary_position))
            {
                // printf("rotary position: %d\n", current_rotary_position);
                previous_rotary_position = current_rotary_position;
            }
        }

        // Push button
        if (button_fd >= 0)
        {
            if (ioctl(button_fd, BUTTON_IOC_GET_STATE, &current_button_state) < 0)
            {
                if (running)
                {
                    perror("button ioctl");
                }
                close(button_fd);
                button_fd = -1;
            }
            else
            {
                // Only change mode when the state changes from released to pressed
                if ((current_button_state == BUTTON_PRESSED) && 
                    (previous_button_state == BUTTON_RELEASED))
                {
                    current_mode = (current_mode + 1) % 3;
                    // printf("mode: %s\n", climate_modes[current_mode]);
                }
                // Update last state
                if (current_button_state != BUTTON_UNKNOWN)
                {
                    previous_button_state = current_button_state;
                }
            }
        }

        // Only read the temperature every 2 seconds
        clock_gettime(CLOCK_MONOTONIC, &current_time);
        if ((current_time.tv_sec - previous_time.tv_sec) >= PRINT_INTERVAL_S)
        {
            previous_time = current_time;

            if (temp_fd >= 0)
            {
                temp_status = (ioctl(temp_fd, TEMP_IOC_GET_TEMP, &temp_c) == 0);
                if (!temp_status && running)
                {
                    perror("temperature ioctl");
                }
                else
                {
                    temp_f = celcius_to_fahrenheit(temp_c);
                }
            }
            
            // Print status of all devices
            printf("[status] mode: %-4s | rotary: %d | temp: %.1f F\n",
                climate_modes[current_mode], current_rotary_position, temp_f);
        }

        usleep(POLL_INTERVAL_US);

    }

    printf("Stopping Climate Control System\n");

    if (rotary_fd >= 0)
    {
        close(rotary_fd);
    }
    if (button_fd >= 0)
    {
        close(button_fd);
    }
    if (temp_fd >= 0)
    {
        close(temp_fd);
    }

    return 0;
}