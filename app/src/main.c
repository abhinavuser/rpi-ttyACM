#include "core/system.h"
#include <libopencm3/stm32/adc.h>
#include <libopencm3/stm32/dac.h>
#include <libopencm3/stm32/dma.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/i2c.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/timer.h>
#include <stddef.h>
#include <stdint.h>


#define LCD_ADDR 0x27

#define SINE_SAMPLES 32

/* Precomputed 32-point 12-bit sine wave (0 to 4095 range, centered at 2048) */
static const uint16_t sine_wave_lut[SINE_SAMPLES] = {
    2048, 2447, 2831, 3185, 3495, 3750, 3939, 4056,
    4095, 4056, 3939, 3750, 3495, 3185, 2831, 2447,
    2048, 1649, 1265,  911,  601,  346,  157,   40,
       1,   40,  157,  346,  601,  911, 1265, 1649
};

/*
 * Hardware Function Generator: 1 kHz Sine Wave on PA4 (Arduino A3)
 *
 * NOTE ON 10V REQUIREMENT:
 * The STM32 operates at 3.3V max (VDDA = 3.3V). Its internal DAC can safely output
 * 0V to 3.3V. Feeding or demanding 10V directly on an STM32 pin will permanently destroy
 * the microcontroller pin. To output an actual 10V (or 10Vpp) sine wave, connect an
 * external non-inverting op-amp (e.g. TL072/LM358 with gain ~3x) powered by a 12V supply.
 *
 * Pin selection:
 * - PA0 (Arduino A0) is actively used by your ADC wire/TDR detector. Furthermore, PA0
 *   does not have a hardware DAC peripheral on STM32F303.
 * - PA4 (Arduino A3) is the hardware DAC1_OUT1 channel.
 * - Using TIM6 + DMA1_CH3 generates the 1 kHz sine wave completely in hardware with
 *   0% CPU load, so your ADC and LCD loop runs smoothly with zero interruption!
 */
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
