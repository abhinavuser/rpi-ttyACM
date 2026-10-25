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
static void dac_function_generator_setup(void) {
  /* Enable peripheral clocks */
  rcc_periph_clock_enable(RCC_GPIOA);
  rcc_periph_clock_enable(RCC_DAC1);
  rcc_periph_clock_enable(RCC_DMA1);
  rcc_periph_clock_enable(RCC_TIM6);

  /* Configure PA4 (DAC1_OUT1 / Arduino A3) as analog */
  gpio_mode_setup(GPIOA, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, GPIO4);

  /* Configure DMA1 Channel 3 (DAC1 Channel 1 request) in circular mode */
  dma_channel_reset(DMA1, DMA_CHANNEL3);
  dma_set_peripheral_address(DMA1, DMA_CHANNEL3, (uint32_t)&DAC_DHR12R1(DAC1));
  dma_set_memory_address(DMA1, DMA_CHANNEL3, (uint32_t)sine_wave_lut);
  dma_set_number_of_data(DMA1, DMA_CHANNEL3, SINE_SAMPLES);
  dma_set_read_from_memory(DMA1, DMA_CHANNEL3);
  dma_enable_memory_increment_mode(DMA1, DMA_CHANNEL3);
  dma_set_peripheral_size(DMA1, DMA_CHANNEL3, DMA_CCR_PSIZE_16BIT);
  dma_set_memory_size(DMA1, DMA_CHANNEL3, DMA_CCR_MSIZE_16BIT);
  dma_set_priority(DMA1, DMA_CHANNEL3, DMA_CCR_PL_HIGH);
  dma_enable_circular_mode(DMA1, DMA_CHANNEL3);
  dma_enable_channel(DMA1, DMA_CHANNEL3);

  /* Configure DAC1 Channel 1 with Timer 6 trigger */
  dac_disable(DAC1, DAC_CHANNEL1);
  dac_buffer_enable(DAC1, DAC_CHANNEL1);
  dac_trigger_enable(DAC1, DAC_CHANNEL1);
  dac_set_trigger_source(DAC1, DAC_CR_TSEL1_T6);
  dac_dma_enable(DAC1, DAC_CHANNEL1);
  dac_enable(DAC1, DAC_CHANNEL1);

  /* Configure Timer 6: 48 MHz / (1499 + 1) = 32 kHz trigger rate (32 samples * 1 kHz = 32 kHz) */
  timer_set_prescaler(TIM6, 0);
  timer_set_period(TIM6, 1499);
  timer_set_master_mode(TIM6, TIM_CR2_MMS_UPDATE);
  timer_enable_counter(TIM6);
}

#include <libopencm3/cm3/nvic.h>

/*
 * Direct DSO TDR Square Wave Generator (10 kHz)
 * Runs autonomously via Timer 7 interrupt toggling GPIO pins.
 * Output pins:
 *   - PB3 (Arduino D13, onboard green LED)
 *   - PF0 (Arduino D7)
 *   - PA1 (Arduino A1)
 * Fast 50 MHz slew rate provides ~2.5 ns rise time for clean TDR reflections.
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
  /* 1. Enable GPIO Clocks */
  rcc_periph_clock_enable(RCC_GPIOB);
  rcc_periph_clock_enable(RCC_GPIOF);
  rcc_periph_clock_enable(RCC_GPIOA);

  /* 2. Configure PB3 (D13), PF0 (D7), PA1 (A1) as High-Speed Push-Pull Outputs */
  gpio_mode_setup(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO3);
  gpio_set_output_options(GPIOB, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO3);

  gpio_mode_setup(GPIOF, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO0);
  gpio_set_output_options(GPIOF, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO0);

  gpio_mode_setup(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO1);
  gpio_set_output_options(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO1);

  /* 3. Configure Timer 7 for 20 kHz interrupt rate -> 10 kHz square wave:
   * Clock = 48 MHz -> 48 MHz / 2400 = 20,000 interrupts/sec
   */
  rcc_periph_clock_enable(RCC_TIM7);
  timer_set_prescaler(TIM7, 0);
  timer_set_period(TIM7, 2399);

  nvic_enable_irq(NVIC_TIM7_IRQ);
  timer_enable_irq(TIM7, TIM_DIER_UIE);
  timer_enable_counter(TIM7);
}

static void delay_ms(uint32_t milliseconds) {
  uint32_t start = system_millis;
  while ((uint32_t)(system_millis - start) < milliseconds)
    ;
}

