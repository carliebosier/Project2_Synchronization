#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>

// Shared memory structure
typedef struct {
    int BankAccount;
    // Add any other shared variables here
} SharedData;

// Global variables for cleanup
int ShmID;
SharedData *ShmPTR;
sem_t *mutex;
pid_t *child_pids;
int num_children;
int num_parents;

// Cleanup function
void cleanup() {
    if (ShmPTR != (void *) -1) {
        shmdt((void *) ShmPTR);
    }
    if (ShmID >= 0) {
        shmctl(ShmID, IPC_RMID, NULL);
    }
    if (mutex != SEM_FAILED) {
        sem_close(mutex);
        sem_unlink("/bankaccount_mutex");
    }
    if (child_pids != NULL) {
        free(child_pids);
    }
    exit(0);
}

// Signal handler for cleanup
void signal_handler(int sig) {
    cleanup();
}

// Dear Old Dad process
void dad_process(SharedData *shared) {
    int localBalance;
    int amount;
    
    while (1) {
        // Sleep random 0-5 seconds
        sleep(rand() % 6);
        
        printf("Dear Old Dad: Attempting to Check Balance\n");
        
        // Generate random number
        int random_num = rand();
        
        if (random_num % 2 == 0) {
            // Even: Check if need to deposit
            sem_wait(mutex);
            localBalance = shared->BankAccount;
            
            if (localBalance < 100) {
                // Try to deposit
                amount = 100 + (rand() % 101); // 100-200
                
                if (amount % 2 == 0) {
                    // Even amount: deposit
                    localBalance += amount;
                    shared->BankAccount = localBalance;
                    sem_post(mutex);
                    printf("Dear old Dad: Deposits $%d / Balance = $%d\n", amount, localBalance);
                } else {
                    // Odd amount: doesn't have money
                    sem_post(mutex);
                    printf("Dear old Dad: Doesn't have any money to give\n");
                }
            } else {
                sem_post(mutex);
                printf("Dear old Dad: Thinks Student has enough Cash ($%d)\n", localBalance);
            }
        } else {
            // Odd: Just check balance
            sem_wait(mutex);
            localBalance = shared->BankAccount;
            sem_post(mutex);
            printf("Dear Old Dad: Last Checking Balance = $%d\n", localBalance);
        }
    }
}

// Lovable Mom process
void mom_process(SharedData *shared) {
    int localBalance;
    int amount;
    
    while (1) {
        // Sleep random 0-10 seconds
        sleep(rand() % 11);
        
        printf("Lovable Mom: Attempting to Check Balance\n");
        
        sem_wait(mutex);
        localBalance = shared->BankAccount;
        
        if (localBalance <= 100) {
            // Always deposit if balance <= 100
            // Generate amount between 100-125 (assuming "between 125" means up to 125, so 100-125)
            amount = 100 + (rand() % 26); // 100-125
            localBalance += amount;
            shared->BankAccount = localBalance;
            sem_post(mutex);
            printf("Lovable Mom: Deposits $%d / Balance = $%d\n", amount, localBalance);
        } else {
            sem_post(mutex);
        }
    }
}

