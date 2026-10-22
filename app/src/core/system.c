#include "core/system.h"
#include <libopencm3/cm3/systick.h>
#include <libopencm3/stm32/rcc.h>

volatile uint32_t system_millis;

void sys_tick_handler(void) { system_millis++; }

static void clock_setup(void) {
  rcc_clock_setup_hsi(&rcc_hsi_configs[RCC_CLOCK_HSI_48MHZ]);
}

static void systick_setup(void) {
  systick_set_clocksource(STK_CSR_CLKSOURCE_AHB_DIV8);
  systick_set_reload((rcc_ahb_frequency / 8U / 1000U) - 1U);
  systick_interrupt_enable();
  systick_counter_enable();
}

void system_setup(void) {
  clock_setup();
  systick_setup();
}