static void i2c_setup(void) {
  rcc_periph_clock_enable(RCC_GPIOB);
  /* Set PB6 (SCL) and PB7 (SDA) to Alternate Function 4 for I2C1 */
  gpio_mode_setup(GPIOB, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO6 | GPIO7);
  gpio_set_output_options(GPIOB, GPIO_OTYPE_OD, GPIO_OSPEED_50MHZ,
                          GPIO6 | GPIO7);
  gpio_set_af(GPIOB, GPIO_AF4, GPIO6 | GPIO7);

  rcc_periph_clock_enable(RCC_I2C1);
  i2c_peripheral_disable(I2C1);
  /* Assume 8MHz AHB clock based on system_setup defaults */
  i2c_set_speed(I2C1, i2c_speed_sm_100k, 8);
  i2c_peripheral_enable(I2C1);
}

static void lcd_write(uint8_t data) {
  i2c_transfer7(I2C1, LCD_ADDR, &data, 1, NULL, 0);
}

static void lcd_send_nibble(uint8_t nibble, uint8_t rs) {
  uint8_t data = (nibble << 4) | (rs ? 1 : 0) | 0x08; /* 0x08 is Backlight */
  lcd_write(data | 0x04);                             /* EN = 1 */
  delay_ms(1);
  lcd_write(data & ~0x04); /* EN = 0 */
  delay_ms(1);
}

static void lcd_send_byte(uint8_t val, uint8_t rs) {
  lcd_send_nibble(val >> 4, rs);
  lcd_send_nibble(val & 0x0F, rs);
}

static void lcd_init(void) {
  delay_ms(50);
  lcd_send_nibble(0x03, 0);
  delay_ms(5);
  lcd_send_nibble(0x03, 0);
  delay_ms(1);
  lcd_send_nibble(0x03, 0);
  delay_ms(1);
  lcd_send_nibble(0x02, 0); /* 4-bit mode */

  lcd_send_byte(0x28, 0); /* 2 lines, 5x8 font */
  lcd_send_byte(0x0C, 0); /* Display ON, Cursor OFF */
  lcd_send_byte(0x01, 0); /* Clear Display */
  delay_ms(2);
}

static void lcd_print(const char *str) {
  while (*str) {
    lcd_send_byte((uint8_t)*str++, 1);
  }
}

static void lcd_set_cursor(uint8_t row, uint8_t col) {
  uint8_t address_offsets[] = {0x00, 0x40, 0x14, 0x54};
  lcd_send_byte(0x80 | (address_offsets[row] + col), 0);
}

static void i2c_setup(void) {
    rcc_periph_clock_enable(RCC_GPIOB);
    gpio_mode_setup(GPIOB, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO6 | GPIO7);
    gpio_set_output_options(GPIOB, GPIO_OTYPE_OD, GPIO_OSPEED_50MHZ, GPIO6 | GPIO7);
    gpio_set_af(GPIOB, GPIO_AF4, GPIO6 | GPIO7);

    rcc_periph_clock_enable(RCC_I2C1);
    i2c_peripheral_disable(I2C1);
    i2c_set_speed(I2C1, i2c_speed_sm_100k, 8);
    i2c_peripheral_enable(I2C1);
}

static void lcd_write(uint8_t data) {
    i2c_transfer7(I2C1, LCD_ADDR, &data, 1, NULL, 0);
}
static void adc_setup(void) {
  rcc_periph_clock_enable(RCC_GPIOA);
  /* Configure PA0 as analog input */
  gpio_mode_setup(GPIOA, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, GPIO0);

  /* STM32F3 ADC setup */
  rcc_periph_clock_enable(RCC_ADC12);
  adc_power_off(ADC1);

  /* Using synchronous clock mode by default for F3 ADC */
  ADC12_CCR |= (1 << 16); // Set CKMODE to 01 (sync AHB clock divided by 1)

  adc_power_on(ADC1);
  delay_ms(1); /* Wait for ADC to stabilize */
}

static uint16_t read_adc(void) {
  uint8_t channel_array[1] = {1}; /* PA0 is ADC1_IN1 */
  adc_set_regular_sequence(ADC1, 1, channel_array);
  adc_start_conversion_regular(ADC1);
  while (!adc_eoc(ADC1))
    ;
  return adc_read_regular(ADC1);
}

int main(void) {
    system_setup();
    tdr_pulse_setup();
    dac_function_generator_setup();
    adc_setup();
    i2c_setup();

    while (1) {
        uint16_t val = read_adc();
        (void)val;
        delay_ms(200);
    }
}
