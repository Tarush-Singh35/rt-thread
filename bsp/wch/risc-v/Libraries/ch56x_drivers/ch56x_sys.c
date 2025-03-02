/*
 * Copyright (c) 2006-2022, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author            Notes
 * 2022-07-15     Emuzit            first version
 */
#include <rthw.h>
#include <rtdebug.h>
#include "ch56x_sys.h"

rt_inline uint8_t _slp_clk_off0_irqn_bit(uint8_t irqn)
{
    uint8_t bitpos;

    switch (irqn)
    {
    case TMR0_IRQn:   bitpos = RB_SLP_CLK_TMR0;  break;
    case TMR1_IRQn:   bitpos = RB_SLP_CLK_TMR1;  break;
    case TMR2_IRQn:   bitpos = RB_SLP_CLK_TMR2;  break;
    /* special case to control PWMX clock in irqn way */
    case PWMX_OFFn:   bitpos = RB_SLP_CLK_PWMX;  break;
    case UART0_IRQn:  bitpos = RB_SLP_CLK_UART0; break;
    case UART1_IRQn:  bitpos = RB_SLP_CLK_UART1; break;
    case UART2_IRQn:  bitpos = RB_SLP_CLK_UART2; break;
    case UART3_IRQn:  bitpos = RB_SLP_CLK_UART3; break;
    default:
        bitpos = 0;
    }

    return bitpos;
}

rt_inline uint8_t _slp_clk_off1_irqn_bit(uint8_t irqn)
{
    uint8_t bitpos;

    switch (irqn)
    {
    case SPI0_IRQn:   bitpos = RB_SLP_CLK_SPI0;  break;
    case SPI1_IRQn:   bitpos = RB_SLP_CLK_SPI1;  break;
#if defined(SOC_CH567)
    case SDC_IRQn:    bitpos = RB_SLP_CLK_SDC;   break;
    case LED_IRQn:    bitpos = RB_SLP_CLK_LED;   break;
    case USB0_IRQn:   bitpos = RB_SLP_CLK_USB0;  break;
    case USB1_IRQn:   bitpos = RB_SLP_CLK_USB1;  break;
    case ECDC_IRQn:   bitpos = RB_SLP_CLK_ECDC;  break;
#elif defined(SOC_CH568)
    case SDC_IRQn:    bitpos = RB_SLP_CLK_SDC;   break;
    case LED_IRQn:    bitpos = RB_SLP_CLK_LED;   break;
    case USB1_IRQn:   bitpos = RB_SLP_CLK_USB1;  break;
    case USB0_IRQn:   bitpos = RB_SLP_CLK_SATA;  break;
    case ECDC_IRQn:   bitpos = RB_SLP_CLK_ECDC;  break;
#else
    case EMMC_IRQn:   bitpos = RB_SLP_CLK_EMMC;  break;
    case HSPI_IRQn:   bitpos = RB_SLP_CLK_HSPI;  break;
    case USBHS_IRQn:  bitpos = RB_SLP_CLK_USBHS; break;
    case USBSS_IRQn:  bitpos = RB_SLP_CLK_USBSS; break;
    case SerDes_IRQn: bitpos = RB_SLP_CLK_SERD;  break;
    case DVP_IRQn:    bitpos = RB_SLP_CLK_DVP;   break;
#endif
    default:
        bitpos = 0;
    }

    return bitpos;
}

#if defined(SOC_SERIES_CH569)
rt_inline uint8_t _wake_clk_off_irqn_bit(uint8_t irqn)
{
    uint8_t bitpos;

    switch (irqn)
    {
    case ETH_IRQn:    bitpos = RB_SLP_CLK_ETH;   break;
    case ECDC_IRQn:   bitpos = RB_SLP_CLK_ECDC;  break;
    default:
        bitpos = 0;
    }

    return bitpos;
}
#endif

/**
 * @brief   Turn on/off device clock for group clk_off0.
 *
 * @param   bits is a bit mask to select corresponding devices.
 *
 * @param   off is to turn off the clock (1) or trun on (0).
 */
void sys_slp_clk_off0(uint8_t bits, int off)
{
    volatile struct sys_registers *sys = (void *)SYS_REG_BASE;
    rt_base_t level;
    uint8_t u8v;

    u8v = sys->SLP_CLK_OFF0.reg;
    u8v = off ? (u8v | bits) : (u8v & ~bits);
    level = rt_hw_interrupt_disable();
    sys_safe_access_enter(sys);
    sys->SLP_CLK_OFF0.reg = u8v;
    sys_safe_access_leave(sys);
    rt_hw_interrupt_enable(level);
}

