/* ------------------------------------------------------------------------
   phase1.c
   University of Arizona
   Computer Science 452
   Fall 2015
   ------------------------------------------------------------------------ */

////TODO////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Address the Zap case where the process gets Zapped while its in Zap (Should return -1)


#include "phase1.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "kernel.h"

/* ------------------------- Prototypes ----------------------------------- */
int sentinel (char *);
extern int start1 (char *);
void dispatcher(void);
void launch();
static void checkDeadlock();

/* ------------------------- Added Functions ----------------------------------- */
void clock_handler(int type, void *todo);
void alarm_handler(int type, void *todo);
void disk_handler(int type, void *todo);
void term_handler(int type, void *todo);
void syscall_handler(int type, void *todo);
void dumpProcesses();
int usableSlot(int slot);
int inKernel();
void insertRL(procPtr slot);
void removeRL(procPtr slot);
void removeChild(procPtr child);
void printReadyList();
int prev_pid_num = -1;
int zap(int pidToZap);
void releaseZapBlocks();
void setZapped(int pidToZap);
int amIZapped(void);

/* -------------------------- Globals ------------------------------------- */

// Patrick's debugging global variable...
int debugflag = 0;

// the process table
procStruct ProcTable[MAXPROC];

// Process lists
static procPtr ReadyList;

// current process ID
procPtr Current;

// the next pid to be assigned
unsigned int pidCounter = SENTINELPID;


/* -------------------------- Functions ----------------------------------- */
/* ------------------------------------------------------------------------
   Name - startup
   Purpose - Initializes process lists and clock interrupt vector.
             Start up sentinel process and the test process.
   Parameters - none, called by USLOSS
   Returns - nothing
   Side Effects - lots, starts the whole thing
   ----------------------------------------------------------------------- */
void startup()
{
    int result; // value returned by call to fork1()
    int startupCounter;

    // initialize the process table
    if (DEBUG && debugflag)
        USLOSS_Console("startup(): initializing process table, ProcTable[]\n");
    for (startupCounter = 0; startupCounter < MAXPROC; startupCounter++)
    {
        ProcTable[startupCounter].status = EMPTY;
    }

    // Initialize the Ready list, etc.
    if (DEBUG && debugflag)
        USLOSS_Console("startup(): initializing the Ready list\n");
    ReadyList = NULL;

    // Initialize the clock interrupt handler
    USLOSS_IntVec[USLOSS_CLOCK_INT]     = clock_handler; //clock_handler;
    USLOSS_IntVec[USLOSS_CLOCK_DEV]     = clock_handler; //clock_handler;
    USLOSS_IntVec[USLOSS_ALARM_INT]     = alarm_handler; //alarm_handler;
    USLOSS_IntVec[USLOSS_ALARM_DEV]     = alarm_handler; //alarm_handler;
    USLOSS_IntVec[USLOSS_TERM_INT]      = term_handler; //term_handler;
    USLOSS_IntVec[USLOSS_TERM_DEV]      = term_handler; //term_handler;
    USLOSS_IntVec[USLOSS_SYSCALL_INT]   = syscall_handler; //syscall_handler;
    USLOSS_IntVec[USLOSS_DISK_INT]      = disk_handler; //disk_handler;
    USLOSS_IntVec[USLOSS_DISK_DEV]      = disk_handler; //disk_handler;

    // startup a sentinel process
    if (DEBUG && debugflag)
        USLOSS_Console("startup(): calling fork1() for sentinel\n");
    result = fork1("sentinel", sentinel, NULL, USLOSS_MIN_STACK,
                    SENTINELPRIORITY);
    if (result < 0) {
        if (DEBUG && debugflag) {
            USLOSS_Console("startup(): fork1 of sentinel returned error, ");
            USLOSS_Console("halting...\n");
        }
        USLOSS_Halt(1);
    }
  
    // start the test process
    if (DEBUG && debugflag)
        USLOSS_Console("startup(): calling fork1() for start1\n");
    result = fork1("start1", start1, NULL, 2 * USLOSS_MIN_STACK, 1);
    if (result < 0) {
        USLOSS_Console("startup(): fork1 for start1 returned an error, ");
        USLOSS_Console("halting...\n");
        USLOSS_Halt(1);
    }

    USLOSS_Console("startup(): Should not see this message! ");
    USLOSS_Console("Returned from fork1 call that created start1\n");

    return;
} /* startup */

