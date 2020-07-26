/*****************************************************************************************
* EE 444 Lab #3 - Function Generator
*  10Hz-10kHz function generator with sine and ramp wave output capabilities. Output signal
*  is centered at 1.65V with Vpk=0 to 1.5V, selectable with a resolution of 21 steps using
*  the TSI touch sensors. Frequency is entered using the keypad and saved with the '#' key,
*  modified with the 'D' key.  Waveform type is selected with the A (sine) and B(ramp)
*  keys. The current state of the waveform is displayed to the LCD as described below.
*
*  LCD Display Scheme:
*   Left side, top row - frequency of output signal
*   Left side, bottom row - desired frequency
*   Right side, top row - waveform amplitude (0-20)
*   Right side, bottom row - waveform type (SIN or TRI)
*
*  For an amplitude of 0, 1.65V DC is output from the generator. At high frequencies, the
*  ramp function approximates a sine wave because of the external low pass filtering and
*  limited number of samples per period.
*
*  Debug bits are defined as follows (in ascending order of assertion rate):
*   DB0 - WaveTask - runs every 1.3ms (64 samples in half Ping-Pong buffer * 1/48kHz sample rate)
*   DB1 - KeyTask - set to run every 8ms
*   DB2 - TSITask - set to run every 8ms
*   DB3 - TSIProcTask - runs every two TSI scans (16ms)
*   DB4 - UITask - runs on every keypress
*   DB5 - LcdLayeredTask - runs on every write to LCD
*
* Trevor Schwarz, Sam Condon
* Finished 2/28/2020
*****************************************************************************************/
#include "app_cfg.h"
#include "os.h"
#include "MCUType.h"
#include "K65TWR_ClkCfg.h"
#include "K65TWR_GPIO.h"
#include "LcdLayered.h"
#include "uCOSKey.h"
#include "TSIModule.h"
#include "WaveModule.h"

/********************************************************************
* Module Defines
********************************************************************/
// KEY MODULE
#define KEYPAD_A_BUTTON 0x11U
#define KEYPAD_B_BUTTON 0x12U
#define KEYPAD_D_BUTTON 0x14U
// LCD MODULE
#define CURSOR_FORWARD 1U
#define CURSOR_BACKWARD 0U
#define CURSOR_ON    1U
#define CURSOR_BLINK 1U
#define MODE_LZ 1U
#define SHOW_FIVE_DIGITS 5U
// ApplyInput()
#define BACKSPACE 1U
#define CHANGE 1U
#define NO_CHANGE 0U
// AppTSIProcTask()
#define AMPL_INCREMENT 0U
#define AMPL_DECREMENT 1U
#define DEASSERTED 0U
#define ASSERTED 1U
#define DOWNPRESS 0U // TSI left index
#define UPPRESS 1U   // TSI right index
// UpdateAmp()
#define MAX_AMPL 20u
#define MIN_AMPL 0u
//


/*****************************************************************************************
* Allocate task control blocks
*****************************************************************************************/
static OS_TCB AppTaskStartTCB;
static OS_TCB AppUITaskTCB;
static OS_TCB AppTSIProcTaskTCB;


/*****************************************************************************************
* Allocate task stack space.
*****************************************************************************************/
static CPU_STK AppTaskStartStk[APP_CFG_TASK_START_STK_SIZE];
static CPU_STK AppUITaskStk[APP_CFG_UI_TASK_STK_SIZE];
static CPU_STK AppTSIProcTaskStk[APP_CFG_TSIPROC_TASK_STK_SIZE];


/*****************************************************************************************
* Task Function Prototypes. 
*****************************************************************************************/
static void  AppStartTask(void *p_arg);
static void  AppUITask(void *p_arg);
static void  AppTSIProcTask(void *p_arg);


/*****************************************************************************************
* Helper Function Prototypes.
*****************************************************************************************/
static INT8U NumInput(INT8C keyin);
static INT8U ApplyInput(INT16U *freq, INT8U val, INT8U backspace);
static INT8U UpdateAmp(INT8U delta, INT8U *amplitude);


// Error trap code template
//while(os_err != OS_ERR_NONE){} // Error trap

