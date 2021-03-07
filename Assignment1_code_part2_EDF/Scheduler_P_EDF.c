#include "Scheduler.h"
#include "Led.h"
#include "TimeTracking.h"

//static void ExecuteTask (Taskp t)
//{
//  /* insert code */
//	t->Taskf(t->ExecutionTime); // execute task
//  /* insert code */
//}

extern minPeriod;
void Scheduler_P_EDF (Task Tasks[])
{ 
  /* insert code */
  /* Use ExecuteTask function to execute a task */
	StartTracking(TT_SCHEDULER);

	uint16_t delta = minPeriod/4;			// spare time for this slot, initialized as the minimum period

	uint8_t i, j, minD;						// loop iterators, prepared for the current minimum ddl
	Taskp t;								// prefared for the current minimum ddl task
	uint16_t temp;							// temple storage initialized as maximum ddl

	for (j = 0; j < NUMTASKS; j++) {		// loop for all tasks
	
		/* To select the task owning the cloest ddl*/
		temp = 0xffff;
		for (i = 0; i < NUMTASKS; i++) {
			if ((&Tasks[i])->Deadline < temp) {
				temp = (&Tasks[i])->Deadline;	// current minimum ddl
				minD = i;						// current position where the task has the minimum ddl
			}
		}
		t = &Tasks[minD];						// the selected task
		/*----------------------------------------*/

		if (t->Activated != t->Invoked) {
			if (delta >= t->Remaining) {			// if the current spare time is enough
				StopTracking(TT_SCHEDULER);
				t->Taskf(t->Remaining);				// to execute all the pending task_i(s)
				StartTracking(TT_SCHEDULER);
				t->Invoked = t->Activated;			// to update "Invoked" to show no more pending 
				for (i = 0; i < NUMTASKS; i++) {	// to update all tasks deadline
					(&Tasks[i])->Deadline -= t->Remaining;
				}
				t->Deadline = 0xffff;				// to update the finished task's ddl to infinite,
													// so it will not be selected again in this hyperperiod
				delta -= t->Remaining;				// the spare time is consumed
				t->Remaining = 0;					// since no more pending, no remaining execution time
			}
			else {
				StopTracking(TT_SCHEDULER);
				t->Taskf(delta);					// try its best to execute as much time as possible for task_i(s)
				StartTracking(TT_SCHEDULER);
				t->Remaining -= delta;				// to update the remaining execution time
				for (i = 0; i < NUMTASKS; i++) {	// to update all tasks deadline
					(&Tasks[i])->Deadline -= delta; 
				}
				delta = 0;							// no more spare time, no more tasks can be executed in this slot
			}
		}
	}

  /* insert code */
	StopTracking(TT_SCHEDULER);
	PrintResults();
}