/* ------------------------------------------------------------------------
   Name - finish
   Purpose - Required by USLOSS
   Parameters - none
   Returns - nothing
   Side Effects - none
   ----------------------------------------------------------------------- */
void finish()
{
    if (DEBUG && debugflag)
        USLOSS_Console("in finish...\n");
} /* finish */

/* ------------------------------------------------------------------------
   Name - fork1
   Purpose - Gets a new process from the process table and initializes
             information of the process.  Updates information in the
             parent process to reflect this child process creation.
   Parameters - the process procedure address, the size of the stack and
                the priority to be assigned to the child process.
   Returns - the process id of the created child or -1 if no child could
             be created or if priority is not between max and min priority.
             return -2 if stack size is too small
   Side Effects - ReadyList is changed, ProcTable is changed, Current
                  process information changed
   ------------------------------------------------------------------------ */
int fork1(char *name, int (*startFunc)(char *), char *arg,
          int stacksize, int priority)
{
	if(!inKernel())
    {
    	USLOSS_Console("fork1(): called while in user mode, by process %d. Halting...\n", Current->pid);
        USLOSS_Halt(1);
    }
    
    int procSlot = -1;
    int forkCounter = 0;

    if (DEBUG && debugflag)
        USLOSS_Console("fork1(): creating process %s\n", name);

    // test if in kernel mode; halt if in user mode
    

    // Return -2 if stack size is too small
    if (stacksize < USLOSS_MIN_STACK)
        return -2;

    // Return -1 if priority is not in bound
    // special case when forking sentinel
    if (ReadyList == NULL)
    {
        if (priority != SENTINELPRIORITY)
            return -1;
    }
    else if (priority < MAXPRIORITY || priority > MINPRIORITY)
        return -1;
    
    //Special case where pid assignment loop could try and assign same pid consecutivly. Avoids situation    
    if( (usableSlot(pidCounter)) && (pidCounter == prev_pid_num)) 
    {
    	pidCounter++;
	}

	// find an empty slot in the process table
    while(!usableSlot(pidCounter) && forkCounter < 50)
    {
    
        pidCounter++;
        forkCounter++;
    }

    procSlot = pidCounter % MAXPROC;
    //For above check to ensure same pid not used twice
	prev_pid_num = pidCounter;
    
  

    /* fill-in entry in process table */
    // fill-in name, startFunc, startArg and state
    if ( strlen(name) >= (MAXNAME - 1) ) {
        USLOSS_Console("fork1(): Process name is too long.  Halting...\n");
        USLOSS_Halt(1);
    }
    strcpy(ProcTable[procSlot].name, name);
    ProcTable[procSlot].startFunc = startFunc;
    if ( arg == NULL )
        ProcTable[procSlot].startArg[0] = '\0';
    else if ( strlen(arg) >= (MAXARG - 1) ) {
        USLOSS_Console("fork1(): argument too long.  Halting...\n");
        USLOSS_Halt(1);
    }
    else
        strcpy(ProcTable[procSlot].startArg, arg);
    
    // fill in related ptr
    ProcTable[procSlot].nextProcPtr     = NULL; // ReadyList, need to be updated later in fork1
    ProcTable[procSlot].childProcPtr    = NULL; // ChildrenList
    ProcTable[procSlot].nextSiblingPtr  = NULL; // ChildrenList, need to be updated later in fork1
    ProcTable[procSlot].quitHead        = NULL;
    ProcTable[procSlot].quitNext        = NULL;
    ProcTable[procSlot].whoZappedMeHead = NULL;
    ProcTable[procSlot].whoZappedMeNext = NULL;
    // fill in others
    ProcTable[procSlot].pid             = pidCounter;
    ProcTable[procSlot].priority        = priority;
    ProcTable[procSlot].stack           = malloc(stacksize);
    ProcTable[procSlot].stackSize       = stacksize;
    ProcTable[procSlot].status          = READY;
    ProcTable[procSlot].amIZapped		= 0;
    // fill in parentPid
    if (Current == NULL)
        ProcTable[procSlot].myParent = NULL;
    else
    {
        ProcTable[procSlot].myParent = Current;
        
		// update parent's childrenList
        if(Current->childProcPtr == NULL)
        {
        	//If child has no parents
        	Current->childProcPtr = &ProcTable[procSlot];
        }
		else
		{
			//Else add new child to front of list
			ProcTable[procSlot].nextSiblingPtr = Current->childProcPtr;
			Current->childProcPtr = &ProcTable[procSlot];
		}
	}
    
    	// Insert new process into readyList
	    insertRL(&ProcTable[procSlot]);
    
    /* end of filling in entry in process table */

    // Initialize context for this process, but use launch function pointer for
    // the initial value of the process's program counter (PC)
    USLOSS_ContextInit(&(ProcTable[procSlot].state), USLOSS_PsrGet(),
                       ProcTable[procSlot].stack,
                       ProcTable[procSlot].stackSize,
                       launch);
                       
  

    // for future phase(s)
    p1_fork(ProcTable[procSlot].pid);

	// call dispatcher if not sentinel
    if (ProcTable[procSlot].priority != SENTINELPRIORITY) 
        dispatcher();

    // More stuff to do here...

    return ProcTable[procSlot].pid;  // -1 is not correct! Here to prevent warning.
} /* fork1 */

