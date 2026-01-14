/*
 * DIAGNÓSTICO MÍNIMO - BitDogLab
 * Apenas pisca o LED onboard para verificar se o hardware está OK
 */

#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"


int main() {
  stdio_init_all();

  // Inicializa WiFi chip apenas para usar o LED onboard (GPIO ligado ao chip
  // WiFi)
  if (cyw43_arch_init()) {
    // Se falhar, fica em loop infinito
    while (1) {
      tight_loop_contents();
    }
  }

  // Pisca o LED onboard indefinidamente
  while (1) {
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
    sleep_ms(250);
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
    sleep_ms(250);
  }

  return 0;
}
