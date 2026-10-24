#include "core/system.h"
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/cm3/nvic.h>

void tim7_isr(void) {
    if (timer_get_flag(TIM7, TIM_SR_UIF)) {
        timer_clear_flag(TIM7, TIM_SR_UIF);
        gpio_toggle(GPIOB, GPIO3);
        gpio_toggle(GPIOF, GPIO0);
        gpio_toggle(GPIOA, GPIO1);
    }
}

static void tdr_pulse_setup(void) {
    rcc_periph_clock_enable(RCC_GPIOB);
    rcc_periph_clock_enable(RCC_GPIOF);
    rcc_periph_clock_enable(RCC_GPIOA);

    gpio_mode_setup(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO3);
    gpio_set_output_options(GPIOB, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO3);

    gpio_mode_setup(GPIOF, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO0);
    gpio_set_output_options(GPIOF, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO0);

    gpio_mode_setup(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO1);
    gpio_set_output_options(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO1);

    rcc_periph_clock_enable(RCC_TIM7);
    timer_set_prescaler(TIM7, 0);
    timer_set_period(TIM7, 2399);

    nvic_enable_irq(NVIC_TIM7_IRQ);
    timer_enable_irq(TIM7, TIM_DIER_UIE);
    timer_enable_counter(TIM7);
}

static void delay_ms(uint32_t milliseconds) {
    uint32_t start = system_millis;
    while ((uint32_t)(system_millis - start) < milliseconds);
}

int main(void) {
    system_setup();
    tdr_pulse_setup();

    while (1) {
        delay_ms(100);
    }
}
