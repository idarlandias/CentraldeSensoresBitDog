/**
 * BitDogLab v3 - SAFE MODE (Sensors + OLED + Buzzer + LEDs)
 * WiFi Disabled to restore board functionality
 * FIXED: LED Brightness and Buzzer Duration
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hardware/clocks.h"
#include "hardware/i2c.h"
#include "hardware/pwm.h"
#include "pico/stdlib.h"


#include "ssd1306.h"
#include "ws2812.pio.h"

// --- PINOUT ---
#define SDA_OLED 14
#define SCL_OLED 15
#define LED_PIN 7
#define BUZZER_PIN 21
#define MPU6050_ADDR 0x68
#define BH1750_ADDR 0x23
#define OLED_ADDR 0x3C

// --- GLOBALS ---
ssd1306_t disp;
static PIO led_pio = NULL;
static int led_sm = -1;

// --- BITMAPS ---
static const uint8_t BMP_OK[25] = {0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 0, 1,
                                   0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0};
static const uint8_t BMP_FULL[25] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                                     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};

// --- UTILS ---
static void led_draw(const uint8_t *bmp, uint8_t r, uint8_t g, uint8_t b) {
  if (led_pio == NULL)
    return;
  for (int i = 0; i < 25; i++) {
    // WS2812 uses GRB format
    uint32_t color =
        bmp[i] ? (((uint32_t)g << 16) | ((uint32_t)r << 8) | (uint32_t)b) : 0;
    // Shift left for PIO if needed (assuming pio code shifts out MSB first from
    // high)
    pio_sm_put_blocking(led_pio, led_sm, color << 8u);
  }
}

static void led_clear() {
  if (led_pio == NULL)
    return;
  for (int i = 0; i < 25; i++)
    pio_sm_put_blocking(led_pio, led_sm, 0);
}

static void buzzer_init_hw() {
  gpio_set_function(BUZZER_PIN, GPIO_FUNC_PWM);
  uint slice = pwm_gpio_to_slice_num(BUZZER_PIN);
  pwm_set_wrap(slice, 12500);
  pwm_set_chan_level(slice, PWM_CHAN_B, 0);
  pwm_set_enabled(slice, true);
  printf("Buzzer Init: Pin=%d, Slice=%d\n", BUZZER_PIN, slice);
}

static void buzzer_beep(int freq, int ms) {
  uint slice = pwm_gpio_to_slice_num(BUZZER_PIN);
  if (freq > 0) {
    uint32_t clock = 125000000;
    uint32_t div = 16;
    uint32_t wrap = clock / div / freq;
    pwm_set_clkdiv(slice, (float)div);
    pwm_set_wrap(slice, wrap);
    // Set 50% duty cycle on Channel B (GPIO 21 is B)
    pwm_set_chan_level(slice, PWM_CHAN_B, wrap / 2);
  } else {
    pwm_set_chan_level(slice, PWM_CHAN_B, 0);
  }
  sleep_ms(ms);
  pwm_set_chan_level(slice, PWM_CHAN_B, 0);
  sleep_ms(50); // Gap between beeps
}

// --- MAIN ---
int main() {
  stdio_init_all();
  sleep_ms(2000);
  printf("=== BitDogLab SAFE MODE (LEDs + Buzzer FIX) ===\n");

  // Init I2C
  i2c_init(i2c1, 400000); // OLED
  gpio_set_function(SDA_OLED, GPIO_FUNC_I2C);
  gpio_set_function(SCL_OLED, GPIO_FUNC_I2C);
  gpio_pull_up(SDA_OLED);
  gpio_pull_up(SCL_OLED);

  i2c_init(i2c0, 100000); // Sensors
  gpio_set_function(0, GPIO_FUNC_I2C);
  gpio_set_function(1, GPIO_FUNC_I2C);
  gpio_pull_up(0);
  gpio_pull_up(1);

  // Init Peripherals
  ssd1306_init(&disp, 128, 64, OLED_ADDR, i2c1);
  ssd1306_clear(&disp);
  buzzer_init_hw();

  // Init LED Matrix
  led_pio = pio0;
  led_sm = pio_claim_unused_sm(led_pio, true);
  if (led_sm < 0) {
    printf("PIO SM Claim Failed!\n");
  } else {
    uint offset = pio_add_program(led_pio, &ws2812_program);
    ws2812_program_init(led_pio, led_sm, offset, LED_PIN, 800000, false);
    printf("PIO Init Success: SM=%d\n", led_sm);
  }
  led_clear();

  // 🎵 Test Melody
  printf("Playing Melody...\n");
  buzzer_beep(660, 200);
  buzzer_beep(880, 200);
  buzzer_beep(990, 200);

  // 💡 Test LEDs (Bright Green)
  printf("Testing LEDs...\n");
  led_draw(BMP_FULL, 0, 50, 0); // All Green, Brightness 50
  sleep_ms(1000);
  led_draw(BMP_OK, 0, 0, 50); // Blue Check
  sleep_ms(1000);
  led_clear();

  // Initial Display
  ssd1306_draw_string(&disp, 0, 0, 1, "SAFE MODE v2");
  ssd1306_draw_string(&disp, 0, 16, 1, "Testing IO...");
  ssd1306_show(&disp);

  // Wake Sensors
  uint8_t cmd[] = {0x6B, 0x00};
  i2c_write_timeout_us(i2c0, MPU6050_ADDR, cmd, 2, false, 10000);
  uint8_t bh_cmd = 0x10;
  i2c_write_timeout_us(i2c0, BH1750_ADDR, &bh_cmd, 1, false, 10000);

  // Loop
  while (1) {
    // Read Sensors
    uint8_t raw[6];
    uint8_t reg = 0x3B;
    i2c_write_timeout_us(i2c0, MPU6050_ADDR, &reg, 1, true, 5000);
    i2c_read_timeout_us(i2c0, MPU6050_ADDR, raw, 6, false, 5000);
    int ax = (int16_t)((raw[0] << 8) | raw[1]);
    int ay = (int16_t)((raw[2] << 8) | raw[3]);
    int az = (int16_t)((raw[4] << 8) | raw[5]);

    uint8_t lux_raw[2];
    float lux = 0;
    if (i2c_read_timeout_us(i2c0, BH1750_ADDR, lux_raw, 2, false, 5000) == 2) {
      lux = ((lux_raw[0] << 8) | lux_raw[1]) / 1.2f;
    }

    // Display
    char str[32];
    ssd1306_clear(&disp);
    snprintf(str, 32, "Lux: %.1f", lux);
    ssd1306_draw_string(&disp, 0, 0, 1, str);
    snprintf(str, 32, "X:%d Y:%d", ax, ay);
    ssd1306_draw_string(&disp, 0, 16, 1, str);

    // Blink indicator
    static bool tog = false;
    ssd1306_draw_string(&disp, 110, 0, 1, tog ? "*" : " ");

    // Visual Heartbeat on LED Matrix (Center pixel)
    static const uint8_t BMP_DOT[25] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
                                        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    if (tog)
      led_draw(BMP_DOT, 20, 0, 0); // Red Dot
    else
      led_clear();

    tog = !tog;
    ssd1306_show(&disp);

    printf("Lux: %.1f, Accel: %d %d %d\n", lux, ax, ay, az);

    sleep_ms(500);
  }
}