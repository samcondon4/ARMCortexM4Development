/***************************************************************
 * Sense.h - header file for sense module. define
 *
 *
 *
 ***************************************************************/

#ifndef SENSE_H_
#define SENSE_H_

//STRUCT TO HOLD STATES OF ALL SENSOR INPUT//////////////////////////////////////////

typedef struct{
	INT8U electrodeState;
	INT8U prevElectrodeState;
	INT8U scanDoneState;
	INT16U tsiBaselineLevels[2]; //index 0 for ELECTRODE1, index 1 for ELECTRODE2
	INT16U tsiTouchLevels[2];	 //same indexing as above
}TSI;

extern TSI SSenseState;
//////////////////////////////////////////////////////////////////////////////////////

//PROTOTYPES///////////////////////
void SensorTask(void);
void TSIInit(TSI* sensestate);
//////////////////////////////////

#endif /* SENSE_H_ */
