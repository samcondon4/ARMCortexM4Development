/* EE344 Lab 2 Code:
 *
 *	Calculates 16 bit checksum on block of memory input by a user.
 *	Validates memory address input and provides feedback if an invalid
 *	memory range was specified.  Enter up to 8 hex characters to specify
 *	a memory address.
 *
 *	Using BasicIO and K65TWR_ClkCfg modules written by Todd Morton
 *
 * Sam Condon
 * 10/22/2019
 */

#include "MCUType.h"
#include "BasicIO.h"
#include "CheckSum.h"
//#include "K65TWR clock config thing"



//NEED TO #INCLUDE THE K65TWR CLOCK CONFIG HEADER FOR THIS TO WORK
void CSStateMachineEnter(void){

	K65TWR_BootClock();
	BIOOpen(BIO_BIT_RATE_9600);

	ADDRESS_T addr;
	INT8U state;
	state = GET_LOW_ADDR;

	INT16U chksum;

	 /***************
	 * STATE MACHINE:
	 *  GET_LOW_ADDR: prompt for low address.  If valid address entered, move to GET_HIGH_ADDR.  Else don't change state
	 *  GET_HIGH_ADDR: prompt for high address.  If valid address entered, move to CS_DISPLAY_HOLD. Else move back to GET_LOW_ADDR or stay put.
	 *  CS_DISPLAY_HOLD: Calculate and display checksum value.  Always move back to GET_LOW_ADDR.
	 */
	while(1){
		switch(state)
		{
		case GET_LOW_ADDR:
			BIOPutStrg(lowaddrstrg);
			L2PGetAddr(&addr, state);
		break;

		case GET_HIGH_ADDR:
			BIOPutStrg(highaddrstrg);
			L2PGetAddr(&addr, state);
		break;

		case CS_DISPLAY_HOLD:
			chksum = CalcChkSum((INT8U*)addr.addrs[0],(INT8U*)addr.addrs[1]);
			BIOPutStrg(csstrg);
			BIOOutHexWord(addr.addrs[0]);
			BIOPutStrg(dashstrg);
			BIOOutHexWord(addr.addrs[1]);
			BIOPutStrg(spacestrg);
			BIOOutHexHWord(chksum);
			while(BIOGetChar() != '\r'){
				//wait until enter is pressed
			}
		break;
		}
		MoveToNextState(&addr, &state);
	}
}

/**************************************************
 * CSCalc() - Takes an input range of memory addresses
 * 				  and returns 16 bit checksum of all the
 * 				  stored values at each address.
 * Return value: INT16U checksum value
 * Arguments: *startaddr: pointer to low memory address
 * 			  *endaddr: pointer to high memory address
 ***************************************************/
static INT16U CSCalc(INT8U* startaddr, INT8U* endaddr){
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


/*******************************************************
 *  CSGetAddr() - Takes pointer to ADDRESS_T structure and the
 *  			   current state of the state machine.  No return value.
 *  			   Sets the low and high address in the ADDRESS_T structure
 *  			   if valid address were entered.  Performs all error checking
 *  			   for invalid hex addresses entered and sets proper flag on the
 *  			   ADDRESS_T structure to indicate the type of entry error.
 *
 *  arguments: *addr: pointer to ADDRESS_T structure to hold memory addresses and flags
 *  		   state: current state of state machine
 *  return: none
 */
static void CSGetAddr(ADDRESS_T* const addr, const INT8U state){
	INT8C addrstrg[9];
	INT32U addrword;
	INT8U chk;

	chk = BIOGetStrg(STRG_LEN, addrstrg);
	if(chk == INVALID){
		BIOPutStrg(toolongstrg);
		addr->inval_flag = INVALID;
	}

	else{
		chk = BIOHexStrgtoWord(addrstrg, &addrword);
		switch(chk)
		{
			case(0):
					addr->addrs[state] = addrword;
					addr->inval_flag = VALID;
			break;

			case(1):
					//should already be checked by the above check if the result of BIOGetStrg exceeds 8 character length
					addr->inval_flag = INVALID;
			break;

			case(2):
					BIOPutStrg(nonhexcharstrg);
					addr->inval_flag = INVALID;
			break;

			case(3):
					BIOPutStrg(nocharacterstrg);
					addr->inval_flag = INVALID;
			break;
		}
		if(addrword > MAX_ADDR){
			BIOPutStrg(hexvallargestrg);
			addr->inval_flag = INVALID;
		}
		else{}

		if((addr->inval_flag == VALID) && (state == GET_HIGH_ADDR)){
			if(addr->addrs[1] < addr->addrs[0]){
				BIOPutStrg(lowtoobigstrg);
				addr->inval_flag = LOW_TOO_HIGH;
			}
			else{}
		}
		else{}

	}
}

/* MoveToNextState() - Takes pointer to the ADDRESS_T structure set by L2PGetAddr()
 * 					   and changes the value at *state to the proper next state.
 * arguments: *addr: pointer to ADDRESS_T structure set by L2PGetAddr()
 * 			 *state: pointer to the current state of the state machine in main()
 * returns: none
 */
static void CSMoveState(ADDRESS_T* const addr, INT8U* state){
	INT8U invalflag;
	invalflag = addr->inval_flag;
	switch(*state)
	{
		case(GET_LOW_ADDR):
				if(invalflag == INVALID){
					*state = GET_LOW_ADDR;
				}
				else{
					*state = GET_HIGH_ADDR;
				}
		break;

		case(GET_HIGH_ADDR):
				if(invalflag==INVALID){
					*state = GET_HIGH_ADDR;
				}
				else if(invalflag == LOW_TOO_HIGH){
					*state = GET_LOW_ADDR;
				}
				else{
					*state = CS_DISPLAY_HOLD;
				}
		break;

		case(CS_DISPLAY_HOLD):
				*state = GET_LOW_ADDR;
		break;
	}
}
