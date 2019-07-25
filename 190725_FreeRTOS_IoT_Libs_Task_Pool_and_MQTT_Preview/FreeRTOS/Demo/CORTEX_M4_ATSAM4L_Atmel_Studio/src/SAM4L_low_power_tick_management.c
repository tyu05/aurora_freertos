/*
 * FreeRTOS Kernel V10.2.1
 * Copyright (C) 2019 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * http://www.FreeRTOS.org
 * http://aws.amazon.com/freertos
 *
 * 1 tab == 4 spaces!
 */

/* Standard includes. */
#include "limits.h"

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"

/* Library includes. */
#include <asf.h>


/*
 * When configCREATE_LOW_POWER_DEMO is set to 1 then the tick interrupt
 * is generated by the AST.  The AST configuration and handling functions are
 * defined in this file.
 *
 * When configCREATE_LOW_POWER_DEMO is set to 0 the tick interrupt is
 * generated by the standard FreeRTOS Cortex-M port layer, which uses the
 * SysTick timer.
 */
#if configCREATE_LOW_POWER_DEMO == 1

/* Constants required to pend a PendSV interrupt from the tick ISR if the
preemptive scheduler is being used.  These are just standard bits and registers
within the Cortex-M core itself. */
#define portNVIC_PENDSVSET_BIT	( 1UL << 28UL )

/* The alarm used to generate interrupts in the asynchronous timer. */
#define portAST_ALARM_CHANNEL	0

/*-----------------------------------------------------------*/

/*
 * The tick interrupt is generated by the asynchronous timer.  The default tick
 * interrupt handler cannot be used (even with the AST being handled from the
 * tick hook function) because the default tick interrupt accesses the SysTick
 * registers when configUSE_TICKLESS_IDLE set to 1.  AST_ALARM_Handler() is the
 * default name for the AST alarm interrupt.  This definition overrides the
 * default implementation that is weakly defined in the interrupt vector table
 * file.
 */
void AST_ALARM_Handler(void);

/*
 * Functions that disable and enable the AST respectively, not returning until
 * the operation is known to have taken effect.
 */
static void prvDisableAST( void );
static void prvEnableAST( void );

/*-----------------------------------------------------------*/

/* Calculate how many clock increments make up a single tick period. */
static const uint32_t ulAlarmValueForOneTick = ( configSYSTICK_CLOCK_HZ / configTICK_RATE_HZ );

/* Holds the maximum number of ticks that can be suppressed - which is
basically how far into the future an interrupt can be generated. Set
during initialisation. */
static TickType_t xMaximumPossibleSuppressedTicks = 0;

/* Flag set from the tick interrupt to allow the sleep processing to know if
sleep mode was exited because of an AST interrupt or a different interrupt. */
static volatile uint32_t ulTickFlag = pdFALSE;

/* The AST counter is stopped temporarily each time it is re-programmed.  The
following variable offsets the AST counter alarm value by the number of AST
counts that would typically be missed while the counter was stopped to compensate
for the lost time.  _RB_ Value needs calculating correctly. */
static uint32_t ulStoppedTimerCompensation = 2 / ( configCPU_CLOCK_HZ / configSYSTICK_CLOCK_HZ );

/*-----------------------------------------------------------*/

/* The tick interrupt handler.  This is always the same other than the part that
clears the interrupt, which is specific to the clock being used to generate the
tick. */
void AST_ALARM_Handler(void)
{
	/* Protect incrementing the tick with an interrupt safe critical section. */
	( void ) portSET_INTERRUPT_MASK_FROM_ISR();
	{
		if( xTaskIncrementTick() != pdFALSE )
		{
			portNVIC_INT_CTRL_REG = portNVIC_PENDSVSET_BIT;
		}

		/* Just completely clear the interrupt mask on exit by passing 0 because
		it is known that this interrupt will only ever execute with the lowest
		possible interrupt priority. */
	}
	portCLEAR_INTERRUPT_MASK_FROM_ISR( 0 );

	/* The CPU woke because of a tick. */
	ulTickFlag = pdTRUE;

	/* If this is the first tick since exiting tickless mode then the AST needs
	to be reconfigured to generate interrupts at the defined tick frequency. */
	ast_write_alarm0_value( AST, ulAlarmValueForOneTick );

	/* Ensure the interrupt is clear before exiting. */
	ast_clear_interrupt_flag( AST, AST_INTERRUPT_ALARM );
}
/*-----------------------------------------------------------*/

