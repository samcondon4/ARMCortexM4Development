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

#define SW2_ENABLE_NO_INTS 0x0U

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
static const INT8C valcmds[] = {'s','b','h'};

void main(void){

    //INITIALIZATION///////////////////////////////////////////
	K65TWR_BootClock();
	BIOOpen(BIO_BIT_RATE_9600);

	GpioSw2Init(SW2_ENABLE_NO_INTS);

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
	//MAIN STATE-MACHINE//////////////////////////////////////
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
			BIOPutStrg("SOFT_COUNT");
			L3PSoftCount();
		break;

		case(COMB_COUNT):
			BIOPutStrg("COMB_COUNT");
			L3PCombCount();
		break;

		case(HARD_COUNT):
			BIOPutStrg("HARD_COUNT");
			L3PHardCount();
		break;

		default:
		break;
		}
		L3PMoveToNextState(&cmd,&state);
	}

}

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

void L3PSoftCount(void){
	INT32U count = 0x0U;
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
		else{
			countflag = 0x0U;
		}
		quit = BIORead();
	}
}

void L3PCombCount(void){
	INT8C quit;
	while(quit != 'q'){
		quit = BIORead();
	}
}

void L3PHardCount(void){
	INT8C quit;
	while(quit != 'q'){
		quit = BIORead();
	}
}

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