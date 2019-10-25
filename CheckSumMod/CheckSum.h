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

#define STRG_LEN 0x9U

//Flags for ADDRESS_T.inval_flag
#define VALID 0x0U
#define INVALID 0x1U
#define LOW_TOO_HIGH 0x2U
////////////////////////////////

//Define states.  Specific numbers wanted so #define used rather than enum.
#define GET_LOW_ADDR 0x0U
#define GET_HIGH_ADDR 0x1U
#define CS_DISPLAY_HOLD 0x2U
////////////////////////////////////////////////////////////////////////////

//Define struct to contain addresses and flags for invalid addresses//
typedef static struct{
	INT32U addrs[2];  //array to hold the high and low adresses.  addrs[0] = low address, addrs[1] = high address//
	INT8U inval_flag;
}ADDRESS_T;
///////////////////////////////////////////////////////////////////////

//Define string constants for strings output in main///
static const INT8C lowaddrstrg[] = "\n\rEnter Low Address: ";
static const INT8C highaddrstrg[] = "\n\rEnter High Address: ";
static const INT8C csstrg[] = "\n\rCS:";
static const INT8C dashstrg[] = "-";
static const INT8C spacestrg[] = " ";
static const INT8C toolongstrg[] = "\n\rString too long.  Enter an address <= 8 characters.";
static const INT8C nonhexcharstrg[] = "\n\rNon-hex character found in string. Try again.";
static const INT8C nocharacterstrg[] = "\n\rNo characters were entered. Try again.";
static const INT8C hexvallargestrg[] = "\n\rhex value too large. Try again.";
static const INT8C lowtoobigstrg[] = "\n\rHigh address is lower than low address.  Try again.";
///////////////////////////////////////////////////////

INT16U CSCalc(INT8U *startaddr, INT8U *endaddr);

void CSStateMachineEnter(void);

static void CSGetAddr(ADDRESS_T* addr, INT8U state);

static void CSMoveState(ADDRESS_T* addr, INT8U* state);  //MAKE SURE TO CLEAR INVALID FLAG IF SET

#endif /* LAB2PROJ_H_ */
