#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include "../include/command.h"
#include "../include/builtin.h"

// ======================= requirement 2.3 =======================
/**
 * @brief 
 * Redirect command's stdin and stdout to the specified file descriptor
 * If you want to implement ( < , > ), use "in_file" and "out_file" included the cmd_node structure
 * If you want to implement ( | ), use "in" and "out" included the cmd_node structure.
 *
 * @param p cmd_node structure
 * 
 */
void redirection(struct cmd_node *p){
	if (p->out_file!=NULL) {
		// Output redirection
		int fd = open(p->out_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
		if (fd < 0) {
			perror("open");
			exit(EXIT_FAILURE);
		}
		dup2(fd, STDOUT_FILENO);
		close(fd);
		//p->args[i] = NULL; // Remove redirection from args
	} 
	else if (p->in_file!=NULL) {
		// Input redirection
		int fd = open(p->in_file, O_RDONLY);
		if (fd < 0) {
			perror("open");
			exit(EXIT_FAILURE);
		}
		dup2(fd, STDIN_FILENO);
		close(fd);
		//p->args[i] = NULL; // Remove redirection from args
	}
    
	
}
// ===============================================================

// ======================= requirement 2.2 =======================
/**
 * @brief 
 * Execute external command
 * The external command is mainly divided into the following two steps:
 * 1. Call "fork()" to create child process
 * 2. Call "execvp()" to execute the corresponding executable file
 * @param p cmd_node structure
 * @return int 
 * Return execution status
 */
int spawn_proc(struct cmd_node *p)
{	
	
	
	pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (pid == 0) {
		if(p->out_file!=NULL || p->in_file!= NULL)
			redirection(p);
		const char *str1 = "ls";
		const char *str2 = "cat";
        // In the child process, use execvp to execute 'ls'
		if(strcmp(p->args[0],str1)==0){
			if (execvp("ls", p->args) < 0) {
				perror("execvp");
				exit(EXIT_FAILURE);
			}
		}
		else if(strcmp(p->args[0],str2)==0){
			if (execvp("cat", p->args) < 0) {
				perror("execvp");
				exit(EXIT_FAILURE);
			}
		}
    } 
	else {
        // Parent process waits for the child to finish
		//waitpid();
        wait(NULL);
    }
  	return 1;
}
// ===============================================================


// ======================= requirement 2.4 =======================
/**
 * @brief 
 * Use "pipe()" to create a communication bridge between processes
 * Call "spawn_proc()" in order according to the number of cmd_node
 * @param cmd Command structure  
 * @return int
 * Return execution status 
 */
int fork_cmd_node(struct cmd *cmd)
{
	int pipefd[2], prev_fd = -1;
    struct cmd_node *current = cmd->head;
    pid_t pid;

    while (current != NULL) {
        // Create a pipe if it's not the last command
        if (current->next != NULL) {
            if (pipe(pipefd) == -1) {
                perror("pipe");
                exit(EXIT_FAILURE);
            }
        }
        //Fork a new process for the current command
        pid = fork();
        if (pid == -1) {
            perror("fork");
            exit(EXIT_FAILURE);
        }

        else if (pid == 0) {
            // Child process
            // Redirect input if necessary
            if (current->in_file != NULL) {
                current->in = open(current->in_file, O_RDONLY);
                if (current->in < 0) {
                    perror("open in_file");
                    exit(EXIT_FAILURE);
                }
                dup2(current->in, STDIN_FILENO);  // Redirect stdin to the input file
                close(current->in);
            } 
			else if (prev_fd != -1) {
                // If there was a previous command, use the read end of the previous pipe
                dup2(prev_fd, STDIN_FILENO);  // Redirect stdin to the previous pipe's read end
                close(prev_fd);
            }
			//spawn_proc(current);

            //Redirect output if necessary
            if (current->out_file != NULL) {
                current->out = open(current->out_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (current->out < 0) {
                    perror("open out_file");
                    exit(EXIT_FAILURE);
                }
                dup2(current->out, STDOUT_FILENO);  // Redirect stdout to the output file
                close(current->out);
            } 
			else if (current->next != NULL) {
                // If there is a next command, use the write end of the current pipe
                dup2(pipefd[1], STDOUT_FILENO);  // Redirect stdout to the pipe's write end
                close(pipefd[0]);  // Close the unused read end of the pipe
                close(pipefd[1]);  // Close the original write end of the pipe
            }

            //Execute the command
            execvp(current->args[0], current->args);
            //If execvp fails
            perror("execvp");
            exit(EXIT_FAILURE);
			spawn_proc(current);
        } 
		else {
            // Parent process

            // Close the previous read end if it's open
            if (prev_fd != -1) {
                close(prev_fd);
            }

            // Close the current pipe's write end in the parent
            if (current->next != NULL) {
                close(pipefd[1]);
                prev_fd = pipefd[0];  // Set the read end of the current pipe for the next command
            }

            // Wait for the child process to finish
            wait(NULL);
        }

        // Move to the next command
        current = current->next;
    }
}

// Helper function to add a command to the pipeline
void add_command(struct cmd *pipeline, char **args, int length, char *in_file, char *out_file) {
    struct cmd_node *new_node = (struct cmd_node *)malloc(sizeof(struct cmd_node));
    new_node->args = args;
    new_node->length = length;
    new_node->in_file = in_file;
    new_node->out_file = out_file;
    new_node->next = NULL;

    // If this is the first command in the pipeline, set the head
    if (pipeline->head == NULL) {
        pipeline->head = new_node;
    } else {
        // Otherwise, append to the list
        struct cmd_node *current = pipeline->head;
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = new_node;
    }
	return 1;
}
// ===============================================================


void shell()
{
	while (1) {
		printf(">>> $ ");
		char *buffer = read_line();
		if (buffer == NULL)
			continue;

		struct cmd *cmd = split_line(buffer);
		
		int status = -1;
		// only a single command
		struct cmd_node *temp = cmd->head;
		
		if(temp->next == NULL){
			status = searchBuiltInCommand(temp);
			if (status != -1){
				int in = dup(STDIN_FILENO), out = dup(STDOUT_FILENO);
				if( in == -1 | out == -1)
					perror("dup");
				redirection(temp);
				status = execBuiltInCommand(status,temp);

				// recover shell stdin and stdout
				if (temp->in_file)  dup2(in, 0);
				if (temp->out_file){
					dup2(out, 1);
				}
				close(in);
				close(out);
			}
			else{
				//external command
				status = spawn_proc(cmd->head);
			}
		}
		// There are multiple commands ( | )
		else{
			
			status = fork_cmd_node(cmd);
		}
		// free space
		while (cmd->head) {
			
			struct cmd_node *temp = cmd->head;
      		cmd->head = cmd->head->next;
			free(temp->args);
   	    	free(temp);
   		}
		free(cmd);
		free(buffer);
		
		if (status == 0)
			break;
	}
}
