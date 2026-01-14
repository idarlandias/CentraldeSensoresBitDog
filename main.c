/**
 * BitDogLab v3 - FreeRTOS EDITION
 * Architecture: lwip_sys_freertos + Multi-Tasking
 * Status: Validated Drivers & WiFi Logic ported from Super-Loop
 * FIXED: Wrapper Functions for Logic
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hardware/adc.h"
#include "hardware/i2c.h"
#include "hardware/pwm.h"
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"

#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"

#include "lwip/api.h"
#include "lwip/sockets.h"

#include "ssd1306.h"
#include "ws2812.pio.h"

// --- CONFIGURATION ---
#define WIFI_SSID "@idarlan"
#define WIFI_PASSWORD "pf255181"
#define SERVER_IP "192.168.1.11"
#define SERVER_PORT 5001
#define HTTP_PATH "/submit_data"

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
QueueHandle_t xSensorQueue;

typedef struct {
  float lux;
  float temp_chip;
  int ax, ay, az;
  int32_t rssi;
  uint32_t uptime_sec;
} sensor_data_t;

static const uint8_t BMP_OK[25] = {0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 0, 1,
                                   0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0};

SemaphoreHandle_t xOledMutex;

static void safe_oled_print(const char *line1, const char *line2,
                            const char *line3, const char *line4) {
  if (xOledMutex && xSemaphoreTake(xOledMutex, portMAX_DELAY) == pdTRUE) {
    ssd1306_clear(&disp);
    if (line1)
      ssd1306_draw_string(&disp, 0, 0, 1, line1);
    if (line2)
      ssd1306_draw_string(&disp, 0, 16, 1, line2);
    if (line3)
      ssd1306_draw_string(&disp, 0, 32, 1, line3);
    if (line4)
      ssd1306_draw_string(&disp, 0, 48, 1, line4);
    ssd1306_show(&disp);
    xSemaphoreGive(xOledMutex);
  }
}

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

  if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
    vTaskDelay(pdMS_TO_TICKS(ms));
    pwm_set_chan_level(slice, PWM_CHAN_B, 0);
    vTaskDelay(pdMS_TO_TICKS(50));
  } else {
    sleep_ms(ms);
    pwm_set_chan_level(slice, PWM_CHAN_B, 0);
    sleep_ms(50);
  }
}

// --- TASKS ---
void vSensorTask(void *pvParameters) {
  uint8_t cmd[] = {0x6B, 0x00};
  i2c_write_timeout_us(i2c0, MPU6050_ADDR, cmd, 2, false, 10000);
  uint8_t bh_cmd = 0x10;
  i2c_write_timeout_us(i2c0, BH1750_ADDR, &bh_cmd, 1, false, 10000);
  sensor_data_t data;
  printf("SensorTask Started\n");
  while (1) {
    printf("Reading I2C...\n");
    uint8_t raw[6];
    uint8_t reg = 0x3B;
    int ret = i2c_write_timeout_us(i2c0, MPU6050_ADDR, &reg, 1, true, 5000);
    if (ret < 0)
      printf("MPU Write Fail: %d\n", ret);

    i2c_read_timeout_us(i2c0, MPU6050_ADDR, raw, 6, false, 5000);
    data.ax = (int16_t)((raw[0] << 8) | raw[1]);
    data.ay = (int16_t)((raw[2] << 8) | raw[3]);
    data.az = (int16_t)((raw[4] << 8) | raw[5]);
    uint8_t lux_raw[2];
    if (i2c_read_timeout_us(i2c0, BH1750_ADDR, lux_raw, 2, false, 5000) == 2) {
      data.lux = ((lux_raw[0] << 8) | lux_raw[1]) / 1.2f;
    } else {
      data.lux = 0;
    }

    // Internal Temperature (ADC Channel 4)
    adc_select_input(4);
    float adc_voltage = adc_read() * 3.3f / (1 << 12);
    data.temp_chip = 27.0f - (adc_voltage - 0.706f) / 0.001721f;

    // RSSI (WiFi Signal Strength)
    cyw43_wifi_get_rssi(&cyw43_state, &data.rssi);

    // Uptime
    data.uptime_sec = xTaskGetTickCount() / configTICK_RATE_HZ;

    printf("Lux: %.2f Temp: %.1fC RSSI: %ld Uptime: %lus\\n", data.lux,
           data.temp_chip, data.rssi, data.uptime_sec);

    xQueueSend(xSensorQueue, &data, 0);
    char s1[32], s2[32], s3[32], s4[32];
    snprintf(s1, 32, "WiFi: %s", WIFI_SSID);
    snprintf(s2, 32, "IP:%s", ip4addr_ntoa(netif_ip4_addr(netif_list)));
    snprintf(s3, 32, "Lux:%.0f T:%.1fC", data.lux, data.temp_chip);
    snprintf(s4, 32, "RSSI:%ld Up:%lus", data.rssi, data.uptime_sec);
    safe_oled_print(s1, s2, s3, s4);
    static bool tog = false;
    static const uint8_t BMP_DOT[25] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
                                        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    if (tog)
      led_draw(BMP_DOT, 10, 0, 0);
    else
      led_clear();
    tog = !tog;
    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}

void vWifiTask(void *pvParameters) {
  sensor_data_t data;
  char buffer[256];
  printf("WiFiTask Started\n");
  while (1) {
    if (xQueueReceive(xSensorQueue, &data, portMAX_DELAY) == pdTRUE) {
      printf("WiFi: Got data from queue (Lux: %.1f)\n", data.lux);
      snprintf(buffer, 256,
               "{\"lux\":%.2f,\"temp\":%.2f,\"rssi\":%ld,\"uptime\":%lu,"
               "\"accel\":{\"x\":%d,\"y\":%d,\"z\":%d}}",
               data.lux, data.temp_chip, data.rssi, data.uptime_sec, data.ax,
               data.ay, data.az);
      int sock = socket(AF_INET, SOCK_STREAM, 0);
      if (sock < 0) {
        printf("WiFi: Socket creation failed\n");
        continue;
      }
      printf("WiFi: Socket created (%d)\n", sock);
      struct sockaddr_in server_addr;
      server_addr.sin_family = AF_INET;
      server_addr.sin_port = htons(SERVER_PORT);
      inet_aton(SERVER_IP, &server_addr.sin_addr);
      printf("WiFi: Connecting to %s:%d...\n", SERVER_IP, SERVER_PORT);
      if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) <
          0) {
        printf("WiFi: Connect failed\n");
        lwip_close(sock);
        continue;
      }
      printf("WiFi: Connected! Sending...\n");
      char req[512];
      int len = snprintf(req, sizeof(req),
                         "POST %s HTTP/1.1\r\nHost: %s\r\nContent-Type: "
                         "application/json\r\nContent-Length: "
                         "%d\r\nConnection: close\r\n\r\n%s",
                         HTTP_PATH, SERVER_IP, (int)strlen(buffer), buffer);
      send(sock, req, len, 0);
      char rx_buf[128];
      recv(sock, rx_buf, sizeof(rx_buf) - 1, 0);
      lwip_close(sock);
      printf("WiFi: Data Sent OK\n");
    }
  }
}

void vMainTask(void *pvParameters) {
  safe_oled_print("FreeRTOS Mode", "Init WiFi...", NULL, NULL);
  if (cyw43_arch_init_with_country(CYW43_COUNTRY_BRAZIL)) {
    safe_oled_print("WiFi Fail", NULL, NULL, NULL);
    vTaskDelete(NULL);
  }
  cyw43_wifi_pm(&cyw43_state,
                cyw43_pm_value(CYW43_NO_POWERSAVE_MODE, 20, 1, 1, 1));
  cyw43_arch_enable_sta_mode();
  safe_oled_print("Connecting...", NULL, NULL, NULL);
  while (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD,
                                            CYW43_AUTH_WPA2_AES_PSK, 30000)) {
    safe_oled_print("Retrying...", NULL, NULL, NULL);
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
  safe_oled_print("Connected!", ip4addr_ntoa(netif_ip4_addr(netif_list)), NULL,
                  NULL);
  buzzer_beep(1000, 200);
  led_draw(BMP_OK, 0, 0, 50);
  vTaskDelay(pdMS_TO_TICKS(1000));
  led_clear();
  xSensorQueue = xQueueCreate(5, sizeof(sensor_data_t));
  xTaskCreate(vSensorTask, "Sensor", 2048, NULL, 4, NULL);
  xTaskCreate(vWifiTask, "WiFi", 2048, NULL, 3, NULL);
  vTaskDelete(NULL);
}

// Wrapper Functions
extern void vPortSVCHandler(void);
extern void xPortPendSVHandler(void);
extern void xPortSysTickHandler(void);

void SVC_Handler(void) { vPortSVCHandler(); }
void PendSV_Handler(void) { xPortPendSVHandler(); }
void SysTick_Handler(void) { xPortSysTickHandler(); }

void vAssertCalled(void) {
  taskDISABLE_INTERRUPTS();
  while (1)
    ;
}

int main() {
  stdio_init_all();
  sleep_ms(2000);
  printf("=== BitDogLab FreeRTOS BOOT ===\n");
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
  ssd1306_init(&disp, 128, 64, OLED_ADDR, i2c1);
  ssd1306_clear(&disp);

  // Initialize ADC for internal temperature
  adc_init();
  adc_set_temp_sensor_enabled(true);

  buzzer_init_hw();
  led_pio = pio0;
  led_sm = pio_claim_unused_sm(led_pio, true);
  uint offset = pio_add_program(led_pio, &ws2812_program);
  ws2812_program_init(led_pio, led_sm, offset, LED_PIN, 800000, false);
  led_clear();
  xOledMutex = xSemaphoreCreateMutex();
  buzzer_beep(660, 100);
  buzzer_beep(880, 100);

  xTaskCreate(vMainTask, "MainInit", 2048, NULL, 1, NULL);
  vTaskStartScheduler();
  while (1)
    ;
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
  while (1)
    ;
}
void vApplicationMallocFailedHook(void) {
  while (1)
    ;
}