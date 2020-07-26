/****************************************************************************************
* EE344 Lab 3 Project:
*	
*	This project implements a state machine
*	to illustrate real time IO using a software
*	polling loop detection, a combination of software/hardware
*	detection, and a pure hardware interrupt detection of a 
*	button press.
*
*
*	Sam Condon 10/25/2019
****************************************************************************************/
#include "MCUType.h"               /* Include project header file                      */
#include "BasicIO.h"
#include "K65TWR_ClkCfg.h"
#include "K65TWR_GPIO.h"

//Memory range for initial checksum//
#define CS_LOW 0x00000000
#define CS_HIGH 0x001fffff
#define MAX_ADDR 0xffffffff
/////////////////////////////////////

#define INVALID 0x0U
#define VALID 0x1U
#define CMD_IN_LEN 0x2U

#define SW2_ENABLE_NO_INTS 0U
#define SW2_ENABLE_INTS 10U

//TYPEDEFS//////////////////////////////////////////////////////////
typedef enum{CMD_PARSE, SOFT_COUNT, COMB_COUNT, HARD_COUNT} STATE_T;

typedef struct{
	INT8C cmd;
	INT8U val_flag;
}CMD_T;

////////////////////////////////////////////////////////////////////

//FUNCTION PROTOTYPES////////////////////////////////////
INT16U CSCalc(INT8U* startaddr, INT8U* endaddr);
void L3PGetCmd(CMD_T* cmd);
void L3PMoveToNextState(CMD_T* cmd, STATE_T* state);
void L3PSoftCount(void);
void L3PCombCount(void);
void L3PHardCount(void);
/////////////////////////////////////////////////////////

//INTERRUPT SERVICE ROUTINE PROTOTYPE///////////////////
void PORTA_IRQHandler(void);
///////////////////////////////////////////////////////

//DEFINE STRING CONSTANTS//////////////////////
static const INT8C csstrg[] = "\n\rCS:";
static const INT8C lowaddr[] = "0x00000000";
static const INT8C highaddr[] = "0x001fffff";
static const INT8C dashstrg[] = "-";
static const INT8C eqlstrg[] = " = ";
static const INT8C cmdparsestrg[] = "\n\rEnter a command. (s) for software only counter.  (b) for hardware and software counter. (h) for interrupt hardware counter: ";
static const INT8C invalstrg[] = "Invalid command entered.  Try again.";
static const INT8C countdisp[] = "count: ";
static const INT8C newline[] = "\n";
static const INT8C carriagereturn[] = "\r";
/////////////////////////////////////////

//OTHER CONSTANTS//
static const INT8C valcmds[] = {'s','b','h'}; //valid commands a user can enter
///////////////////

//COUNT///////
static INT32U count;
static INT8U dispflag;
//////////////

void main(void){

    //INITIALIZATION///////////////////////////////////////////
	K65TWR_BootClock();
	BIOOpen(BIO_BIT_RATE_9600);

	INT16U checksum = CSCalc((INT8U*)CS_LOW, (INT8U*)CS_HIGH);
	BIOPutStrg(csstrg);
	BIOPutStrg(lowaddr);
	BIOPutStrg(dashstrg);
	BIOPutStrg(highaddr);
	BIOPutStrg(eqlstrg);
	BIOOutHexHWord(checksum);
	//////////////////////////////////////////////////////////

	//MAIN VARIABLE DEFINITIONS//
	CMD_T cmd;
	STATE_T state;
	/////////////////////////////

	//MAIN STATE-MACHINE////////////////////////////////////////////////////////////////////////////////////
	state = CMD_PARSE;

	while(1){
		switch(state)
		{
		case CMD_PARSE:
			BIOPutStrg(cmdparsestrg);
			L3PGetCmd(&cmd); //Get command input from user, set flag if invalid input
			if(cmd.val_flag != 1){
				BIOPutStrg(invalstrg);
			}
		break;

		case(SOFT_COUNT):
			count = 0;
			GpioSw2Init(SW2_ENABLE_NO_INTS);
			L3PSoftCount();
		break;

		case(COMB_COUNT):
			count = 0;
			GpioSw2Init(SW2_ENABLE_INTS);
			L3PCombCount();
		break;

		case(HARD_COUNT):
			count = 0;
			dispflag = 0;
			SW2_CLR_ISF();
			NVIC_ClearPendingIRQ(PORTA_IRQn);
			NVIC_EnableIRQ(PORTA_IRQn);
			GpioSw2Init(SW2_ENABLE_INTS);
			L3PHardCount();
			NVIC_DisableIRQ(PORTA_IRQn);
		break;

		default:
		break;
		}
		L3PMoveToNextState(&cmd,&state);
	}
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

}

/*************************************************************
 * L3PGetCmd - Get command to move through the state machine.
 * 			   Perform error checking on input command and set
 * 			   invalid flag if the input is invalid.
 *
 * Parameters:
 * 		cmd - pointer to a CMD_T structure.
 *
 * Returns:
 * 		nothing
 *************************************************************/
