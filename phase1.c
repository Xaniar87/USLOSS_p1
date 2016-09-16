/* ------------------------------------------------------------------------
   phase1.c
   University of Arizona
   Computer Science 452
   Fall 2016
 
   Jason Elish
   Yikai Cao
   ------------------------------------------------------------------------ */


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
int zap(int pidToZap);
void releaseZapBlocks();
void setZapped(int pidToZap);
int amIZapped(void);
void dumpProcess(procPtr aProcPtr);
void dumpProcessHeader();
char* statusString(int status);
int kidsCount(procPtr aProcPtr);
void enableInterrupts();
void timeSlice(void);
void disableInterrupts();
int blockMe(int newStatus);
int readCurStartTime(void);
int unblockProc(int pid);
void cleanProcess(procPtr processToClean);
void cleanZombies(procPtr zombieHead);
int readtime(void);


/* -------------------------- Globals ------------------------------------- */

// Patrick's debugging global variable...
int debugflag = 0;

// the process table
procStruct ProcTable[MAXPROC];

// Process lists
static procPtr ReadyList;

// current process ID
procPtr Current;

//For making sure we don't reuse same PID
int prevPid = -1;

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
	//Check if in Kernel mode, halt if not
    if(!inKernel())
    {
    	USLOSS_Console("fork1(): called while in user mode, by process %d. Halting...\n", Current->pid);
        USLOSS_Halt(1);
    }
    
    //Turn off interrupts during fork creation process
    disableInterrupts();
    
	//If startFunc or name variable is NULL
    if(startFunc == NULL || name == NULL)
    {
    	return -1;
	}
    
    int procSlot = -1;
    int forkCounter = 0;

    if (DEBUG && debugflag)
        USLOSS_Console("fork1(): creating process %s\n", name);

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
    if( (usableSlot(pidCounter)) && (pidCounter == prevPid)) 
    {
    	pidCounter++;
	}

	// find an empty slot in the process table
    while(!usableSlot(pidCounter) && forkCounter < MAXPROC)
    {
        pidCounter++;
        forkCounter++;
    }
    
    //No slots available
    if(forkCounter == MAXPROC)
        return -1;

	//Set available process slot to use
    procSlot = pidCounter % MAXPROC;
    //For above check to ensure same pid not used twice
	prevPid = pidCounter;
    
  

    /* fill-in entry in process table */
    // fill-in name, startFunc, startArg and state
    if ( strlen(name) >= (MAXNAME - 1) )
	{
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
    ProcTable[procSlot].quitHead        = NULL; //List for tracking quit children
    ProcTable[procSlot].quitNext        = NULL; //List for tracking quit children
    ProcTable[procSlot].whoZappedMeHead = NULL; //List for tracking who zapped a process for reactivating during quit
    ProcTable[procSlot].whoZappedMeNext = NULL; //List for tracking who zapped a process for reactivating during quit
    
    // fill in others
    ProcTable[procSlot].pid             = pidCounter;
    ProcTable[procSlot].priority        = priority;
    ProcTable[procSlot].stack           = malloc(stacksize);
    ProcTable[procSlot].stackSize       = stacksize;
    ProcTable[procSlot].status          = READY;
    ProcTable[procSlot].amIZapped		= 0;
    ProcTable[procSlot].timeRun         = 0;
    ProcTable[procSlot].timeStart       = 0;
    ProcTable[procSlot].quitStatus		= 0;
    
    // fill in parentPid
    //If no current process, no parent
    if (Current == NULL)
        ProcTable[procSlot].parentPtr = NULL;
    else
    {
        //Else parent is Current process
        ProcTable[procSlot].parentPtr = Current;
        
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

    return ProcTable[procSlot].pid;
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
    
	//Turn on interrupts
    enableInterrupts();
    
    if (DEBUG && debugflag)
        USLOSS_Console("launch(): started\n");

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
   
   	//Disable interrupts
    disableInterrupts();
   
   
    //If process currently has no children, should not be trying to join. Return -2
    if(Current->childProcPtr == NULL)
    {
        //Enable interupts since returning out of join function
        enableInterrupts();
        return -2;
    }

	//If no child has quit yet, block and take off ready list
	if(Current->quitHead == NULL)
	{
		Current->status = JOIN_BLOCKED;
		removeRL(Current);
		dispatcher();
     
        //Dispatcher may have reenabled interupts, need to disable them again while we process rest of join
        disableInterrupts();
        		
		*status = Current->quitHead->quitStatus;
        //Save temp ID since will be moving past current quit process
        int temp_pid = Current->quitHead->pid;
        //Remove child from parents children list
        removeChild(Current->quitHead);
        
        Current->quitHead->status = EMPTY;
        
        strcpy(Current->quitHead->name, " " );
        
		//quitHead = next element on quitlist
        Current->quitHead = Current->quitHead->quitNext;
        
		//Reenable Interrupts
        enableInterrupts();
		
		//Condition where join process was zapped while waiting for child to quit
        if(isZapped())
		{
        	return -1;
        }   
        //Return pid of child and status
        return (temp_pid);
        
    }
    // if child process already quitted report quit status
	else
	{
		*status = Current->quitHead->quitStatus;
        //Store pid into temp spot because about to take it off the quitList
        int temp_pid = Current->quitHead->pid;
        
        //Change status of quitprocess to Empty so slot can be filled on process table
        Current->quitHead->status = EMPTY;	
        strcpy(Current->quitHead->name, " " );
		removeChild(Current->quitHead);

        //Move the head of the list to the next Process
        Current->quitHead = Current->quitHead->quitNext;  
		
        //Turn interrupts back on
        enableInterrupts();
        
        if(isZapped())
		{
	   		return -1;
   		}
        
        //Return pid of process that quit (front of quitList)
        return (temp_pid);
        
    }
}


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

    //Disable interrupts  
    disableInterrupts();
    
	// check if all children have quitted
	if(Current->childProcPtr != NULL)
	{
        procPtr tempPtr = Current->childProcPtr;
        while(tempPtr != NULL)
        {
        	//If a child has not quit, exit with error
            if(tempPtr->status != QUITTED)
            {
                USLOSS_Console("quit(): process %d, '%s', has active children. Halting...\n", Current->pid, Current->name);
                USLOSS_Halt(1);
            }
        
            tempPtr = tempPtr->nextSiblingPtr;
        }
        
        //Else if we are here Current process has quit children that haven't reported yet, but parent wants to quit. So clean out "zombies"
        cleanZombies(Current->childProcPtr);
    }
    
    // check if isZapped, if so release processes that are in Zap_Block
	if(isZapped())
	{
		releaseZapBlocks();
	}
	
	//Change status to successfully quit 
	Current->status = QUITTED;
    
	//If child has parent
	if(Current->parentPtr != NULL)
	{
		// if has a parent, let the parent know child quits and set its status to ready
		if(Current->parentPtr->status == JOIN_BLOCKED)
		{
			Current->parentPtr->status = READY;
			insertRL(Current->parentPtr);
		}
		
        // If quitList is currently empty, make Current quit process the head
        if(Current->parentPtr->quitHead == NULL) 
		{
            Current->parentPtr->quitHead = Current;
            Current->quitNext = NULL;
        }
        // quitList not empty
        else 
		{
        	procPtr tempPtr = Current->parentPtr->quitHead;
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
	
	//Update quit processes quit status for reporting back
	Current->quitStatus = status;
	//Take quit process off of readylist
	removeRL(Current);
	p1_quit(Current->pid);
    dispatcher();
  
} /* quit */

//Function that runs through quit children of a parent that are not joined when parent wants to quit.
void cleanZombies(procPtr zombieHead)
{
	procPtr tempZombie = zombieHead;
	
	while(tempZombie != NULL)
	{
		tempZombie->status = EMPTY;
		tempZombie = tempZombie->nextSiblingPtr;
	
	}
	
}

/* ------------------------- zap ----------------------------------- */
int zap(int pidToZap)
{
    //Check if in Kernel mode, halt if not
    if(!inKernel())
    {
        USLOSS_Console("zap(): called while in user mode, by process %d. Halting...\n", Current->pid);
        USLOSS_Halt(1);
    }
    
    //Want to consider Zap atomic up until dispatcher call. After that could be interupted
    disableInterrupts();
    //Translate to pointer for easier code understanding
    procPtr procToZap = &(ProcTable[ (pidToZap % MAXPROC) ]);
	
    // check if zapping itself
	if(Current->pid == pidToZap)
	{
		USLOSS_Console("zap(): process %d tried to zap itself.  Halting...\n", Current->pid);
		USLOSS_Halt(1);
	}
	
	
    // check if zapping nonexistant process
	if(procToZap->status == EMPTY || procToZap->pid != pidToZap)
	{
		USLOSS_Console("zap(): process being zapped does not exist.  Halting...\n");
		USLOSS_Halt(1);
	} 
	//Check if process attempting to zap has already quit, if so want to return before we go into zap_block
	if(procToZap->status == QUITTED && !isZapped())
		return 0;
	else if (procToZap->status == QUITTED && isZapped())
		return -1;	
 
	//Child function to perform actual zapping of desired process
	setZapped(pidToZap);
	
	//Block current process
	Current->status = ZAP_BLOCKED;
	//Remove blocked process from ready list
	removeRL(Current);
	
	dispatcher();
	
    //If Process was zapped during its block
    if(isZapped())
    {
        return -1;
    }
	else
    {
        //Would only have reactivated this processes if zapped process quit, so don't need to check status. Can assume zapped process quit ok.
        return 0;
    }
}

/* ------------------------- releaseZapBlocks ----------------------------------- */
void releaseZapBlocks()
{
    //Sets processes that were blocked due to zapping Current back to Ready status and reinserts onto readyList
    
	//Set tempPtr to head of processes to release
    procPtr tempPtr = Current->whoZappedMeHead;
    
	while(tempPtr != NULL)
	{
        //Travel through all processes which zapped Current and set to Ready and reinsert
		if(tempPtr->status == ZAP_BLOCKED)
		{
			tempPtr->status = READY;
			insertRL(tempPtr);
		}
		tempPtr = tempPtr->whoZappedMeNext;
	}
}

/* ------------------------- isZapped ----------------------------------- */
int isZapped(void)
{
    //Check if in Kernel mode, halt if not
    if(!inKernel())
    {
        USLOSS_Console("isZapped(): called while in user mode, by process %d. Halting...\n", Current->pid);
        USLOSS_Halt(1);
    }
    
    return Current->amIZapped;
}

/* ------------------------- setZapped ----------------------------------- */
void setZapped(int pidToZap)
{
	//Set zappedPtr to process we want to Zap
	procPtr zappedPtr = &ProcTable[(pidToZap % MAXPROC)];
    
	//Set process zapped variable to indiciate zapped
	zappedPtr->amIZapped = 1;
	
    //Update Current processes list of who zapped it
	//If list NULL
    if(zappedPtr->whoZappedMeHead == NULL)
	{
		zappedPtr->whoZappedMeHead = Current;
		Current->whoZappedMeNext = NULL;
	}
    //Else list not null, insert at back. 
	else
	{
		procPtr tempPtr = zappedPtr->whoZappedMeHead;
		while(tempPtr->whoZappedMeNext != NULL)
		{
			tempPtr = tempPtr->whoZappedMeNext;
		}
		
		tempPtr->whoZappedMeNext = Current;
		Current->whoZappedMeNext = NULL;
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
    //Check if in Kernel mode, halt if not
    if(!inKernel())
    {
        USLOSS_Console("dispatcher(): called while in user mode, by process %d. Halting...\n", Current->pid);
        USLOSS_Halt(1);
    }
    
	//If not process currently running
	if(Current == NULL || Current->status == QUITTED || Current->status == EMPTY)
	{
        Current = ReadyList;
        Current->timeStart = USLOSS_Clock();
        Current->status = RUNNING;
        enableInterrupts();
        p1_switch(0, Current->pid);
        USLOSS_ContextSwitch(NULL, &(Current->state));
	}
	else //Else some process current running
    {
        if(Current->status == RUNNING)
		{
			Current->status = READY;
            removeRL(Current);
			insertRL(Current);
		}
        
        procPtr prevProc = Current;
        Current = ReadyList;
		//Set current status to Running
		Current->status = RUNNING;
		        
        //Set start time for process about to begin
        if(prevProc->pid != Current->pid)
        {
            //Record runtime of Current process before switching
            //If switching due to a timeSlice, timerun was switched to 0 in timeslice function
            prevProc->timeRun += (USLOSS_Clock() - prevProc->timeStart);
            Current->timeStart = USLOSS_Clock();
            p1_switch(prevProc->pid, Current->pid);
            enableInterrupts();
            USLOSS_ContextSwitch(&prevProc->state, &ReadyList->state);
        }
        
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


/* ------------------------- checkDeadlock ----------------------------------- */
/* check to determine if deadlock has occurred... */
static void checkDeadlock()
{
    // itereate to see if all other processes either quit or empty
    int i;
    //Because sentinal will always be Ready or Running start count at 1
    int numOfProcesses = 1;
    //Tripped if sentinal called and not all process table slots are quitted or empty
    int deadLockFlag = 0;
    //Go through table. If not quitted or empty or sentinal, add one to counter and set flag
    for (i = 0; i < MAXPROC; i++) {
        if (ProcTable[i].status != EMPTY && ProcTable[i].status != QUITTED && ProcTable[i].priority != 6) {
            numOfProcesses++;
            deadLockFlag = 1;
        }
	}	
	//If we did have deadlock, print message with number of processes stuck
	if(deadLockFlag == 1)
	{
		USLOSS_Console("checkDeadlock(): numProc = %d. Only Sentinel should be left. Halting...\n", numOfProcesses);
		USLOSS_Halt(1);
	}
        
    //Else successful completion
    USLOSS_Console("All processes completed.\n");
    USLOSS_Halt(0);
} /* checkDeadlock */


/* ------------------------- enableInterrupts ----------------------------------- */
void enableInterrupts()
{
	// turn the interrupts on if we are in kernel mode
    if( (USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()) == 0 ) {
        //not in kernel mode
        USLOSS_Console("Kernel Error: Not in kernel mode, may not ");
        USLOSS_Console("disable interrupts\n");
        USLOSS_Halt(1);
    } else
    	//We ARE in kernel mode
    	USLOSS_PsrSet( USLOSS_PsrGet() | USLOSS_PSR_CURRENT_INT );	
}


/* ------------------------- disableInterrupts ----------------------------------- */
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
    timeSlice();
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
void insertRL(procPtr toBeAdded )
{
    //Function that adds a process to the readylist at the end of its priority
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
			//Else move to next process
			tempTrail = tempPtr;
			tempPtr = tempPtr->nextProcPtr;
			
		}

   }
    
}

/* ------------------------- getpid -----------------------------------  */
int getpid()
{
    //Check if in Kernel mode, halt if not
    if(!inKernel())
    {
        USLOSS_Console("getpid(): called while in user mode, by process %d. Halting...\n", Current->pid);
        USLOSS_Halt(1);
    }
	return Current->pid;	
}

/* ------------------------- RemoveRL ----------------------------------- */

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
		//Process should always be in readyList if we try to remove it, so don't need while condition.
        while(1)
		{
			if(toRemove->pid == tempPtr->pid)
			{
				tempTrail->nextProcPtr = tempPtr->nextProcPtr;
				break;
			}
			//Else move to next process
			tempTrail = tempPtr;
			tempPtr = tempPtr->nextProcPtr;
		}
	}
}

/* ------------------------- RemoveChild --------------------------------- */
void removeChild(procPtr child)
{
    //Function removes a child from a parents children list
    
	//If first element on child list is one to remove	
	if(child->parentPtr->childProcPtr->pid == child->pid)
	{
		child->parentPtr->childProcPtr = child->nextSiblingPtr;
	}
	else
	{
        //Search through list of children and remove particular child
		procPtr tempPtr = child->parentPtr->childProcPtr;
		procPtr tempTrail = child->parentPtr->childProcPtr;
		while(tempPtr->pid != child->pid)
		{
			tempTrail = tempPtr;
			tempPtr = tempPtr->nextSiblingPtr;
		}
		
		tempTrail->nextSiblingPtr = tempPtr->nextSiblingPtr;
	}
	
}

/* ------------------------- %%%%%%%%%%%%%%%%%%%%%% -------------------------- */
/* ------------------------PHASE 2 REQURIED FUNCTIONS ------------------------ */

/* ------------------------- blockMe ----------------------------------- */
int blockMe(int newStatus)
{
    //Check if in Kernel mode, halt if not
    if(!inKernel())
    {
        USLOSS_Console("blockMe(): called while in user mode, by process %d. Halting...\n", Current->pid);
        USLOSS_Halt(1);
    }
    
    //Set status to status code passed in (Currently have status 11 for SELF_BLOCKED
    if(newStatus <= 10)
    {
        USLOSS_Console("ERROR: status given for blockMe function must be greater than 10. Exiting...");
        USLOSS_Halt(1);
    }
    else
    {
        Current->status = newStatus;
    }

    //Remove from readyList and call dispatcher
    removeRL(Current);
    dispatcher();
    //When process is reactivated, check and see if it was zapped. If so, return -1
    if(Current->amIZapped)
    {
        return -1;
    }
    //Else return 0
    return 0;
    
}


/* ------------------------- unblockProc ----------------------------------- */
int unblockProc(int pid)
{
    /*This   operation   unblocks   process  pid  that   had   previously
     been   blocked   by   calling blockMe. The status of that process is 
     changed to READY, and it is put on the Ready List.
     The dispatcher will be called as a side-effect of this function.
     */
    
    //Check if in Kernel mode, halt if not
    if(!inKernel())
    {
        USLOSS_Console("unblockProc(): called while in user mode, by process %d. Halting...\n", Current->pid);
        USLOSS_Halt(1);
    }
    procPtr ProcToUnBlock = &ProcTable[pid % MAXPROC];
  
    //if the indicated process was not blocked, does not exist, is the current process, or is blocked on a status less than or equal to 10.
    if(ProcToUnBlock->status <= 10 || ProcToUnBlock->pid == Current->pid || ProcToUnBlock->pid != pid)
        return -2;
  
    ProcToUnBlock->status = READY;
    insertRL(ProcToUnBlock);
    dispatcher();
    
    //If process is zapped return -1, else return 0
    if(isZapped())
        return -1;
    else
        return 0;
}

/* ------------------------- timeSlice ----------------------------------- */
int readCurStartTime(void)
{
    //Check if in Kernel mode, halt if not
    if(!inKernel())
    {
        USLOSS_Console("readCurStartTime(): called while in user mode, by process %d. Halting...\n", Current->pid);
        USLOSS_Halt(1);
    }
    //Simple function to return time in microseconds that current processes started
    return Current->timeStart;
}


void timeSlice(void)
{
    //Check if in Kernel mode, halt if not
    if(!inKernel())
    {
        USLOSS_Console("timeSlice(): called while in user mode, by process %d. Halting...\n", Current->pid);
        USLOSS_Halt(1);
    }
    
    if( (USLOSS_Clock() - Current->timeStart) > SLICE_LENGTH)
    {
        //If we are calling dispatcher due to timeslice, indicate timeSliceFlag and call dispatcher
        Current->timeRun += USLOSS_Clock() - Current->timeStart;
        Current->timeStart = USLOSS_Clock();
        dispatcher();
    }
}


/* ------------------------- readtime ----------------------------------- */
int readtime(void)
{
    //Check if in Kernel mode, halt if not
    if(!inKernel())
    {
        USLOSS_Console("readtime(): called while in user mode, by process %d. Halting...\n", Current->pid);
        USLOSS_Halt(1);
    }
	//Divide result by 1000 to get result in milliseconds instead of microseconds
	return (  (Current->timeRun + (USLOSS_Clock() - Current->timeStart) ) / 1000);
}



/* ------------------------- dumpProcesses ----------------------------------- */
void dumpProcesses()
{
    //Check if in Kernel mode, halt if not
    if(!inKernel())
    {
        USLOSS_Console("dumpProcesses(): called while in user mode, by process %d. Halting...\n", Current->pid);
        USLOSS_Halt(1);
    }
    
    int i;
    
    dumpProcessHeader();
    
    for(i = 0; i < MAXPROC; i++)
    {
        dumpProcess(&ProcTable[i]);
    }
}


/* ------------------------- dumpProcess ----------------------------------- */
void dumpProcess(procPtr aProcPtr)
{
	
    int pid = aProcPtr->pid;
    //If Process entry is EMPTY, pid = -1
    if(aProcPtr->status == EMPTY)
    	pid = -1;
    
    //If there ends up being no parent, status -2
	int parentPid = -2;
    
	if (aProcPtr->parentPtr != NULL && aProcPtr->status != EMPTY)
        parentPid = aProcPtr->parentPtr->pid;
        
    int priority = -1;
  
    //If Proctable slot is empty, use -1 for priority, otherwise use its priority.
	if(aProcPtr->status != EMPTY)
    	priority = aProcPtr->priority;
    
	char *status = statusString(aProcPtr->status);
    int kids = kidsCount(aProcPtr);
    char *name = aProcPtr->name;
    int timeRun = -1;
  
    //If Proctstatus not empty and time run is greater than 0, get time run, else use -1
	if(aProcPtr->status != EMPTY && aProcPtr->timeRun > 0)
    {
    	  	timeRun = (aProcPtr->timeRun);
    }
    //Case if status within known status values
	if(aProcPtr->status < 10)
    {
	    USLOSS_Console(" %-5d  %-5d   %-10d %-15s %-10d %-7d %-10s\n", pid, parentPid, priority, status, kids, timeRun, name);
	}
	//Case if status outside known status values
	else
	{
		USLOSS_Console(" %-5d  %-5d   %-10d %-15d %-10d %-7d %-10s\n", pid, parentPid, priority, aProcPtr->status, kids, timeRun, name);
	}
		
}

/* ------------------------- dumpProcessHeader ----------------------------------- */
void dumpProcessHeader()
{
    USLOSS_Console("PID	Parent	Priority   Status          # Kids     CPUtime   Name\n");
}

/* ------------------------- statusString ----------------------------------- */
//Return name of status if one we have on our list
char* statusString(int status)
{
    switch(status)
    {
        case 0: return "EMPTY"; break;
        case 1: return "READY"; break;
        case 2: return "JOIN_BLOCK"; break;
        case 3: return "QUIT"; break;
        case 4: return "ZAP_BLOCK";
        case 5: return "RUNNING";
        default: return "UNKNOWN";
    }
}

/* ------------------------- kidsCount ----------------------------------- */
//Used for DumpProcess to see how many children process has
int kidsCount(procPtr aProcPtr)
{
    int kidsCounter = 0;
    procPtr tmpKid = aProcPtr->childProcPtr;
    
    while(tmpKid != NULL)
    {
        kidsCounter++;
        tmpKid = tmpKid->nextSiblingPtr;
    }
    
    return kidsCounter;
}


