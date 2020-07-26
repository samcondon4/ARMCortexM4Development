/****************************************************************************************
 * WaveModule.c - This module runs on board the K65TWR Development board under the uCOS
 *               Real-Time Operating System.
*
*                Through the functionality of Wave Task, this module provides one with
*                the ability to output a waveform that is sinusoidal or triangular in 
*                nature. This waveform can be of frequencies 10 - 10000 Hz. and can 
*                have a peak to peak amplitude of 3.0 V. All waveform parameters are
*                set by using the Public functions WaveGet() and WaveSet(). The DC 
*                offset of the output signal is always 1.65 V.
* 		  
*
* 02/28/2020 : Initial Version Working, Sam Condon / Trevor Schwarz
*****************************************************************************************/
#include "MCUType.h"
#include "app_cfg.h"
#include "os.h"
#include "LcdLayered.h"
#include "WaveModule.h"
#include "K65TWR_GPIO.h"

/************************************************************
 * MODULE DEFINES
 ************************************************************/
#define BUF_SIZE 128u // number of samples in ping-pong buffer
#define SAMPLE_SIZE 2u
#define PIT_VAL 1249U // (desired interrupt period / count period) - 1
#define MSB_MASK 0x80000000u // 32 bit mask

//Fixed point math processing defines//
#define ONE_32 4294967296u // 2^32, represents a value of 1 in 2^32 fixed point format
#define ONE_16 65536u // 2^16, represents a value of 1 in 2^32 fixed point format 

// SINEWAVE --------------------------
#define DOWN_SHIFT_20 20u
#define DAC_OFFSET_ALIGN 0x800u //shifting constant for output of arm sin function to DAC usable range
#define XARG_INCREMENT //(1/48000)*(2^31)
#define DAC_MASK 0x0fffu //mask to clear upper 4 bits of 16bit dac samples since dac takes 12 bits
#define SAMPLE_PERIOD 44739u //q31_t Fixed point sampling period representation. Numerically this is (20.83 uS)*2^31
#define DAC_SAMP_SCALE_SINE 195225786u //(1/20)*(3.0/3.3)*(2^32) scale factor to map dac sample into appropriate amplitude range
#define DAC_MID 8796093022208u //2048*(2^32), represents dac input for (1/2)Vref as 32 bit fixed point
#define SCALED_MID_SIN 399822410100u //2048*(1/20)*(3.0/3.3)*(2^32), represents the midpoint of a scaled sin wave without offset (not centered at (1/2)Vref)
#define DAC_SHIFT(x) ((DAC_MID) - (SCALED_MID_SIN)*x) //calculate the offset to center a scaled sin wave around (1/2)Vref
// TRIWAVE ---------------------------

#define SAMPLE_FREQ 48000u // waveform sampling frequency
#define DAC_SAMP_SCALE_TRI 214748365u // (1/20)*(2^32), represents scale factor for normalized ramp function
#define VOUT_MID 7086696038u //(1.65V)*2^32, DC Offset value
#define SCALED_MID_TRI 322122547u // (1.5)*(1/20)*2^32, scaled waveform offset
#define VOUT_TO_DAC 1301505241u //(1/3.3)*(2^32) vout to dac scale factor

#define GAIN_SCALE 2978u //(1/20)*(3.0/3.3)*2^16, Scale factor for DAC input amplitude scaling
#define DAC_MID_VAL 134152192u //(2047*2^16), DAC input representing (1/2)*Vref
#define SCALED_MID_VAL 6097826u //(2047)*(3.0/3.3)*(1/20)*2^16, Scale factor DAC value DC offset 

////////////////////////////////////////

/**************************************
 * BUFFER Struct:
 *
 *     This struct contains the halfwayflag semaphore for
 *     the DMA ISR to post and WaveTask to pend.     
 *************************************/
typedef struct{
	INT8U bufindex;
	OS_SEM halfwayflag;
}BUF_UPDATE_FLAG_T;

/************************************************************
 * PRIVATE RESOURCES
 ************************************************************/
static INT16U waveOutputBuffer[BUF_SIZE]; //Ouput buffer to DAC
static BUF_UPDATE_FLAG_T waveBufUpdateFlag; //Flag struct
static OS_TCB waveTaskTCB;
static CPU_STK waveTaskStack[APP_CFG_WAVE_TASK_STK_SIZE];
static WAVE_T waveParams;
static OS_MUTEX waveMutexKey;

