#include "Scheduler.h"
#include "Led.h"
#include "TimeTracking.h"

//static void ExecuteTask (Taskp t)
//{
//  /* ----------------------- INSERT CODE HERE ----------------------- */
//
//  t->Invoked++;
//  t->Taskf(t->ExecutionTime); // execute task
//
//  /* ---------------------------------------------------------------- */
//
//}

extern minPeriod;
void Scheduler_P_FP(Task Tasks[]) {

    StartTracking(TT_SCHEDULER);

    uint16_t delta = minPeriod / 4;         // spare time for this slot, initialized as the minimum period

    int i;
    for (i = NUMTASKS - 1; i >= 0; i--) {  // loop for all the tasks in Tasks[i], ps: Lower indices -> lower priorities

        Taskp t = &Tasks[i];                // to extract task_i

        if (t->Activated != t->Invoked) {   // if there are task_i(s) pending

            if (delta >= t->Remaining) {    // if the current spare time is enough
                StopTracking(TT_SCHEDULER);
                t->Taskf(t->Remaining);     // to execute all the pending task_i(s)
                StartTracking(TT_SCHEDULER);
                t->Invoked = t->Activated;  // to update "Invoked" to show no more pending 
                delta -= t->Remaining;      // the spare time is consumed
                t->Remaining = 0;           // since no more pending, no remaining execution time
            }
            else {                          // if the current spare time is insufficient
                StopTracking(TT_SCHEDULER);
                t->Taskf(delta);            // try its best to execute as much time as possible for task_i(s)
                StartTracking(TT_SCHEDULER);
                t->Remaining -= delta;      // to update the remaining execution time
                //t->Invoked is not [necessary] to be updated here, as there are still remaining tasks in the queue
                delta = 0;                  // no more spare time, no more tasks can be executed in this slot
            }
        }
    }

    StopTracking(TT_SCHEDULER);
    PrintResults();
}
