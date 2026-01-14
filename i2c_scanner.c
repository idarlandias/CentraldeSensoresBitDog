/*
 * ====================================================================
 * I2C SCANNER - BitDogLab Full Pin Scan
 * ====================================================================
 * This program scans ALL possible I2C pin combinations and addresses
 * to find exactly where sensors are connected.
 *
 * Results are shown on the OLED display (I2C1 GP14/15)
 * ====================================================================
 */

#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "pico/stdlib.h"
#include "ssd1306.h"
#include <stdio.h>
#include <string.h>


// OLED is always on I2C1 GP14/15
#define OLED_SDA 14
#define OLED_SCL 15
#define OLED_ADDR 0x3C

// All valid I2C pin pairs for RP2040
typedef struct {
  i2c_inst_t *bus;
  uint8_t sda;
  uint8_t scl;
  const char *name;
} i2c_pin_pair_t;

// List of all possible I2C configurations
const i2c_pin_pair_t pin_configs[] = {
    // I2C0 options
    {i2c0, 0, 1, "I2C0 GP0/1"},
    {i2c0, 4, 5, "I2C0 GP4/5"},
    {i2c0, 8, 9, "I2C0 GP8/9"},
    {i2c0, 12, 13, "I2C0 GP12/13"},
    {i2c0, 16, 17, "I2C0 GP16/17"},
    {i2c0, 20, 21, "I2C0 GP20/21"},
    // I2C1 options (skip 14/15 - reserved for OLED)
    {i2c1, 2, 3, "I2C1 GP2/3"},
    {i2c1, 6, 7, "I2C1 GP6/7"},
    {i2c1, 10, 11, "I2C1 GP10/11"},
    {i2c1, 18, 19, "I2C1 GP18/19"},
    {i2c1, 26, 27, "I2C1 GP26/27"},
};
#define NUM_CONFIGS (sizeof(pin_configs) / sizeof(pin_configs[0]))

// Found devices storage
#define MAX_FOUND 16
typedef struct {
  uint8_t addr;
  uint8_t config_idx;
} found_device_t;

found_device_t found_devices[MAX_FOUND];
int found_count = 0;

ssd1306_t disp;

// Restore OLED pins
void restore_oled() {
  gpio_set_function(OLED_SDA, GPIO_FUNC_I2C);
  gpio_set_function(OLED_SCL, GPIO_FUNC_I2C);
  gpio_pull_up(OLED_SDA);
  gpio_pull_up(OLED_SCL);
  i2c_init(i2c1, 400000);
}

// Scan a specific pin configuration
void scan_config(uint8_t config_idx) {
  const i2c_pin_pair_t *cfg = &pin_configs[config_idx];

  // Skip if using OLED pins
  if (cfg->sda == OLED_SDA || cfg->scl == OLED_SCL)
    return;

  // Configure pins
  gpio_set_function(cfg->sda, GPIO_FUNC_I2C);
  gpio_set_function(cfg->scl, GPIO_FUNC_I2C);
  gpio_pull_up(cfg->sda);
  gpio_pull_up(cfg->scl);
  i2c_init(cfg->bus, 100000);

  // Scan all addresses
  for (uint8_t addr = 0x08; addr < 0x78; addr++) {
    if (addr == OLED_ADDR)
      continue; // Skip OLED address

    uint8_t rxdata;
    int ret = i2c_read_timeout_us(cfg->bus, addr, &rxdata, 1, false, 5000);

    if (ret >= 0 && found_count < MAX_FOUND) {
      found_devices[found_count].addr = addr;
      found_devices[found_count].config_idx = config_idx;
      found_count++;
    }
  }

  // Reset pins to input (high-Z)
  gpio_set_function(cfg->sda, GPIO_FUNC_SIO);
  gpio_set_function(cfg->scl, GPIO_FUNC_SIO);
  gpio_set_dir(cfg->sda, GPIO_IN);
  gpio_set_dir(cfg->scl, GPIO_IN);
}

void update_display_scanning(int config_idx) {
  restore_oled();
  ssd1306_clear(&disp);
  ssd1306_draw_string(&disp, 0, 0, 1, "=== I2C SCANNER ===");

  char buf[32];
  snprintf(buf, sizeof(buf), "Scanning %d/%d", config_idx + 1, NUM_CONFIGS);
  ssd1306_draw_string(&disp, 0, 15, 1, buf);
  ssd1306_draw_string(&disp, 0, 30, 1, pin_configs[config_idx].name);

  snprintf(buf, sizeof(buf), "Found: %d devices", found_count);
  ssd1306_draw_string(&disp, 0, 50, 1, buf);
  ssd1306_show(&disp);
}

void show_results() {
  restore_oled();
  ssd1306_clear(&disp);

  if (found_count == 0) {
    ssd1306_draw_string(&disp, 0, 0, 1, "=== SCAN COMPLETE ===");
    ssd1306_draw_string(&disp, 0, 25, 1, "NO DEVICES FOUND!");
    ssd1306_draw_string(&disp, 0, 45, 1, "Check wiring");
  } else {
    ssd1306_draw_string(&disp, 0, 0, 1, "=== DEVICES FOUND ===");

    int y = 12;
    for (int i = 0; i < found_count && y < 60; i++) {
      char buf[32];
      const i2c_pin_pair_t *cfg = &pin_configs[found_devices[i].config_idx];

      // Identify device type
      const char *type = "???";
      if (found_devices[i].addr == 0x68)
        type = "MPU6050";
      else if (found_devices[i].addr == 0x6B)
        type = "MPU/LSM";
      else if (found_devices[i].addr == 0x23)
        type = "BH1750";
      else if (found_devices[i].addr == 0x5C)
        type = "BH1750";
      else if (found_devices[i].addr == 0x76)
        type = "BME280";
      else if (found_devices[i].addr == 0x77)
        type = "BMP280";

      snprintf(buf, sizeof(buf), "0x%02X %s", found_devices[i].addr, type);
      ssd1306_draw_string(&disp, 0, y, 1, buf);

      snprintf(buf, sizeof(buf), " GP%d/%d", cfg->sda, cfg->scl);
      ssd1306_draw_string(&disp, 70, y, 1, buf);

      y += 10;
    }
  }
  ssd1306_show(&disp);
}

int main() {
  stdio_init_all();
  sleep_ms(2000);
  printf("\n=== I2C SCANNER START ===\n");

  // Initialize OLED first
  restore_oled();
  ssd1306_init(&disp, 128, 64, OLED_ADDR, i2c1);
  ssd1306_clear(&disp);
  ssd1306_draw_string(&disp, 0, 0, 1, "=== I2C SCANNER ===");
  ssd1306_draw_string(&disp, 0, 25, 1, "Starting scan...");
  ssd1306_show(&disp);
  sleep_ms(1000);

  // Scan all configurations
  for (int i = 0; i < NUM_CONFIGS; i++) {
    update_display_scanning(i);
    printf("Scanning: %s\n", pin_configs[i].name);
    scan_config(i);
    sleep_ms(100);
  }

  // Show results
  printf("\n=== RESULTS ===\n");
  for (int i = 0; i < found_count; i++) {
    const i2c_pin_pair_t *cfg = &pin_configs[found_devices[i].config_idx];
    printf("Found: 0x%02X on %s\n", found_devices[i].addr, cfg->name);
  }
  printf("Total: %d devices\n", found_count);

  show_results();

  // Blink LED to indicate complete
  while (1) {
    sleep_ms(2000);
  }

  return 0;
}
