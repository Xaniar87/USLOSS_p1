/* Patrick's DEBUG printing constant... */
#define DEBUG 0

/* ------------------------- Added Constants ----------------------------------- */
#define EMPTY 0
#define READY 1
#define JOIN_BLOCKED 2
#define QUITTED 3
#define BLOCKED 4
typedef struct procStruct procStruct;

typedef struct procStruct * procPtr;

struct procStruct {
   procPtr         nextProcPtr;
   procPtr         childProcPtr;
   procPtr         nextSiblingPtr;
   procPtr		   quitHead;
   procPtr 		   quitNext;
   procPtr         myParent;       // added attribute
   procPtr         whoZappedMeHead;
   procPtr		   whoZappedMeNext;
   char            name[MAXNAME];     /* process's name */
   char            startArg[MAXARG];  /* args passed to process */
   USLOSS_Context  state;             /* current context for process */
   int             pid;               /* process id */
   int             priority;
   int (* startFunc) (char *);   /* function where process begins -- launch */
   char           *stack;
   unsigned int    stackSize;
   int             status;        /* READY, BLOCKED, QUIT, etc. */
   int			   quitStatus;
   int             amIZapped;
 

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

