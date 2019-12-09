/********************************************************
 * Cooperative Multitasking Security System
 *
 * 		-This program runs a cooperative multitasking, time-slice scheduled,
 * 		 security system equipped with touch intrusion detection and envionment
 * 		 monitoring.  In the case of a touch intrusion, or the environment
 * 		 reaches above 40 degrees C, or below 0 degrees C, an alarm will sound
 * 		 and LED's will flash corresponding to any touch intrusions.
 *
 *
 * Sam Condon 12/2/2019
 *********************************************************/

#include "MCUType.h"
#include "Lab5Main.h"
#include "CheckSum.h"
#include "K65TWR_ClkCfg.h"
#include "K65TWR_GPIO.h"
#include "Key.h"
#include "BasicIO.h"
#include "LCD.h"
#include "AlarmWave.h"
#include "SysTickDelay.h"
#include "Sense.h"
#include "Temp.h"

//DEFINE CHECKSUM MEMORY RANGE TO TEST//
#define CS_LOW (INT8U*)0x00000000
#define CS_HIGH (INT8U*)0x001fffff
////////////////////////////////////////

//DEFINE UNIT FLAGS FOR READABILITY//
#define CELCIUS 0u
#define FAHRENHEIT 1u
//////////////////////////////////////

//PROTOTYPES/////////////////////////////
static void ControlDisplayTask(void);
static void LEDTask(void);
static void AlarmDisplay(ALARM_FLAGS_T alarmflags);
static void TempDisplay(void);
static void TempOut(void);
////////////////////////////////////////

//LCD CONSTANTS/////////////////////
static const INT8C ArmedStrg[] = "ARMED";
static const INT8C DisarmedStrg[] = "DISARMED";
static const INT8C AlarmStrg[] = "ALARM";
static const INT8C MinusStrg[] = "-";
static const INT8C Degree = 0xDF;
static const INT8C Celcius = 'C';
static const INT8C Fahrenheit = 'F';
static const INT8C Space = ' ';
static const INT8C TempAlarmStrg[] = "TEMP ALARM";
///////////////////////////////////////

//PUBLIC GLOBALS////////////////////////
SYS_STATE_T L5mSysState = ARMED;
SYS_STATE_T L5mPrevSysState = ARMED;
ALARM_FLAGS_T L5mAlarmFlags = NONE;
TSI SSenseState;
TEMP_COM_T TTempCom;
///////////////////////////////////////

void main(void){

	//LOCAL MAIN VARIABLE DEFS/DECS//
	INT16U chksum;
	////////////////////////////////

    BIOOpen(BIO_BIT_RATE_9600);

	//INITIALIZATION/////////////
	K65TWR_BootClock();
	GpioDBugBitsInit();
	GpioLED8Init();
	GpioLED9Init();
	(void)SysTickDlyInit();
	KeyInit();
	LcdInit();
	AlarmWaveInit();
	TSIInit(&SSenseState);
	TempInit();
	////////////////////////////

	//DISPLAY CHECKSUM ON LCD////////////////
	chksum = CSCalc(CS_LOW,CS_HIGH);
	LcdMoveCursor(2U,1U);
	LcdDispByte((INT8U)(chksum>>8));
	LcdMoveCursor(2U,3U);
	LcdDispByte((INT8U)chksum);
	//////////////////////////////////////////

	//SET INITIAL TempTask COMMUNICATION VALUES//
	TTempCom.unit = CELCIUS;
	////////////////////////////////////////////////

	//TIME-SLICE SCHEDULER/////////////////////////////////////////////////////////////////
	LED8_TURN_ON(); //set initial led state
	while(1){
		SysTickWaitEvent(10U);  //time slice set to 10 ms for now.  MAY CHANGE LATER
		KeyTask(); //check for key press
		ControlDisplayTask(); //update system state and write any neccesary values to the LCD
		AlarmWaveControlTask(); //send alarm wave over DAC0 when in the ALARM state
		LEDTask(); //update led's
		SensorTask(); //update TSI structure
		TempTask(); //sample temperature from adc and convert to fahrenheit or celcius
	}
	//////////////////////////////////////////////////////////////////////////////////////
}

/*************************************************************************************************
 * ControlDisplayTask() - This task updates the system state variable L5mSysState and writes any
 * 						  necessary values to the LCD display.  Sets the stage for proper action
 * 						  to be taken after any key press.
 *
 **************************************************************************************************/
