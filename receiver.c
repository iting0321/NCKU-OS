#include "receiver.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>

#define SHM_SIZE 1024
#define SEM_NAME "/posix_shm"
#define EXIT_MSG "EXIT"
#define SHM_NAME "/posix_shm_example"
#define SEM_EMPTY "/posix_sem_empty"
#define SEM_FULL "/posix_sem_full"

sem_t* sem_empty = NULL;
sem_t* sem_full = NULL;
const char* name = "OS";

void receive(message_t* message_ptr, mailbox_t* mailbox_ptr) {
    //struct timespec start, end;

    sem_wait(sem_full);

    if (mailbox_ptr->flag == 1) {
        // Message Passing (System V)
        printf("======================\n");
       
        if (msgrcv(mailbox_ptr->storage.msqid, message_ptr, sizeof(message_t) - sizeof(long), 0, 0) == -1) {
            perror("msgrcv failed");
            exit(EXIT_FAILURE);
        }
        printf("Receiving Message : %s\n",message_ptr->text);
        
    } else if (mailbox_ptr->flag == 2) {
        // Shared Memory
  
        // if (sem == SEM_FAILED) {
        //     perror("sem_open failed");
        //     exit(EXIT_FAILURE);
        // }
        // Open shared memory
    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open failed");
        exit(EXIT_FAILURE);
    }

    // Map shared memory to address space
    mailbox_ptr->storage.shm_addr = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (mailbox_ptr->storage.shm_addr == MAP_FAILED) {
        perror("mmap failed");
        exit(EXIT_FAILURE);
    }

    // Open semaphores
    // sem_init(&sem,1,2);

    

    // Wait until the shared memory is full

    // Measure the time spent reading from shared memory
    //clock_gettime(CLOCK_MONOTONIC, &start);
    strcpy(message_ptr->text, mailbox_ptr->storage.shm_addr);
    printf("=================\n");
    printf("Receiving Message : %s\n",message_ptr->text);
    //clock_gettime(CLOCK_MONOTONIC, &end);

    //*time_spent += (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;

    // Signal the sender that the shared memory is empty

    // Clean up
    munmap(mailbox_ptr->storage.shm_addr, SHM_SIZE);
    close(shm_fd);
    } 
    else {
        fprintf(stderr, "Invalid mailbox flag.\n");
        return;
    }

    sem_post(sem_empty);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <mechanism>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int mechanism = atoi(argv[1]);
    mailbox_t mailbox;
    message_t message;
    struct timespec start, end;
    double time_spent = 0.0;

    sem_empty = sem_open(SEM_EMPTY,O_CREAT, 0777, 0);
    sem_full = sem_open(SEM_FULL,O_CREAT,0777,0);
    

    // Initialize mailbox based on mechanism
    if (mechanism == 1) {
        mailbox.flag = 1;
        // mailbox.storage.msqid = msgget(IPC_PRIVATE, 0666 | IPC_CREAT);
        // if (mailbox.storage.msqid == -1) {
        //     perror("msgget failed");
        //     exit(EXIT_FAILURE);
        // }
    } 
    else if (mechanism == 2) {
        mailbox.flag = 2;
        do {
            printf("0000000000000\n");
            clock_gettime(CLOCK_MONOTONIC, &start);
            receive(&message, &mailbox);
            clock_gettime(CLOCK_MONOTONIC, &end);
            time_spent += (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
            
        } while (strcmp(message.text, EXIT_MSG) != 0);
    } 
    else {
        fprintf(stderr, "Invalid mechanism. Use 1 for Message Passing, 2 for Shared Memory.\n");
        exit(EXIT_FAILURE);
    }

    

    // Receive messages until exit message is received
    // do {
    //     printf("message : %s\n",message.text)''
    //     clock_gettime(CLOCK_MONOTONIC, &start);
    //     receive(&message, &mailbox);
    //     clock_gettime(CLOCK_MONOTONIC, &end);
    //     time_spent += (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
        
    // } while (strcmp(message.text, EXIT_MSG) != 0);
    

    sem_destroy(sem_empty);
    sem_destroy(sem_full);
    printf("Total time spent on communication: %.6f seconds\n", time_spent);

    //sem_close(sem_empty);
    //sem_close(sem_full);
    //sem_unlink(SEM_EMPTY);
    //sem_unlink(SEM_FULL);
    return 0;
}
