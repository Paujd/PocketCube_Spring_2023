/*
 * state_machine.c
 *
 *  Created on: Nov 9, 2022
 *      Author: NilRi
 */
#include "obc.h"

void OBC_Init()
{
	uint8_t currentState, previousState;

	previousState = _INIT;
	Send_to_WFQueue( &previousState, sizeof(previousState), PREVIOUS_STATE_ADDR, OBCsender);

	currentState = _NOMINAL;
	Send_to_WFQueue(&currentState, sizeof(currentState), CURRENT_STATE_ADDR, OBCsender);
}

void OBC_Nominal()
{
	uint32_t CPUFreq;
	uint8_t currentState, previousState;

	CPUFreq = HAL_RCC_GetHCLKFreq();
	if(CPUFreq!=26000000)
	{
		OBC_SystemClock_26MHz(); // Change CPU frequency to 26MHz
	}

	xEventGroupSync(xEventGroup,   COMMS_START_EVENT,    COMMS_DONE_EVENT, portMAX_DELAY);

	previousState = _NOMINAL;
	Send_to_WFQueue( &previousState, sizeof(previousState), PREVIOUS_STATE_ADDR, OBCsender);
	currentState = _CONTINGENCY;
	Send_to_WFQueue(&currentState, sizeof(currentState), CURRENT_STATE_ADDR, OBCsender);
}

void OBC_Contingency()
{
	uint32_t CPUFreq;
	uint8_t currentState, previousState;

	HAL_PWREx_EnableLowPowerRunMode(); // Enter into LOW POWER RUN MODE

	CPUFreq = HAL_RCC_GetHCLKFreq();
	if(CPUFreq != 26000000)
	{
		OBC_SystemClock_26MHz(); // Change CPU frequency to 26MHz
	}

	Read_Flash(PREVIOUS_STATE_ADDR,&previousState,1);

	 if (previousState ==_SUNSAFE)
	 {
		 vTaskResume(ADCS_Handle   ); // Resume ADCS task
		 vTaskResume(PAYLOAD_Handle); // Resume PAYLOAD task
	 }

	xTaskNotify(COMMS_Handle, CONTINGENCY_NOTI, eSetBits); // Notify COMMS that we have entered the CONTINGENCY state
	xTaskNotify(ADCS_Handle,  CONTINGENCY_NOTI, eSetBits); // Notify ADCS that we have entered the CONTINGENCY state

	xEventGroupSync(xEventGroup,   COMMS_START_EVENT,    COMMS_DONE_EVENT, portMAX_DELAY);

	previousState = _CONTINGENCY;
	Send_to_WFQueue( &previousState, sizeof(previousState), PREVIOUS_STATE_ADDR, OBCsender);
	currentState = _SUNSAFE;
	Send_to_WFQueue(&currentState, sizeof(currentState), CURRENT_STATE_ADDR, OBCsender);
	HAL_PWREx_DisableLowPowerRunMode();
}

