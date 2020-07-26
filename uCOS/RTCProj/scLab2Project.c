/*****************************************************************************************
/*
 * EE444 Lab 2 - MicroC/OS Realtime Clock
 *
 * 		This lab demonstrates the functionality of uC/OS and the Real Time Clock on board
 * 		the Kinetis K-65 Microcontroller.  The program allows a user to input a time in the
 * 		IS0 8601 time display standard, then updates the time value each second.  All user input,
 * 		validity checking, and system state control is handled in the main module while time
 * 		set updating and rtc incrementing is handled in the time module.
 *
 *
*****************************************************************************************/

//MAIN MODULE////////////////////////////////////////////////////////////////////////////////


//INCLUDE DEPENDENCIES//////////////////////////
#include "MCUType.h"
#include "app_cfg.h"
#include "os.h"
#include "LcdLayered.h"
#include "K65TWR_GPIO.h"
#include "LcdLayered.h"
#include "K65TWR_ClkCfg.h"
#include "uCOSKey.h"
#include "scTime.h"
///////////////////////////////////////////////

/***********************************************
 * defines used privately
 **********************************************/
#define INVALID 0u
#define VALID 1u

/************************************************************
 * Enumerated types:
 *
 * 	SYS_STATE_T: System state machine states
 * 	TIME_SET_STATE_T: Time set state machine states
 * 		HTC: Hour tens change
 * 		HOC: Hour ones change
 * 		MTC: Minute tens change
 * 		STC: Second tens change
 * 		SOC: Second ones change
 ************************************************************/
typedef enum{TIME_SET, TIME} SYS_STATE_T;
typedef enum{HTC, HOC, MTC, MOC, STC, SOC} TIME_SET_STATE_T;

/*****************************************************************
 * Time Set Struct:
 * 		This structure contains all data needed for the time set
 * 		state
 ****************************************************************/
typedef struct{
	TIME_SET_STATE_T timesetstate; //holds state of the TIME SET state machine
	INT8U keyasciival;
	INT8U keyintval; //holds integer value of a key press
	INT8U keyvalid;
	INT8U cursorpos; //position of cursor column relative to leftmost digit of TIME SET displayed time
	TIME_T time; //local copy of TIME_T struct that will set the timeOfDay struct in scTime module
}TIME_SET_DATA_T;


/*****************************************************************************************
* Allocate task control blocks
*****************************************************************************************/
static OS_TCB AppTaskStartTCB;
static OS_TCB UITaskTCB;
static OS_TCB DispTimeTaskTCB;

/*****************************************************************************************
* Allocate task stack space.
*****************************************************************************************/
static CPU_STK AppTaskStartStk[APP_CFG_TASK_START_STK_SIZE];
static CPU_STK DispTimeTaskStk[APP_CFG_DISP_TIME_TASK_STK_SIZE];
static CPU_STK UITaskStk[APP_CFG_UI_TASK_STK_SIZE];

/*****************************************************************************************
* Task Prototypes.
*   - All private to the module
*****************************************************************************************/
static void  AppStartTask(void *p_arg);
static void DispTimeTask(void *p_arg);
static void UITask(void *p_arg);

/*******************************************
 * Private function prototypes.
 *******************************************/
static INT8S l2pKeyCheck(TIME_SET_DATA_T *timeset);

/***************************************************
 * Private globals
 ***************************************************/
SYS_STATE_T SysState;
INT8U SysChgFlag;


/* main()
*****************************************************************************************/
void main(void) {

    OS_ERR  os_err;

    K65TWR_BootClock();
    CPU_IntDis();               /* Disable all interrupts, OS will enable them  */

    OSInit(&os_err);                    /* Initialize uC/OS-III                         */

    OSTaskCreate(&AppTaskStartTCB,                  /* Address of TCB assigned to task */
                 "Start Task",                      /* Name you want to give the task */
                 AppStartTask,                      /* Address of the task itself */
                 (void *) 0,                        /* p_arg is not used so null ptr */
                 APP_CFG_TASK_START_PRIO,           /* Priority you assign to the task */
                 &AppTaskStartStk[0],               /* Base address of task�s stack */
                 (APP_CFG_TASK_START_STK_SIZE/10u), /* Watermark limit for stack growth */
                 APP_CFG_TASK_START_STK_SIZE,       /* Stack size */
                 0,                                 /* Size of task message queue */
                 0,                                 /* Time quanta for round robin */
                 (void *) 0,                        /* Extension pointer is not used */
                 (OS_OPT_TASK_NONE), /* Options */
                 &os_err);                          /* Ptr to error code destination */

    OSStart(&os_err);               /*Start multitasking(i.e. give control to uC/OS)    */

}