/* Override the default definition of vPortSetupTimerInterrupt() that is weakly
defined in the FreeRTOS Cortex-M3 port layer with a version that configures the
asynchronous timer (AST) to generate the tick interrupt. */
void vPortSetupTimerInterrupt( void )
{
struct ast_config ast_conf;

	/* Ensure the AST can bring the CPU out of sleep mode. */
	sleepmgr_lock_mode( SLEEPMGR_RET );

	/* Ensure the 32KHz oscillator is enabled. */
	if( osc_is_ready( OSC_ID_OSC32 ) == pdFALSE )
	{
		osc_enable( OSC_ID_OSC32 );
		osc_wait_ready( OSC_ID_OSC32 );
	}

	/* Enable the AST itself. */
	ast_enable( AST );

	ast_conf.mode = AST_COUNTER_MODE;  /* Simple up counter. */
	ast_conf.osc_type = AST_OSC_32KHZ;
	ast_conf.psel = 0; /* No prescale so the actual frequency is 32KHz/2. */
	ast_conf.counter = 0;
	ast_set_config( AST, &ast_conf );

	/* The AST alarm interrupt is used as the tick interrupt.  Ensure the alarm
	status starts clear. */
	ast_clear_interrupt_flag( AST, AST_INTERRUPT_ALARM );

	/* Enable wakeup from alarm 0 in the AST and power manager.  */
	ast_enable_wakeup( AST, AST_WAKEUP_ALARM );
	bpm_enable_wakeup_source( BPM, ( 1 << BPM_BKUPWEN_AST ) );

	/* Tick interrupt MUST execute at the lowest interrupt priority. */
	NVIC_SetPriority( AST_ALARM_IRQn, configLIBRARY_LOWEST_INTERRUPT_PRIORITY);
	ast_enable_interrupt( AST, AST_INTERRUPT_ALARM );
	NVIC_ClearPendingIRQ( AST_ALARM_IRQn );
	NVIC_EnableIRQ( AST_ALARM_IRQn );

	/* Automatically clear the counter on interrupt. */
	ast_enable_counter_clear_on_alarm( AST, portAST_ALARM_CHANNEL );

	/* Start with the tick active and generating a tick with regular period. */
	ast_write_alarm0_value( AST, ulAlarmValueForOneTick );
	ast_write_counter_value( AST, 0 );

	/* See the comments where xMaximumPossibleSuppressedTicks is declared. */
	xMaximumPossibleSuppressedTicks = ULONG_MAX / ulAlarmValueForOneTick;
}
/*-----------------------------------------------------------*/

static void prvDisableAST( void )
{
	while( ast_is_busy( AST ) )
	{
		/* Nothing to do here, just waiting. */
	}
	AST->AST_CR &= ~( AST_CR_EN );
	while( ast_is_busy( AST ) )
	{
		/* Nothing to do here, just waiting. */
	}
}
/*-----------------------------------------------------------*/

static void prvEnableAST( void )
{
	while( ast_is_busy( AST ) )
	{
		/* Nothing to do here, just waiting. */
	}
	AST->AST_CR |= AST_CR_EN;
	while( ast_is_busy( AST ) )
	{
		/* Nothing to do here, just waiting. */
	}
}
/*-----------------------------------------------------------*/

