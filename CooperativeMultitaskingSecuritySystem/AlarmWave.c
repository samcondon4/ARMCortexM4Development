/********************************************************************************
* AlarmWave - Module containing the initialization routine for PIT channel 1
* 			  and an associated DMA channel to send sample outputs to DAC0
* 			  on a PIT trigger.  Also contains a control task that turns on and
* 			  off the PIT timer based on the state of L5mSysState.  This effectively
* 			  turns on and off the alarm sound output from DAC0
*
* Sam Condon, 11/11/2019 - basic function sends 64 samples stored in dac_buf[] through dac at 19200 sps
* Sam Condon, 11/23/2019 - added DMA functionality
***********************************************************************************/

//INCLUDE DEPENDENCIES////////
#include "MCUType.h"
#include "MK65F18.h"
#include "K65TWR_GPIO.h"
#include "AlarmWave.h"
#include "SysTickDelay.h"
#include "BasicIO.h"
#include "Lab5Main.h"
//////////////////////////////

//DEFINES AND MACROS////////////////////////////////////////////////////////////////////
#define LD_VAL0 3119U;
#define TMR_ENABLE 0x00000001
#define TMR_DISABLE ~TMR_ENABLE

#define TMR_TCTRL(channel) (PIT->CHANNEL[channel].TCTRL)
#define TMR_EN(channel) (INT8U)(PIT->CHANNEL[channel].TCTRL & PIT_TCTRL_TEN_MASK)

#define DAC_SHIFT2 12U
#define DAC_EN2(x) (1U<<(DAC_SHIFT2 + x))

#define DAC0_SHIFT6 31U
#define DAC0_EN6 (1U<<(DAC0_SHIFT6))

#define DMA_DAC0_CH 0u
#define TCD_ATTR_SSIZE_SHIFT 24u
#define SIZE_CODE_16BIT 001u
#define DMA_BYTES_PER_SAMP 2u
#define BYTES_PER_BLOCK 128u
#define ALARM_SAMPS_PER_BLOCK 64u
////////////////////////////////////////////////////////////////////////////////////////

//PRIVATE PROTOTYPES///////////////////////////////////////////////////////////////////
static void EnableTimer(INT8U channel);
static void DisableTimer(INT8U channel);
static void DacIdle(void);

///////////////////////////////////////////////////////////////////////////////////////

//DAC WRITE VALUES////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static const INT16U dac_buf[] = {2048u,2784u,3416u,3859u,4064u,4022u,3769u,3373u,2922u,2501u,2182u,2006u,
								 1979u,2074u,2242u,2423u,2560u,2615u,2575u,2279u,2100u,1960u,1888u,1898u,
								 1977u,2099u,2224u,2315u,2344u,2300u,2192u,2048u,1903u,1795u,1751u,1780u,
								 1871u,1996u,2118u,2197u,2207u,2135u,1995u,1816u,1644u,1521u,1480u,1536u,1672u,
								 1853u,2021u,2116u,2089u,1913u,1594u,1173u,722u,326u,73u,32u,236u,680u,1312u,2048u,
								};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*****************************************************************************
 * AlamWaveInit() - Initialize PIT timer, NVIC and DAC to send sin wave to speaker.
 * 					PIT timer is initialized but not enabled through this function.
 * 					Enabled later.
 *
 ******************************************************************************/
