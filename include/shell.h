#ifndef SHELL_H
#define SHELL_H

#include "command.h"

int spawn_proc(struct cmd_node *,int in_fd,int out_fd);
int fork_cmd_node(struct cmd *cmd);
void redirection(struct cmd_code *cmd);
void shell();

#endif