/* ------------------------------------------------------------------------
   Name - launch
   Purpose - Dummy function to enable interrupts and launch a given process
             upon startup.
   Parameters - none
   Returns - nothing
   Side Effects - enable interrupts
   ------------------------------------------------------------------------ */
void launch()
{
    int result;

    if (DEBUG && debugflag)
        USLOSS_Console("launch(): started\n");

    // Enable interrupts
	USLOSS_PsrSet( (USLOSS_PSR_CURRENT_INT | USLOSS_PsrGet()));
    // Call the function passed to fork1, and capture its return value
    result = Current->startFunc(Current->startArg);

    if (DEBUG && debugflag)
        USLOSS_Console("Process %d returned to launch\n", Current->pid);

    quit(result);

} /* launch */


/* ------------------------------------------------------------------------
   Name - join
   Purpose - Wait for a child process (if one has been forked) to quit.  If 
             one has already quit, don't wait.
   Parameters - a pointer to an int where the termination code of the 
                quitting process is to be stored.
   Returns - the process id of the quitting child joined on.
             -1 if the process was zapped in the join
             -2 if the process has no children
   Side Effects - If no child process has quit before join is called, the 
                  parent is removed from the ready list and blocked.
   ------------------------------------------------------------------------ */
int join(int *status)
{
	//Check for kernel mode
	if(!inKernel())
    {
    	USLOSS_Console("join: called while in user mode, by process %d. Halting...\n", Current->pid);
        USLOSS_Halt(1);
    }
	
	//If no child has quit yet, block and take off ready list
	if(Current->quitHead == NULL)
	{
		Current->status = JOIN_BLOCKED;
		removeRL(Current);
		dispatcher();
		
		*status = Current->quitHead->quitStatus;
        //Save temp ID since will be moving past current quit process
        int temp_pid = Current->quitHead->pid;
        //Change quit processes Status to empty so ProcSlot can be given to new process
        Current->quitHead->status = EMPTY;
        removeChild(Current->quitHead);
        //quitHead = next element on quitlist
        Current->quitHead = Current->quitHead->quitNext;
        //Remove child from parents children list
        return (temp_pid);
        
        
    }
	else
	{
		*status = Current->quitHead->quitStatus;
        //Store pid into temp spot because about to take it off the quitList
        int temp_pid = Current->quitHead->pid;
        //Change status of quitprocess to Empty so slot can be filled on process table
        Current->quitHead->status = EMPTY;
        removeChild(Current->quitHead);
        //Move the head of the list to the next Process
        Current->quitHead = Current->quitHead->quitNext;
        //Return pid of process that quit (front of quitList)
        return (temp_pid);
        
    }

	
}
	
