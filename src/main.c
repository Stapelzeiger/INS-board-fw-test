
#include <stddef.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/spi.h>
#include "mpu60X0.h"
#include "i2c.h"
#include "uart.h"
#include "sensors.h"
#include "can.h"

#include <os.h>
#include <platform-abstraction/threading.h>

int uart_write(const char *p, int len)
{
    return 0;
}

int uart_read(char *p, int len)
{
    return 0;
}

void delay(unsigned int n)
{
    while (n-- > 0) {
        __asm__ volatile ("nop":::);
    }
}

const uint32_t f_apb1 = 42000000;
const uint32_t f_cpu = 168000000;

#define FPCCR (*((volatile uint32_t *)0xE000EF34))
#define CPACR (*((volatile uint32_t *)0xE000ED88))

void fpu_config(void)
{
    // Enable the Floating-point coprocessor (CP10 and CP11 to full access)
    CPACR |= (0x03<<(2*10)|(0x03<<(2*11)));

    __asm__ volatile (
        "dsb \n\t"  /* wait for store to complete */
        "isb \n\t"  /* reset pipeline, FPU is now enabled */
        :::);

    uint32_t fpccr = 0;
    // Disable automatic state preservation of FP state
    fpccr &= ~(1<<31);
    // Enable Lazy context save of FP state
    fpccr |= (1<<30);
    FPCCR = fpccr;
}

os_thread_t my_thread;
uint8_t my_thread_stack[1024];

void my_thread_main(void *arg)
{
    (void) arg;

    i2c_gpio_init();
    i2c_init();

    mpu_spi_init();

    gpio_clear(GPIOB, GPIO15);

    os_thread_sleep_us(1000000);

    while (1) {
        if (mpu_ping() && EEPROM_ping() && HMC5883L_ping() && H3LIS331DL_ping() && MS5611_ping()) {
            gpio_set(GPIOB, GPIO14);
            gpio_clear(GPIOA, GPIO10);
        } else {
            gpio_set(GPIOA, GPIO10);
            gpio_clear(GPIOB, GPIO14);
        }
        gpio_toggle(GPIOA, GPIO8);
        os_thread_sleep_us(100000);
    }
}

int main(void)
{
    rcc_clock_setup_hse_3v3(&hse_16mhz_3v3[CLOCK_3V3_168MHZ]);

    fpu_config();

    os_init();

    rcc_periph_clock_enable(RCC_GPIOA);
    rcc_periph_clock_enable(RCC_GPIOB);
    rcc_periph_clock_enable(RCC_GPIOC);

    // LEDs
    gpio_mode_setup(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO8 | GPIO10);
    gpio_mode_setup(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO14 | GPIO15);
    gpio_clear(GPIOA, GPIO10);
    gpio_clear(GPIOB, GPIO14);

    // SD LED on
    gpio_set(GPIOB, GPIO15);

    // VCC_IO enable
    gpio_mode_setup(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO5);
    gpio_set(GPIOB, GPIO5);

    uart_conn1_init(38400);
    uart_conn1_write("=== boot ===\n");


    delay(10000000);
     // VCC_A enable
    gpio_mode_setup(GPIOC, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO14);
    gpio_clear(GPIOC, GPIO14);
    delay(1000000);
    gpio_set(GPIOC, GPIO14);
    delay(1000000);

    os_thread_create(&my_thread, my_thread_main, my_thread_stack, sizeof(my_thread_stack), "my thread", 3, NULL);

    // init SCL (PB8), SDA (PB9)
    gpio_set_af(GPIOB, GPIO_AF4, GPIO8 | GPIO9);
    gpio_set_output_options(GPIOB, GPIO_OTYPE_OD, GPIO_OSPEED_50MHZ, GPIO8 | GPIO9);
    gpio_mode_setup(GPIOB, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO8 | GPIO9);
    i2c_init();

    mpu_spi_init();

    // if (mpu60X0_ping(&mpu) && HMC5883L_ping()) {
    //     gpio_set(GPIOB, GPIO14);
    //     gpio_clear(GPIOA, GPIO10);
    // } else {
    //     gpio_set(GPIOA, GPIO10);
    //     gpio_clear(GPIOB, GPIO14);
    // }

    // if (mpu60X0_ping(&mpu)) {
    //     gpio_set(GPIOB, GPIO14);
    //     gpio_clear(GPIOA, GPIO10);
    // } else {
    //     gpio_set(GPIOA, GPIO10);
    //     gpio_clear(GPIOB, GPIO14);
    // }

    can_test();

    while (1) {
        gpio_toggle(GPIOA, GPIO8);
        delay(1000000);
    }

    // os_run();

    // while(1);

    return 0;
}