/* Override the default definition of vPortSuppressTicksAndSleep() that is weakly
defined in the FreeRTOS Cortex-M3 port layer with a version that manages the
asynchronous timer (AST), as the tick is generated from the low power AST and
not the SysTick as would normally be the case on a Cortex-M. */
void vPortSuppressTicksAndSleep( TickType_t xExpectedIdleTime )
{
uint32_t ulAlarmValue, ulCompleteTickPeriods, ulInterruptStatus;
eSleepModeStatus eSleepAction;
TickType_t xModifiableIdleTime;
enum sleepmgr_mode xSleepMode;

	/* THIS FUNCTION IS CALLED WITH THE SCHEDULER SUSPENDED. */

	/* Make sure the AST reload value does not overflow the counter. */
	if( xExpectedIdleTime > xMaximumPossibleSuppressedTicks )
	{
		xExpectedIdleTime = xMaximumPossibleSuppressedTicks;
	}

	/* Calculate the reload value required to wait xExpectedIdleTime tick
	periods. */
	ulAlarmValue = ulAlarmValueForOneTick * xExpectedIdleTime;
	if( ulAlarmValue > ulStoppedTimerCompensation )
	{
		/* Compensate for the fact that the AST is going to be stopped
		momentarily. */
		ulAlarmValue -= ulStoppedTimerCompensation;
	}

	/* Stop the AST momentarily.  The time the AST is stopped for is accounted
	for as best it can be, but using the tickless mode will inevitably result in
	some tiny drift of the time maintained by the kernel with respect to
	calendar time. */
	prvDisableAST();

	/* Enter a critical section but don't use the taskENTER_CRITICAL() method as
	that will mask interrupts that should exit sleep mode. */
	ulInterruptStatus = cpu_irq_save();

	/* The tick flag is set to false before sleeping.  If it is true when sleep
	mode is exited then sleep mode was probably exited because the tick was
	suppressed for the entire xExpectedIdleTime period. */
	ulTickFlag = pdFALSE;

	/* If a context switch is pending then abandon the low power entry as
	the context switch might have been pended by an external interrupt that
	requires processing. */
	eSleepAction = eTaskConfirmSleepModeStatus();
	if( eSleepAction == eAbortSleep )
	{
		/* Restart tick. */
		prvEnableAST();

		/* Re-enable interrupts - see comments above the cpsid instruction()
		above. */
		cpu_irq_restore( ulInterruptStatus );
	}
	else
	{
		/* Adjust the alarm value to take into account that the current time
		slice is already partially complete. */
		ulAlarmValue -= ast_read_counter_value( AST );
		ast_write_alarm0_value( AST, ulAlarmValue );

		/* Restart the AST. */
		prvEnableAST();

		/* Allow the application to define some pre-sleep processing. */
		xModifiableIdleTime = xExpectedIdleTime;
		configPRE_SLEEP_PROCESSING( xModifiableIdleTime );

		/* xExpectedIdleTime being set to 0 by configPRE_SLEEP_PROCESSING()
		means the application defined code has already executed the WAIT
		instruction. */
		if( xModifiableIdleTime > 0 )
		{
			/* Find the deepest allowable sleep mode. */
			xSleepMode = sleepmgr_get_sleep_mode();

			if( xSleepMode != SLEEPMGR_ACTIVE )
			{
				/* Sleep until something happens. */
				bpm_sleep( BPM, xSleepMode );
			}
		}

		/* Allow the application to define some post sleep processing. */
		configPOST_SLEEP_PROCESSING( xModifiableIdleTime );

		/* Stop AST.  Again, the time the SysTick is stopped for is	accounted
		for as best it can be, but using the tickless mode will	inevitably
		result in some tiny drift of the time maintained by the	kernel with
		respect to calendar time. */
		prvDisableAST();

		/* Re-enable interrupts - see comments above the cpsid instruction()
		above. */
		cpu_irq_restore( ulInterruptStatus );

		if( ulTickFlag != pdFALSE )
		{
			/* The tick interrupt has already executed, although because this
			function is called with the scheduler suspended the actual tick
			processing will not occur until after this function has exited.
			Reset the alarm value with whatever remains of this tick period. */
			ulAlarmValue = ulAlarmValueForOneTick - ast_read_counter_value( AST );
			ast_write_alarm0_value( AST, ulAlarmValue );

			/* The tick interrupt handler will already have pended the tick
			processing in the kernel.  As the pending tick will be processed as
			soon as this function exits, the tick value	maintained by the tick
			is stepped forward by one less than the	time spent sleeping.  The
			actual stepping of the tick appears later in this function. */
			ulCompleteTickPeriods = xExpectedIdleTime - 1UL;
		}
		else
		{
			/* Something other than the tick interrupt ended the sleep.  How
			many complete tick periods passed while the processor was
			sleeping? */
			ulCompleteTickPeriods = ast_read_counter_value( AST ) / ulAlarmValueForOneTick;

			/* The alarm value is set to whatever fraction of a single tick
			period remains. */
			ulAlarmValue = ast_read_counter_value( AST ) - ( ulCompleteTickPeriods * ulAlarmValueForOneTick );
			if( ulAlarmValue == 0 )
			{
				/* There is no fraction remaining. */
				ulAlarmValue = ulAlarmValueForOneTick;
				ulCompleteTickPeriods++;
			}
			ast_write_counter_value( AST, 0 );
			ast_write_alarm0_value( AST, ulAlarmValue );
		}

		/* Restart the AST so it runs up to the alarm value.  The alarm value
		will get set to the value required to generate exactly one tick period
		the next time the AST interrupt executes. */
		prvEnableAST();

		/* Wind the tick forward by the number of tick periods that the CPU
		remained in a low power state. */
		vTaskStepTick( ulCompleteTickPeriods );
	}
}


#endif /* configCREATE_LOW_POWER_DEMO == 1 */

