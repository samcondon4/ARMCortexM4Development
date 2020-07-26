/*********************************************************************************
 * TSIModule.c - A TSI module that runs under MicroC/OS for the two touch sensors
 * onboard the K65TWR Development kit.
 * This version provides public resources TSIInit, and TSIPend. TSIInit simply
 * initializes the touch sensor electrodes while TSIPend pends on a semaphore
 * that is posted after both the right and left electrodes have been scanned.
 * It then returns an array that contains the state of both the left and right
 * electrodes.
 *
 * Requires the following be defined in app_cfg.h
 * 			APP_CFG_TSI_TASK_PRIO
 *			APP_CFG_TSI_TASK_STK_SIZE
 *
 *02/22/2020 Original TSIModule.c complete, Sam Condon
 *********************************************************************************/
/*********************************************************************
* Header Files - Dependencies
********************************************************************/
#include "MCUType.h"
#include "app_cfg.h"
#include "os.h"
#include "TSIModule.h"
#include "K65TWR_GPIO.h"
#include "MK65F18.h"

/*********************************************************************
* Module defines
********************************************************************/
#define RIGHT_TOUCH_OFFSET 0x01ff
#define LEFT_TOUCH_OFFSET 0x01ff
#define LEFT_CH 12u //TSI left electrode channel
#define RIGHT_CH 11u //TSI right electrode channel
#define LEFT_IND 1u//TSI left electrode index into status data
#define RIGHT_IND 0u//TSI right electrode index into status data

/**********************************************************************
 * TSI_T: TSI status struct, holds threshold and status values of each
 * 		  electrode
 *
 * 		  index 0 holds data for the right electrode
 * 		  index 1 holds data for the left electrode
 * Sam Condon, 02/22/2020 
 **********************************************************************/
typedef struct{
	INT16U tsiTouchLevels[2]; //array that holds oscillator count threshold
	INT8U tsiStates[2]; //array that holds state of each electrode
	OS_SEM flag;
}TSI_T;

/********************************************************************
* Private Resources
********************************************************************/
static TSI_T tsiBuffer;
static void TSITask(void *p_arg);
static OS_TCB TSITaskTCB;
static CPU_STK TSITaskStk[APP_CFG_TSI_TASK_STK_SIZE];
OS_SEM ElectrodeScanCompleteFlag;


