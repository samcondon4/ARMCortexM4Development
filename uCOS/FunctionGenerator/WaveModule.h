/*************************************************************************************** WaveModule.h - This module runs on board the K65TWR Development board under the
*                uCOS Real-Time Operating System. 
*
*                Through the functionality of Wave Task, this module provides one with
*                the ability to output a waveform that is sinusoidal or triangular in 
*                nature. This waveform can be of frequencies MIN_FREQ - MAX_FREQ Hz. 
*                and can have a peak to peak amplitude of 3.0 V. All waveform parameter
*                are set by using the Public functions WaveGet() and WaveSet(). The DC 
*                offset of the output signal is always 1.65 V.
* 		  
*
* 02/28/2020 : Initial Version Working, Sam Condon / Trevor Schwarz
*****************************************************************************************/

#ifndef WAVEMODULE_H_
#define WAVEMODULE_H_

/*********************************************
 * PUBLIC RESOURCES AND DEFINES
 *********************************************/
#define SINWAVE 1U
#define TRIWAVE 0U

#define MIN_FREQ 10U
#define MAX_FREQ 10000

/**********************************************************
* Wave Struct:
*
*     This type provides a structure containing fields 
*     for all needed manipulations of the output waveform.
***********************************************************/
typedef struct {
    INT8U type;
    INT32U freq;
    INT8U ampl;
}WAVE_T;

/***************************************************************************
 *WaveInit() - Initialization function for the WaveModule. After calling this
 	       function, a waveform starting at a default value of 10 Hz. will
	       be output from DAC0.

             Parameters: none
             Returns: none
 ***************************************************************************/
void WaveInit(void);

/****************************************************************************
 *WaveGet() - Copies private WaveParams structure to a local copy used external
 *            to the module.
 *
 *          Parameters: 
 *              localwave: pointer to local copy of a WAVE_T structure
 *          
 *          Returns:
 *          	none
 ****************************************************************************/
void WaveGet(WAVE_T* localwave);

/****************************************************************************
 *WaveSet() - Copies local WAVE_T structure to private WaveParams structure. 
 *
 *          Parameters: 
 *              localwave: pointer to local copy of a WAVE_T structure
 *          
 *          Returns:
 *          	none
 ****************************************************************************/
void WaveSet(WAVE_T* localwave);

/****************************************************************************
 *WaveTypeGet() - Copies private WaveParams field 'type' to a local field 
 *
 *          Parameters: 
 *              localtype: pointer to a local 'type' parameter 
 *          
 *          Returns:
 *          	none
 ****************************************************************************/
void WaveTypeGet(INT8U* localtype);

/****************************************************************************
 *WaveTypeSet() - Copies local 'type' field to private WaveParams 'type' field
 *
 *          Parameters: 
 *              localtype: pointer to a local 'type' parameter 
 *          
 *          Returns:
 *          	none
 ****************************************************************************/
void WaveTypeSet(INT8U* localtype);

/******************************************************************************
 *WaveFreqGet() - Copies private WaveParams field 'freq' to local field 
 *
 *          Parameters:
 *              localfreq: pointer to local 'freq' parameter
 *
 *          Returns:
 *              none
 ******************************************************************************/
void WaveFreqGet(INT32U* localfreq);

/****************************************************************************
 *WaveFreqSet() - Copies local 'freq' field to private WaveParams 'freq' field
 *
 *          Parameters: 
 *              localtype: pointer to a local 'type' parameter 
 *          
 *          Returns:
 *          	none
 ****************************************************************************/
void WaveFreqSet(INT32U* localfreq);

/******************************************************************************
 *WaveAmplGet() - Copies private WaveParams field 'ampl' to local field 
 *
 *          Parameters:
 *              localfreq: pointer to local 'freq' parameter
 *
 *          Returns:
 *              none
 ******************************************************************************/
void WaveAmplGet(INT8U* localampl);

/****************************************************************************
 *WaveAmplSet() - Copies local 'ampl' field to private WaveParams 'ampl' field
 *
 *          Parameters: 
 *              localtype: pointer to a local 'type' parameter 
 *          
 *          Returns:
 *          	none
 ****************************************************************************/
void WaveAmplSet(INT8U* localampl);

////////////////////////////////////////////////


#endif /* WAVEMODULE_H_ */
