/******************************************************************

SPECIFICATIONS TO BE IMPLEMENTED:
Implement an application that receives input through argv[], including the name of a file F and a string indicating a numerical value N greater than or equal to 1.
Once launched, the application should create the File F and activate N threads. Additionally, the application should also start a child process in which N additional threads are activated. The two resulting processes will be conveniently identified as A (the parent) and B (the child) in the following description.
Each thread of process A will read strings from standard input. Each string read should be communicated to the corresponding thread in process B via shared memory, and it will write it on a new line in file F. For simplicity, it is assumed that each string does not exceed a size of 4KB.
The application should handle the SIGINT signal (or CTRL_C_EVENT in the case of WinAPI) so that when process A is hit by this signal, it should send the same signal to process B. If process B is hit, it should print the current contents of file F to standard output.
If there is no input provided, the application should use no more than 5% of the CPU's processing capacity.

SPECIFICATION TO BE IMPLEMENTED: 
Implementare un'applicazione che riceva in input tramite argv[] il 
nome di un file F ed una stringa indicante un valore numerico N maggiore
o uguale ad 1.
L'applicazione, una volta lanciata dovra' creare il file F ed attivare 
N thread. Inoltre, l'applicazione dovra' anche attivare un processo 
figlio, in cui vengano attivati altri N thread. 
I due processi che risulteranno attivi verranno per comodita' identificati
come A (il padre) e B (il figlio) nella successiva descrizione.

Ciascun thread del processo A leggera' stringhe da standard input. 
Ogni stringa letta dovra' essere comunicata al corrispettivo thread 
del processo B tramite memoria condivisa, e questo la scrivera' su una 
nuova linea del file F. Per semplicita' si assuma che ogni stringa non
ecceda la taglia di 4KB. 

L'applicazione dovra' gestire il segnale SIGINT (o CTRL_C_EVENT nel caso
WinAPI) in modo tale che quando il processo A venga colpito esso dovra' 
inviare la stessa segnalazione verso il processo B. Se invece ad essere 
colpito e' il processo B, questo dovra' riversare su standard output il 
contenuto corrente del file F.

Qalora non vi sia immissione di input, l'applicazione dovra' utilizzare 
non piu' del 5% della capacita' di lavoro della CPU.

*****************************************************************/
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <sys/sem.h>
#include <semaphore.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SHMEMSIZE 4096

char *fileName;
FILE *file;
int numeroThreads; // Number of threads
int sdA, sdB; // Semaphore IDs
char **memory_segments; // An array of shared memory segments
pid_t pid; // Process ID

// Child process signal handler
void child_handler(int dummy)
{
    file = fopen(fileName, "r");

    char *buffer;
    while (fscanf(file, "%ms", &buffer) != EOF)
    {
        printf("%s\n", buffer);
        free(buffer);
    }
}

// Thread function for reading and writing to shared memory
void *functB(void *i)
{
    int index = (int)i;
    int ret;
    struct sembuf semaforo;

    semaforo.sem_num = index;
    semaforo.sem_flg = 0;

    while (1)
    {
        semaforo.sem_op = -1;
        ret = semop(sdB, &semaforo, 1);

        fprintf(file, "%s\n", memory_segments[index]);
        fflush(file);

        semaforo.sem_op = 1;
        ret = semop(sdA, &semaforo, 1);
    }
}

// Thread function for reading from stdin and writing to shared memory
void *functA(void *i)
{
    int index = (int)i;
    int ret;
    struct sembuf semaforo;

    semaforo.sem_num = index;
    semaforo.sem_flg = 0;

    while (1)
    {
        semaforo.sem_op = -1;
        ret = semop(sdA, &semaforo, 1);
        printf("Writer thread number %d is accessing memory...\n", index);

        scanf("%s", memory_segments[index]); // Placing the string in shared memory

        semaforo.sem_op = 1;
        ret = semop(sdB, &semaforo, 1);
    }
}

int main(int argc, char **argv)
{
    fileName = *argv[1];
    numeroThreads = atoi(argv[2]); // Convert the second command-line argument to an integer
    pthread_t tid; // Thread ID
    int ret;

    memory_segments = malloc(sizeof(char*) * numeroThreads);

    if (memory_segments == NULL) {
        printf("memory allocation error (1)\n");
        return -1;
    }

    for (int i = 0; i < numeroThreads; i++) {
        // Create shared memory segments
        memory_segments[i] = (char *)mmap(NULL, SHMEMSIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, 0, 0);
        if (memory_segments[i] == NULL) {
            printf("mmap error\n");
            return -1;
        }
    }

    file = fopen(fileName, "w+");

    // Create and initialize semaphores for reading and writing
    sdA = semget(IPC_PRIVATE, numeroThreads, IPC_CREAT | 0660);
    for (int i = 0; i < numeroThreads; i++) {
        ret = semctl(sdA, i, SETVAL, 1);
        if (ret == -1) {
            printf("semctl error\n");
            exit(-1);
        }
    }

    sdB = semget(IPC_PRIVATE, numeroThreads, IPC_CREAT | 0660);
    for (int i = 0; i < numeroThreads; i++) {
        ret = semctl(sdB, i, SETVAL, 0);
        if (ret == -1) {
            printf("semctl error\n");
            exit(-1);
        }
    }

    if ((pid = fork()) == -1) {
        printf("fork error\n");
        return -1;
    }

    if (pid == 0) {
        // Child process

        signal(SIGINT, child_handler);

        for (int i = 0; i < numeroThreads; i++) {
            if (pthread_create(&tid, NULL, functB, (void *)i) == -1) {
                printf("pthread_create error (child)\n");
                return -1;
            }
        }
    } else {
        // Parent process

        signal(SIGINT, child_handler);

        for (int i = 0; i < numeroThreads; i++) {
            if (pthread_create(&tid, NULL, functA, (void *)i) == -1) {
                printf("pthread_create error (parent)\n");
                return -1;
            }
        }
    }

    while (1)
        pause(); // Main process pauses indefinitely
}

