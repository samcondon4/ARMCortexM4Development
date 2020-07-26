/********************************************************************
 * Lab5Main.h - header file for Lab5Main module.
 * 			    defines public variables to be used
 * 			    between modules
 *
 * Sam Condon, 11/26/2019
 *********************************************************************/

#ifndef LAB5MAIN_H_
#define LAB5MAIN_H_

/*****************************************************
 * Define public global variables:
 *
 * 	-L5mAlarmFlags: alarm state indicator
 *  -L5mSysState: current system state
 *  -L5mPrevSysState: previous system state
 *********************************************************/
typedef enum{NONE, TOUCH, TEMP} ALARM_FLAGS_T;
extern ALARM_FLAGS_T L5mAlarmFlags;

typedef enum{ARMED, DISARMED, ALARM} SYS_STATE_T;
extern SYS_STATE_T L5mSysState;
extern SYS_STATE_T L5mPrevSysState;

#endif /* LAB5MAIN_H_ */
