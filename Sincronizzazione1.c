/******************************************************************
Welcome to the Operating System examination

You are editing the '/home/esame/prog.c' file. You cannot remove 
this file, just edit it so as to produce your own program according to
the specification listed below.

In the '/home/esame/'directory you can find a Makefile that you can 
use to compile this program to generate an executable named 'prog' 
in the same directory. Typing 'make posix' you will compile for 
Posix, while typing 'make winapi' you will compile for WinAPI just 
depending on the specific technology you selected to implement the
given specification. Most of the required header files (for either 
Posix or WinAPI compilation) are already included in the head of the
prog.c file you are editing. 

At the end of the examination, the last saved snapshot of this file
will be automatically stored by the system and will be then considered
for the evaluation of your exam. Modifications made to prog.c which are
not saved by you via the editor will not appear in the stored version
of the prog.c file. 
In other words, unsaved changes will not be tracked, so please save 
this file when you think you have finished software development.
You can also modify the Makefile if requesed, since this file will also
be automatically stored together with your program and will be part
of the final data to be evaluated for your exam.

PLEASE BE CAREFUL THAT THE LAST SAVED VERSION OF THE prog.c FILE (and of
the Makfile) WILL BE AUTOMATICALLY STORED WHEN YOU CLOSE YOUR EXAMINATION 
VIA THE CLOSURE CODE YOU RECEIVED, OR WHEN THE TIME YOU HAVE BEEN GRANTED
TO DEVELOP YOUR PROGRAM EXPIRES. 


SPECIFICATION TO BE IMPLEMENTED:
Implementare una programma che riceva in input, tramite argv[], il nomi
di N file (con N maggiore o uguale a 1).
Per ogni nome di file F_i ricevuto input dovra' essere attivato un nuovo thread T_i.
Il main thread dovra' leggere indefinitamente stringhe dallo standard-input 
e dovra' rendere ogni stringa letta disponibile ad uno solo degli altri N thread
secondo uno schema circolare.
Ciascun thread T_i a sua volta, per ogni stringa letta dal main thread e resa a lui disponibile, 
dovra' scriverla su una nuova linea del file F_i. 

L'applicazione dovra' gestire il segnale SIGINT (o CTRL_C_EVENT nel caso
WinAPI) in modo tale che quando il processo venga colpito esso dovra' 
riversare su standard-output e su un apposito file chiamato "output-file" il 
contenuto di tutti i file F_i gestiti dall'applicazione 
ricostruendo esattamente la stessa sequenza di stringhe (ciascuna riportata su 
una linea diversa) che era stata immessa tramite lo standard-input.

In caso non vi sia immissione di dati sullo standard-input, l'applicazione dovra' utilizzare 
non piu' del 5% della capacita' di lavoro della CPU.

*****************************************************************/
#ifdef Posix_compile
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/mman.h>
#include <sys/sem.h>
#include <semaphore.h>
#include <fcntl.h>
#else
#include <windows.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Various defines
#define MAX_INPUT_SIZE 1024

// Global variables
int *inputFileArray;
char **inputStringArray;
int noFiles;

// Global structures
typedef struct _threadArg {
    int threadNo;
    int fileDescr;
} threadArg;

typedef threadArg *threadArgPtr;

// Global semaphores
typedef int semaphore;

semaphore semGoAhead;
semaphore semIsReady;

// Global mutexes
semaphore criticalSem;