void OBC_Sunsafe()
{
	uint32_t CPUFreq;
	uint8_t currentState, previousState;

	CPUFreq = HAL_RCC_GetHCLKFreq();
	if(CPUFreq != 2000000)
	{
		OBC_SystemClock_2MHz(); // Change CPU frequency to 2MHz
	}

	switch(previousState){
		case _CONTINGENCY:
			xTaskNotify(COMMS_Handle   , SUNSAFE_NOTI, eSetBits); // Notify COMMS that we have entered the SUNSAFE state
			xTaskNotify(ADCS_Handle    , SUNSAFE_NOTI, eSetBits); // Notify ADCS that we have entered the SUNSAFE state

			xEventGroupSync(xEventGroup,   COMMS_START_EVENT,    COMMS_DONE_EVENT, portMAX_DELAY);

			vTaskSuspend(ADCS_Handle   ); // Suspend the ADCS task
			vTaskSuspend(PAYLOAD_Handle); // Suspend the PAYLOAD task
		case _SURVIVAL:
			vTaskResume(COMMS_Handle);

			xTaskNotify(COMMS_Handle   , SUNSAFE_NOTI, eSetBits); // Notify COMMS that we have entered the SUNSAFE state
	}

	/****TIMER 7 CONFIGURATION FOR A 10s SLEEP*****/
	TIM7->SR&=~(1<<0); // Clear update interrupt flag
	TIM7->PSC = 20000; // 16-bit Prescaler : TIM2 frequency = CPU_F/PSC = 2.000.000/20.000 = 100 Hz.
	TIM7->ARR = 1000;  // 16-bit Auto Reload Register : ARR = Sleep_time * TIM7_F = 10*100=1000.

	HAL_TIM_Base_Start_IT(&htim7);

	SysTick->CTRL &= ~SysTick_CTRL_TICKINT_Msk; // Suspend the scheduler (systick timer) interrupt to not wake up the MCU
	HAL_SuspendTick();                          // Suspend the tim6 interrupts to not wake up the MCU

	HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI); // Enter in SLEEP MODE with the WFI (Wait For Interrupt) instruction

	/* SLEEPING ~ 10s */

	HAL_TIM_Base_Stop_IT(&htim7);

	SysTick->CTRL  |= SysTick_CTRL_TICKINT_Msk; // Enable the scheduler (systick timer) interrupt
	HAL_ResumeTick();                           // Enable the tim7 interrupt

	previousState = _SUNSAFE;
	Send_to_WFQueue( &previousState, sizeof(previousState), PREVIOUS_STATE_ADDR, OBCsender);
	currentState = _SURVIVAL;
	Send_to_WFQueue(&currentState, sizeof(currentState), CURRENT_STATE_ADDR, OBCsender);
}

void OBC_Survival()
{
	uint32_t CPUFreq;
	uint8_t currentState, previousState;

	CPUFreq = HAL_RCC_GetHCLKFreq();
	if(CPUFreq != 2000000)
	{
		OBC_SystemClock_2MHz(); // Change CPU frequency to 2MHz
	}

	HAL_PWREx_EnableLowPowerRunMode(); // Enter into LOW POWER RUN MODE

	if(previousState == _SUNSAFE)
	{
		xTaskNotify(COMMS_Handle, SUNSAFE_NOTI, eSetBits); // Notify COMMS that we have entered the SURVIVAL state

		xEventGroupSync(xEventGroup,   COMMS_START_EVENT,    COMMS_DONE_EVENT, portMAX_DELAY);

		vTaskSuspend(COMMS_Handle); // Suspend the ADCS task
	 }

	/****TIMER 7 CONFIGURATION FOR A 10s SLEEP*****/
	TIM7->SR&=~(1<<0); // Clear update interrupt flag
	TIM7->PSC = 20000; // 16-bit Prescaler : TIM2 frequency = CPU_F/PSC = 2.000.000/20.000 = 100 Hz.
	TIM7->ARR = 1000;  // 16-bit Auto Reload Register : ARR = Sleep_time * TIM7_F = 10*100=1000.

	HAL_TIM_Base_Start_IT(&htim7);

	SysTick->CTRL &= ~SysTick_CTRL_TICKINT_Msk; // Suspend the scheduler (systick timer) interrupt to not wake up the MCU
	HAL_SuspendTick();                          // Suspend the tim6 interrupts to not wake up the MCU

	HAL_PWR_EnterSLEEPMode(PWR_LOWPOWERREGULATOR_ON, PWR_SLEEPENTRY_WFI); // Enter in SLEEP MODE with the WFI (Wait For Interrupt) instruction

	/* SLEEPING ~ 10s */

	HAL_TIM_Base_Stop_IT(&htim7);
	SysTick->CTRL  |= SysTick_CTRL_TICKINT_Msk;
	HAL_ResumeTick();

	previousState = _SURVIVAL;
	Send_to_WFQueue( &previousState, sizeof(previousState), PREVIOUS_STATE_ADDR, OBCsender);
	currentState = _NOMINAL;
	Send_to_WFQueue(&currentState, sizeof(currentState), CURRENT_STATE_ADDR, OBCsender);
	HAL_PWREx_DisableLowPowerRunMode();
}




