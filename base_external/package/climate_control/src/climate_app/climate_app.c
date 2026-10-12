/**
 * @file climate_app.c
 * @author Bhumika Sood
 * @brief Reads inputs from the climate control devices and writes results to LCD.
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <string.h>

#include "rotary_ioctl.h"
#include "button_ioctl.h"
#include "bme280_ioctl.h"
#include "lcd_ioctl.h"

// Timing details
#define POLL_INTERVAL_US    20000
#define SENSOR_INTERVAL_MS  2000
#define PRINT_INTERVAL_S    2
#define SET_TEMP_TIMEOUT    5

// LCD settings
#define DEGREE_SYMBOL   "\337"
#define LCD_MODE_COL    12

enum lcd_states
{
    STATUS_SCREEN = 0, 
    SET_TEMP_SCREEN
};

// Climate settings
#define TEMP_DEFAULT_F  78  
#define TEMP_MIN_F      60
#define TEMP_MAX_F      85
#define TEMP_STEP       1

const char *climate_modes[] = {"HEAT", "COOL", "OFF"};

// Signal flag
static volatile sig_atomic_t running = 1;

// Function prototypes
static void signal_handler(int signal_num);
static int open_device(const char *node, int flag);
static float celcius_to_fahrenheit(int temp_c);
static int rotary_step(int previous, int current);
static void format_status_screen(char *lcd_output, size_t size, int temp, int humidity, int mode);
static void format_temp_screen(char *lcd_output, size_t size, int desired_temp);

static void signal_handler(int signal_num)
{
    (void)signal_num;
    running = 0;
}

// Open device
static int open_device(const char *node, int flag)
{
    int fd = open(node, flag);
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

// Return number of steps rotary switch moved
static int rotary_step(int previous, int current)
{
    int step = (current - previous + ROTARY_NUM_POSITIONS) % ROTARY_NUM_POSITIONS;
 
    if (step > ROTARY_NUM_POSITIONS / 2)
    {
        step -= ROTARY_NUM_POSITIONS;
    }
    return step;
}

// Format LCD status screen
static void format_status_screen(char *lcd_output, size_t size, int temp, int humidity, int mode)
{
    char temp_text[16];      // TEMP:XX*F
    char humidity_text[16];  // HUMID:XX% 

    // Temperature output
    snprintf(temp_text, sizeof(temp_text), "TEMP:%02d" DEGREE_SYMBOL "F", temp);
    // Humidity output
    snprintf(humidity_text, sizeof(humidity_text), "HUMID:%02d%%", humidity);

    // Format rows 1 and 2 of LCD out with Mode right justified on the first row
    snprintf(lcd_output, size, "%-*.*s%4.4s\n%.*s",
             LCD_MODE_COL, LCD_MODE_COL, temp_text, climate_modes[mode],
             LCD_COLUMNS, humidity_text);
}

// Format LCD set temp screen
static void format_temp_screen(char *lcd_output, size_t size, int desired_temp)
{
    char set_temp_text[16];  // SET TEMP:XX

    // Temperature output
    snprintf(set_temp_text, sizeof(set_temp_text), "SET TEMP:%02d", desired_temp);
    snprintf(lcd_output, size, "%.*s\n", LCD_COLUMNS, set_temp_text);
}

int main(void)
{
    // Initial values for rotary switch
    int rotary_fd;
    int previous_rotary_position = ROTARY_POS_UNKNOWN;
    int current_rotary_position = ROTARY_POS_UNKNOWN;
    int step_value = 0; 
    // Initial values for push button
    int button_fd;
    int current_mode = 2; 
    int previous_button_state = BUTTON_RELEASED;
    int current_button_state = BUTTON_UNKNOWN;
    // Initial values for BME280 sensor
    int bme280_fd;
    int temp_c, temp_status = 0;
    int humidity, humidity_status = 0;
    float temp_f, relative_humidity = 0;
    // Initial values for LCD display
    int lcd_fd;
    int current_screen = STATUS_SCREEN;
    int desired_temp = TEMP_DEFAULT_F;
    char lcd_output[34];
    static char previous_lcd_output[34] = "";

    // Timer values
    struct timespec current_time, previous_time, rotary_time;
    clock_gettime(CLOCK_MONOTONIC, &previous_time);
    // Set previous time behind by 2 seconds
    previous_time.tv_sec -= PRINT_INTERVAL_S;
    rotary_time = previous_time;

    // Handle signals
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Open devices
    rotary_fd = open_device("/dev/" ROTARY_DEVICE_NAME, O_RDONLY);
    button_fd = open_device("/dev/" BUTTON_DEVICE_NAME, O_RDONLY);
    bme280_fd = open_device("/dev/" BME280_DEVICE_NAME, O_RDONLY);
    lcd_fd = open_device("/dev/" LCD_DEVICE_NAME, O_WRONLY);

    printf("Climate Control System Started\n");

    while (running)
    {
        // Get current time
        clock_gettime(CLOCK_MONOTONIC, &current_time);

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
                if (previous_rotary_position != ROTARY_POS_UNKNOWN)
                {
                    // Change desired temperature in accordance with rotary switch
                    step_value = rotary_step(previous_rotary_position, current_rotary_position);
                    desired_temp += step_value * TEMP_STEP;
                    // Limit desired temperature to min or max values
                    desired_temp = (desired_temp < TEMP_MIN_F) ? TEMP_MIN_F : 
                                   (desired_temp > TEMP_MAX_F) ? TEMP_MAX_F : desired_temp;
                    current_screen = SET_TEMP_SCREEN;
                    rotary_time = current_time;
                }
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

            if (bme280_fd >= 0)
            {
                temp_status = (ioctl(bme280_fd, BME280_IOC_GET_TEMP, &temp_c) == 0);
                humidity_status = (ioctl(bme280_fd, BME280_IOC_GET_HUMIDITY, &humidity) == 0);

                if (temp_status)
                {
                    temp_f = celcius_to_fahrenheit(temp_c);
                }
                else if (running)
                {
                    perror("temperature ioctl");
                }

                if (!humidity_status && running)
                {
                    perror("humidity ioctl");
                }
                else
                {
                    relative_humidity = humidity / 100.0;
                }
            }
            
            // Print status of all devices
            printf("[status] mode: %-4s | set temp: %d F | actual temp: %.2f F | humidity: %.2f %%\n",
                climate_modes[current_mode], desired_temp, temp_f, relative_humidity);
        }

        // Display on LCD
        if (current_screen == SET_TEMP_SCREEN && ((current_time.tv_sec - rotary_time.tv_sec) >= SET_TEMP_TIMEOUT))
        {
            current_screen = STATUS_SCREEN;
        }
 
        if (current_screen == SET_TEMP_SCREEN)
        {
            format_temp_screen(lcd_output, sizeof(lcd_output), desired_temp);
        }
        else
        {
            format_status_screen(lcd_output, sizeof(lcd_output), (int)temp_f, (int)relative_humidity, current_mode);
        }

        // Only update LCD if output has changed
        if (lcd_fd >= 0 && strcmp(lcd_output, previous_lcd_output) != 0)
        {
            if (write(lcd_fd, lcd_output, strlen(lcd_output)) < 0)
            {
                perror("lcd write");
            }
            else
            {
                strcpy(previous_lcd_output, lcd_output);
            }
        }
        usleep(POLL_INTERVAL_US);
    }

    printf("Stopping Climate Control System\n");

    if (lcd_fd >= 0)
    {
        ioctl(lcd_fd, LCD_IOC_CLEAR);
        close(lcd_fd);
    }
    if (rotary_fd >= 0)
    {
        close(rotary_fd);
    }
    if (button_fd >= 0)
    {
        close(button_fd);
    }
    if (bme280_fd >= 0)
    {
        close(bme280_fd);
    }

    return 0;
}