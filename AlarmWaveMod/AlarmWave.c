/********************************************************************************
* AlarmWave - This module controls the PIT and DAC based on the state
*			  of a public variable AlarmState.  When AlarmState = ALARM_ON
*			  the DAC outputs a 300 hz. sin wave that turns on and off
*			  with a period of 1s.
*
* Sam Condon, 11/11/2019
***********************************************************************************/

//INCLUDE DEPENDENCIES////////
#include "MCUType.h"
#include "MK65F18.h"
#include "K65TWR_GPIO.h"
#include "AlarmWave.h"
#include "Lab4Main.h"
#include "SysTickDelay.h"
#include "BasicIO.h"
//////////////////////////////

//DEFINES AND MACROS////////////////////////////////////////////////////////////////////
#define LD_VAL0 3119U;
#define TMR_ENABLE 0x00000001
#define TMR_DISABLE ~TMR_ENABLE

#define TMR_TCTRL(channel) (PIT->CHANNEL[channel].TCTRL)
#define TMR_EN(channel) (INT8U)(PIT->CHANNEL[channel].TCTRL & PIT_TCTRL_TEN_MASK)

#define DAC_SHIFT2 12U
#define DAC_EN2(x) (1U<<(DAC_SHIFT2 + x))

#define DAC0_SHIFT6 31U
#define DAC0_EN6 (1U<<(DAC0_SHIFT6))
////////////////////////////////////////////////////////////////////////////////////////

//PRIVATE PROTOTYPES///////////////////////////////////////////////////////////////////
static void ToggleTimerState(INT8U channel);
static void EnableTimer(INT8U channel);
static void DisableTimer(INT8U channel);
static void DacIdle(void);

void PIT0_IRQHandler(void);
///////////////////////////////////////////////////////////////////////////////////////

//DAC WRITE VALUES////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static const INT16U dac_buf[] = {2047U,2247U,2446U,2641U,2830U,3012U,3184U,3346U,3495U,3630U,3749U,3853U,3939U,4006U,4055U,4085U,
								 4095U,4085U,4055U,4006U,3939U,3853U,3749U,3630U,3495U,3346U,3184U,3012U,2830U,2641U,2446U,2247U,
								 2047U,1846U,1647U,1452U,1263U,1081U,909U,747U,598U,463U,344U,240U,154U,87U,38U,9U,0U,9U,38U,87U,
								 154U,240U,344U,463U,598U,747U,909U,1081U,1263U,1452U,1647U,1846U};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*****************************************************************************
 * AlamWaveInit() - Initialize PIT timer, NVIC and DAC to send sin wave to speaker.
 * 					PIT timer is initialized but not enabled through this function.
 * 					Enabled later.
 *
 ******************************************************************************/
void AlarmWaveInit(void){
	////INITIALIZE PIT TIMER////////////
	SIM->SCGC6 |= SIM_SCGC6_PIT(1);
	PIT->MCR = PIT->MCR & ~(0x2U);
	PIT->CHANNEL[0].LDVAL = LD_VAL0;
	PIT->CHANNEL[0].TCTRL |= 2U;
	NVIC_EnableIRQ(PIT0_IRQn);
	/////////////////////////////////////

	////INITIALIZE DAC///////////////////
	SIM->SCGC2 |= DAC_EN2(0);
	SIM->SCGC6 |= DAC0_EN6;
	DAC0->C0 |= (DAC_C0_DACEN_MASK|DAC_C0_DACRFS_MASK|DAC_C0_DACTRGSEL_MASK);
	DacIdle();
	//////////////////////////////////////
}

/*********************************************************************************
 * AlarmWaveControlTask() - Enable/disable PIT timer based on AlarmState value.
 *
 ********************************************************************************/
void AlarmWaveControlTask(void){

	static INT32U tscount = 0; //time-slice counter
	static INT32U awctcount = 0; //AlarmWaveControlTask() counter

	tscount++;

	if(tscount >= 5U){
		DB3_TURN_ON();
		tscount = 0;
		switch(AlarmState)
		{
			case(ALARM_OFF):
				awctcount = 21U; //so that PIT timer starts right after ALARM_ON state is entered
				if(TMR_EN(0) != 0){
					DisableTimer(0U);
				}
				else{}
			break;

			case(ALARM_ON):
				awctcount++;
				if(awctcount >= 20U){
					awctcount = 0;
					ToggleTimerState(0U);
				}
			break;
		}
		DB3_TURN_OFF();
	}


}

/*************************************************
 * ToggleTimerState() - Toggle PIT timer on/off to turn on/off the
 * 						sin wave output
 *************************************************/
static void ToggleTimerState(INT8U channel){
	if(TMR_EN(channel) == 0U){
		EnableTimer(0U);
	}

	else{
		DisableTimer(0U);
	}
}

/***************************************************
 * EnableTimer() - Turn on PIT timer.
 *
 ***************************************************/
static void EnableTimer(INT8U channel){
	PIT->CHANNEL[channel].TCTRL |= TMR_ENABLE;
}

/***************************************************
 * DisableTimer() - Turn off PIT timer.
 *
 ***************************************************/
static void DisableTimer(INT8U channel){
	PIT->CHANNEL[channel].TCTRL &= TMR_DISABLE;
	DacIdle();
}

/*****************************************************
 * DacIdle() - Write 0.5 of wave full scale to set a
 * 			   1.65 V. idle output from DAC0
 *
 *****************************************************/
static void DacIdle(void){
	DAC0->DAT[0].DATH = (dac_buf[0]>>8);
	DAC0->DAT[0].DATL = (INT8U)(dac_buf[0]);
}

/******************************************************
 * PIT0_IRQHandler() - Update DAC output from the dac_buf
 * 					   constant table.
 *
 ******************************************************/
void PIT0_IRQHandler(void){
	static INT8U datindex = 0;

	DB4_TURN_ON();
	PIT->CHANNEL[0].TFLG |= PIT_TFLG_TIF(1);

	if(datindex > 63){
		datindex = 0;
	}
	else{}

	//Write value to DAC//
	DAC0->DAT[0].DATH = (dac_buf[datindex]>>8);
	DAC0->DAT[0].DATL = (INT8U)(dac_buf[datindex]);
	//////////////////////

	datindex++;

	DB4_TURN_OFF();
}

