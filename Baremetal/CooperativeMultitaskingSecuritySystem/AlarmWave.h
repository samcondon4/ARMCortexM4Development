/*******************************************************
* AlarmWave.h - Header for public functions in the 
*				alarm wave module
*
* Sam Condon - 11/11/2019
*****************************************************/

#ifndef ALWAVE_H_
#define ALWAVE_H_

/***********************************************************
 * Initialize PIT timer and DAC to send sin wave to speaker
 ***********************************************************/
void AlarmWaveInit(void);

/**************************************
 * Run AlarmWave task
 ************************************/
void AlarmWaveControlTask(void);

#endif