void L3PGetCmd(CMD_T* cmd){
	INT8U chk;
	INT8C cmdstrgtemp[2];

	chk = BIOGetStrg(CMD_IN_LEN,cmdstrgtemp);

	cmd->val_flag = INVALID;

	if(chk > 0){
		cmd->val_flag = INVALID;
	}
	else{
		for(INT8U k = 0; k < 3; k++){
			if(cmdstrgtemp[0] == valcmds[k]){
				cmd->val_flag = VALID;
				cmd->cmd = cmdstrgtemp[0];
			}
			else{}
		}
	}
}

/***************************************************
 * L3PMoveToNextState - Move to next state of state machine
 * 						based on the current state and the
 * 						validity of inputed command
 *
 * Parameters:
 * 		cmd - pointer to CMD_T structure used to test the val_flag, which tells us if the command
 *
 ***************************************************/
void L3PMoveToNextState(CMD_T* cmd, STATE_T* state){
	if(*state != CMD_PARSE){
		*state = CMD_PARSE;
	}
	else if(cmd->val_flag == 1){
		switch(cmd->cmd)
		{
		case('s'):
			*state = SOFT_COUNT;
		break;

		case('b'):
			*state = COMB_COUNT;
		break;

		case('h'):
			*state = HARD_COUNT;
		break;

		default:
			*state = CMD_PARSE;
		break;
		}
	}
	else{
		*state = CMD_PARSE;
	}
}

/*****************************************
 * PORTA_IRQHandler - Interrupt service routine for
 * 					  the button push.  Increments
 * 					  the count variable, resets the display
 * 					  flag to tell LP3HardCount() to display a value.
 *
 * 	Parameters: none
 * 	Returns: none
 *****************************************/
void PORTA_IRQHandler(void){
	SW2_CLR_ISF();
	count++;
	dispflag = 0;
}

/***********************************************************
 * L3PSoftCount - Polling loop checking the PDIR of PORTA.
 * 				  Increments count on every button push
 * 				  detection and displays count afterwards.
 *
 *	Parameters: none
 *	Returns: none
 ************************************************************/
void L3PSoftCount(void){
	INT8U countflag = 0x0U;
	INT8C quit;
	BIOPutStrg(carriagereturn);
	BIOPutStrg(newline);
	BIOPutStrg(countdisp);
	BIOOutDecWord(count,0);
	while(quit != 'q'){
		if((SW2_INPUT == 0x00000000) && (countflag != 0x1U)){
			count++;
			countflag = 0x1U;
			BIOPutStrg(carriagereturn);
			BIOPutStrg(countdisp);
			BIOOutDecWord(count,0);
		}
		else if ((SW2_INPUT != 0x00000000) && (countflag == 0x1U)){
			countflag = 0x0U;
		}
		else{}
		quit = BIORead();
	}
}

/******************************************************************
 * L3PCombCount - Polling loop checks the ISF flag set by hardware.
 *
 *	Parameters: none
 *	Returns: none
 *****************************************************************/
void L3PCombCount(void){
	GpioSw2Init(SW2_ENABLE_INTS);
	SW2_CLR_ISF();
	INT8C quit = 'p';
	BIOPutStrg(carriagereturn);
	BIOPutStrg(newline);
	BIOPutStrg(countdisp);
	BIOOutDecWord(count,0);
	while(quit != 'q'){
		if(SW2_ISF != 0){
			SW2_CLR_ISF();
			count++;
			BIOPutStrg(carriagereturn);
			BIOPutStrg(countdisp);
			BIOOutDecWord(count,0);
		}
		else{}
		quit = BIORead();
	}
}

/******************************************************************
 * L3PHardCount - Checks display flag to see if new count value needs
 * 				  to be displayed.
 *
 *	Parameters: none
 *	Returns: none
 *****************************************************************/
void L3PHardCount(void){
	INT8C quit;
	dispflag = 0;
	while(quit != 'q'){
		if(dispflag != 1){
			BIOPutStrg(carriagereturn);
			BIOPutStrg(countdisp);
			BIOOutDecWord(count,0);
			dispflag = 1;
		}
		else{}
		quit = BIORead();
	}
}

/************************************************************************
 * CSCalc - Calculates 16-bit check sum between a starting and ending
 * 		    memory address.
 *
 * Parameters:
 * 		startaddr: pointer to a byte in memory at lowest address
 * 		endaddr: pointer to a byte in memory at highest address
 * Returns:
 * 		16 bit unsigned checksum value.
 ***********************************************************************/

INT16U CSCalc(INT8U* startaddr, INT8U* endaddr){
	INT16U chksum;
	INT8U brkflag;
	chksum = 0;
	brkflag = 0;

	for(INT8U* k = startaddr; k <= endaddr; k++){
		if(brkflag == 1){
			break;
		}
		else{
			chksum += (INT16U)*k;
		}
		if(k==(INT8U*)MAX_ADDR){
			brkflag = 1;
		}
		else{}
	}
	return chksum;
}
