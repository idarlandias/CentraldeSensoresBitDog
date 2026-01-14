/*
 * ====================================================================
 * SENSOR TEST - No FreeRTOS (Debug Version)
 * ====================================================================
 * Simple program to test sensor reading without FreeRTOS
 * Based on working i2c_scanner config
 * ====================================================================
 */

#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "pico/stdlib.h"
#include "ssd1306.h"
#include <stdio.h>
#include <string.h>


// OLED on I2C1 GP14/15
#define OLED_SDA 14
#define OLED_SCL 15
#define OLED_ADDR 0x3C

// Sensors on I2C0 GP0/1 (Scanner confirmed!)
#define SENSOR_SDA 0
#define SENSOR_SCL 1
#define MPU6050_ADDR 0x68
#define BH1750_ADDR 0x23

ssd1306_t disp;

void init_i2c_buses() {
  // I2C1 for OLED
  i2c_init(i2c1, 400000);
  gpio_set_function(OLED_SDA, GPIO_FUNC_I2C);
  gpio_set_function(OLED_SCL, GPIO_FUNC_I2C);
  gpio_pull_up(OLED_SDA);
  gpio_pull_up(OLED_SCL);

  // I2C0 for Sensors
  i2c_init(i2c0, 100000);
  gpio_set_function(SENSOR_SDA, GPIO_FUNC_I2C);
  gpio_set_function(SENSOR_SCL, GPIO_FUNC_I2C);
  gpio_pull_up(SENSOR_SDA);
  gpio_pull_up(SENSOR_SCL);
}

void mpu6050_wake() {
  uint8_t cmd[] = {0x6B, 0x00}; // PWR_MGMT_1 = 0 (wake up)
  i2c_write_timeout_us(i2c0, MPU6050_ADDR, cmd, 2, false, 10000);
}

void bh1750_init() {
  uint8_t cmd = 0x10; // Continuous High Res Mode
  i2c_write_timeout_us(i2c0, BH1750_ADDR, &cmd, 1, false, 10000);
}

int main() {
  stdio_init_all();
  sleep_ms(2000);
  printf("\n=== SENSOR TEST (No FreeRTOS) ===\n");

  // Init I2C buses
  init_i2c_buses();

  // Init OLED
  ssd1306_init(&disp, 128, 64, OLED_ADDR, i2c1);
  ssd1306_clear(&disp);
  ssd1306_draw_string(&disp, 0, 0, 1, "SENSOR TEST");
  ssd1306_draw_string(&disp, 0, 15, 1, "No FreeRTOS");
  ssd1306_show(&disp);
  sleep_ms(1000);

  // Wake up MPU6050
  ssd1306_draw_string(&disp, 0, 30, 1, "Waking MPU...");
  ssd1306_show(&disp);
  mpu6050_wake();
  sleep_ms(100);

  // Init BH1750
  ssd1306_draw_string(&disp, 0, 45, 1, "Init BH1750...");
  ssd1306_show(&disp);
  bh1750_init();
  sleep_ms(200); // BH1750 needs time to take first reading

  // Main loop
  int count = 0;
  while (1) {
    count++;

    // Read MPU6050 acceleration
    int16_t ax = 0, ay = 0, az = 0;
    uint8_t accel_reg = 0x3B;
    uint8_t accel_buf[6];

    i2c_write_timeout_us(i2c0, MPU6050_ADDR, &accel_reg, 1, true, 5000);
    int ret =
        i2c_read_timeout_us(i2c0, MPU6050_ADDR, accel_buf, 6, false, 5000);

    if (ret == 6) {
      ax = (int16_t)((accel_buf[0] << 8) | accel_buf[1]);
      ay = (int16_t)((accel_buf[2] << 8) | accel_buf[3]);
      az = (int16_t)((accel_buf[4] << 8) | accel_buf[5]);
    }

    // Read BH1750 light
    float lux = 0;
    uint8_t lux_buf[2];
    if (i2c_read_timeout_us(i2c0, BH1750_ADDR, lux_buf, 2, false, 5000) == 2) {
      lux = ((lux_buf[0] << 8) | lux_buf[1]) / 1.2f;
    }

    // Update display
    ssd1306_clear(&disp);
    char buf[32];

    snprintf(buf, sizeof(buf), "Lux: %.1f #%d", lux, count);
    ssd1306_draw_string(&disp, 0, 0, 1, buf);

    snprintf(buf, sizeof(buf), "AX: %d", ax);
    ssd1306_draw_string(&disp, 0, 15, 1, buf);

    snprintf(buf, sizeof(buf), "AY: %d", ay);
    ssd1306_draw_string(&disp, 0, 30, 1, buf);

    snprintf(buf, sizeof(buf), "AZ: %d", az);
    ssd1306_draw_string(&disp, 0, 45, 1, buf);

    ssd1306_show(&disp);

    // Debug output
    printf("Loop %d: Lux=%.1f, AX=%d, AY=%d, AZ=%d\n", count, lux, ax, ay, az);

    sleep_ms(500); // 2Hz update
  }

  return 0;
}
