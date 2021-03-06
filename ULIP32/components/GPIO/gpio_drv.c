#include <stdio.h>
#include "gpio_drv.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
// #include "esp_task_wdt.h"
// #include "soc/gpio_periph.h"
// #define TEST_INTR             1
// #define TWDT_TIMEOUT_S        3
#define GPIO_OUTPUT           33
#define GPIO_OUTPUT_PIN_SEL   (1ULL<<GPIO_OUTPUT)
#define ESP_INTR_FLAG_DEFAULT 0
static portMUX_TYPE my_mutex = portMUX_INITIALIZER_UNLOCKED;
static int cnt = 0;

typedef struct gpio_intr {
    int intr;
    int gpio;
    int state;
    int flags;
    gpio_func_t func;
    void *user_data;
} gpio_intr_t;

static gpio_intr_t gpio_intr[GPIO_NUM_INTERRUPT] = {
    {
        .intr = 0,
        .gpio = -1,
        .state = GPIO_PIN_INTR_DISABLE,
        .flags = 0,
        .func = NULL,
        .user_data = NULL,
    },
    {
        .intr = 1,
        .gpio = -1,
        .state = GPIO_PIN_INTR_DISABLE,
        .flags = 0,
        .func = NULL,
        .user_data = NULL,
    },
    {
        .intr = 2,
        .gpio = -1,
        .state = GPIO_PIN_INTR_DISABLE,
        .flags = 0,
        .func = NULL,
        .user_data = NULL,
    },
    {
        .intr = 3,
        .gpio = -1,
        .state = GPIO_PIN_INTR_DISABLE,
        .flags = 0,
        .func = NULL,
        .user_data = NULL,
    },
    {
        .intr = 4,
        .gpio = -1,
        .state = GPIO_PIN_INTR_DISABLE,
        .flags = 0,
        .func = NULL,
        .user_data = NULL,
    }
};



static void IRAM_ATTR gpio_interrupt_handler(void *arg)
{
    gpio_intr_t * p = (gpio_intr_t *) arg;
    uint32_t io_num = p->gpio;

    // ets_printf("recivefromisr\n");

    int status;
    int i;
    
    for (i = 0; i < GPIO_NUM_INTERRUPT; i++) {
        p = &gpio_intr[i];
        if (!p->func) continue;
        status = BIT(p->gpio);
        if (io_num == p->gpio) {
            /* Disable interrupt */
            if (p->flags & GPIO_INTR_DISABLED) {
#if defined(TEST_INTR)
                cnt += 1;
                gpio_set_level(GPIO_OUTPUT, cnt & 1);
#endif
                // printf("disabling gpio: %d", p->gpio);
                gpio_set_intr_type(p->gpio, GPIO_INTR_DISABLE);
            }
            if (p->gpio < 32) {
                SET_PERI_REG_MASK(GPIO_STATUS_W1TC_REG, status);

            } else {
                SET_PERI_REG_MASK(GPIO_STATUS1_W1TC_REG, status);

            }

            //GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, status);
            // esp_task_wdt_reset();
            p->func(p->intr, p->user_data);
        }
    }

}


int gpio_drv_init(void)
{
    
    ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT));
    // gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));

    // xTaskCreate(gpio_task_example, "gpio_task_example", 8192, NULL, 16, &xHandle);
    
    // esp_task_wdt_init(TWDT_TIMEOUT_S, false);
    // esp_task_wdt_add(xHandle);
    // ETS_GPIO_INTR_ATTACH(gpio_interrupt_handler, NULL);
    // ETS_GPIO_INTR_ENABLE();
#if defined(TEST_INTR)

    // gpio_config_t io_conf2;
    gpio_config_t io_conf2 = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = GPIO_OUTPUT_PIN_SEL,
        .pull_down_en = 0,
        .pull_up_en = 0
    };

    gpio_config(&io_conf2);
#endif
    return 0;
}


void gpio_drv_release(void)
{
    // taskENTER_CRITICAL(&my_mutex);
    // ESP_LOGD("GPIO", "uninstall isr");
    // gpio_intr_t *p;
    // for (int i = 0; i < GPIO_NUM_INTERRUPT; i++)
    // {
    //     p = &gpio_intr[i];
    //     gpio_isr_handler_remove(p->gpio);
    // }
    
    // gpio_intr_disable
    // gpio_uninstall_isr_service();
    // vTaskSuspend(xHandle);
    // taskEXIT_CRITICAL(&my_mutex);

}


