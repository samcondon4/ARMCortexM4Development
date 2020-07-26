//TIME MODULE///////////////////////////////////////////////////////////

/****************************************************************
 * The time module contains all public resources used to set, get,
 * and pend the current time.  It also contains all initialization
 * for the RTC that is used to post the timeSecFlag, which tells the
 * timeTask to increment the current timeOfDay and post the timeChgFlag
 ****************************************************************/

#include "MCUType.h"
#include "os.h"
#include "scTime.h"
#include "app_cfg.h"
#include "MK65F18.h"
#include "K65TWR_GPIO.h"

static OS_TCB TimeTaskTCB;
static CPU_STK TimeTaskStk[APP_CFG_TIME_TASK_STK_SIZE];

//MODULE GLOBALS/////////////////////
TIME_T timeOfDay;

//MUTEX KEY FOR timeOfDay RESOURCE//
static OS_MUTEX timeMutexKey;

//SEMAPHORS////////
OS_SEM timeSecFlag;
OS_SEM timeChgFlag;

//PRIVATE PROTOTYPES////////////////
static void timeTask(void);
void RTC_Seconds_IRQHandler(void); //irq handler for the rtc interrupt occuring every second

/***********************************************
 * TimeInit() - Initialize mutex, semaphors, and RTC
 *
 ************************************************/
void TimeInit(void){
	OS_ERR os_err;

	//CREATE MUTEX//
	OSMutexCreate(&timeMutexKey, "timeOfDay mutex", &os_err);

	//CREATE SEMAPHORS//
	OSSemCreate(&timeSecFlag, "Time Second Alert Flag", 0, &os_err);
	OSSemCreate(&timeChgFlag, "Time Change Flag", 0, &os_err);

	//ENABLE RTC AND TIME SECONDS INTERRUPT////////////////////////////
	SIM->SCGC6 |= SIM_SCGC6_RTC_MASK; //enable RTC clock and interrupts
	RTC->CR |= RTC_CR_SWR_MASK; //reset RTC registers
	RTC->CR &= ~(RTC_CR_SWR_MASK);
	RTC->CR |= RTC_CR_OSCE_MASK; //enable clock oscillator
	RTC->TSR &= ~RTC_TSR_TSR_MASK; //write to clear TIF and TOF flags and provide time for oscillator stabilization
	RTC->SR |= RTC_SR_TCE_MASK;
	RTC->IER |= RTC_IER_TSIE_MASK;  //enable the time seconds interrupt

	//ENABLE RTC IRQ///////////////////////////////////////////////////
	NVIC_ClearPendingIRQ(RTC_Seconds_IRQn);
	NVIC_EnableIRQ(RTC_Seconds_IRQn);
	__enable_irq();
	///////////////////////////////////////////////////////////////////

	//INITIALIZE TIME OF DAY VALUES//
	timeOfDay.hr = 12u;
	timeOfDay.min = 0u;
	timeOfDay.sec = 0u;

	//Create Time Task
	OSTaskCreate(&TimeTaskTCB,
			     "Time Task",
				 timeTask,
				 (void *) 0,
				 APP_CFG_TIME_TASK_PRIO,
				 &TimeTaskStk[0],
				 (APP_CFG_TIME_TASK_STK_SIZE/10u),
				 APP_CFG_TIME_TASK_STK_SIZE,
				 0u,
				 0u,
				 (void *) 0,
				 (OS_OPT_TASK_NONE),
				 &os_err
				 );
}

/***********************************************************************************
 * TimeTask() - Pend on timeSecFlag and increment timeOfDay structure every second
 *
 ************************************************************************************/
static void timeTask(void){

	OS_ERR os_err;

	while(1){

		DB3_TURN_OFF();
		//Pend on semaphore that will be posted by the RTC Seconds IRQ Handler
		(void)OSSemPend(&timeSecFlag, 0u, OS_OPT_PEND_BLOCKING, (void *)0, &os_err);
		DB3_TURN_ON();

		OSMutexPend(&timeMutexKey, 0, OS_OPT_PEND_BLOCKING, (void *)0, &os_err);
		INT8U scarry = 0u;
		INT8U mincarry = 0u;

		if(timeOfDay.sec >= 59u){
			scarry = 1u;
			timeOfDay.sec = 0u;
		}
		else{
			scarry = 0u;
			timeOfDay.sec += 1;
		}

		if(scarry == 1u){
			if(timeOfDay.min >= 59u){
				mincarry = 1u;
				timeOfDay.min = 0u;
			}
			else{
				mincarry = 0u;
				timeOfDay.min += 1u;
			}
		}
		else{}

		if(mincarry == 1u){
			if(timeOfDay.hr >= 23u){
				timeOfDay.hr = 0u;
			}
			else{
				timeOfDay.hr += 1u;
			}
		}
		else{}
		OSMutexPost(&timeMutexKey, OS_OPT_POST_NONE, &os_err);

		(void)OSSemPost(&timeChgFlag, OS_OPT_POST_NONE, &os_err); //post semaphore alerting DispTimeTask that the time has been updated
	}

}


/* TimePend - Copies curent timeOfDay to *ltime when change is signaled*/
void TimePend(TIME_T *ltime, OS_ERR *p_err){

	(void)OSSemPend(&timeChgFlag, 0u, OS_OPT_PEND_BLOCKING, (void *)0, p_err);
	OSMutexPend(&timeMutexKey, 0, OS_OPT_PEND_BLOCKING, (void *)0, p_err);
	ltime->hr = timeOfDay.hr;
	ltime->min = timeOfDay.min;
	ltime->sec = timeOfDay.sec;
	OSMutexPost(&timeMutexKey, OS_OPT_POST_NONE, p_err);

}

/* TimeSet - Copies *ltime to timeOfDay*/
void TimeSet(TIME_T *ltime){

	OS_ERR os_err;

	OSMutexPend(&timeMutexKey, 0, OS_OPT_PEND_BLOCKING, (void *)0, &os_err);
	timeOfDay.hr = ltime->hr;
	timeOfDay.min = ltime->min;
	timeOfDay.sec = ltime->sec;
	OSMutexPost(&timeMutexKey, OS_OPT_POST_NONE, &os_err);
}

/* TimeGet - Copies current timeOfDay to *ltime */
void TimeGet(TIME_T *ltime){

	OS_ERR os_err;

	OSMutexPend(&timeMutexKey, 0, OS_OPT_PEND_BLOCKING, (CPU_TS *)0, &os_err);
	ltime->hr = timeOfDay.hr;
	ltime->min = timeOfDay.min;
	ltime->sec = timeOfDay.sec;
	OSMutexPost(&timeMutexKey, OS_OPT_POST_NONE, &os_err);

}


/* RTC_Seconds_IRQHandler simply posts the timeSecFlag so that DispTimeTask will increment*/
void RTC_Seconds_IRQHandler(void){

	OS_ERR os_err;
	(void)OSSemPost(&timeSecFlag, OS_OPT_POST_NONE, &os_err); //post semaphore letting timeTask know that a second has elapsed

}