static void ControlDisplayTask(void){

	static INT32U cdtcount = 0U; //time slice counter
	static INT8U statechange = 1U; //indicates a change of L5mSysState

	static ALARM_FLAGS_T alarmdisplay = NONE; //flags the type of alarm that was triggered

	cdtcount++;

//////SYSTEM STATE MACHINE///////////////////////////////////////////
	static INT8C key;

	if(cdtcount >= 5){ //run only every 50 ms.
		DB2_TURN_ON();
		L5mPrevSysState = L5mSysState;
		cdtcount = 0;
		key = KeyGet();

		if(key == DC2){ //if key == B
			TTempCom.unit = !TTempCom.unit;
		}
		else{}

		switch(L5mSysState)
		{
			case(ARMED):
				if(statechange == 1U){
					LcdClrLine(1U);
					LcdMoveCursor(1U,1U);
					LcdDispStrg(ArmedStrg);
					statechange = 0U;
					L5mAlarmFlags = NONE;
				}
				else{}

				if(L5mAlarmFlags != NONE){
					alarmdisplay = L5mAlarmFlags;
					statechange = 1U;
					L5mSysState = ALARM;
				}
				else if(key == DC4){ //if key == D
					statechange = 1U;
					L5mSysState = DISARMED;
				}
				else{}



			break;

			case(DISARMED):
				if(statechange == 1U){
					LcdClrLine(1U);
					LcdMoveCursor(1U,1U);
					LcdDispStrg(DisarmedStrg);
					statechange = 0U;
				}
				else{}

				if(key == DC1){ //if key == A
					statechange = 1U;
					L5mSysState = ARMED;
				}
				else{}

			break;

			case(ALARM):
				if(statechange == 1U){
					AlarmDisplay(alarmdisplay);
					statechange = 0U;
				}
				else{}

				if(key == DC4){ //if key == D
					statechange = 1U;
					L5mSysState = DISARMED;
				}
				else{}

			break;

			default:
			break;
		}

		TempDisplay();

		DB2_TURN_OFF();
	}
	else{}
/////////////////////////////////////////////////////////////////////////

}

/*************************************************************************************************
 * AlarmDisplay - writes proper alarm indicator to the lcd based on which sensor
 * 				  triggered the alarm
 *
 * 	Parameters:
 * 		-alarmflag: alarm indicator flag
 * 			TOUCH: display "ALARM" on the lcd
 *			TEMP: display "TEMP ALARM" on the lcd
 *
 *	Returns: none
 *
 *	11/26/2019 SSC
 ********************************************************************************************/
static void AlarmDisplay(ALARM_FLAGS_T alarmflag){
	switch(alarmflag)
	{

		case(TOUCH):
			LcdClrLine(1U);
			LcdMoveCursor(1U, 6U);
			LcdDispStrg(AlarmStrg);
		break;

		case(TEMP):
			LcdClrLine(1u);
			LcdMoveCursor(1u, 3u);
			LcdDispStrg(TempAlarmStrg);
		break;

		default:
		break;
	}
}

/****************************************************************************************************
 * TempDisplay - logic controlling how/when temperature is written to the lcd.
 *
 * Parameters: none
 * Returns: none
 *
 * 12/2/2019, SSC
 *****************************************************************************************************/
static void TempDisplay(void){

	static INT8U prevunit = 255;
	static INT8U prevtempdispval = 255;

	if(prevunit != TTempCom.unit){ //if b key is pressed, changing units, rewrite unit on lcd
		LcdMoveCursor(2u, 15u);
		LcdDispChar(Degree);
		LcdMoveCursor(2u, 16u);
		if(TTempCom.unit==CELCIUS){
			LcdDispChar(Celcius);
		}
		else{
			LcdDispChar(Fahrenheit);
		}
	}
	else{}

	if(prevtempdispval != TTempCom.tempdispval){ //if temperature display value has changed, rewrite temperature on lcd
		TempOut();
	}
	else{}

	//update previous value placeholders
	prevunit = TTempCom.unit;
	prevtempdispval = TTempCom.tempdispval;

}

/***********************************************************************************
 * TempOut - write current temperature to led.  Helper function to TempDisplay above
 *
 * 	Parameters: none
 * 	Returns: none
 *
 * 12/2/2019, SSC
 ************************************************************************************/
