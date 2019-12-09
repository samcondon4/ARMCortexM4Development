/*****************************************************************************************************
 * Sense - Module containing initialization routine for TSI touch senors.
 * 		   Module also contains task to control sampling and scanning of touch
 * 		   sensors.
 *
 * Sam Condon, 11/25/2019
 ******************************************************************************************************/

//INCLUDE DEPENDENCIES////////
#include "MCUType.h"
#include "K65TWR_GPIO.h"
#include "Lab5Main.h"
#include "Sense.h"
//////////////////////////////

//DEFINES FOR ELECTRODE 1 AND 2 ARRAY INDEXING AND OFFSET//
#define ELECTRODE1 0
#define ELECTRODE2 1

#define E1_TOUCH_OFFSET 0x008f
#define E2_TOUCH_OFFSET 0x09ff
///////////////////////////////////////////////////////////

//TYPEDEFS///////////////////////////////////////////////////
typedef enum{START, ELECTRODE_2, WRITE}SENSE_TASK_STATE_T;
/////////////////////////////////////////////////////////////

/**********************************************************************
 * SensorTask() - Runs the SENSE_TASK state machine
 *
 *	States:
 *
 *		-START: Starting state starts a scan on electrode 1.  This is only run once on reset
 *		-ELECTRODE_2: Waits for the eosf from an electrode 1 scan, then starts a scan on electrode 2
 *		-WRITE: Waits for eosf from electrode 2 scan, updates public SenseState struct,
 *				then starts a scan on electrode 1
 *
 * 11/25/2019
 **********************************************************************/
void SensorTask(void){

	static SENSE_TASK_STATE_T sensetaskstate = START;

	static INT8U electrode1state;
	static INT8U electrode2state;

	DB3_TURN_ON();
	switch(sensetaskstate)
	{

		case(START):
			//UNCONDITIONALLY START A SCAN OF ELECTRODE 1///////////////
			L5mAlarmFlags = 0U;

			TSI0->DATA = TSI_DATA_TSICH(11U); //select electrode 1
			TSI0->DATA |= TSI_DATA_SWTS(1); //set scan trigger
			sensetaskstate = ELECTRODE_2;
			////////////////////////////////////////////////////////////
		break;

		case(ELECTRODE_2):

			//L5mAlarmFlags = 0U;
			SSenseState.prevElectrodeState = SSenseState.electrodeState;

			if((TSI0->GENCS & TSI_GENCS_EOSF_MASK) != 0U){
				TSI0->GENCS |= TSI_GENCS_EOSF(1);
				if((INT16U)(TSI0->DATA & TSI_DATA_TSICNT_MASK) >= SSenseState.tsiTouchLevels[ELECTRODE1]){
					electrode1state = 1U;
				}
				else{
					electrode1state = 0U;
				}

				sensetaskstate = WRITE;
				TSI0->DATA = TSI_DATA_TSICH(12U);
				TSI0->DATA |= TSI_DATA_SWTS(1);
			}
			else{}

		break;

		case(WRITE):

			if((TSI0->GENCS & TSI_GENCS_EOSF_MASK) != 0U){
				TSI0->GENCS |= TSI_GENCS_EOSF(1);
				if((INT16U)(TSI0->DATA & TSI_DATA_TSICNT_MASK) >= SSenseState.tsiTouchLevels[ELECTRODE2]){
					electrode2state = 1U;
				}
				else{
					electrode2state = 0U;
				}

				SSenseState.electrodeState = (2*electrode1state + electrode2state);

				if(SSenseState.electrodeState > 0){
					L5mAlarmFlags = TOUCH;
				}

				sensetaskstate = ELECTRODE_2;
				TSI0->DATA = TSI_DATA_TSICH(11U);
				TSI0->DATA |= TSI_DATA_SWTS(1);
			}
			else{}

		break;

	}

	DB3_TURN_OFF();

}



/**********************************************************************
 * TSIInit() - Initialize touch sensors on K65TWR board
 *
 *	Parametes:
 *		sensestate - pointer to the TSI structure that will hold sensor
 *					 calibration parameters of touch sensors
 *	Returns: none
 *
 *	11/25/2019
 **********************************************************************/
void TSIInit(TSI* sensestate){
	SIM->SCGC5 |= SIM_SCGC5_PORTB_MASK;
	SIM->SCGC5 |= SIM_SCGC5_TSI(1);
	PORTB->PCR[18] &= (0xffffffff & PORT_PCR_MUX(0x0U));
	PORTB->PCR[19] &= (0xffffffff & PORT_PCR_MUX(0x0U));


	//CONFIGURE TSI/////////////////////////
	TSI0->GENCS |= TSI_GENCS_TSIEN_MASK;
	TSI0->GENCS |= TSI_GENCS_REFCHRG(5U);
	TSI0->GENCS |= TSI_GENCS_DVOLT(1U);
	TSI0->GENCS |= TSI_GENCS_EXTCHRG(5U);
	TSI0->GENCS |= TSI_GENCS_PS(5U);
	TSI0->GENCS |= TSI_GENCS_NSCN(15U);

	////////////////////////////////////////

	//TSI0 ELECTRODE2 CALIBRATION//////////////////////////////////////////////////////////////////////
	TSI0->DATA = TSI_DATA_TSICH(11);
	TSI0->DATA |= TSI_DATA_SWTS(1);

	while((TSI0->GENCS & TSI_GENCS_EOSF_MASK) == 0U){
		//wait for scan to complete
	}
	TSI0->GENCS |= TSI_GENCS_EOSF(1); //clear end of scan flag

	sensestate->tsiBaselineLevels[ELECTRODE2] = (INT16U)(TSI0->DATA & TSI_DATA_TSICNT_MASK);
	sensestate->tsiTouchLevels[ELECTRODE2] = (INT16U)(sensestate->tsiBaselineLevels[ELECTRODE2] + E2_TOUCH_OFFSET);
	/////////////////////////////////////////////////////////////////////////////////////////

	//TSI0 ELECTRODE1 CALIBRATION///////////////////////////////////////////////////////////////////////
	TSI0->DATA = TSI_DATA_TSICH(12);
	TSI0->DATA |= TSI_DATA_SWTS(1);

	while((TSI0->GENCS & TSI_GENCS_EOSF_MASK) == 0U){
		//wait for scan to complete
	}
	TSI0->GENCS |= TSI_GENCS_EOSF(1);

	sensestate->tsiBaselineLevels[ELECTRODE1] = (INT16U)(TSI0->DATA & TSI_DATA_TSICNT_MASK);
	sensestate->tsiTouchLevels[ELECTRODE1] = (INT16U)(sensestate->tsiBaselineLevels[ELECTRODE1] + E1_TOUCH_OFFSET);

	L5mAlarmFlags = NONE;
	////////////////////////////////////////////////////////////////
}