/*****************************************************************************************
* AppStartTask()
*
* This task creates all tasks required to start the program, then suspends itself
*****************************************************************************************/
static void AppStartTask(void *p_arg) {

    OS_ERR os_err;

    (void)p_arg;                        /* Avoid compiler warning for unused variable   */

    /* Run initialization code before creating tasks*/
    OS_CPU_SysTickInitFreq(SYSTEM_CLOCK);
    LcdInit();
    GpioDBugBitsInit();
    KeyInit();
    TimeInit();

    SysState = TIME_SET;
    SysChgFlag = 1u;

    OSTaskCreate(&DispTimeTaskTCB,                  /* Create Display Task                    */
                "Display Time Task",
                DispTimeTask,
                (void *) 0,
                APP_CFG_DISP_TIME_TASK_PRIO,
                &DispTimeTaskStk[0],
                (APP_CFG_DISP_TIME_TASK_STK_SIZE / 10u),
                APP_CFG_DISP_TIME_TASK_STK_SIZE,
                0,
                0,
                (void *) 0,
                (OS_OPT_TASK_NONE),
                &os_err);

    OSTaskCreate(&UITaskTCB,                  /* Create UI-Task                    */
                    "UI Task",
                    UITask,
                    (void *) 0,
                    APP_CFG_UI_TASK_PRIO,
                    &UITaskStk[0],
                    (APP_CFG_UI_TASK_STK_SIZE / 10u),
                    APP_CFG_UI_TASK_STK_SIZE,
                    0u,
                    0u,
                    (void *) 0,
                    (OS_OPT_TASK_NONE),
                    &os_err);



    OSTaskSuspend((OS_TCB *)0, &os_err); //suspend StartTask so other tasks can run.

}

/*******************************************************************
 * DispTimeTask()
 *
 * This task simply displays the updated time captured by time pend
 *******************************************************************/
static void DispTimeTask(void *p_arg){
	OS_ERR os_err;

	TIME_T disptime; //local instance of time struct holding all current time data
	TIME_T *ltime;

	ltime = &disptime;

	(void)p_arg;

	while(1){

		TimePend(ltime, &os_err); //pend on change of time.  Pass a pointer to the local instance of TIME_T struct

		DB2_TURN_ON();
		LcdDispTime(LCD_ROW_1, LCD_COL_9, SEND_B_LAYER, disptime.hr, disptime.min, disptime.sec);
		DB2_TURN_OFF();

	}
}

/*******************************************************************
 * UITask()
 *
 * This task handles all user input and changes state accordingly.
 * Debug bits are found at the bottom, where keypend is called
 *******************************************************************/