/**
 * @brief   Turn on/off device clock for group clk_off1.
 *
 * @param   bits is a bit mask to select corresponding devices.
 *
 * @param   off is to turn off the clock (1) or trun on (0).
 */
void sys_slp_clk_off1(uint8_t bits, int off)
{
    volatile struct sys_registers *sys = (void *)SYS_REG_BASE;
    rt_base_t level;
    uint8_t u8v;

    u8v = sys->SLP_CLK_OFF1.reg;
    u8v = off ? (u8v | bits) : (u8v & ~bits);
    level = rt_hw_interrupt_disable();
    sys_safe_access_enter(sys);
    sys->SLP_CLK_OFF1.reg = u8v;
    sys_safe_access_leave(sys);
    rt_hw_interrupt_enable(level);
}

/**
 * @brief   Turn on/off device clock, specified by its irq number.
 *
 * @param   irqn is the irq number of the target device.
 *          PWMX does not have irqn, use special PWMX_OFFn number.
 *
 * @param   off is to turn off the clock (1) or trun on (0).
 *
 * @return  Returns if irqn-device has corresponding clk off bit :
 *          0 if device not found; otherwise bit position of off0/off1.
 */
int sys_clk_off_by_irqn(uint8_t irqn, int off)
{
    volatile struct sys_registers *sys = (void *)SYS_REG_BASE;
    uint8_t u8v;
    size_t offset;

    uint8_t bitpos = 0;

    if (irqn < END_OF_IRQn)
    {
        if ((bitpos = _slp_clk_off0_irqn_bit(irqn)) != 0)
        {
            offset = offsetof(struct sys_registers, SLP_CLK_OFF0);
        }
        else if ((bitpos = _slp_clk_off1_irqn_bit(irqn)) != 0)
        {
            offset = offsetof(struct sys_registers, SLP_CLK_OFF1);
        }
#if defined(SOC_SERIES_CH569)
        else if ((bitpos = _wake_clk_off_irqn_bit(irqn)) != 0)
        {
            offset = offsetof(struct sys_registers, SLP_WAKE_CTRL);
        }
#endif
        if (bitpos)
        {
            volatile uint8_t *cxreg = (void *)sys;
            rt_base_t level;
            u8v = cxreg[offset];
            u8v = off ? (u8v | bitpos) : (u8v & ~bitpos);
            level = rt_hw_interrupt_disable();
            sys_safe_access_enter(sys);
            cxreg[offset] = u8v;
            sys_safe_access_leave(sys);
            rt_hw_interrupt_enable(level);
        }
    }

    return bitpos;
}

/**
 * @brief   Setup HCLK frequency.
 *
 * @param   freq is the desired hclk frequency.
 *          supported : 120/96/80/60/48/40/32/30/15/10/6/3/2 MHz
 *
 * @return  Returns 0 if hclk is successfully set.
 */
int sys_hclk_set(uint32_t freq)
{
    volatile struct sys_registers *sys = (void *)SYS_REG_BASE;

    uint8_t plldiv;

    int clksel = -1;

    if (freq >= 30000000)
    {
        if (freq <= 120000000)
        {
            /* supported : 120/96/80/60/48/40/32/30 MHz */
            plldiv = 480000000 / freq;  // 30M => 16 & 0xf => 0
            clksel = RB_CLK_SEL_PLL;
        }
    }
    else if (freq >= 2000000)
    {
        /* supported : 15/10/6/3/2 MHz */
        plldiv = 30000000 / freq;
        clksel = 0;
    }

    if (clksel >= 0)
    {
        rt_base_t level = rt_hw_interrupt_disable();
        sys_safe_access_enter(sys);
        sys->CLK_PLL_DIV.reg = clk_pll_div_wdat(plldiv);
        sys->CLK_CFG_CTRL.reg = clk_cfg_ctrl_wdat(clksel);
        sys_safe_access_leave(sys);
        rt_hw_interrupt_enable(level);
        clksel = 0;
    }

    return clksel;
}

/**
 * @brief   Get current HCLK frequency.
 *
 * @return  Returns current HCLK frequency (Hz).
 */
uint32_t sys_hclk_get(void)
{
    volatile struct sys_registers *sys = (void *)SYS_REG_BASE;

    uint8_t plldiv = sys->CLK_PLL_DIV.pll_div;

    if (sys->CLK_CFG_CTRL.sel_pll == CLK_SEL_PLL_USB_480M)
    {
        return plldiv ? 480000000 / plldiv : 30000000;
    }
    else
    {
        return plldiv ?  30000000 / plldiv : 2000000;
    }
}
