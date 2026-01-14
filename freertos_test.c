/*
 * ====================================================================
 * FREERTOS MINIMAL TEST - No WiFi, Just Sensor Task
 * ====================================================================
 * Tests if FreeRTOS scheduler works at all
 * Based on working sensor_test code
 * ====================================================================
 */

#include "hardware/i2c.h"
#include "pico/stdlib.h"
#include <stdio.h>


// FreeRTOS
#include "FreeRTOS.h"
#include "task.h"

// OLED Driver
#include "ssd1306.h"

// Hardware config
#define OLED_SDA 14
#define OLED_SCL 15
#define OLED_ADDR 0x3C
#define MPU6050_ADDR 0x68
#define BH1750_ADDR 0x23

ssd1306_t disp;

// Single task - no WiFi
void vSensorOnlyTask(void *pvParameters) {
  printf("[RTOS] Sensor-only task started\n");

  // Init I2C0 for sensors
  i2c_init(i2c0, 100000);
  gpio_set_function(0, GPIO_FUNC_I2C);
  gpio_set_function(1, GPIO_FUNC_I2C);
  gpio_pull_up(0);
  gpio_pull_up(1);

  // Init I2C1 for OLED
  i2c_init(i2c1, 400000);
  gpio_set_function(14, GPIO_FUNC_I2C);
  gpio_set_function(15, GPIO_FUNC_I2C);
  gpio_pull_up(14);
  gpio_pull_up(15);

  // Init OLED
  ssd1306_init(&disp, 128, 64, OLED_ADDR, i2c1);
  ssd1306_clear(&disp);
  ssd1306_draw_string(&disp, 0, 0, 1, "RTOS TEST");
  ssd1306_draw_string(&disp, 0, 15, 1, "Task running!");
  ssd1306_show(&disp);

  // Wake MPU6050
  uint8_t wake_cmd[] = {0x6B, 0x00};
  i2c_write_timeout_us(i2c0, MPU6050_ADDR, wake_cmd, 2, false, 10000);

  // Main loop - using vTaskDelay
  int count = 0;
  while (1) {
    count++;

    // Read MPU6050
    int16_t ax = 0, ay = 0, az = 0;
    uint8_t accel_reg = 0x3B;
    uint8_t buf[6];
    i2c_write_timeout_us(i2c0, MPU6050_ADDR, &accel_reg, 1, true, 5000);
    if (i2c_read_timeout_us(i2c0, MPU6050_ADDR, buf, 6, false, 5000) == 6) {
      ax = (int16_t)((buf[0] << 8) | buf[1]);
      ay = (int16_t)((buf[2] << 8) | buf[3]);
      az = (int16_t)((buf[4] << 8) | buf[5]);
    }

    // Read BH1750
    float lux = 0;
    uint8_t lux_buf[2];
    if (i2c_read_timeout_us(i2c0, BH1750_ADDR, lux_buf, 2, false, 5000) == 2) {
      lux = ((lux_buf[0] << 8) | lux_buf[1]) / 1.2f;
    }

    // Update display
    ssd1306_clear(&disp);
    char line[32];

    snprintf(line, sizeof(line), "RTOS OK #%d", count);
    ssd1306_draw_string(&disp, 0, 0, 1, line);

    snprintf(line, sizeof(line), "Lux: %.1f", lux);
    ssd1306_draw_string(&disp, 0, 15, 1, line);

    snprintf(line, sizeof(line), "AX:%d AY:%d", ax, ay);
    ssd1306_draw_string(&disp, 0, 30, 1, line);

    snprintf(line, sizeof(line), "AZ:%d", az);
    ssd1306_draw_string(&disp, 0, 45, 1, line);

    ssd1306_show(&disp);

    printf("Loop %d: Lux=%.1f, AX=%d\n", count, lux, ax);

    // FreeRTOS delay
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

// Static allocation hooks
void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
                                   StackType_t **ppxIdleTaskStackBuffer,
                                   uint32_t *pulIdleTaskStackSize) {
  static StaticTask_t xIdleTaskTCB;
  static StackType_t uxIdleTaskStack[configMINIMAL_STACK_SIZE];
  *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;
  *ppxIdleTaskStackBuffer = uxIdleTaskStack;
  *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer,
                                    StackType_t **ppxTimerTaskStackBuffer,
                                    uint32_t *pulTimerTaskStackSize) {
  static StaticTask_t xTimerTaskTCB;
  static StackType_t uxTimerTaskStack[configTIMER_TASK_STACK_DEPTH];
  *ppxTimerTaskTCBBuffer = &xTimerTaskTCB;
  *ppxTimerTaskStackBuffer = uxTimerTaskStack;
  *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}

void vApplicationStackOverflowHook(TaskHandle_t pxTask, char *pcTaskName) {
  printf("!!! STACK OVERFLOW: %s !!!\n", pcTaskName);
  while (1)
    ;
}

int main() {
  stdio_init_all();
  sleep_ms(2000);
  printf("\n=== FREERTOS MINIMAL TEST ===\n");

  // Create single task
  xTaskCreate(vSensorOnlyTask, "SensorOnly", 4096, NULL, 1, NULL);

  // Start scheduler
  printf("Starting scheduler...\n");
  vTaskStartScheduler();

  // Should never reach here
  printf("ERROR: Scheduler exited!\n");
  while (1)
    ;

  return 0;
}
