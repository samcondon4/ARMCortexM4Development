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
 *02/22/2020 Sam Condon, Original TSIModule.c complete  
 *********************************************************************************/

#ifndef TSIMODULE_H_
#define TSIMODULE_H_

//PUBLIC RESOURCES///////////////////////////////////////////////////////

/********************************************************************
* TSIInit() - Initialization routine for the TSI module.
*             First a calibration is run to test the oscillator
*             count when there is no touch. Offsets defined by
*             RIGHT_TOUCH_OFFSET and LEFT_TOUCH_OFFSET are then
*             added to those measured levels
*
* Sam Condon, 02/22/2020
********************************************************************/
void TSIInit(void);		

/********************************************************************
* TSIPend() - A function to provide access to the TSI buffer after a new
* 	      scan is run.
*
* Sam Condon, 02/22/2020
********************************************************************/
INT8U* TSIPend(INT16U tout, OS_ERR *os_err);		//Pend on a TSI press
/////////////////////////////////////////////////////////////////////////

#endif /* TSIMODULE_H_ */
