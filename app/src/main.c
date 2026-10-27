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

#include <libopencm3/stm32/usart.h>

/*
 * USART2 Setup (PA2 = TX, PA15 = RX, 115200 8N1)
 * Connected directly to ST-LINK Virtual COM Port (USB) and pin A7.
 */
static void usart_setup(void) {
  rcc_periph_clock_enable(RCC_GPIOA);
  rcc_periph_clock_enable(RCC_USART2);

  gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO2 | GPIO15);
  gpio_set_af(GPIOA, GPIO_AF7, GPIO2 | GPIO15);
  gpio_set_output_options(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO2);

  usart_set_baudrate(USART2, 115200);
  usart_set_databits(USART2, 8);
  usart_set_stopbits(USART2, USART_STOPBITS_1);
  usart_set_mode(USART2, USART_MODE_TX);
  usart_set_parity(USART2, USART_PARITY_NONE);
  usart_set_flow_control(USART2, USART_FLOWCONTROL_NONE);

  usart_enable(USART2);
}

static void usart_send_char(char c) {
  usart_send_blocking(USART2, (uint16_t)c);
}

static void usart_send_str(const char *str) {
  while (*str) {
    usart_send_char(*str++);
  }
}

/* Helper to format unsigned integer into string buffer */
static void uint_to_str(uint32_t val, char *buf) {
  char temp[12];
  int i = 0;
  if (val == 0) {
    buf[0] = '0';
    buf[1] = '\0';
    return;
  }
  while (val > 0) {
    temp[i++] = (char)('0' + (val % 10));
    val /= 10;
  }
  int j = 0;
  while (i > 0) {
    buf[j++] = temp[--i];
  }
  buf[j] = '\0';
}

/* Helper to format value in tenths (e.g. 50 -> "5.0", 505 -> "50.5") into string buffer */
static void format_tenths(uint32_t tenths, char *buf) {
  char int_part[12];
  uint32_t whole = tenths / 10U;
  uint32_t frac = tenths % 10U;
  uint_to_str(whole, int_part);
  int i = 0;
  while (int_part[i]) {
    buf[i] = int_part[i];
    i++;
  }
  buf[i++] = '.';
  buf[i++] = (char)('0' + frac);
  buf[i] = '\0';
}

int main(void) {
  system_setup();
  tdr_pulse_setup(); /* START SQUARE WAVE IMMEDIATELY AT BOOT! */
  dac_function_generator_setup();
  adc_setup();
  usart_setup();     /* UART link to Raspberry Pi at 115200 baud */
  i2c_setup();
  lcd_init();

  lcd_set_cursor(0, 0);
  lcd_print("TDR Demo Started");
  lcd_set_cursor(1, 0);
  lcd_print("UART: 115200 8N1");
  delay_ms(1500);
  lcd_send_byte(0x01, 0); /* Clear display */
  delay_ms(2);

  while (1) {
    uint16_t val = read_adc();
    /* Compute voltage in mV: val * 3300 / 4095 */
    uint32_t vmV = ((uint32_t)val * 3300U) / 4095U;
    uint32_t v_int = vmV / 1000U;
    uint32_t v_dec = (vmV % 1000U) / 10U; /* 2 decimal digits */

    char v_str[16];
    char v_dec_str[8];
    uint_to_str(v_int, v_str);
    uint_to_str(v_dec, v_dec_str);

    const char *status;
    uint32_t d_tenths = 0; /* Distance in tenths of a centimeter (0 to 160 -> 0.0 to 16.0 cm) */
    uint32_t t_tenths = 0; /* Time in tenths of a nanosecond (0 to 16 -> 0.0 to 1.6 ns) */

    /* Original exact thresholds: LED forward voltage sits cleanly in 200..3800 range */
    if (val > 3800) {
      status = "OPEN";
      /* Scale distance dynamically across 16.0 cm breadboard (0.0 to 16.0 cm) */
      uint32_t delta = (uint32_t)(val - 3800);
      d_tenths = (delta * 160U) / 295U;
      if (d_tenths > 160U) d_tenths = 160U;
      /* Physics: signal on breadboard (v ~ 20 cm/ns) -> 2-way delay t = 2*d / 20 = d / 10 ns */
      t_tenths = (d_tenths + 5U) / 10U; /* Round to nearest 0.1 ns */
    } else if (val < 200) {
      status = "SHORT";
      /* Scale distance dynamically across 16.0 cm breadboard (0.0 to 16.0 cm) */
      d_tenths = ((uint32_t)val * 160U) / 200U;
      if (d_tenths > 160U) d_tenths = 160U;
      t_tenths = (d_tenths + 5U) / 10U;
    } else {
      status = "HEALTHY";
      d_tenths = 0;
      t_tenths = 0;
    }

    char d_str[16];
    char t_str[16];
    format_tenths(d_tenths, d_str);
    format_tenths(t_tenths, t_str);

    /* 1. Update LCD Display */
    lcd_set_cursor(0, 0);
    if (val > 3800) {
      lcd_print("FAULT: OPEN     ");
      lcd_set_cursor(1, 0);
      lcd_print("d=");
      lcd_print(d_str);
      lcd_print("cm t=");
      lcd_print(t_str);
      lcd_print("ns  ");
    } else if (val < 200) {
      lcd_print("FAULT: SHORT    ");
      lcd_set_cursor(1, 0);
      lcd_print("d=");
      lcd_print(d_str);
      lcd_print("cm t=");
      lcd_print(t_str);
      lcd_print("ns  ");
    } else {
      lcd_print("NORMAL (LOAD OK)");
      lcd_set_cursor(1, 0);
      lcd_print("V=");
      lcd_print(v_str);
      lcd_print(".");
      if (v_dec < 10) lcd_print("0");
      lcd_print(v_dec_str);
      lcd_print("V (Intact)");
    }

    /* 2. Send Telemetry Frame to Raspberry Pi via UART */
    /* Format: V:X.XX,T:X.X,D:X.X,STATUS:XXXX\r\n (D in cm, T in ns) */
    usart_send_str("V:");
    usart_send_str(v_str);
    usart_send_str(".");
    if (v_dec < 10) usart_send_str("0");
    usart_send_str(v_dec_str);
    usart_send_str(",T:");
    usart_send_str(t_str);
    usart_send_str(",D:");
    usart_send_str(d_str);
    usart_send_str(",STATUS:");
    usart_send_str(status);
    usart_send_str("\r\n");

    delay_ms(300); /* Send update roughly 3 times a second */
  }
}
