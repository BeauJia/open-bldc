/*
 * Open-BLDC - Open BrushLess DC Motor Controller
 * Copyright (C) 2009-2010 by Piotr Esden-Tempski <piotr@esden.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file   gprot.c
 * @author Piotr Esden-Tempski <piotr@esden.net>
 * @date   Tue Aug 17 02:00:29 2010
 *
 * @brief  Here you find the implementation of the Governor PC software protocol.
 *
 * @todo document
 */

#include <stm32/gpio.h>

#include <lg/types.h>
#include <lg/gpdef.h>
#include <lg/gprotc.h>

#include "gprot.h"
#include "driver/led.h"
#include "driver/usart.h"
#include "pwm/pwm.h"
#include "comm_tim.h"
#include "driver/adc.h"
#include "sensor_process.h"
#include "comm_process.h"
#include "control_process.h"
#include "main.h"

/**
 * Commutate once trigger flag
 */
#define GPROT_FLAG_PWM_COMM (1 << 0)
/**
 * Run commutation timer flag
 */
#define GPROT_FLAG_COMM_TIM (1 << 1)
/**
 * Run closed loop control flag
 */
#define GPROT_FLAG_ADC_COMM (1 << 2)
/**
 * Pull all phases low flag
 */
#define GPROT_FLAG_ALL_LO (1 << 3)
/**
 * Pull all phases high flag
 */
#define GPROT_FLAG_ALL_HI (1 << 4)

/**
 * Dummy value...
 */
u16 gprot_dummy_val;
/**
 * Flags register
 */
u16 gprot_flag_reg;
/**
 * Previous value of the flag register for detection of flag transitions.
 */
u16 gprot_flag_reg_old;

/* Private function declarations */
void gprot_trigger_output(void *data);
void gprot_register_changed(void *data, u8 addr);
void gprot_update_flags(void);

/* Function implementations */
/**
 * @todo document
 *
 */
void gprot_init()
{
	gpc_init(gprot_trigger_output, 0, gprot_register_changed, 0);

	gprot_flag_reg = 0;
	gprot_flag_reg_old = 0;
	gpc_setup_reg(GPROT_FLAG_REG_ADDR, &gprot_flag_reg);

	gpc_setup_reg(GPROT_PWM_OFFSET_REG_ADDR, &pwm_offset);
	gpc_setup_reg(GPROT_PWM_VAL_REG_ADDR, &pwm_val);
	gpc_setup_reg(GPROT_COMM_TIM_FREQ_REG_ADDR, &(comm_tim_data.freq));
	gpc_setup_reg(GPROT_ADC_ZERO_VALUE_REG_ADDR,
		      (u16 *) & (sensors.half_battery_voltage));
	gpc_setup_reg(GPROT_COMM_TIM_SPARK_ADVANCE_REG_ADDR,
		      (u16 *) & (comm_params.spark_advance));
	gpc_setup_reg(GPROT_COMM_TIM_DIRECT_CUTOFF_REG_ADDR,
		      &(comm_params.direct_cutoff));
	gpc_setup_reg(GPROT_COMM_TIM_IIR_POLE_REG_ADDR, &(comm_params.iir));
	gpc_setup_reg(GPROT_ADC_GLOBAL_CURRENT_REG_ADDR,
		      (u16 *) & (sensors.global_current));
	gpc_setup_reg(GPROT_ADC_PHASE_VOLTAGE_REG_ADDR,
		      (u16 *) & (sensors.phase_voltage));
	gpc_setup_reg(10, (u16 *) & new_cycle_time);
}

/**
 * @todo document
 *
 * @param data @todo document
 */
void gprot_trigger_output(void *data)
{
	data = data;
	usart_enable_send();
}

/**
 * @todo document
 *
 * @param data @todo document
 * @param addr @todo document
 */
void gprot_register_changed(void *data, u8 addr)
{
	data = data;
	if (addr == GPROT_FLAG_REG_ADDR) {
		gprot_update_flags();
	}
}

/**
 * @todo document
 *
 */
void gprot_update_flags(void)
{
	if (gprot_flag_reg & GPROT_FLAG_PWM_COMM) {
		pwm_comm();
		return;
	}

	if (gprot_flag_reg & GPROT_FLAG_COMM_TIM) {
		if (!(gprot_flag_reg_old & GPROT_FLAG_COMM_TIM)) {
			control_process_ignite();
		}
	} else {
		if (gprot_flag_reg_old & GPROT_FLAG_COMM_TIM) {
			control_process_kill();
		}
	}

	if (gprot_flag_reg & GPROT_FLAG_ADC_COMM) {
		demo = true;
	} else {
		if (gprot_flag_reg_old & GPROT_FLAG_ADC_COMM) {
			demo = false;
		}
	}

	if (gprot_flag_reg & GPROT_FLAG_ALL_LO) {
		pwm_all_lo();
	} else {
		if (gprot_flag_reg_old & GPROT_FLAG_ALL_LO) {
			pwm_off();
		}
	}

	if (gprot_flag_reg & GPROT_FLAG_ALL_HI) {
		pwm_all_hi();
	} else {
		if (gprot_flag_reg_old & GPROT_FLAG_ALL_HI) {
			pwm_off();
		}
	}

	/* add other flags here (up to 16) */

	gprot_flag_reg_old = gprot_flag_reg;
}
