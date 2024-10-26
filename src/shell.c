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
		p->out = open(p->out_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
		if (p->out < 0) {
			perror("open");
			exit(EXIT_FAILURE);
		}
		dup2(p->out, STDOUT_FILENO);
		close(p->out);
	} 
	if (p->in_file!=NULL) {
		// Input redirection
		p->in = open(p->in_file, O_RDONLY);
		if (p->in < 0) {
			perror("open");
			exit(EXIT_FAILURE);
		}
		dup2(p->in, STDIN_FILENO);
		close(p->in);
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
int spawn_proc(struct cmd_node *p, int in_fd, int out_fd)
{	
	pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (pid == 0) {
		if (in_fd != STDIN_FILENO) {
            dup2(in_fd, STDIN_FILENO);
            close(in_fd);
        }

        // If there is an output pipe, redirect stdout to its write end
        if (out_fd != STDOUT_FILENO) {
            dup2(out_fd, STDOUT_FILENO);
            close(out_fd);
        }

        // Handle file redirection if specified
        if (p->out_file != NULL || p->in_file != NULL) {
            redirection(p);
        }

        // Command execution logic
        if (strcmp(p->args[0], "ls") == 0) {
            if (execvp("ls", p->args) < 0) {
                perror("execvp");
                exit(EXIT_FAILURE);
            }
        } else if (strcmp(p->args[0], "cat") == 0) {
            if (execvp("cat", p->args) < 0) {
                perror("execvp");
                exit(EXIT_FAILURE);
            }
        } else {
            // Handle any other command
            if (execvp(p->args[0], p->args) < 0) {
                perror("execvp");
                exit(EXIT_FAILURE);
            }
        }

    } else {
        // Parent process waits for the child to finish
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
	int pipefd[2], in_fd = STDIN_FILENO;
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
        // Spawn the current process and connect it to the pipeline
        spawn_proc(current, in_fd, (current->next != NULL) ? pipefd[1] : STDOUT_FILENO);
		//spawn_proc(current);
        // Close the write end of the current pipe in the parent process
        if (current->next != NULL) {
            close(pipefd[1]);
            in_fd = pipefd[0];  // The next command will read from the current pipe's read end
        }

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
				status = spawn_proc(cmd->head,cmd->head->in,cmd->head->out);
				//status = spawn_proc(cmd->head);
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