//   return -1;  // -1 is not correct! Here to prevent warning.
 /* join */


/* ------------------------------------------------------------------------
   Name - quit
   Purpose - Stops the child process and notifies the parent of the death by
             putting child quit info on the parents child completion code
             list.
   Parameters - the code to return to the grieving parent
   Returns - nothing
   Side Effects - changes the parent of pid child completion status list.
   ------------------------------------------------------------------------ */
void quit(int status)
{
	if(!inKernel())
    {
    	USLOSS_Console("quit(): called while in user mode, by process %d. Halting...\n", Current->pid);
        USLOSS_Halt(1);
    }
	//If process tries to quit before all children have quit
	if(Current->childProcPtr != NULL)
	{
	  	USLOSS_Console("quit(): process %d, '%s', has active children. Halting...\n", Current->pid, Current->name);
	    USLOSS_Halt(1);
	}
	
	if(isZapped())
	{
		releaseZapBlocks();
	}
	 
	Current->status = QUITTED;
	//If child has parent
	if(Current->myParent != NULL)
	{
		// if has a parent, let the parent know child quits and set its status to ready
		if(Current->myParent->status == JOIN_BLOCKED)
		{
			Current->myParent->status = READY;
			insertRL(Current->myParent);
		}
		
                
        // If quitList is currently empty, make Current quit process the head
        if(Current->myParent->quitHead == NULL) {
            Current->myParent->quitHead = Current;
            Current->quitNext = NULL;
        }
        else {
        	//Set temp pointer to the front of parent quit list
        	procPtr tempPtr = Current->myParent->quitHead;
        	//Travel the list until we come to the last existing quitProcess
        	while(tempPtr->quitNext != NULL)
        	{
        		tempPtr = tempPtr->quitNext;
			}
			//Once we find it, add Current process to end of list
			tempPtr->quitNext = Current;
			Current->quitNext = NULL;
		
        }
	}
	
	Current->quitStatus = status;
	//Take quit process off of readylist
	removeRL(Current);
    dispatcher();
    p1_quit(Current->pid);
} /* quit */

int zap(int pidToZap)
{
	
	if(Current->pid == pidToZap)
	{
		USLOSS_Console("Error, Process cannot Zap itself...exiting...");
		USLOSS_Halt(1);
	}
	
	if(ProcTable[ (pidToZap % 50) ].status == EMPTY)
	{
		USLOSS_Console("Error, attempt to Zap nonexistant process....exiting...");
		USLOSS_Halt(1);
	} 
		
	if(isZapped())
	{
		return -1;
	}
	
	if( ProcTable[(pidToZap % MAXPROC)].status == QUITTED )
		return 0;
	
	setZapped(pidToZap);
	Current->status = BLOCKED;
	removeRL(Current);
	dispatcher();
	
	if( ProcTable[(pidToZap % MAXPROC)].status == QUITTED )
		return 0;
///////////////!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!//////////////////////!!!!!!!!!!!!!!/ CHECK THIS	
	
	 /////Should not get here...would signify error////////////!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	 return -2;
}

void releaseZapBlocks()
{
	procPtr tempPtr = Current->whoZappedMeHead;
	while(tempPtr != NULL)
	{
		if(tempPtr->status == BLOCKED)
		{
			tempPtr->status = READY;
			insertRL(tempPtr);
		}
		tempPtr = tempPtr->whoZappedMeNext;
	}
}

int isZapped(void)
{
	return Current->amIZapped;
}

