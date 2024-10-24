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
#define KEY_FILE_NAME "keyfile"
#define BLU_BOLD "\x1b[;34;1m"
#define RED     "\033[31m" // Red text 
#define RESET   "\033[0m"  // Reset to default color

sem_t* sem_empty = NULL;
sem_t* sem_full = NULL;
const char* name = "OS";

void receive(message_t* message_ptr, mailbox_t* mailbox_ptr,double* time_spent) {
    struct timespec start, end;

    sem_wait(sem_full);

    if (mailbox_ptr->flag == 1) {
        // Message Passing 
        
        int msqid= msgget(mailbox_ptr->storage.msqid, 0666 | IPC_CREAT);
        if (msqid == -1) {
            perror("msgget failed");
            exit(EXIT_FAILURE);
        }
        clock_gettime(CLOCK_MONOTONIC, &start);
       
        if (msgrcv(msqid, message_ptr, sizeof(message_t) , 0, 0) == -1) {
            perror("msgrcv failed");
            exit(EXIT_FAILURE);
        }
        clock_gettime(CLOCK_MONOTONIC, &end);
        if(strcmp(message_ptr->text,EXIT_MSG)==0)
            printf(RED"End of input file!exit!\n"RESET);
        else
            printf(BLU_BOLD"Receiving Message : %s\n",message_ptr->text,RESET);
        *time_spent += (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    } 
    else if (mailbox_ptr->flag == 2) {
        // Shared Memory
    // Measure the time spent reading from shared memory
    clock_gettime(CLOCK_MONOTONIC, &start);
    strcpy(message_ptr->text, mailbox_ptr->storage.shm_addr);
    clock_gettime(CLOCK_MONOTONIC, &end);

    if(strcmp(message_ptr->text,EXIT_MSG)==0)
        printf(RED"End of input file!exit!\n"RESET);
    else
        printf(BLU_BOLD"Receiving Message : %s\n",message_ptr->text,RESET);

    *time_spent += (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    
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
    double time_spent_on_communication = 0.0;
    key_t key = ftok(KEY_FILE_NAME, 666);
    mailbox.storage.msqid = key;

    sem_empty = sem_open(SEM_EMPTY,O_CREAT, 0777, 0);
    sem_full = sem_open(SEM_FULL,O_CREAT,0777,0);
    
    printf(BLU_BOLD "Message Passing\n" RESET);
    // Initialize mailbox based on mechanism
    if (mechanism == 1) {
        mailbox.flag = 1;
        do {
            receive(&message, &mailbox, &time_spent_on_communication);
            
        } while (strcmp(message.text, EXIT_MSG) != 0);
        
    } 
    else if (mechanism == 2) {
        mailbox.flag = 2;
        int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
        if (shm_fd == -1) {
            perror("shm_open failed");
            exit(EXIT_FAILURE);
        }

        // Map shared memory to address space
        mailbox.storage.shm_addr = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
        if (mailbox.storage.shm_addr == MAP_FAILED) {
            perror("mmap failed");
            exit(EXIT_FAILURE);
        }
        do {
            receive(&message, &mailbox,&time_spent_on_communication); 
        } while (strcmp(message.text, EXIT_MSG) != 0);
        // Clean up
        munmap(mailbox.storage.shm_addr, SHM_SIZE);
        close(shm_fd);
    } 
    else {
        fprintf(stderr, "Invalid mechanism. Use 1 for Message Passing, 2 for Shared Memory.\n");
        exit(EXIT_FAILURE);
    }


    sem_close(sem_empty);
    sem_close(sem_full);
   
    printf("Total time spent on communication: %.6f seconds\n", time_spent_on_communication);

    
    return 0;
}