void AlarmWaveInit(void){
	////INITIALIZE PIT TIMER////////////
	SIM->SCGC6 |= SIM_SCGC6_PIT(1);
	PIT->MCR = PIT->MCR & ~(0x2U);
	PIT->CHANNEL[0].LDVAL = LD_VAL0;
	PIT->CHANNEL[0].TCTRL |= 2U;
	/////////////////////////////////////

	////INITIALIZE DAC///////////////////
	SIM->SCGC2 |= DAC_EN2(0);
	SIM->SCGC6 |= DAC0_EN6;
	DAC0->C0 |= (DAC_C0_DACEN_MASK|DAC_C0_DACRFS_MASK|DAC_C0_DACTRGSEL_MASK);
	DacIdle();
	//////////////////////////////////////

	//INITIALIZE DMA////////////////////////////////////////////////////////////////////////////////////
	SIM->SCGC6 |= SIM_SCGC6_DMAMUX_MASK;
	SIM->SCGC7 |= SIM_SCGC7_DMA_MASK;
	DMAMUX->CHCFG[DMA_DAC0_CH] |= (DMAMUX_CHCFG_ENBL(0) | DMAMUX_CHCFG_TRIG(0));

	//configure dma source//
	DMA0->TCD[DMA_DAC0_CH].SADDR = DMA_SADDR_SADDR(dac_buf);
	DMA0->TCD[DMA_DAC0_CH].ATTR |= (1u) << TCD_ATTR_SSIZE_SHIFT;

	DMA0->TCD[DMA_DAC0_CH].ATTR = DMA_ATTR_SMOD(0) | DMA_ATTR_SSIZE(SIZE_CODE_16BIT)
								  | DMA_ATTR_DMOD(0) | DMA_ATTR_DSIZE(SIZE_CODE_16BIT);
	DMA0->TCD[DMA_DAC0_CH].SOFF = DMA_SOFF_SOFF(DMA_BYTES_PER_SAMP);
	DMA0->TCD[DMA_DAC0_CH].SLAST = DMA_SLAST_SLAST(-(BYTES_PER_BLOCK));
	/////////////////////////

	//configure dma destination//
	DMA0->TCD[DMA_DAC0_CH].DADDR = DMA_DADDR_DADDR(&DAC0->DAT[0].DATL);
	DMA0->TCD[DMA_DAC0_CH].DOFF = DMA_DOFF_DOFF(0u);
	/////////////////////////////

	//enable DMAMUX and DMA//
	DMAMUX->CHCFG[DMA_DAC0_CH] = DMAMUX_CHCFG_ENBL(1) | DMAMUX_CHCFG_TRIG(1) | DMAMUX_CHCFG_SOURCE(60);
	DMA0->SERQ = DMA_SERQ_SERQ(DMA_DAC0_CH);
	////////////////////////

	//set minor loops//
	DMA0->TCD[DMA_DAC0_CH].NBYTES_MLNO = DMA_NBYTES_MLNO_NBYTES(DMA_BYTES_PER_SAMP);
	DMA0->TCD[DMA_DAC0_CH].CITER_ELINKNO = DMA_CITER_ELINKNO_CITER(ALARM_SAMPS_PER_BLOCK);
	DMA0->TCD[DMA_DAC0_CH].BITER_ELINKNO = DMA_BITER_ELINKNO_BITER(ALARM_SAMPS_PER_BLOCK);
	///////////////////

	//set offset after major loop to 0
	DMA0->TCD[DMA_DAC0_CH].DLAST_SGA = DMA_DLAST_SGA_DLASTSGA(0);

	//finish DMA TCD structure initialization//
	DMA0->TCD[DMA_DAC0_CH].CSR = DMA_CSR_ESG(0) | DMA_CSR_MAJORELINK(0) |
								 DMA_CSR_BWC(3) | DMA_CSR_INTHALF(0) |
							     DMA_CSR_INTMAJOR(0) | DMA_CSR_DREQ(0) |
								 DMA_CSR_START(0);

	//////////////////////////////////////////////////////////////////////////////////////////////////////
}

/*********************************************************************************
 * AlarmWaveControlTask() - Enable/disable PIT timer based on AlarmState value.
 *
 ********************************************************************************/
void AlarmWaveControlTask(void){

	static INT32U tscount = 0; //time-slice counter

	tscount++;

	if(tscount >= 5U){
		DB5_TURN_ON();
		tscount = 0;
		if(L5mPrevSysState != L5mSysState){
			switch(L5mSysState)
					{
					case(ALARM):
						EnableTimer(0u);
					break;

					default:
						DisableTimer(0u);
					break;
					}
			}
		DB5_TURN_OFF();
	}

	else{}

}

/***************************************************
 * EnableTimer() - Turn on PIT timer.
 *
 ***************************************************/
static void EnableTimer(INT8U channel){
	PIT->CHANNEL[channel].TCTRL |= TMR_ENABLE;
}

/***************************************************
 * DisableTimer() - Turn off PIT timer.
 *
 ***************************************************/
static void DisableTimer(INT8U channel){
	PIT->CHANNEL[channel].TCTRL &= TMR_DISABLE;
	DacIdle();
}

/*****************************************************
 * DacIdle() - Write 0.5 of wave full scale to set a
 * 			   1.65 V. idle output from DAC0
 *
 *****************************************************/
static void DacIdle(void){
	DAC0->DAT[0].DATH = (dac_buf[0]>>8);
	DAC0->DAT[0].DATL = (INT8U)(dac_buf[0]);
}