// Poor Student process
void student_process(SharedData *shared, int student_id) {
    int localBalance;
    int need;
    
    while (1) {
        // Sleep random 0-5 seconds
        sleep(rand() % 6);
        
        if (num_children > 1) {
            printf("Poor Student(%d): Attempting to Check Balance\n", student_id);
        } else {
            printf("Poor Student: Attempting to Check Balance\n");
        }
        
        // Generate random number
        int random_num = rand();
        
        if (random_num % 2 == 0) {
            // Even: Attempt to withdraw
            sem_wait(mutex);
            localBalance = shared->BankAccount;
            
            need = 50 + (rand() % 51); // 50-100
            
            if (num_children > 1) {
                printf("Poor Student(%d) needs $%d\n", student_id, need);
            } else {
                printf("Poor Student needs $%d\n", need);
            }
            
            if (need <= localBalance) {
                // Withdraw
                localBalance -= need;
                shared->BankAccount = localBalance;
                sem_post(mutex);
                if (num_children > 1) {
                    printf("Poor Student(%d): Withdraws $%d / Balance = $%d\n", student_id, need, localBalance);
                } else {
                    printf("Poor Student: Withdraws $%d / Balance = $%d\n", need, localBalance);
                }
            } else {
                // Not enough cash
                sem_post(mutex);
                if (num_children > 1) {
                    printf("Poor Student(%d): Not Enough Cash ($%d)\n", student_id, localBalance);
                } else {
                    printf("Poor Student: Not Enough Cash ($%d)\n", localBalance);
                }
            }
        } else {
            // Odd: Just check balance
            sem_wait(mutex);
            localBalance = shared->BankAccount;
            sem_post(mutex);
            if (num_children > 1) {
                printf("Poor Student(%d): Last Checking Balance = $%d\n", student_id, localBalance);
            } else {
                printf("Poor Student: Last Checking Balance = $%d\n", localBalance);
            }
        }
    }
}

int main(int argc, char *argv[]) {
    pid_t pid;
    int status;
    int i;
    
    // Parse command line arguments
    num_parents = 1; // Default: just Dad
    num_children = 1; // Default: one student
    
    if (argc >= 2) {
        num_parents = atoi(argv[1]);
        if (num_parents < 1) num_parents = 1;
        if (num_parents > 2) num_parents = 2; // Max 2 parents (Dad and Mom)
    }
    
    if (argc >= 3) {
        num_children = atoi(argv[2]);
        if (num_children < 1) num_children = 1;
    }
    
    // Initialize random seed
    srand(time(NULL) + getpid());
    
    // Create shared memory segment
    ShmID = shmget(IPC_PRIVATE, sizeof(SharedData), IPC_CREAT | 0666);
    if (ShmID < 0) {
        perror("shmget error");
        exit(1);
    }
    
    // Attach shared memory segment
    ShmPTR = (SharedData *) shmat(ShmID, NULL, 0);
    if (ShmPTR == (void *) -1) {
        perror("shmat error");
        exit(1);
    }
    
    // Initialize shared variables
    ShmPTR->BankAccount = 0;
    
    // Create named semaphore for mutual exclusion
    // Unlink first in case it exists from a previous run
    sem_unlink("/bankaccount_mutex");
    mutex = sem_open("/bankaccount_mutex", O_CREAT, 0644, 1);
    if (mutex == SEM_FAILED) {
        perror("sem_open error");
        cleanup();
        exit(1);
    }
    
    // Set up signal handlers for cleanup
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Allocate array for child process IDs
    child_pids = (pid_t *) malloc((num_children + num_parents - 1) * sizeof(pid_t));
    if (child_pids == NULL) {
        perror("malloc error");
        cleanup();
        exit(1);
    }
    
    // Create child processes for students
    for (i = 0; i < num_children; i++) {
        pid = fork();
        if (pid < 0) {
            perror("fork error");
            cleanup();
            exit(1);
        } else if (pid == 0) {
            // Child process - Student
            student_process(ShmPTR, i + 1);
            exit(0);
        } else {
            child_pids[i] = pid;
        }
    }
    
    // Create Mom process if requested
    if (num_parents == 2) {
        pid = fork();
        if (pid < 0) {
            perror("fork error");
            cleanup();
            exit(1);
        } else if (pid == 0) {
            // Mom process
            mom_process(ShmPTR);
            exit(0);
        } else {
            child_pids[num_children] = pid;
        }
    }
    
    // Parent process - Dear Old Dad
    dad_process(ShmPTR);
    
    // This should never be reached due to infinite loop
    // But if it is, wait for all children
    for (i = 0; i < num_children + (num_parents > 1 ? 1 : 0); i++) {
        wait(&status);
    }
    
    cleanup();
    return 0;
}