/*****************************************************************************************
* main()
* Taken from uCOSAppDemo.c
*****************************************************************************************/
void main(void) {

    OS_ERR  os_err;

    K65TWR_BootClock();
    CPU_IntDis();               /* Disable all interrupts, OS will enable them  */

    OSInit(&os_err);                    /* Initialize uC/OS-III                         */

    OSTaskCreate(&AppTaskStartTCB,                  /* Address of TCB assigned to task */
                 "Start Task",                      /* Name you want to give the task */
                 AppStartTask,                      /* Address of the task itself */
                 (void *) 0,                        /* p_arg is not used so null ptr */
                 APP_CFG_TASK_START_PRIO,           /* Priority you assign to the task */
                 &AppTaskStartStk[0],               /* Base address of task�s stack */
                 (APP_CFG_TASK_START_STK_SIZE/10u), /* Watermark limit for stack growth */
                 APP_CFG_TASK_START_STK_SIZE,       /* Stack size */
                 0,                                 /* Size of task message queue */
                 0,                                 /* Time quanta for round robin */
                 (void *) 0,                        /* Extension pointer is not used */
                 (OS_OPT_TASK_NONE), /* Options */
                 &os_err);                          /* Ptr to error code destination */

    OSStart(&os_err);               /*Start multitasking(i.e. give control to uC/OS)    */

}

/*****************************************************************************************
* STARTUP TASK
* This should run once and be suspended. Could restart everything by resuming.
* (Resuming not tested)
* Todd Morton, 01/06/2016
* Modified by Trevor Schwarz for Lab 3, 2/27/2020
*****************************************************************************************/
static void AppStartTask(void *p_arg) {

    OS_ERR os_err;

    (void)p_arg;                        /* Avoid compiler warning for unused variable   */

    OS_CPU_SysTickInitFreq(SYSTEM_CLOCK);
    /* Initialize StatTask. This must be called when there is only one task running.
     * Therefore, any function call that creates a new task must come after this line.
     * Or, alternatively, you can comment out this line, or remove it. If you do, you
     * will not have accurate CPU load information                                       */
//    OSStatTaskCPUUsageInit(&os_err); // NOTE: DISABLED IN app_cfg.h
    GpioDBugBitsInit();
    LcdInit();
    KeyInit();
    TSIInit();
    WaveInit();

    OSTaskCreate(&AppUITaskTCB,           /* Create UITask                   */
                "App UITask ",
                AppUITask,
                (void *) 0,
                APP_CFG_UI_TASK_PRIO,
                &AppUITaskStk[0],
                (APP_CFG_UI_TASK_STK_SIZE / 10u),
                APP_CFG_UI_TASK_STK_SIZE,
                0,
                0,
                (void *) 0,
                (OS_OPT_TASK_NONE),
                &os_err);

    OSTaskCreate(&AppTSIProcTaskTCB,    /* Create DispTimeTask              */
                "App TSIProcTask ",
                AppTSIProcTask,
                (void *) 0,
                APP_CFG_TSIPROC_TASK_PRIO,
                &AppTSIProcTaskStk[0],
                (APP_CFG_TSIPROC_TASK_STK_SIZE / 10u),
                APP_CFG_TSIPROC_TASK_STK_SIZE,
                0,
                0,
                (void *) 0,
                (OS_OPT_TASK_NONE),
                &os_err);

    OSTaskSuspend((OS_TCB *)0, &os_err);

}