int gpio_interrupt_open(int intr, int gpio, int state,
                        int flags, gpio_func_t func,
                        void *user_data)
{
    gpio_intr_t *p;

    if (intr >= GPIO_NUM_INTERRUPT) return -1;

    p = &gpio_intr[intr];
    p->gpio = gpio;
    p->state = state;
    p->flags = flags;
    p->func = func;
    p->user_data = user_data;

    uint64_t pin_select = BIT64(gpio);

    gpio_config_t io_conf;
    //disable interrupt
    io_conf.intr_type = GPIO_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_INPUT;
    //bit mask of the pins that you want to set,e.g.GPIO18/19
    io_conf.pin_bit_mask = pin_select;
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //disable pull-up mode
    io_conf.pull_up_en = 0;
    //configure GPIO with the given settings
    gpio_config(&io_conf);
    // ETS_GPIO_INTR_DISABLE();
    // GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, BIT(gpio));
    // gpio_pin_intr_state_set(gpio, state);
    // ETS_GPIO_INTR_ENABLE();
    //void * arg = &p->gpio
    gpio_intr_disable(gpio);
    gpio_set_intr_type(gpio, state);
    gpio_isr_handler_add(gpio, gpio_interrupt_handler, (void *) p);
    gpio_intr_enable(gpio);
    ESP_LOGI("GPIO", "interrupt open in pin: %d", gpio);
    return 0;
}


int gpio_interrupt_close(int intr)
{
    gpio_intr_t *p;

    if (intr >= GPIO_NUM_INTERRUPT) return -1;

    p = &gpio_intr[intr];
    gpio_intr_disable(p->gpio);
    gpio_set_intr_type(p->gpio, GPIO_INTR_DISABLE);
    gpio_isr_handler_remove(p->gpio);
    gpio_intr_enable(p->gpio);
    p->gpio = -1;
    p->flags = 0;
    p->func = NULL;
    p->user_data = NULL;
    // gpio_reset_pin(p->gpio);

    return 0;
}


void gpio_interrupt_enable(int intr, gpio_int_type_t state)
{
    gpio_intr_t *p;

    if (intr >= GPIO_NUM_INTERRUPT) return;

    p = &gpio_intr[intr];
    gpio_set_intr_type(p->gpio, state);
    
}


void gpio_interrupt_disable(int intr)
{
    gpio_intr_t *p;

    if (intr >= GPIO_NUM_INTERRUPT) return;

    p = &gpio_intr[intr];
    gpio_set_intr_type(p->gpio, GPIO_PIN_INTR_DISABLE);
}

/*
void gpio16_output_conf(void)
{
    WRITE_PERI_REG(PAD_XPD_DCDC_CONF,
                   (READ_PERI_REG(PAD_XPD_DCDC_CONF) & 0xffffffbc) | (uint32)0x1); 	// mux configuration for XPD_DCDC to output rtc_gpio0

    WRITE_PERI_REG(RTC_GPIO_CONF,
                   (READ_PERI_REG(RTC_GPIO_CONF) & (uint32)0xfffffffe) | (uint32)0x0);	//mux configuration for out enable

    WRITE_PERI_REG(RTC_GPIO_ENABLE,
                   (READ_PERI_REG(RTC_GPIO_ENABLE) & (uint32)0xfffffffe) | (uint32)0x1);	//out enable
}


void gpio16_output_set(uint8 value)
{
    WRITE_PERI_REG(RTC_GPIO_OUT,
                   (READ_PERI_REG(RTC_GPIO_OUT) & (uint32)0xfffffffe) | (uint32)(value & 1));
}


void gpio16_input_conf(void)
{
    WRITE_PERI_REG(PAD_XPD_DCDC_CONF,
                   (READ_PERI_REG(PAD_XPD_DCDC_CONF) & 0xffffffbc) | (uint32)0x1); 	// mux configuration for XPD_DCDC and rtc_gpio0 connection

    WRITE_PERI_REG(RTC_GPIO_CONF,
                   (READ_PERI_REG(RTC_GPIO_CONF) & (uint32)0xfffffffe) | (uint32)0x0);	//mux configuration for out enable

    WRITE_PERI_REG(RTC_GPIO_ENABLE,
                   READ_PERI_REG(RTC_GPIO_ENABLE) & (uint32)0xfffffffe);	//out disable
}


uint8 gpio16_input_get(void)
{
    return (uint8)(READ_PERI_REG(RTC_GPIO_IN_DATA) & 1);
}
*/