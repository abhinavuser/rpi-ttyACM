#include "core/system.h"
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>

#define LED_PORT GPIOB
#define LED_PIN  GPIO3
#define LED_RCC  RCC_GPIOB

static void gpio_setup(void) {
    rcc_periph_clock_enable(LED_RCC);
    gpio_mode_setup(LED_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, LED_PIN);
}

static void delay_ms(uint32_t milliseconds) {
    uint32_t start = system_millis;
    while ((uint32_t)(system_millis - start) < milliseconds);
}

int main(void) {
    system_setup();
    gpio_setup();

    while (1) {
        gpio_toggle(LED_PORT, LED_PIN);
        delay_ms(500);
    }
}
