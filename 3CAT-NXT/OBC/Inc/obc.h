/*
 * obc.h
 *
 *  Created on: Nov 9, 2022
 *      Author: NilRi
 */

#ifndef INC_OBC_H_
#define INC_OBC_H_

#include "FreeRTOS.h"
#include "task.h"
#include "event_groups.h"

#include "events.h"

#include "flash.h"
#include "clockSpeedShift.h"
#include "notifications.h"

#include "definitions.h"
#include "stm32l4xx_hal.h"
#include "stm32l4xx_hal_tim.h"

extern RTC_HandleTypeDef hrtc;
extern TIM_HandleTypeDef htim7;

extern TaskHandle_t COMMS_Handle;
extern TaskHandle_t ADCS_Handle;
extern TaskHandle_t PAYLOAD_Handle;

extern EventGroupHandle_t xEventGroup;

void OBC_Init();
void OBC_Nominal();
void OBC_Contingency();
void OBC_Sunsafe();
void OBC_Survival();


#endif /* INC_OBC_H_ */