// THREAD ROUTINE
void *threadRoutine(void *arg) {
    threadArgPtr myArg = (threadArgPtr)arg;
    int threadNo = myArg->threadNo;
    int fileDescr = myArg->fileDescr;
    free(myArg);

#ifdef DEBUG
    printf("Thread no. %d opens file descriptor %d\n", threadNo, fileDescr);
    fflush(stdout);
#endif

    // Thread signal handling
    sigset_t mySigset;
    sigfillset(&mySigset);
    if (sigprocmask(SIG_BLOCK, &mySigset, NULL) == -1) {
        perror("ERROR in signal handling setup");
        exit(EXIT_FAILURE);
    }

    // Perpetual cycle
    while (1) {
        // We check whether data is ready for our thread

        struct sembuf sops;
        sops.sem_num = threadNo;
        sops.sem_op = -1;

        if (semop(semIsReady, &sops, 1) == -1) {
            perror("ERROR in semaphore operation");
            exit(EXIT_FAILURE);
        }

        // We can now work on the critical area and write the string onto the file

        // String is copied onto our file
        if (dprintf(fileDescr, "%s", inputStringArray[threadNo]) < 0) {
            perror("ERROR in flushing string onto file");
            exit(EXIT_FAILURE);
        }

#ifdef DEBUG
        printf("String %s printed correctly onto file by thread no. %d\n", inputStringArray[threadNo], threadNo);
#endif

        // Telling the main thread our string has been consumed
        sops.sem_num = threadNo;
        sops.sem_op = 1;

        if (semop(semGoAhead, &sops, 1) == -1) {
            perror("ERROR in semaphore operation");
            exit(EXIT_FAILURE);
        }
    }

    pause();
    return NULL;
}

void signalRoutine(int sigNo) {
    // All files' indices are set to 0
    for (size_t index = 0; index < noFiles; index++) {
        lseek(inputFileArray[index], 0, SEEK_SET);
    }

    // A file stream is initialized for each file descriptor
    FILE **fileStreams = calloc(noFiles, sizeof(FILE *));
    if (fileStreams == NULL) {
        perror("ERROR");
        exit(EXIT_FAILURE);
    }

    for (size_t index = 0; index < noFiles; index++) {
        if ((fileStreams[index] = fdopen(inputFileArray[index], "r+")) == NULL) {
            perror("ERROR");
            exit(EXIT_FAILURE);
        }
    }

    // We open the output file
    int outputFileDescr;
    if ((outputFileDescr = open("output-file", O_TRUNC | O_CREAT | O_WRONLY, 0666)) == -1) {
        perror("ERROR in output file initialization/opening");
        exit(EXIT_FAILURE);
    }

    char newString[MAX_INPUT_SIZE];
    int noFails = 0;

    while (1) {
        for (size_t index = 0; index < noFiles; index++) {
            errno = 0;

            // We read a new line from the file
            if (fgets(newString, MAX_INPUT_SIZE, fileStreams[index]) == NULL) {
                if (errno != 0) {
                    perror("ERROR");
                    exit(EXIT_FAILURE);
                }

                // If the file has ended, we take note of that via this counter
                noFails++;
                continue;
            }

            // New string is written onto the desired file
            dprintf(outputFileDescr, "%s", newString);
        }

        // If all files have ended, the printing process can be ended
        if (noFails == noFiles) {
            break;
        }
        noFails = 0;
    }

    // Output file can now be safely closed
    close(outputFileDescr);

    // We can also free this dynamically allocated data structure
    free(fileStreams);

    // We flush the file onto stdout
    puts("--- OVERALL OUTPUT ---");
    system("cat output-file");
    puts("--- OUTPUT PRINTED ---");
    fflush(stdout);

    return;
}

