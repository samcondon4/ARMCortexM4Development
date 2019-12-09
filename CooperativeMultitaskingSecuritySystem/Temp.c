/*************************************************************************************************
 * Temp - Module containing initialization routine for ADC and PIT channel 1 to sample temp sensor data.
 * 		  Module also contains task to control sampling of the ADC and fixed point math conversion to celcius
 * 		  or fahrenheit.  Task to be run in a time slice scheduler every 50 slices.
 *
 * Sam Condon, 11/30/2019
 **************************************************************************************************/

#include "MCUType.h"
#include "Temp.h"
#include "Lab5Main.h"
#include "MK65F18.h"
#include "BasicIO.h"
#include "K65TWR_GPIO.h"

#define CELCIUS 0u
#define FAHRENHEIT 1u
#define TWO_HERTZ_LDVAL 29999999u

/****************************************
 * FIXED POINT MATH CONSTANTS (ALL WITH SCALE FACTOR OF 2^16
 * 	(fixed point math op constants for ADC conversion processing)
 * 		-MULT_VAL_1: 3.3v(VrefH)*2^16
 * 		-MULT_VAL_2: (1/0.0195)*2^16
 * 		-SUB_VAL: 0.4*2^16
 * 	(fixed point math op constants for celcius to fahrenheit conversion)
 * 		-MULT_VAL_3: (9/5)*2^16
 * 		-ADD_VAL: 32*2^16
 ****************************************/
#define MULT_VAL_1 216269u
#define MULT_VAL_2 3360821u
#define SUB_VAL 26214u

#define MULT_VAL_3 117965u
#define ADD_VAL 2097152u
///////////////////

/*******************************************************
 * TempInit() - Initialization routine for the ADC
 *
 * 	Parameters: none
 * 	Returns: none
 *
 * 11/30/2019, SSC
 *******************************************************/
void TempInit(void){

	//INITIALIZE ADC//////////////////////////////////////////////////////////
	SIM->SCGC6 |= SIM_SCGC6_ADC0_MASK;

	ADC0->CFG1 = ADC_CFG1_ADIV(3) | ADC_CFG1_MODE(3) | ADC_CFG1_ADLSMP_MASK | ADC_CFG1_ADICLK(1); //set clock source/divide, sample size, and conversion speed
	ADC0->SC3 = ADC_SC3_AVGE(1) | ADC_SC3_AVGS(3); //set conversion to be an average over 32 cts samples each conversion
	ADC0->SC1[0] = ADC_SC1_ADCH(3);

	SIM->SOPT7 = SIM_SOPT7_ADC0TRGSEL(5u); //set PIT1 to trigger ADC0
	SIM->SOPT7 |= SIM_SOPT7_ADC0ALTTRGEN(1);
	ADC0->SC2 |= ADC_SC2_ADTRG_MASK; //enable hardware triggering on ADC0
	//////////////////////////////////////////////////////////////////////////

	//INITIALIZE PIT1 FOR ADC TRIGGERING AT 2 Hz.///
	SIM->SCGC6 |= SIM_SCGC6_PIT(1u);
	PIT->MCR &= PIT_MCR_MDIS(0u);
	PIT->CHANNEL[1].LDVAL = TWO_HERTZ_LDVAL;
	PIT->CHANNEL[1].TFLG |= PIT_TFLG_TIF_MASK;
	PIT->CHANNEL[1].TCTRL |= PIT_TCTRL_TIE_MASK;
	////////////////////////////////////////////////

	//ENABLE ADC AND PIT INTERRUPTS IN NVIC/////////////////

	////////////////////////////////////////////////////////

}

/***************************************************************
 * TempTask() - start or sample a conversion from the ADC, convert
 * 				any sample from ADC to celcius or fahrenheit
 *
 * 	Parameters: none
 * 	Returns: none
 *
 * 11/30/2019
 ***************************************************************/
void TempTask(void){

	static INT8U convinitflag = 0u; //flag for an initial conversion
	static INT8S adctaskcnt = 0u; //time slice counter
	static INT64U adcreadval; //to hold data read from adc data result register

	//FIXED POINT MATH PROCESSING INTERMEDIATE VARIABLES//////////
	INT64U postproc1;
	INT64U adcvin;
	INT64U tscaled;
	INT8S tcelcius;
	INT16S tfahrenheit;

	INT64S postproc2;
	///////////////////////////////////////////////////////////////
	adctaskcnt++;

	switch(convinitflag)
	{

		case(0u): //perform initial conversion
			adctaskcnt = -2; //set to -2 after initial conversion to offset TempTask from other tasks
			convinitflag = 1u;
			PIT->CHANNEL[1].TCTRL |= PIT_TCTRL_TEN_MASK; //enable PIT channel 1 to start hardware triggering of ADC
		break;

		case(1u):
			if((adctaskcnt >= 50) && ((ADC0->SC1[0] & ADC_SC1_COCO_MASK) != 0u)){ //if 500 mS. has elapsed, and conversion complete flag is set, sample adc data result and convert to fahrenheit or celcius
				DB6_TURN_ON();
				adctaskcnt = 0u;
				adcreadval = ADC0->R[0]; //read data
				postproc1 = adcreadval*MULT_VAL_1;
				adcvin = postproc1 >> 16; //now we have the voltage in, scaled by 2^16
				if(adcvin < SUB_VAL){ //if voltage signifies a negative temperature
					TTempCom.negflag = 1u;
					tscaled = ((SUB_VAL - adcvin)*MULT_VAL_2)>>16;
				}
				else{
					TTempCom.negflag = 0u;
					tscaled = ((adcvin - SUB_VAL)*MULT_VAL_2)>>16;
				}

				tcelcius = (INT8U)(tscaled >> 16u); //temperature result in degrees celcius
				if((tcelcius > 40) || (TTempCom.negflag == 1u)){ //set alarm flag if temperatuer is measured outside of allowed range
					L5mAlarmFlags = TEMP;
				}
				else{}

				if(TTempCom.unit == CELCIUS){
					TTempCom.tempdispval = tcelcius;
				}
				else{ //convert celcius to fahrenheit
					if(TTempCom.negflag==1u){
						tcelcius = tcelcius*-1;
					}
					else{}
					postproc2 =  tcelcius*MULT_VAL_3;
					tfahrenheit = (postproc2 + ADD_VAL)>>16;
					if(tfahrenheit >= 0){
						TTempCom.tempdispval = tfahrenheit;
						TTempCom.negflag = 0u;
					}
					else{
						tfahrenheit = tfahrenheit*-1;
						TTempCom.negflag = 1u;
						TTempCom.tempdispval = tfahrenheit;
					}

				}
				DB6_TURN_OFF();
			}
			else{}

		break;

		default:
		break;

	}

}
