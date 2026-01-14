/**
 * BitDogLab v3 - CONNECTED MODE (WiFi + Sensors + Safe Peripherals)
 * Architecture: lwip_threadsafe_background (Polling-Free WiFi)
 * FIXED: Power Save Disabled + Connection Retries
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hardware/clocks.h"
#include "hardware/i2c.h"
#include "hardware/pwm.h"
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"


#include "lwip/ip_addr.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"


#include "ssd1306.h"
#include "ws2812.pio.h"

// --- CONFIGURATION ---
#define WIFI_SSID "@idarlan"
#define WIFI_PASSWORD "pf255181"
#define SERVER_IP "192.168.1.11"
#define SERVER_PORT 5001
#define HTTP_PATH "/api/sensor"

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
static volatile bool wifi_connected = false;

// --- BITMAPS ---
static const uint8_t BMP_OK[25] = {0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 0, 1,
                                   0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0};
static const uint8_t BMP_FAIL[25] = {1, 0, 0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 1,
                                     0, 0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 1};

// --- UTILS ---
static void led_draw(const uint8_t *bmp, uint8_t r, uint8_t g, uint8_t b) {
  if (led_pio == NULL)
    return;
  for (int i = 0; i < 25; i++) {
    uint32_t color =
        bmp[i] ? (((uint32_t)g << 16) | ((uint32_t)r << 8) | (uint32_t)b) : 0;
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
}

static void buzzer_beep(int freq, int ms) {
  uint slice = pwm_gpio_to_slice_num(BUZZER_PIN);
  if (freq > 0) {
    uint32_t clock = 125000000;
    uint32_t div = 16;
    uint32_t wrap = clock / div / freq;
    pwm_set_clkdiv(slice, (float)div);
    pwm_set_wrap(slice, wrap);
    pwm_set_chan_level(slice, PWM_CHAN_B, wrap / 2);
  } else {
    pwm_set_chan_level(slice, PWM_CHAN_B, 0);
  }
  sleep_ms(ms);
  pwm_set_chan_level(slice, PWM_CHAN_B, 0);
  sleep_ms(50);
}

// --- HTTP CLIENT ---
static volatile bool http_completed = false;

static err_t on_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p,
                     err_t err) {
  if (!p) {
    tcp_close(pcb);
    http_completed = true;
    return ERR_OK;
  }
  pbuf_free(p);
  return ERR_OK;
}

static err_t on_connect(void *arg, struct tcp_pcb *pcb, err_t err) {
  if (err != ERR_OK) {
    http_completed = true;
    return err;
  }

  char *json = (char *)arg;
  char req[512];
  int len = snprintf(req, sizeof(req),
                     "POST %s HTTP/1.1\r\n"
                     "Host: %s\r\n"
                     "Content-Type: application/json\r\n"
                     "Content-Length: %d\r\n"
                     "Connection: close\r\n\r\n%s",
                     HTTP_PATH, SERVER_IP, (int)strlen(json), json);

  tcp_write(pcb, req, len, TCP_WRITE_FLAG_COPY);
  tcp_output(pcb);
  free(json);
  return ERR_OK;
}

void send_data(float lux, int ax, int ay, int az) {
  if (!wifi_connected)
    return;

  // Use cyw43_arch_lwip_begin/end for thread safety with background
  // architecture
  cyw43_arch_lwip_begin();

  char *json = malloc(256);
  if (!json) {
    cyw43_arch_lwip_end();
    return;
  }
  snprintf(json, 256, "{\"lux\":%.2f,\"accel\":{\"x\":%d,\"y\":%d,\"z\":%d}}",
           lux, ax, ay, az);

  struct tcp_pcb *pcb = tcp_new_ip_type(IPADDR_TYPE_V4);
  if (!pcb) {
    free(json);
    cyw43_arch_lwip_end();
    return;
  }

  tcp_arg(pcb, json);
  tcp_recv(pcb, on_recv);

  ip_addr_t ip;
  ipaddr_aton(SERVER_IP, &ip);

  // Note: tcp_connect in lwIP is non-blocking usually, but in raw mode it
  // registers callback
  if (tcp_connect(pcb, &ip, SERVER_PORT, on_connect) != ERR_OK) {
    free(json);
    tcp_close(pcb);
  }

  cyw43_arch_lwip_end();
}

// --- MAIN ---
int main() {
  stdio_init_all();
  sleep_ms(2000);
  printf("=== BitDogLab CONNECTED MODE v2 (No Powersave) ===\n");

  // Init I2C
  i2c_init(i2c1, 400000);
  gpio_set_function(SDA_OLED, GPIO_FUNC_I2C);
  gpio_set_function(SCL_OLED, GPIO_FUNC_I2C);
  gpio_pull_up(SDA_OLED);
  gpio_pull_up(SCL_OLED);

  i2c_init(i2c0, 100000);
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
  uint offset = pio_add_program(led_pio, &ws2812_program);
  ws2812_program_init(led_pio, led_sm, offset, LED_PIN, 800000, false);
  led_clear();

  // 🎵 Startup Melody
  buzzer_beep(660, 100);
  buzzer_beep(880, 100);

  // 📡 WiFi Init
  printf("Initializing WiFi...\n");
  ssd1306_draw_string(&disp, 0, 0, 1, "Init WiFi...");
  ssd1306_show(&disp);

  if (cyw43_arch_init_with_country(CYW43_COUNTRY_BRAZIL)) {
    printf("WiFi Init FAILED\n");
    ssd1306_draw_string(&disp, 0, 16, 1, "WiFi FAIL!");
    led_draw(BMP_FAIL, 20, 0, 0); // Red Cross
    buzzer_beep(200, 500);
    ssd1306_show(&disp);
  } else {
    printf("WiFi Init OK.\n");

    // CRITICAL: Disable Power Save Mode
    cyw43_wifi_pm(&cyw43_state,
                  cyw43_pm_value(CYW43_NO_POWERSAVE_MODE, 20, 1, 1, 1));
    printf("Power Save DISABLED\n");

    cyw43_arch_enable_sta_mode();

    // Retry Loop
    bool success = false;
    for (int i = 0; i < 3; i++) {
      printf("Connecting... Attempt %d/3\n", i + 1);
      char msg[20];
      snprintf(msg, 20, "Try %d/3...", i + 1);
      ssd1306_draw_string(&disp, 0, 16, 1, msg);
      ssd1306_show(&disp);

      if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD,
                                             CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        printf("Attempt %d Failed.\n", i + 1);
        ssd1306_draw_string(&disp, 0, 16, 1, "Failed.     "); // Overwrite
      } else {
        success = true;
        break;
      }
      sleep_ms(1000);
    }

    if (!success) {
      printf("WiFi Connect FAILED (All Attempts)\n");
      ssd1306_draw_string(&disp, 0, 32, 1, "Net ERR");
      led_draw(BMP_FAIL, 20, 0, 0);
      buzzer_beep(200, 1000); // Long fail beep
    } else {
      printf("WiFi CONNECTED: %s\n", ip4addr_ntoa(netif_ip4_addr(netif_list)));
      char wifimsg[32];
      snprintf(wifimsg, 32, "IP:%s", ip4addr_ntoa(netif_ip4_addr(netif_list)));

      ssd1306_clear(&disp);
      ssd1306_draw_string(&disp, 0, 0, 1, "Connected!");
      ssd1306_draw_string(&disp, 0, 16, 1, wifimsg);

      wifi_connected = true;
      led_draw(BMP_OK, 0, 0, 50); // Blue check
      buzzer_beep(1000, 100);
      sleep_ms(100);
      buzzer_beep(1000, 100); // Double beep
      sleep_ms(2000);
      led_clear();
    }
  }
  ssd1306_show(&disp);
  sleep_ms(1000);

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

    // WiFi Status
    if (wifi_connected)
      ssd1306_draw_string(&disp, 100, 0, 1, "W");

    // Visual Heartbeat
    static bool tog = false;
    static const uint8_t BMP_DOT[25] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
                                        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    if (tog)
      led_draw(BMP_DOT, 10, 0, 0);
    else
      led_clear();
    tog = !tog;

    ssd1306_show(&disp);

    // Send Data
    if (wifi_connected) {
      send_data(lux, ax, ay, az);
    }

    printf("Lux: %.1f, Accel: %d %d %d\n", lux, ax, ay, az);

    sleep_ms(2000);
  }
}