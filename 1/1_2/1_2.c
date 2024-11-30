#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

/*Note: Value of LOCK is 0 and value of UNLOCK is 1.*/
#define LOCK 0
#define UNLOCK 1

volatile int a = 0;
volatile int lock = UNLOCK;
pthread_mutex_t mutex;
void spin_lock() {
    asm volatile(
        "loop:\n\t"
        "movl $0, %%eax\n\t"         // Set EAX to 0 (trying to lock)
        "xchgl %[lock], %%eax\n\t"   // Atomically exchange EAX with lock
        // "mfence\n\t"                 // Full fence to prevent memory reordering
        "cmpl $1, %%eax\n\t"         // Compare EAX with 1
        "jne loop\n\t"               // If EAX != 1, retry (stay in loop)
        :
        : [lock] "m" (lock)          // lock is treated as memory
        : "eax"
    );
}

void spin_unlock() {
    asm volatile(
        "movl $1, %%eax\n\t"         // Set EAX to 1 (unlock state)
        "xchgl %[lock], %%eax\n\t"   // Atomically exchange EAX with lock
        // "mfence\n\t"                 // Full fence to prevent memory reordering
        :
        : [lock] "m" (lock)          // lock is treated as memory
        : "eax"
    );
}

// void spin_lock() {
//     asm volatile(
//         "loop:\n\t"
//         "movl $0, %%eax\n\t"         // Set EAX to 0 (LOCK state)
//         "xchgl %%eax,%[lock]\n\t"   // Atomically exchange the value of 'lock' with EAX
//         "cmpl $1, %%eax\n\t"         // Compare EAX with 1        "jnz loop\n\t"               // If not, keep looping
//         :
//         : [lock] "r" (lock)          // Input: 'lock'
//         : "eax"         // Clobbers: EAX and memory
//     );
// }

// void spin_unlock() {
//     asm volatile(
//         "movl $1, %%eax\n\t"         // Set EAX to 1 (UNLOCK state)
//         "xchgl %%eax,%[lock]\n\t"   // Atomically exchange the value of 'lock' with EAX
//         :
//         : [lock] "r" (lock)          // Input: 'lock'
//         : "eax"      // Clobbers: EAX and memory
//     );
// }

void *thread(void *arg) {

    for(int i=0; i<10000; i++){
        spin_lock();
        a = a + 1;
        spin_unlock();
    }
    return NULL;
}

int main() {
    FILE *fptr;
    fptr = fopen("1.txt", "a");
    pthread_t t1, t2;

    pthread_mutex_init(&mutex, 0);
    pthread_create(&t1, NULL, thread, NULL);
    pthread_create(&t2, NULL, thread, NULL);
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    pthread_mutex_destroy(&mutex);

    fprintf(fptr, "%d ", a);
    fclose(fptr);
}