void setZapped(int pidToZap)
{
	procPtr zappedPtr = &ProcTable[(pidToZap % MAXPROC)];
	//Set process zapped variable to indiciate zapped
	zappedPtr->amIZapped = 1;
	
	if(zappedPtr->whoZappedMeHead == NULL)
	{
		zappedPtr->whoZappedMeHead = Current;
		Current->whoZappedMeNext = NULL;
	}
	else
	{
		Current->whoZappedMeNext = zappedPtr->whoZappedMeHead;
		zappedPtr->whoZappedMeHead = Current;
	}
}

/* ------------------------------------------------------------------------
   Name - dispatcher
   Purpose - dispatches ready processes.  The process with the highest
             priority (the first on the ready list) is scheduled to
             run.  The old process is swapped out and the new process
             swapped in.
   Parameters - none
   Returns - nothing
   Side Effects - the context of the machine is changed
   ----------------------------------------------------------------------- */
void dispatcher(void)
{
	
	if(Current == NULL)
	{
		Current = ReadyList;
		USLOSS_ContextSwitch(NULL, &(Current->state));
	}
	else
	{
		procPtr prevProc = Current;
		Current = ReadyList;
		USLOSS_ContextSwitch(&prevProc->state, &ReadyList->state);

	}
   
} /* dispatcher */


/* ------------------------------------------------------------------------
   Name - sentinel
   Purpose - The purpose of the sentinel routine is two-fold.  One
             responsibility is to keep the system going when all other
             processes are blocked.  The other is to detect and report
             simple deadlock states.
   Parameters - none
   Returns - nothing
   Side Effects -  if system is in deadlock, print appropriate error
                   and halt.
   ----------------------------------------------------------------------- */
int sentinel (char *dummy)
{
    if (DEBUG && debugflag)
        USLOSS_Console("sentinel(): called\n");
    while (1)
    {
        checkDeadlock();
        USLOSS_WaitInt();
    }
} /* sentinel */


/* check to determine if deadlock has occurred... */
static void checkDeadlock()
{
	  // itereate to see if all other processes either quit or empty
    int i;
    int numOfProcesses = 1;
    int deadLockFlag = 0;
    for (i = 0; i < MAXPROC; i++) {
        if (ProcTable[i].status != EMPTY && ProcTable[i].status != QUITTED && ProcTable[i].priority != 6) {
            numOfProcesses++;
            deadLockFlag = 1;
        }
	}	
	
	if(deadLockFlag == 1)
	{
		USLOSS_Console("checkDeadlock(): numProc = %d. Only Sentinel should be left. Halting...\n", numOfProcesses);
		USLOSS_Halt(1);
	}
        
    
    
    USLOSS_Console("All processes completed.\n");
    USLOSS_Halt(0);
} /* checkDeadlock */


/*
 * Disables the interrupts.
 */
void disableInterrupts()
{
    // turn the interrupts OFF iff we are in kernel mode
    if( (USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()) == 0 ) {
        //not in kernel mode
        USLOSS_Console("Kernel Error: Not in kernel mode, may not ");
        USLOSS_Console("disable interrupts\n");
        USLOSS_Halt(1);
    } else
        // We ARE in kernel mode
        USLOSS_PsrSet( USLOSS_PsrGet() & ~USLOSS_PSR_CURRENT_INT );
} /* disableInterrupts */

/* ------------------------- clock_handler ----------------------------------- */
void clock_handler(int type, void *todo)
{
    
}

/* ------------------------- alarm_handler ----------------------------------- */
void alarm_handler(int type, void *todo) {
    
}

/* ------------------------- term_handler ----------------------------------- */
void term_handler(int type, void *todo) {
    
}

/* ------------------------- syscall_handler ----------------------------------- */
void syscall_handler(int type, void *todo) {
    
}

/* ------------------------- disk_handler ----------------------------------- */
void disk_handler(int type, void *todo) {
    
}

