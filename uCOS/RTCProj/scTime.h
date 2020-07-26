/*
 * scTime.h
 *
 *  Created on: Feb 1, 2020
 *      Author: condons3
 */

#ifndef SCTIME_H_
#define SCTIME_H_

typedef struct {
	INT8U hr;
	INT8U min;
	INT8U sec;
}TIME_T;


/*TimePend - Copies current timeOfDay to   *ltime when change is signaled*/
void TimePend(TIME_T *ltime, OS_ERR *p_err);

/*TimeGet - Copies current timeOfDay to *ltime */
void TimeGet(TIME_T  *ltime);

/*TimeSet - Copies *ltime to timeOfDay */
void TimeSet(TIME_T *ltime);

/*TimeInit - Initializes the RTC, semaphore, task and mutex key, etc. */
void TimeInit(void);

#endif /* SCTIME_H_ */
