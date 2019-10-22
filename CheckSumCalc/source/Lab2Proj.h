/*
 * Lab2Proj.h
 *
 *  Created on: Oct 17, 2019
 *      Author: condons3
 */

#ifndef LAB2PROJ_H_
#define LAB2PROJ_H_

#define MAX_ADDR 0xffffffff
#define MIN_ADDR 0x00000000

#define STRG_LEN 9

//Flags for ADDRESS_T.inval_flag
#define VALID 0
#define INVALID 1
#define LOW_TOO_HIGH 2
////////////////////////////////

//Define states.  Specific numbers wanted so #define used rather than enum.
#define GET_LOW_ADDR 0
#define GET_HIGH_ADDR 1
#define CS_DISPLAY_HOLD 2
////////////////////////////////////////////////////////////////////////////


typedef struct{
	INT32U addrs[2];  //array to hold the high and low adresses.  addrs[0] = low address, addrs[1] = high address//
	INT8U inval_flag;
}ADDRESS_T;

INT16U CalcChkSum(INT8U *startaddr, INT8U *endaddr);

void L2PGetAddr(ADDRESS_T* addr, INT8U state);

void MoveToNextState(ADDRESS_T* addr, INT8U* state);  //MAKE SURE TO CLEAR INVALID FLAG IF SET

#endif /* LAB2PROJ_H_ */