int main(int argc, char **argv) {
    // Checking if necessary args are specified
    if (argc <= 1) {
        fprintf(stderr, "ERROR: Usage %s fileName1 [...] fileNameN\n", (argc == 1) ? argv[0] : "progName");
        exit(EXIT_FAILURE);
    }

    // Dynamically allocating a vector of pointers to strings
    // featuring all passed file names
    inputFileArray = calloc(argc - 1, sizeof(int));
    if (inputFileArray == NULL) {
        perror("ERROR");
        exit(EXIT_FAILURE);
    }

    // Setting up a string array
    inputStringArray = calloc(argc - 1, sizeof(*inputStringArray));
    if (inputStringArray == NULL) {
        perror("ERROR");
        exit(EXIT_FAILURE);
    }

    noFiles = argc - 1;

    // Initializing all global semaphores
    if ((semGoAhead = semget(IPC_PRIVATE, argc - 1, 0660)) == -1) {
    semaphoreError:
        perror("ERROR in semaphore initialization");
        exit(EXIT_FAILURE);
    }

    if ((semIsReady = semget(IPC_PRIVATE, argc - 1, 0660)) == -1) {
        goto semaphoreError;
    }

    // SIGNAL HANDLING FOR THE MAIN THREAD
    signal(SIGINT, &signalRoutine);

    // The setup of all global semaphores is carried out individually
    // throughout the for cycle for further convenience, despite a negligible computational overhead

    // Starting one thread for each of the files provided
    pthread_t newThread;
    for (size_t index = 1; index < argc; index++) {
        // Semaphores are initialized first
        if (semctl(semGoAhead, index - 1, SETVAL, 1) == -1) {
            perror("ERROR in semaphore setup");
            exit(EXIT_FAILURE);
        }

        if (semctl(semIsReady, index - 1, SETVAL, 0) == -1) {
            perror("ERROR in semaphore setup");
            exit(EXIT_FAILURE);
        }

        // Setting up passed data structure
        threadArgPtr newArg = calloc(1, sizeof(newArg));
        if (newArg == NULL) {
            perror("ERROR");
            exit(EXIT_FAILURE);
        }

        // Opening the corresponding file (it is created if it does not
        // yet exist, otherwise it is simply overwritten, i.e., truncated)
        newArg->threadNo = (int)(index - 1);

        if ((newArg->fileDescr = open(argv[index], O_CREAT | O_TRUNC | O_RDWR, 0660)) == -1) {
            perror("ERROR in file opening");
            exit(EXIT_FAILURE);
        }

        // Placing the dynamically allocated string into
        // the global file name vector
        inputFileArray[index - 1] = newArg->fileDescr;

        // Effectively creating the relevant new thread
        if (pthread_create(&newThread, NULL, &threadRoutine, newArg) != 0) {
            perror("ERROR in thread initialization");
            exit(EXIT_FAILURE);
        }
    }

    // Data structures for the perpetual cycle
    char inputBuffer[MAX_INPUT_SIZE];
    inputBuffer[MAX_INPUT_SIZE - 1] = '\0'; // Totally unnecessary

    struct sembuf sops;

    size_t cycleIndex = 0;

    // Perpetual cycle
    while (1) {
#ifdef DEBUG
        printf("Cycle no. %d\n", (int)cycleIndex);
#endif

        // Let us read a string first from stdin
        if (fgets(inputBuffer, MAX_INPUT_SIZE - 1, stdin) == NULL) {
            perror("ERROR in fetching input");
            exit(EXIT_FAILURE);
        }

        // Checks if a potential(!) overflow occurred
        if (strlen(inputBuffer) >= MAX_INPUT_SIZE - 1) {
            fprintf(stderr, "The string you provided is too large! Please provide a shorter one.\n");
            continue;
        }

        // Checking if the main thread can go ahead in writing
        // into the i-th thread's critical area (i.e., whether it has
        // already been consumed (written onto a file) or not)

        sops.sem_num = (int)cycleIndex;
        sops.sem_op = -1;

    tryAgainOp1:
        if (semop(semGoAhead, &sops, 1) == -1) {
            if (errno == EINTR) {
                goto tryAgainOp1;
            }
            perror("ERROR in semaphore operation");
            exit(EXIT_FAILURE);
        }

        // The critical area (inputStringArray) is now accessible to us

        // Currently stored string is freed
        if (inputStringArray[cycleIndex] != NULL)
            free(inputStringArray[cycleIndex]);

        // The new one is dynamically allocated
        if ((inputStringArray[cycleIndex] = strdup(inputBuffer)) == NULL) {
            perror("ERROR");
            exit(EXIT_FAILURE);
        }

        // Telling the cycleIndex-th thread it may go ahead

        sops.sem_num = cycleIndex;
        sops.sem_op = 1;

    tryAgainOp2:
        if (semop(semIsReady, &sops, 1) == -1) {
            if (errno == EINTR) {
                goto tryAgainOp2;
            }
            perror("ERROR in semaphore operation");
            exit(EXIT_FAILURE);
        }

        // Thread counter is increased accordingly
        cycleIndex = (cycleIndex + 1) % (argc - 1);

        // The perpetual cycle may go ahead
    }

    pause();
    return 0;
}

