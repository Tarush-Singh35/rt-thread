/*
 * Copyright (c) 2006-2022, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author            Notes
 * 2022-07-15     Emuzit            first version
 * 2022-07-20     Emuzit            add watchdog test
 * 2022-07-26     Emuzit            add hwtimer test
 */
#include <rtthread.h>
#include <rtdebug.h>
#include <drivers/pin.h>
#include <drivers/watchdog.h>
#include <drivers/hwtimer.h>
#include "board.h"

static const rt_base_t gpio_int_pins[8] = GPIO_INT_PINS;

/* note : PIN_IRQ_MODE_RISING_FALLING not supported */
static const uint32_t gpint_mode[] =
{
    PIN_IRQ_MODE_RISING,
    PIN_IRQ_MODE_RISING,
    PIN_IRQ_MODE_RISING,
    PIN_IRQ_MODE_RISING,
    PIN_IRQ_MODE_FALLING,
    PIN_IRQ_MODE_FALLING,
    PIN_IRQ_MODE_FALLING,
    PIN_IRQ_MODE_FALLING,
};

static struct rt_mailbox *gpint_mb = RT_NULL;
static struct rt_thread  *gpint_thread = RT_NULL;

static rt_base_t led0, led1;

static rt_device_t wdg_dev;

static struct rt_device *tmr_dev_0;
static struct rt_device *tmr_dev_1;

static void gpio_int_callback(void *pin)
{
    led1 = (led1 == PIN_LOW) ? PIN_HIGH : PIN_LOW;
    rt_pin_write(LED1_PIN, led1);

    if (gpint_mb != RT_NULL)
    {
        /* non-block, silently ignore RT_EFULL */
        rt_mb_send(gpint_mb, (uint32_t)pin);
    }
}

static void gpio_int_thread(void *param)
{
    while (1)
    {
        rt_err_t res;
        uint32_t pin;

        res = rt_mb_recv(gpint_mb, &pin, RT_WAITING_FOREVER);
        if (res == RT_EOK)
        {
            rt_kprintf("gpio_int #%d (%d)\n", pin, rt_pin_read(pin));
        }
        rt_thread_mdelay(100);

#ifdef RT_USING_WDT
        rt_device_control(wdg_dev, RT_DEVICE_CTRL_WDT_KEEPALIVE, RT_NULL);
#endif
    }
}

#ifdef RT_USING_HWTIMER
static rt_err_t tmr_timeout_cb(rt_device_t dev, rt_size_t size)
{
    rt_tick_t tick = rt_tick_get();

    int tmr = (dev == tmr_dev_1) ? 1 : 0;

    rt_kprintf("hwtimer %d timeout callback fucntion @tick %d\n", tmr, tick);

    return RT_EOK;
}
#endif