/*****************************************************************************************
* UITask - User Interface
*  Handles user keypad input to set the frequency and waveform type of the system output.
*  Uses helper functions to achieve frequency entry and outputs state of frequency and
*  signal type to the UI_LAYER of the LCD.
*
* Trevor Schwarz, 2/28/2020
*****************************************************************************************/
static void AppUITask(void *p_arg){
    OS_ERR os_err;
    (void)p_arg;

    INT8U keyinput; // local variable to hold keypad codes
    INT16U freqentry; // frequency entered by keypad
    INT8U reset; // out of reset flag, required for correct TIME_SET state behavior
    INT8U numentry; // holds Key code conversions
    INT8U validentry;

    INT32U wavemodfreq;
    INT8U wavemodtype;

    reset=1; // high flag
    keyinput=0u; // 0 values for reset
    freqentry=0u;
    numentry=0u;

    while(1){
        DB4_TURN_ON();
        if(reset){
            // Enable A layer cursor
            (void)LcdCursor(LCD_ROW_2, LCD_COL_5, UI_LAYER, CURSOR_ON, CURSOR_BLINK); // cursor on and blinking
            // Display generator frequency
            WaveFreqGet(&wavemodfreq);
            LcdDispDecWord(LCD_ROW_1, LCD_COL_1, UI_LAYER, wavemodfreq, SHOW_FIVE_DIGITS, MODE_LZ);
            LcdDispString( LCD_ROW_1, LCD_COL_6, UI_LAYER, "HZ");
            // Display frequency entry
            freqentry=MIN_FREQ;
            LcdDispDecWord(LCD_ROW_2, LCD_COL_1, UI_LAYER, freqentry, SHOW_FIVE_DIGITS, MODE_LZ);
            LcdDispString( LCD_ROW_2, LCD_COL_6, UI_LAYER, "HZ");
            // Display wave type
            WaveTypeGet(&wavemodtype);
            if(wavemodtype==SINWAVE){
                LcdDispString(LCD_ROW_2, LCD_COL_14, UI_LAYER, "SIN");
            } else {
                LcdDispString(LCD_ROW_2, LCD_COL_14, UI_LAYER, "TRI");
            }
            reset=0;
        }

        switch(keyinput){
            case KEYPAD_A_BUTTON: // send new data to WaveModule, display on LCD
                wavemodtype=SINWAVE;
                WaveTypeSet(&wavemodtype);
                LcdDispString(LCD_ROW_2, LCD_COL_14, UI_LAYER, "SIN");
                break;
            case KEYPAD_B_BUTTON:
                wavemodtype=TRIWAVE;
                WaveTypeSet(&wavemodtype);
                LcdDispString(LCD_ROW_2, LCD_COL_14, UI_LAYER, "TRI");
                break;
            case '#':
                if(freqentry>=MIN_FREQ){
                    wavemodfreq=(INT32U) freqentry;
                    WaveFreqSet(&wavemodfreq);
                    LcdDispDecWord(LCD_ROW_1, LCD_COL_1, UI_LAYER, (INT32U) freqentry, SHOW_FIVE_DIGITS, MODE_LZ);
                }
		else{} 
                break;
            case KEYPAD_D_BUTTON:
                (void)ApplyInput(&freqentry,0,BACKSPACE); // remove last entry
                LcdDispDecWord(LCD_ROW_2, LCD_COL_1, UI_LAYER, (INT32U) freqentry, SHOW_FIVE_DIGITS, MODE_LZ);
                break;
            default:
                numentry=NumInput(keyinput);
                if (numentry<10) { // if a number key was pressed
                    validentry=ApplyInput(&freqentry,numentry,!BACKSPACE);
                    if (validentry) { // if the modification to frequency is legal
                        // display the current frequency entry
                        LcdDispDecWord(LCD_ROW_2, LCD_COL_1, UI_LAYER, (INT32U) freqentry, SHOW_FIVE_DIGITS, MODE_LZ);
                    } else {}
                } else {
                    // invalid entry, change nothing
                }
                break;
        }
        DB4_TURN_OFF();
        keyinput=KeyPend(0,&os_err); // wait for new key press

    }
}

