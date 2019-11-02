/* CheckSum.c:
 *
 *	Calculates 16 bit checksum on block of memory input by a user.
 *	Validates memory address input and provides feedback if an invalid
 *	memory range was specified.  Enter up to 8 hex characters to specify
 *	a memory address.
 *
 *	State machine implemented through CSStateMachineEnter to demonstrate
 *	functionality.
 *
 *	Using BasicIO and K65TWR_ClkCfg modules written by Todd Morton
 *
 * Sam Condon
 * 10/22/2019
 */

#include "MCUType.h"
#include "CheckSum.h"

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