void main(void)
{
    rt_hwtimerval_t timerval;
    rt_hwtimer_mode_t mode;
    rt_size_t tsize;

    uint32_t seconds;
    rt_err_t res;

    int i;

    rt_kprintf("\nCH569W-R0-1v0, HCLK: %dMHz\n\n", sys_hclk_get() / 1000000);

    /* Enable all gpio interrupt with various modes.
     * LED0 or GND touching can be used to trigger pin interrupt.
    */
    gpint_mb = rt_mb_create("pximb", 8, RT_IPC_FLAG_FIFO);
    if (gpint_mb == RT_NULL)
    {
        rt_kprintf("gpint mailbox create failed !\n");
    }
    else
    {
        gpint_thread = rt_thread_create("pxith", gpio_int_thread, RT_NULL,
                                        512, RT_MAIN_THREAD_PRIORITY, 50);
        if (gpint_thread == RT_NULL)
        {
            rt_kprintf("gpint thread create failed !\n");
        }
        else
        {
            rt_thread_startup(gpint_thread);

            for (i = 0; i < 8; i++)
            {
                rt_base_t pin = gpio_int_pins[i];
                rt_pin_mode(pin, PIN_MODE_INPUT_PULLUP);
                res = rt_pin_attach_irq(
                      pin, gpint_mode[i], gpio_int_callback, (void *)pin);
                if (res != RT_EOK)
                {
                    rt_kprintf("rt_pin_attach_irq failed (%d:%d)\n", i, res);
                }
                else
                {
                    rt_pin_irq_enable(pin, PIN_IRQ_ENABLE);
                }
            }
        }
    }

#ifdef RT_USING_WDT
    /* Test watchdog with 30s timeout, keepalive with gpio interrupt.
     *
     * CAVEAT: With only 8-bit WDOG_COUNT and fixed clocking at Fsys/524288,
     * watchdog of ch56x may be quite limited with very short timeout.
    */
    seconds = 30;
    wdg_dev = rt_device_find("wdt");
    if (!wdg_dev)
    {
        rt_kprintf("watchdog device not found !\n");
    }
    else if (rt_device_init(wdg_dev) != RT_EOK ||
             rt_device_control(wdg_dev, RT_DEVICE_CTRL_WDT_SET_TIMEOUT, &seconds) != RT_EOK)
    {
        rt_kprintf("watchdog setup failed !\n");
    }
    else
    {
        rt_kprintf("WDT_TIMEOUT in %d seconds, trigger gpio interrupt to keep alive.\n\n", seconds);
    }
#endif

#ifdef RT_USING_HWTIMER
    /* setup two timers, ONESHOT & PERIOD each
    */
    tmr_dev_0 = rt_device_find("timer0");
    tmr_dev_1 = rt_device_find("timer1");
    if (tmr_dev_0 == RT_NULL || tmr_dev_1 == RT_NULL)
    {
        rt_kprintf("hwtimer device(s) not found !\n");
    }
    else if (rt_device_open(tmr_dev_0, RT_DEVICE_OFLAG_RDWR) != RT_EOK ||
             rt_device_open(tmr_dev_1, RT_DEVICE_OFLAG_RDWR) != RT_EOK)
    {
        rt_kprintf("hwtimer device(s) open failed !\n");
    }
    else
    {
        rt_device_set_rx_indicate(tmr_dev_0, tmr_timeout_cb);
        rt_device_set_rx_indicate(tmr_dev_1, tmr_timeout_cb);

        timerval.sec = 3;
        timerval.usec = 500000;
        tsize = sizeof(timerval);
        mode = HWTIMER_MODE_ONESHOT;
        if (rt_device_control(tmr_dev_0, HWTIMER_CTRL_MODE_SET, &mode) != RT_EOK)
        {
            rt_kprintf("timer0 set mode failed !\n");
        }
        else if (rt_device_write(tmr_dev_0, 0, &timerval, tsize) != tsize)
        {
            rt_kprintf("timer0 start failed !\n");
        }
        else
        {
            rt_kprintf("timer0 started !\n");
        }

        timerval.sec = 5;
        timerval.usec = 0;
        tsize = sizeof(timerval);
        mode = HWTIMER_MODE_PERIOD;
        if (rt_device_control(tmr_dev_1, HWTIMER_CTRL_MODE_SET, &mode) != RT_EOK)
        {
            rt_kprintf("timer1 set mode failed !\n");
        }
        else if (rt_device_write(tmr_dev_1, 0, &timerval, tsize) != tsize)
        {
            rt_kprintf("timer1 start failed !\n");
        }
        else
        {
            rt_kprintf("timer1 started !\n");
        }
    }
#endif

    rt_pin_mode(LED1_PIN, PIN_MODE_OUTPUT);
    rt_pin_write(LED1_PIN, led1 = PIN_HIGH);

    rt_pin_mode(LED0_PIN, PIN_MODE_OUTPUT);
    rt_pin_write(LED0_PIN, led0 = PIN_LOW);

    while (1)
    {
        /* flashing LED0 every 1 second */
        rt_thread_mdelay(500);
        led0 = (led0 == PIN_LOW) ? PIN_HIGH : PIN_LOW;
        rt_pin_write(LED0_PIN, led0);
    }
}
