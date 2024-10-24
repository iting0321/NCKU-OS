#include "sender.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>

#define KEY_FILE_NAME "keyfile"
#define SHM_SIZE 1024
#define SEM_NAME "/sem_example"
#define EXIT_MSG "EXIT"
#define SHM_NAME "/posix_shm_example"
#define SEM_EMPTY "/posix_sem_empty"
#define SEM_FULL "/posix_sem_full"
#define BLU_BOLD "\x1b[;34;1m"
#define RED     "\033[31m" // Red text 
#define RESET   "\033[0m"  // Reset to default color

sem_t* sem_empty = NULL;
sem_t* sem_full = NULL;

void send(message_t message, mailbox_t* mailbox_ptr, double* time_spent) {
    struct timespec start, end;

    sem_wait(sem_empty);

    if (mailbox_ptr->flag == 1) {
        int msqid = msgget(mailbox_ptr->storage.msqid, 0666 | IPC_CREAT);
        if (msqid == -1) {
            perror("msgget failed");
            exit(EXIT_FAILURE);
        }
        // Message Passing (Posix)
        if(strcmp(message.text,EXIT_MSG)==0)
            printf(RED "Sender exit!\n" RESET);
        else{
            printf(BLU_BOLD"Sending Message : %s %s\n",RESET,message.text);

        }
        clock_gettime(CLOCK_MONOTONIC, &start);
        if (msgsnd(msqid, &message, sizeof(message_t), 0) == -1) {
            perror("msgsnd failed");
            exit(EXIT_FAILURE);
        }
        clock_gettime(CLOCK_MONOTONIC, &end);
    } 
    else if (mailbox_ptr->flag == 2) {
        
        // Shared Memory
        if(strcmp(message.text,EXIT_MSG)==0)
            printf(RED "Sender exit!\n" RESET);
        else
             printf(BLU_BOLD"Sending Message : %s %s\n",RESET,message.text);
        clock_gettime(CLOCK_MONOTONIC, &start);
        strcpy(mailbox_ptr->storage.shm_addr, message.text);
        clock_gettime(CLOCK_MONOTONIC, &end);

        *time_spent += (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
   
    } 
    else {
        fprintf(stderr, "Invalid mailbox flag.\n");
        return;
    }

    *time_spent += (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;

    sem_post(sem_full);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <mechanism> <input_file>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int mechanism = atoi(argv[1]);
    const char *input_file = argv[2];
    mailbox_t mailbox;
    message_t message;
    key_t key = ftok(KEY_FILE_NAME, 666);
    mailbox.storage.msqid = key;
    int shm_fd =0;

    double time_spent_on_communication = 0.0;
    

    FILE *file = fopen(input_file, "r");
    if (!file) {
        perror("fopen failed");
        exit(EXIT_FAILURE);
    }
    printf(BLU_BOLD "Message Passing\n" RESET);

    // Initialize mailbox based on mechanism
    if (mechanism == 1) {
        mailbox.flag = 1;
        sem_empty = sem_open(SEM_EMPTY,O_CREAT, 0777, 1);
        sem_full = sem_open(SEM_FULL,O_CREAT,0777,0);

        while (fgets(message.text, sizeof(message.text), file)) {
            message.text[strcspn(message.text, "\n")] = '\0'; // Remove newline
            send(message, &mailbox, &time_spent_on_communication);
        }
        strcpy(message.text, EXIT_MSG);
        send(message, &mailbox, &time_spent_on_communication);
        
    }
    else if (mechanism == 2) {
        mailbox.flag = 2;
        shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0644);
        if (shm_fd == -1) {
            perror("shm_open failed");
            exit(EXIT_FAILURE);
        }
        ftruncate(shm_fd, SHM_SIZE);

        // Map shared memory to address space
        mailbox.storage.shm_addr = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
        if (mailbox.storage.shm_addr == MAP_FAILED) {
            perror("mmap failed");
            exit(EXIT_FAILURE);
        }
        sem_empty = sem_open(SEM_EMPTY,O_CREAT, 0777, 1);
        sem_full = sem_open(SEM_FULL,O_CREAT,0777,0);
        while (fgets(message.text, sizeof(message.text), file)) {
        message.text[strcspn(message.text, "\n")] = '\0'; // Remove newline
        send(message, &mailbox, &time_spent_on_communication);
        }
        strcpy(message.text, EXIT_MSG);
        send(message, &mailbox, &time_spent_on_communication);
        munmap(mailbox.storage.shm_addr, SHM_SIZE);
        close(shm_fd);


        
        
    } 
    else {
        fprintf(stderr, "Invalid mechanism. Use 1 for Message Passing, 2 for Shared Memory.\n");
        exit(EXIT_FAILURE);
    }

    

    // // Send exit message to receiver
    // strcpy(message.text, EXIT_MSG);
    // send(message, &mailbox, &time_spent_on_communication);
    // munmap(mailbox.storage.shm_addr, SHM_SIZE);
    // close(shm_fd);
    // Clean up
    fclose(file);
    

    printf("Total time spent on communication: %.6f seconds\n", time_spent_on_communication);
    sem_close(sem_empty);
    sem_close(sem_full);

 


    return 0;
}
