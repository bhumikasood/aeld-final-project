/**
 * @file temp_sensor.h
 * @author Bhumika Sood
 * @brief Device structure for the BME280 temperature sensor driver
 */
 
#ifndef TEMP_SENSOR_H
#define TEMP_SENSOR_H

#undef PDEBUG
#ifdef __KERNEL__
#define PDEBUG(fmt, args...) printk(KERN_DEBUG "temp_sensor: " fmt "\n", ## args)
#else
#define PDEBUG(fmt, args...) fprintf(stderr, fmt, ## args)
#endif

// Defined in the Adafruit BME280 Library
typedef struct 
{
    uint16_t dig_T1; 
    int16_t dig_T2;  
    int16_t dig_T3;  
    
    uint16_t dig_P1; 
    int16_t dig_P2;  
    int16_t dig_P3;  
    int16_t dig_P4;  
    int16_t dig_P5;  
    int16_t dig_P6;  
    int16_t dig_P7;  
    int16_t dig_P8;  
    int16_t dig_P9;  

    uint8_t dig_H1; 
    int16_t dig_H2; 
    uint8_t dig_H3; 
    int16_t dig_H4; 
    int16_t dig_H5; 
    int8_t dig_H6;  
} bme280_calib_data;

struct temp_sensor_dev
{
    struct cdev cdev;
    struct i2c_client *client;
    bme280_calib_data temp_calibration;
};

#endif /* TEMP_SENSOR_H */
