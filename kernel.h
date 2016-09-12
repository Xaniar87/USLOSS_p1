/* Patrick's DEBUG printing constant... */
#define DEBUG 0

/* ------------------------- Added Constants ----------------------------------- */
#define EMPTY 0
#define READY 1
#define JOIN_BLOCKED 2
#define QUITTED 3
#define ZAP_BLOCKED 4
#define RUNNING 5
#define SELF_BLOCK 11
#define SLICE_LENGTH 80000
typedef struct procStruct procStruct;

typedef struct procStruct * procPtr;

struct procStruct {
   procPtr         nextProcPtr;
   procPtr         childProcPtr;
   procPtr         nextSiblingPtr;
   procPtr		   quitHead;        // added attribute
   procPtr 		   quitNext;        // added attribute
   procPtr         parentPtr;       // added attribute
   procPtr         whoZappedMeHead; // added attribute
   procPtr		   whoZappedMeNext; // added attribute
   char            name[MAXNAME];     /* process's name */
   char            startArg[MAXARG];  /* args passed to process */
   USLOSS_Context  state;             /* current context for process */
   int             pid;               /* process id */
   int             priority;
   int (* startFunc) (char *);   /* function where process begins -- launch */
   char           *stack;
   unsigned int    stackSize;
   int             status;        /* READY, BLOCKED, QUIT, etc. */
   int			   quitStatus;      // added attribute
   int             amIZapped;       // added attribute
   int             timeStart;       // added attribute When process began running
   int             timeRun;         //How long process has run for
 

   /* other fields as needed... */
};

struct psrBits {
    unsigned int curMode:1;
    unsigned int curIntEnable:1;
    unsigned int prevMode:1;
    unsigned int prevIntEnable:1;
    unsigned int unused:28;
};

union psrValues {
   struct psrBits bits;
   unsigned int integerPart;
};

/* Some useful constants.  Add more as needed... */
#define NO_CURRENT_PROCESS NULL
#define MINPRIORITY 5
#define MAXPRIORITY 1
#define SENTINELPID 1
#define SENTINELPRIORITY (MINPRIORITY + 1)