/*****************************************************************************************
* Function Prototypes.
*****************************************************************************************/
static void WaveTask(void* p_arg);
static void PITInit(void);
static void DACInit(void);
static void DMAInit(void);
static INT16U SineCalc(q31_t xarg);
void DMA0_DMA16_IRQHandler(void);

/***************************************************************************
 *WaveInit() - Initialization function for the WaveModule. After calling this
 	       function, a waveform starting at a default value of 10 Hz. will
	       be output from DAC0.

             Parameters: none
             Returns: none
  
 Sam Condon, 02/27/2020
 ***************************************************************************/
void WaveInit(void){

    OS_ERR os_err;
    q31_t xarg; //q31 format input for CMSIS sin function	
    INT8U k; //iterator used in for loops below 

    waveParams.freq=10;
    waveBufUpdateFlag.bufindex = 0u;
    xarg = 0u;

    //Populate first half of buffer before enabling DMA and PIT
    for(k = (BUF_SIZE/2)*waveBufUpdateFlag.bufindex; k < (BUF_SIZE/2)*(waveBufUpdateFlag.bufindex+1); k++){
            xarg += (q31_t)waveParams.freq*SAMPLE_PERIOD; //increment xarg to next sample
            xarg &= !MSB_MASK; // ensure xarg remains positive
            waveOutputBuffer[k] = SineCalc(xarg); //update buffer index k
    }

    waveBufUpdateFlag.bufindex = 1u; //set index of buffer to next half

    DACInit(); // prep for samples from DMA
    DMAInit(); // configure DMA
    PITInit(); // configure PIT to set sample rate
    DMAMUX->CHCFG[0] = DMAMUX_CHCFG_ENBL(1)|DMAMUX_CHCFG_TRIG(1)|DMAMUX_CHCFG_SOURCE(60); // ENABLE
    NVIC_EnableIRQ(DMA0_DMA16_IRQn); // DMA CH0 Interrupts enabled
    DMA0->SERQ=DMA_SERQ_SERQ(0); // Enable DMA channel


	OSSemCreate(&waveBufUpdateFlag.halfwayflag, "Buffer Halfway Flag", 0u, &os_err);
	OSTaskCreate((OS_TCB*)&waveTaskTCB,
	             (CPU_CHAR*)"Wave Task",
	             (OS_TASK_PTR)WaveTask,
	             (void*) 0,
	             (OS_PRIO) APP_CFG_WAVE_TASK_PRIO,
	             (CPU_STK*)&waveTaskStack[0],
	             (CPU_STK)(APP_CFG_WAVE_TASK_STK_SIZE / 10u),
	             (CPU_STK_SIZE) APP_CFG_WAVE_TASK_STK_SIZE,
	             (OS_MSG_QTY) 0,
	             (OS_TICK) 0,
	             (void*) 0,
	             (OS_OPT)(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
	             (OS_ERR*)&os_err);

	OSMutexCreate(&waveMutexKey, "Wave Params Mutex", &os_err);

}

/****************************************************************************
 *WaveGet() - Copies private waveParams structure to a local copy used external
 *            to the module.
 *
 *          Parameters: 
 *              localwave: pointer to local copy of a WAVE_T structure
 *          
 *          Returns:
 *          	none
 *
 * Sam Condon, 02/27/2020
 ****************************************************************************/
void WaveGet(WAVE_T* localwave){
    OS_ERR os_err;
    OSMutexPend(&waveMutexKey, 0, OS_OPT_PEND_BLOCKING, (CPU_TS*)0, &os_err);
    *localwave = waveParams;
    OSMutexPost(&waveMutexKey, OS_OPT_POST_NONE, &os_err);
}

/****************************************************************************
 *WaveSet() - Copies local WAVE_T structure to private waveParams structure.
 *
 *          Parameters: 
 *              localwave: pointer to local copy of a WAVE_T structure
 *          
 *          Returns:
 *          	none
 *
 * Sam Condon, 02/27/2020
 ****************************************************************************/
void WaveSet(WAVE_T* localwave){
    OS_ERR os_err;
    OSMutexPend(&waveMutexKey, 0, OS_OPT_PEND_BLOCKING, (CPU_TS*)0, &os_err);
    waveParams = *localwave;
    OSMutexPost(&waveMutexKey, OS_OPT_POST_NONE, &os_err);
}

/****************************************************************************
 *WaveTypeGet() - Copies private WaveParams field 'type' to a local field 
 *
 *          Parameters: 
 *              localtype: pointer to a local 'type' parameter 
 *          
 *          Returns:
 *          	none
 *
 * Sam Condon, 02/27/2020
 ****************************************************************************/
void WaveTypeGet(INT8U* localtype){
    OS_ERR os_err;
    OSMutexPend(&waveMutexKey, 0, OS_OPT_PEND_BLOCKING, (CPU_TS*)0, &os_err);
    *localtype = waveParams.type;
    OSMutexPost(&waveMutexKey, OS_OPT_POST_NONE, &os_err);
}

/****************************************************************************
 *WaveTypeSet() - Copies local 'type' field to private WaveParams 'type' field
 *
 *          Parameters: 
 *              localtype: pointer to a local 'type' parameter 
 *          
 *          Returns:
 *          	none
 *
 * Sam Condon, 02/27/2020
 ****************************************************************************/
void WaveTypeSet(INT8U* localtype){
    OS_ERR os_err;
    OSMutexPend(&waveMutexKey, 0, OS_OPT_PEND_BLOCKING, (CPU_TS*)0, &os_err);
    waveParams.type = *localtype;
    OSMutexPost(&waveMutexKey, OS_OPT_POST_NONE, &os_err);
}

/******************************************************************************
 *WaveFreqGet() - Copies private WaveParams field 'freq' to local field 
 *
 *          Parameters:
 *              localfreq: pointer to local 'freq' parameter
 *
 *          Returns:
 *              none
 *
 * Sam Condon, 02/27/2020
 ******************************************************************************/
void WaveFreqGet(INT32U* localfreq){
    OS_ERR os_err;
    OSMutexPend(&waveMutexKey, 0, OS_OPT_PEND_BLOCKING, (CPU_TS*)0, &os_err);
    *localfreq = waveParams.freq;
    OSMutexPost(&waveMutexKey, OS_OPT_POST_NONE, &os_err);
}

/****************************************************************************
 *WaveFreqSet() - Copies local 'freq' field to private WaveParams 'freq' field
 *
 *          Parameters: 
 *              localtype: pointer to a local 'type' parameter 
 *          
 *          Returns:
 *          	none
 *
 * Sam Condon, 02/27/2020
 ****************************************************************************/
void WaveFreqSet(INT32U* localfreq){
    OS_ERR os_err;
    OSMutexPend(&waveMutexKey, 0, OS_OPT_PEND_BLOCKING, (CPU_TS*)0, &os_err);
    waveParams.freq = *localfreq;
    OSMutexPost(&waveMutexKey, OS_OPT_POST_NONE, &os_err);
}

/******************************************************************************
 *WaveAmplGet() - Copies private WaveParams field 'ampl' to local field 
 *
 *          Parameters:
 *              localfreq: pointer to local 'freq' parameter
 *
 *          Returns:
 *              none
 *
 * Sam Condon, 02/27/2020
 ******************************************************************************/
void WaveAmplGet(INT8U* localampl){
    OS_ERR os_err;
    OSMutexPend(&waveMutexKey, 0, OS_OPT_PEND_BLOCKING, (CPU_TS*)0, &os_err);
    *localampl = waveParams.ampl;
    OSMutexPost(&waveMutexKey, OS_OPT_POST_NONE, &os_err);
}

/****************************************************************************
 *WaveAmplSet() - Copies local 'ampl' field to private WaveParams 'ampl' field
 *
 *          Parameters: 
 *              localtype: pointer to a local 'type' parameter 
 *          
 *          Returns:
 *          	none
 *
 * Sam Condon, 02/27/2020
 ****************************************************************************/
void WaveAmplSet(INT8U* localampl){
    OS_ERR os_err;
    OSMutexPend(&waveMutexKey, 0, OS_OPT_PEND_BLOCKING, (CPU_TS*)0, &os_err);
    waveParams.ampl = *localampl;
    OSMutexPost(&waveMutexKey, OS_OPT_POST_NONE, &os_err);
}

/**************************************************************************
 * WaveTask() - Writes to the ping-pong buffer, OutputBuffer with next 
 * 	        BUF_SIZE chunk of waveform samples.
 *
 * Sam Condon, Trevor Schwarz, 02/27/2020
 *************************************************************************/
static void WaveTask(void* p_arg){

	OS_ERR os_err;
	//sin process variables
	INT64U sinecalcret;
	INT64U sineprocinter;
	INT64U xarg; //x value for ramp function processing 
	q31_t xargsin; //q31 format fixed point for input into sin function
	
	//ramp process variables
	INT64U xi; //sample index for ramp function
	INT64U x1; //peak ramp sample index

	INT8U index; // 0 or 1 flag for portion of ramp wave
	INT8U k; // for loop iterator

	xarg = 0u;
	xargsin=0u;
	xi = 0u;
	x1 = 0u;
	index = 0u;

	(void)p_arg;

	while(1){
		DB0_TURN_OFF();
		OSSemPend(&waveBufUpdateFlag.halfwayflag, 0u, OS_OPT_PEND_BLOCKING, (CPU_TS *)0, &os_err);
		DB0_TURN_ON();

		if(waveParams.type==TRIWAVE){
		    x1=( (SAMPLE_FREQ/2)*(ONE_16) )/(INT64U)waveParams.freq;
		} else{}

		for(k = (BUF_SIZE/2)*waveBufUpdateFlag.bufindex; k < (BUF_SIZE/2)*(waveBufUpdateFlag.bufindex+1); k++){

		    if(waveParams.type==TRIWAVE){
	            //
		        xi+=ONE_16;
	            if(xi>x1 && index==0){
	                index=1; // shift index up to xi's current location
	            } else if (xi>(2*x1)){
	                index=0;
	                xi-=(2*x1);
	            } else {}
	            // argument adjustment according to index
	            if(index==0){
	                xarg = xi*ONE_32;
	                xarg = xarg/x1;
	                xarg -= 1;
	            } else {
	                // index 1
	                xarg = ((xi-x1)*ONE_32);
	                xarg = xarg/x1;
	                xarg -= 1;
	                xarg = ONE_32 - xarg;
	            }

	            xarg = xarg >> 20; //shift xarg down to 12 bit
	            xarg = xarg*GAIN_SCALE*(INT64U)waveParams.ampl;
	            xarg += DAC_MID_VAL;
	            xarg -= SCALED_MID_VAL*(INT64U)waveParams.ampl;
	            xarg = xarg >> 16; //shift xarg down to 16 bit to set in output buffer

	            waveOutputBuffer[k] = xarg;
		    } else{ // SINWAVE
	            xargsin += (q31_t)waveParams.freq*SAMPLE_PERIOD; //move xarg to next sample
	            xargsin &= ~MSB_MASK; //mask out sign bit of xarg
	            sinecalcret = SineCalc(xargsin);
	            sineprocinter = sinecalcret*(DAC_SAMP_SCALE_SINE)*(INT64U)waveParams.ampl + DAC_SHIFT((INT64U)waveParams.ampl);
	            waveOutputBuffer[k] = (INT16U)(sineprocinter>>32);
		    }


		}
	}
}

/**********************************************************************
* SineCalc() - Helper function for using the arm_sin function
*
* Sam Condon, 2/28/2020
***********************************************************************/
static INT16U SineCalc(q31_t xarg){

	q31_t sinout;
	INT16U sinout2;
	sinout = arm_sin_q31(xarg);
	sinout2 = (INT16U)(sinout>>DOWN_SHIFT_20); //shift result of arm sin function down to 12 bits for input into DAC0
	sinout2 = sinout2 - DAC_OFFSET_ALIGN; //shift twelve bit sample from range (0x800 -> 0x7ff) -> (0x000 -> 0xfff)
	sinout2 &= DAC_MASK; //mask upper 4 bits of sample data
	return sinout2;

}

/*****************************************************************************************
* PITInit() - Initialize PIT to trigger the DMA at 48 kHz.
*
* Trevor Schwarz, 2/22/2020
*****************************************************************************************/
static void PITInit(void){
    SIM->SCGC6 |= SIM_SCGC6_PIT(1); // Enable PIT's gate clock
    PIT->MCR = PIT_MCR_MDIS(0); // Enable PIT timer in module control register
    PIT->CHANNEL[0].LDVAL = PIT_VAL; // Set to fire every 20.833us (48kHz)
    PIT->CHANNEL[0].TCTRL=PIT_TCTRL_TEN(1); // Timer enabled
    PIT->CHANNEL[0].TCTRL|=PIT_TCTRL_TIE(1); // Interrupt firing enabled
}

/*****************************************************************************************
* DACInit() - Initialize DAC to output samples from DMA input
*
* Trevor Schwarz, 2/22/2020
*****************************************************************************************/
static void DACInit(void){
    SIM->SCGC2 |= SIM_SCGC2_DAC0(1); // Enable DAC0's gate clocks
    SIM->SCGC6 |= SIM_SCGC6_DAC0(1);
    DAC0->C0 |= DAC_C0_DACSWTRG(1); // Enable software trigger
    DAC0->C0 |= DAC_C0_DACRFS(1); // Set DACREF_2 as the reference voltage (VDDA)
    DAC0->C0 |= DAC_C0_DACEN(1); // DAC0 enabled
    DAC0->C1 |= DAC_C1_DMAEN(1); // enable input as DMA
}

/*****************************************************************************************
* DMAInit() - Configures DMA channel 0 to be triggered by PIT channel 0
* 
* Trevor Schwarz, 2/22/2020
*****************************************************************************************/
static void DMAInit(void){
    SIM->SCGC6 |= SIM_SCGC6_DMAMUX(1); // Enable gate clocks
    SIM->SCGC7 |= SIM_SCGC7_DMA(1);
    DMAMUX->CHCFG[0] |= DMAMUX_CHCFG_ENBL(0); // disable to change settings
    DMAMUX->CHCFG[0] |= DMAMUX_CHCFG_TRIG(0); // no trigger

    // SOURCE SETUP ----------------------------------------------------------------------
    DMA0->TCD[0].SADDR = DMA_SADDR_SADDR(waveOutputBuffer); // start address of PING-PONG buffer

    // 16 bit samples from source, 16 samples to destination (masked to 12 bits)
    DMA0->TCD[0].ATTR = DMA_ATTR_SSIZE(1) | DMA_ATTR_DSIZE(1) | DMA_ATTR_SMOD(0) | DMA_ATTR_DMOD(0);
    DMA0->TCD[0].SOFF = DMA_SOFF_SOFF(2); // source address offset of 2 bytes (16bits) per sent sample ( SAMPLE_SIZE )

    DMA0->TCD[0].SLAST = DMA_SLAST_SLAST(-(BUF_SIZE*SAMPLE_SIZE)); // length of table start return (offset value) -------------------- SIZE (BYTES) OF PING PONG BUFFER

    // DESTINATION SETUP -----------------------------------------------------------------
    DMA0->TCD[0].DADDR = DMA_DADDR_DADDR(&DAC0->DAT[0].DATL); // destination address is DAC data low register
    DMA0->TCD[0].DOFF = DMA_DOFF_DOFF(0); // destination offset not applicable

    DMA0->TCD[0].NBYTES_MLNO = DMA_NBYTES_MLNO_NBYTES(2); // minor loop same as sample size
    DMA0->TCD[0].CITER_ELINKNO = DMA_CITER_ELINKNO_ELINK(0) | DMA_CITER_ELINKNO_CITER(BUF_SIZE); // minor loops per major loop = number of samples
    DMA0->TCD[0].BITER_ELINKNO = DMA_BITER_ELINKNO_ELINK(0) | DMA_BITER_ELINKNO_BITER(BUF_SIZE); // value reloaded into CITER field
    DMA0->TCD[0].DLAST_SGA = DMA_DLAST_SGA_DLASTSGA(0); // destination offset not applicable (destination doesn't change)

    // ENABLE INTERRUPTS at halfway and end of major loop, set default values for other fields
    DMA0->TCD[0].CSR = DMA_CSR_ESG(0) | DMA_CSR_MAJORELINK(0) | DMA_CSR_BWC(3) |
                       DMA_CSR_DREQ(0) |DMA_CSR_START(0) |
                       DMA_CSR_INTHALF(1) | DMA_CSR_INTMAJOR(1);

}

/************************************************************************
 *DMA0_DMA16_IRQHandler() - Interrupt every time we run through 1/2 of the output
 *                          buffer.
 ************************************************************************/
void DMA0_DMA16_IRQHandler(void){
    OS_ERR os_err;
    OSIntEnter();
    DMA0->CINT = DMA_CINT_CINT(0); // clear interrupt flag
    // toggle index and post semaphore
    waveBufUpdateFlag.bufindex^=1; // toggle index
    (void)OSSemPost(&waveBufUpdateFlag.halfwayflag, OS_OPT_POST_1, &os_err); // set the toggle flag
    OSIntExit();
}