static void TempOut(void){

	INT8U lastcursorcol;

	for(INT8U k = 0; k<4; k++){
		LcdMoveCursor(2u, 11u+k);
		LcdDispChar(Space);
	}
	if(TTempCom.tempdispval >= 100u){
		LcdMoveCursor(2u, 12u);
		lastcursorcol = 12u;
	}
	else if(TTempCom.tempdispval >= 10u){
		LcdMoveCursor(2u, 13u);
		lastcursorcol = 13u;
	}
	else{
		LcdMoveCursor(2u, 14u);
		lastcursorcol = 14u;
	}

	LcdDispDecWord(TTempCom.tempdispval, 0u);
	if(TTempCom.negflag == 1u){
		LcdMoveCursor(2u, lastcursorcol-1);
		LcdDispStrg(MinusStrg);
	}
	else{}

}

/***********************************************************************************************
 * LEDTask - update state of led's based on L5mSysState and touch sensor input
 *
 *  Parameters: none
 *  Returns: none
 *
 *  12/2/2019, SSC
 ***********************************************************************************************/
static void LEDTask(void){

	static INT32U ledcount = 0U;
	static INT8U ledstates[] = {0,1};

	static INT8U alarmleddisp = 0U;

	ledcount++;

//LED TASK STATE TEST////////////////////////////////////////////////////////////////
	if(ledcount >= 5U){
		DB4_TURN_ON();
		switch(L5mSysState)
			{

			case(ARMED):
				if((ledcount >= 25U) || (L5mPrevSysState != L5mSysState)){ //if in armed state, toggle led's every 250 mS.
					ledcount = 0;
					if(ledstates[0] != ledstates[1]){
						if(ledstates[0] == 0U){
							ledstates[0] = 1U;
						}
						else{
							ledstates[0] = 0U;
						}
						if(ledstates[1] == 0U){
							ledstates[1] = 1U;
						}
						else{
							ledstates[1] = 0U;
						}
					}
					else{
						ledstates[0] = 1;
						ledstates[1] = 0;
					}

				}
				else{}
			break;

			case(DISARMED):
				if((ledcount >= 50U) || (L5mPrevSysState != L5mSysState)){ //if in disarmed state, toggle led state every 500 mS.
					ledcount = 0U;
					if(SSenseState.electrodeState == 3U){
						ledstates[0] = !ledstates[0];
						ledstates[1] = ledstates[0];
					}
					else if(SSenseState.electrodeState == 2U){
						ledstates[0] = 0U;
						ledstates[1] = !ledstates[1];
					}
					else if(SSenseState.electrodeState == 1U){
						ledstates[1] = 0U;
						ledstates[0] = !ledstates[0];
					}
					else{
						ledstates[0] = 0U;
						ledstates[1] = 0U;
					}
				}
				else{}
			break;

			case(ALARM):
				if(L5mPrevSysState != ALARM){
					alarmleddisp = SSenseState.electrodeState;
				}
				else if(SSenseState.electrodeState != SSenseState.prevElectrodeState){
					alarmleddisp += SSenseState.electrodeState;
				}
				else{}
				if(ledcount >= 10U || (SSenseState.electrodeState != SSenseState.prevElectrodeState)){ //if in alarm state, toggle led state every 100 mS.
					ledcount = 0;
					switch(alarmleddisp)
					{
						case(0u):
							ledstates[0] = 0u;
							ledstates[1] = 0u;
						break;

						case(1U):
							ledstates[0] = !ledstates[0];
							ledstates[1] = 0U;
						break;

						case(2U):
							ledstates[1] = !ledstates[1];
							ledstates[0] = 0U;
						break;

						default:
							ledstates[0] = !ledstates[0];
							ledstates[1] = ledstates[0];
						break;
					}
				}
			break;

			default:
			break;

			}

		//physically turn led's on or off based on change of state set above
		if(ledstates[0] == 1U){
			LED8_TURN_ON();
		}
		else{
			LED8_TURN_OFF();
		}
		if(ledstates[1] == 1U){
			LED9_TURN_ON();
		}
		else{
			LED9_TURN_OFF();
		}
		DB4_TURN_OFF();
	}
	else{}
/////////////////////////////////////////////////////////////////////////////////////
}


