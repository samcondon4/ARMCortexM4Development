/************************************************************************
 * Temp.h - Header file containing all public resources for the Temp module
 *
 * Sam Condon, 12/01/2019
 *************************************************************************/

#ifndef TEMP_H_
#define TEMP_H_

void TempInit(void); //public initialization function
void TempTask(void); //public task

//Structure to hold all intertask communication data for TempTask and other modules/tasks
typedef struct{
	INT8U unit; //0 for celcius, 1 for fahrenheit.  This will be set in an external module to tell TempTask which unit to measure a temperature in
	INT8U tempdispval;
	INT8U negflag;
}TEMP_COM_T;
extern TEMP_COM_T TTempCom;

#endif /* TEMP_H_ */