/********************************************************************
* TSIInit() - Initialization routine for the TSI module.
*             First a calibration is run to test the oscillator
*             count when there is no touch. Offsets defined by
*             RIGHT_TOUCH_OFFSET and LEFT_TOUCH_OFFSET are then
*             added to those measured levels
*
* Sam Condon, 02/22/2020
********************************************************************/
void TSIInit(void){

	OS_ERR os_err;

	INT16U baselinelevelleft;
	INT16U baselinelevelright;

	//Configure SCGC and GPIO Port////////////////////////
	SIM->SCGC5 |= SIM_SCGC5_PORTB_MASK;
	SIM->SCGC5 |= SIM_SCGC5_TSI(1);
	PORTB->PCR[18] &= (0xffffffff & PORT_PCR_MUX(0x0U));
	PORTB->PCR[19] &= (0xffffffff & PORT_PCR_MUX(0x0U));
	//////////////////////////////////////////////////////

	//CONFIGURE TSI/////////////////////////
	TSI0->GENCS |= TSI_GENCS_REFCHRG(5U);
	TSI0->GENCS |= TSI_GENCS_DVOLT(1U);
	TSI0->GENCS |= TSI_GENCS_EXTCHRG(5U);
	TSI0->GENCS |= TSI_GENCS_PS(5U);
	TSI0->GENCS |= TSI_GENCS_NSCN(15U);
	TSI0->GENCS |= TSI_GENCS_TSIEN_MASK;

	//TSI0 LEFT ELECTRODE CALIBRATION//////////////////////////////////////////////////////////////////////
	TSI0->DATA = TSI_DATA_TSICH(LEFT_CH);
	TSI0->DATA |= TSI_DATA_SWTS(1);

	while((TSI0->GENCS & TSI_GENCS_EOSF_MASK) == 0U){
		//wait for scan to complete
	}
	TSI0->GENCS |= TSI_GENCS_EOSF(1); //clear end of scan flag

	baselinelevelleft = (INT16U)(TSI0->DATA & TSI_DATA_TSICNT_MASK);
	tsiBuffer.tsiTouchLevels[LEFT_IND] = (INT16U)(baselinelevelleft + LEFT_TOUCH_OFFSET);
	////////////////////////////////////////////////////////////////////////////////////////////////////

	//TSI0 RIGHT ELECTRODE CALIBRATION///////////////////////////////////////////////////////////////////////
	TSI0->DATA = TSI_DATA_TSICH(RIGHT_CH);
	TSI0->DATA |= TSI_DATA_SWTS(1);

	while((TSI0->GENCS & TSI_GENCS_EOSF_MASK) == 0U){
		//wait for scan to complete
	}
	TSI0->GENCS |= TSI_GENCS_EOSF(1);

	baselinelevelright = (INT16U)(TSI0->DATA & TSI_DATA_TSICNT_MASK);
	tsiBuffer.tsiTouchLevels[RIGHT_IND] = (INT16U)(baselinelevelright + RIGHT_TOUCH_OFFSET);
	//////////////////////////////////////////////////////////////////////////////////////////////////////

	//CREATE SEMAPHOR FLAGS//////////////////////////////////////////////////////////////////////
	OSSemCreate(&(tsiBuffer.flag), "TSI Scan Complete Semaphore", 0, &os_err);
	OSSemCreate(&ElectrodeScanCompleteFlag, "Electrode Scan Complete Semaphore", 0u, &os_err);
	//////////////////////////////////////////////////////////////////////////////////////////////

	//CREATE TSI TASK///////////////////
	OSTaskCreate((OS_TCB*)&TSITaskTCB,
	             (CPU_CHAR*)"TSI Task",
	             (OS_TASK_PTR)TSITask,
	             (void*) 0,
	             (OS_PRIO) APP_CFG_TSI_TASK_PRIO,
	             (CPU_STK*)&TSITaskStk[0],
	             (CPU_STK)(APP_CFG_TSI_TASK_STK_SIZE / 10u),
	             (CPU_STK_SIZE) APP_CFG_TSI_TASK_STK_SIZE,
	             (OS_MSG_QTY) 0,
	             (OS_TICK) 0,
	             (void*) 0,
	             (OS_OPT)(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
	             (OS_ERR*)&os_err);
	////////////////////////////////////

}

/********************************************************************
* TSIPend() - A function to provide access to the TSI buffer via a
*             semaphore.
*    -Public
*
* Sam Condon, 02/24/2020
********************************************************************/
INT8U* TSIPend(INT16U tout, OS_ERR *os_err){
	OSSemPend(&(tsiBuffer.flag),tout, OS_OPT_PEND_BLOCKING, (CPU_TS *)0, os_err);
	return(tsiBuffer.tsiStates);
}

/********************************************************************
* TSITask() - Scan right and left electrodes, update the TSI buffer,
* 			  post semaphore that TSIPend waits on.
*
*  	 -Private
* Sam Condon, 02/24/2020
********************************************************************/
static void TSITask(void *p_arg){

	OS_ERR os_err;

	INT8U electrodeindex = RIGHT_CH;
	
	(void)p_arg;

	while(1){


		TSI0->GENCS |= TSI_GENCS_EOSF(1);
		TSI0->DATA = TSI_DATA_TSICH(electrodeindex);
		TSI0->DATA |= TSI_DATA_SWTS(1);

		while((TSI0->GENCS & TSI_GENCS_EOSF_MASK) == 0U){
		    //wait for scan to complete
		    DB2_TURN_OFF();
		    OSTimeDly(8u, OS_OPT_TIME_DLY, &os_err);
		    DB2_TURN_ON();
		}

		if((INT16U)(TSI0->DATA & TSI_DATA_TSICNT_MASK) >= tsiBuffer.tsiTouchLevels[electrodeindex-11u]){
			tsiBuffer.tsiStates[electrodeindex-11u] = 1U;
		}
		else{
			tsiBuffer.tsiStates[electrodeindex-11u] = 0U;
		}
		if(electrodeindex == LEFT_CH){
			electrodeindex = RIGHT_CH;
			OSSemPost(&(tsiBuffer.flag), OS_OPT_POST_1, &os_err);
		}
		else{
			electrodeindex = LEFT_CH;
		}


	}
}