/*****************************************************************************************
* TSIProcessTask() - Reads touch sensor inputs
*  Interprets TSI module input and updates amplitude status as appropriate.  Helper functions
*  do not allow the amplitude to be outside the range of 0-20.  Will not accept a sensor
*  input until that sensor has been deasserted for at least one sampling (press and hold
*  does not change amplitude by more than one).
*
* Trevor Schwarz, 2/28/2020
*****************************************************************************************/
static void AppTSIProcTask(void *p_arg){
    OS_ERR os_err;
    (void)p_arg;

    INT8U *touchsense; // pointer to an array  
    INT8U lcdchange; // track whether the display needs to update
    INT8U reset;
    INT8U prevstate[2]={0,0}; // used to track last state of button presses

    INT8U amplitude;

    reset=1;

    while(1) {
        DB3_TURN_OFF(); // don't assert debug bit while waiting
        touchsense=TSIPend(0, &os_err); // read the touch sensors
        WaveAmplGet(&amplitude); // read the wave data
        DB3_TURN_ON();

        // ANALYZE INPUTS
        if(reset){
            lcdchange=TRUE; // the display needs to show the starting value on reset
            reset=0u;
        } else {
            lcdchange=FALSE;
        }

        if(touchsense[DOWNPRESS] && !prevstate[DOWNPRESS]){ // if DOWNPRESS and legal, decrement
            lcdchange=UpdateAmp(AMPL_DECREMENT, &amplitude);
            prevstate[DOWNPRESS]=ASSERTED;
        }
        if (touchsense[UPPRESS] && !prevstate[UPPRESS]){ // if UPPRESS and legal, increment
            lcdchange=UpdateAmp(AMPL_INCREMENT, &amplitude);
            prevstate[UPPRESS]=ASSERTED;
        }
        if (!touchsense[DOWNPRESS]){ // if no DOWNPRESS, make next decrement legal
            prevstate[DOWNPRESS]=DEASSERTED;
        }
        if (!touchsense[UPPRESS]){ // if no UPPRESS, make next increment legal
            prevstate[UPPRESS]=DEASSERTED;
        } else {}

        if(lcdchange){
            DB3_TURN_OFF(); // pends on mutex key
            WaveAmplSet(&amplitude);
            DB3_TURN_ON();
            LcdDispDecWord(LCD_ROW_1, LCD_COL_12, TSI_LAYER, amplitude, SHOW_FIVE_DIGITS, MODE_LZ);
        }

    }
}

/*****************************************************************************************
* UpdateAmp() - Helper Function -
*  Changes the amplitude of the waveform if it won't exceed legal range, rejects changes
*  and notifies caller if not.
*
*  parameters:
*   delta = requested change to the amplitude (+1 or -1)
*   amplitude = pointer to value delta is applied to
*
*  returns:
*   success = boolean flag indicating whether amplitude was changed
*
* Trevor Schwarz, 2/27/2020
*****************************************************************************************/
static INT8U UpdateAmp(INT8U delta, INT8U *amplitude){
    INT8U success;
    success=TRUE;
    if(delta==AMPL_INCREMENT && *amplitude<MAX_AMPL){
        *amplitude+=1u;
    } else if (delta==AMPL_DECREMENT && *amplitude>MIN_AMPL){
        *amplitude-=1u;
    } else {
        success=FALSE;
    }
    return success;
}


/*****************************************************************************************
* NumInput() - Helper Function -
*  Checks if ASCII from KeyPend is a valid keypad number entry and if so returns the
*  decimal equivalent.  Otherwise, it returns 10.
*
*  parameter:
*   keyin = ASCII encoding
*
*  return:
*   translation = INT8U decimal equivalent
*
* Trevor Schwarz, 1/30/2020
*****************************************************************************************/
static INT8U NumInput(INT8C keyin){
    INT8U translation;
    switch(keyin){
        case '0':
            translation=0U;
            break;
        case '1':
            translation=1U;
            break;
        case '2':
            translation=2U;
            break;
        case '3':
            translation=3U;
            break;
        case '4':
            translation=4U;
            break;
        case '5':
            translation=5U;
            break;
        case '6':
            translation=6U;
            break;
        case '7':
            translation=7U;
            break;
        case '8':
            translation=8U;
            break;
        case '9':
            translation=9U;
            break;
        default:
            translation=10U;
            break;
    }
    return translation;
}

/*****************************************************************************************
* ApplyInput() - Helper Function -
*  Updates the local frequency based on the frequency and value entered.  Callee needs to
*  ensure values below 10Hz are not passed to the waveform generator. If the new frequency
*  would increase the value of the entry above 10kHz, the change is rejected and the callee
*  is notified.
*
*  parameters:
*   freq = pointer to current value
*   val = value to insert
*   backspace = flag to remove units place
*
*  returns:
*   valid = boolean flag indicating whether val was accepted and changed the frequency
*
* Trevor Schwarz, 2/27/2020
*****************************************************************************************/
static INT8U ApplyInput(INT16U *freq, INT8U val, INT8U backspace){
    INT8U valid;
    INT16U save;
    valid=TRUE; // set to FALSE if no changes are made (invalid input)
    save=0u; // frequency value placeholder

    if(backspace==BACKSPACE){
        *freq=*freq/10u; // integer division discards units place
    } else {
        save=*freq; // prevent values above 10kHz
        *freq=*freq*10u+val;
        if(*freq>MAX_FREQ){
            *freq=save; // restore valid value
            valid=FALSE;
        }
    }

    return valid;
}