/* ------------------------- dumpProcesses ----------------------------------- */
void dumpProcesses()
{
	int i;
	printf("Process ID     Name     Status     Priority\n");
	for(i = 0; i < MAXPROC; i++)
	{
		printf("%d      %10s        %d     %d\n", ProcTable[i].pid, ProcTable[i].name, ProcTable[i].status, ProcTable[i].priority);
	}
}

void printReadyList()
{
	procPtr temp = ReadyList; 
	printf("ID     Name       Status     Priority\n");
	while(temp->nextProcPtr != NULL)
	{
		printf("%d     %s     %d     %d\n", temp->pid, temp->name, temp->status, temp->priority);
		temp = temp->nextProcPtr;
	}
}

/* ------------------------- inKernel ----------------------------------- */
int inKernel()
{
    // 0 if in kernel mode, !0 if in user mode
    return ( (USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()));
}

/* ------------------------- usableSlot ----------------------------------- 
 Return: 1 if usable, 0 if not usable
 ------------------------------------------------------------------------ */
int usableSlot(int slot)
{
    if (ProcTable[slot % MAXPROC].status == EMPTY)
        return 1;
       
    return 0;
}

/* ------------------------- insertRL -----------------------------------
 ------------------------------------------------------------------------ */

//Function that adds a process to the readylist at the end of its priority
void insertRL(procPtr toBeAdded )
{
    procPtr newReady = toBeAdded;
    
    // if RL is empty
    if (ReadyList == NULL)
    {
        ReadyList = newReady;
        newReady->nextProcPtr = NULL;
    }
    //Needs to inserted at front of list
    else if(newReady->priority < ReadyList->priority)
    {
    	newReady->nextProcPtr = ReadyList;
    	ReadyList = newReady;
    }
    // RL is not empty and not inserting at front
    else
    {
    	procPtr tempPtr = ReadyList;
    	procPtr tempTrail = ReadyList;
    	//With sentinal we know that we will always find a slot of lesser priority
    	while(1)
		{
			if(newReady->priority < tempPtr->priority)
			{
				newReady->nextProcPtr = tempPtr;
				tempTrail->nextProcPtr = newReady;
				break;
			}
			
			tempTrail = tempPtr;
			tempPtr = tempPtr->nextProcPtr;
		}

   }
    
}

/* ------------------------- RemoveRL -----------------------------------
 ------------------------------------------------------------------------ */
int getpid()
{
	return Current->pid;	
}
/* ------------------------- RemoveRL -----------------------------------
 ------------------------------------------------------------------------ */

//Function to remove process from ReadyList
void removeRL(procPtr slot)
{
	procPtr toRemove = slot;
	
	//Not possible for ReadyList to be Null.
	//Check if process to remove is at front of readyList
	if(toRemove->pid == ReadyList->pid)
	{
		ReadyList = ReadyList->nextProcPtr;
	}
	//else find it and remove it
	else
	{
		procPtr tempPtr = ReadyList;
		procPtr tempTrail = ReadyList;
		while(1)
		{
			if(toRemove->pid == tempPtr->pid)
			{
				tempTrail->nextProcPtr = tempPtr->nextProcPtr;
				break;
			}
			tempTrail = tempPtr;
			tempPtr = tempPtr->nextProcPtr;
		}
	}
}

/* ------------------------- RemoveChild -----------------------------------
 ------------------------------------------------------------------------ */
//Function removes a child from a parents children list
void removeChild(procPtr child)
{
	//If first element on child list is one to remove	
	if(child->myParent->childProcPtr->pid == child->pid)
	{
		child->myParent->childProcPtr = child->nextSiblingPtr;
	}
	else
	{
		procPtr tempPtr = child->myParent->childProcPtr;
		procPtr tempTrail = child->myParent->childProcPtr;
		while(tempPtr->pid != child->pid)
		{
			tempTrail = tempPtr;
			tempPtr = tempPtr->nextSiblingPtr;
		}
		tempTrail->nextSiblingPtr = tempPtr->nextSiblingPtr;
	}
	
}

		