static void UITask(void *p_arg){
	OS_ERR os_err;

	TIME_SET_DATA_T timesetdata;
	TIME_T *ltime;

	INT8U cursoronflag = 0u;

	ltime = &(timesetdata.time);

	(void)p_arg;

	while(1){

		DB0_TURN_ON();

		//SYSTEM STATE MACHINE///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		switch(SysState){

		case(TIME):
			if(SysChgFlag == 1u){
				LcdDispClear(SEND_C_LAYER);
				SysChgFlag = 0u;
			}
			else{}

			if(timesetdata.keyasciival == '#'){
				SysState = TIME_SET;
				SysChgFlag = 1u;
			}
			else{}
		break;

		case(TIME_SET):

			if((SysChgFlag == 1u)){ //If TIME_SET has just been entered, execute the following
				TimeGet(ltime);
				LcdDispClear(SEND_C_LAYER); //Clear the bottom layer time set
				LcdDispTime(LCD_ROW_2, LCD_COL_9, SEND_C_LAYER, timesetdata.time.hr, timesetdata.time.min, timesetdata.time.sec); //Display the current time
				timesetdata.timesetstate = HTC;
				timesetdata.cursorpos = 0u;
				SysChgFlag = 0u;
				LcdCursor(LCD_ROW_2, LCD_COL_9 + timesetdata.cursorpos, SEND_A_LAYER, 1u, 1u); //Turn on cursor in the initial position
			}
			else{}

			timesetdata.keyintval = (INT8U)(timesetdata.keyasciival - 48); //convert the ascii value of the key press to an integer value

			if(timesetdata.keyintval <= 9u){ //this actually tests if 0 <= timesetdata.keyintval <= 9u due to limited range of unsigned char (INT8U) data type
				timesetdata.keyvalid = l2pKeyCheck(&timesetdata); //check validity of the key press

				if(timesetdata.keyvalid == VALID){

					switch(timesetdata.timesetstate){

					case(HTC):
						timesetdata.timesetstate = HOC;
						timesetdata.time.hr = (timesetdata.time.hr%10) + (10*timesetdata.keyintval); //change 10's value of hr display
						timesetdata.cursorpos += 1;
					break;

					case(HOC):
						timesetdata.timesetstate = MTC;
						timesetdata.time.hr = (timesetdata.time.hr/10)*10 + timesetdata.keyintval; //change one's value of hr display
						timesetdata.cursorpos += 2;
					break;

					case(MTC):
						timesetdata.timesetstate = MOC;
						timesetdata.time.min = (timesetdata.time.min%10) + (10*timesetdata.keyintval); //change 10's value of min display
						timesetdata.cursorpos += 1;
					break;

					case(MOC):
						timesetdata.timesetstate = STC;
						timesetdata.time.min = (timesetdata.time.min/10)*10 + timesetdata.keyintval; //change one's value of min display
						timesetdata.cursorpos += 2;
					break;

					case(STC):
						timesetdata.timesetstate = SOC;
						timesetdata.time.sec = (timesetdata.time.sec%10) + 10*timesetdata.keyintval; //change 10's value of sec display
						timesetdata.cursorpos += 1;
					break;

					case(SOC):
						timesetdata.time.sec = (timesetdata.time.sec/10)*10 + timesetdata.keyintval; //change one's value of sec display
					break;


					}
					LcdDispTime(LCD_ROW_2, LCD_COL_9, SEND_C_LAYER, timesetdata.time.hr, timesetdata.time.min, timesetdata.time.sec); //update display
					cursoronflag = 1u; //turn on flag for cursor
				}
				else{}

			}

			else if((timesetdata.keyasciival == DC2) && (timesetdata.timesetstate != HTC)){ //if the B key is pressed, move back one state in the time set process
				timesetdata.timesetstate -= 1u;
				if(timesetdata.timesetstate % 2u == 0u){
					timesetdata.cursorpos -= 1u;
				}
				else{
					timesetdata.cursorpos -= 2u;
				}
				cursoronflag = 1u;
			}

			else if((timesetdata.keyasciival == DC1) || timesetdata.keyasciival == DC3){  //if A key is pressed, update timeOfDay, else just return to TIME state

				SysState = TIME;
				SysChgFlag = 1u;
				cursoronflag = 0u;

				if(timesetdata.keyasciival == DC1){
					TimeSet(&timesetdata.time);
				}
				else{}

			}

			else{
				cursoronflag = 1u; //make sure cursor continues to blink when no key is pressed
			}

			LcdCursor(LCD_ROW_2, LCD_COL_9 + timesetdata.cursorpos, SEND_A_LAYER, cursoronflag, cursoronflag);


		break;

		default:
		break;


		}
		if(SysChgFlag != 1u){
			timesetdata.keyasciival = KeyPend(0u, &os_err);
		}
		else{}
		///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		DB0_TURN_OFF();
	}
}



/**********************************************************************************
 * l2pKeyCheck() - checks the validity of a key press based on the timeset state
 *
 * 					timesetdata: structure that holds all relevant information
 * 								 to the time set process
 *
 * 					Returns:
 * 							flag value.  If the key press was an invalid entry
 * 							a 0 is returned, else a 1 is returned
 **********************************************************************************/
static INT8S l2pKeyCheck(TIME_SET_DATA_T *timesetdata){

	INT8U retval;

	TIME_T curtime;
	curtime = timesetdata->time;

	INT8U keyintval = timesetdata->keyintval;


	switch(timesetdata->timesetstate){

	case(HTC):
		if(keyintval > 2u){
			retval = INVALID;
		}
		else{
			retval = VALID;
		}
	break;

	case(HOC):
		if(((curtime.hr/10)*10 + keyintval) > 23){
			retval = INVALID;
		}
		else{
			retval = VALID;
		}
	break;

	case(MTC):
		if(keyintval > 5u){
			retval = INVALID;
		}
		else{
			retval = VALID;
		}
	break;

	case(MOC):
		retval = VALID; //we know this is valid because we have already checked that keyintval is between 0 and 9
	break;

	case(STC):
		if(keyintval > 5u){
			retval = INVALID;
		}
		else{
			retval = VALID;
		}
	break;

	case(SOC):
		retval = VALID; //we already know that input is between 0 and 9 so this is always valid
	break;

	default:
	break;

	}

	return retval;

}


