#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>

#include "builtins.c"

#define MSH_RL_BUFSIZE 1024
#define MSH_TOK_BUFSIZE 16
#define MSH_TOK_DELIM " \t\r\n\a"
#define MAX_PATH_SIZE 1024
#define PIPE_DELIM ">|"

#define COOL_BLUE(s) "\x1b[38;5;68m" s "\x1b[0m"

#define bool uint8_t;

typedef enum {
	CMDOUT_NORMAL,
	CMDOUT_FILE,
	CMDOUT_PIPE
} CmdOutMode;

typedef struct {
	const char *filePath;
	CmdOutMode	outMode;
} CmdOptions;

typedef struct {
	char **items;
	size_t count;
} Args;

typedef struct {
	CmdOptions opts;
	Args	   args;
} Cmd;

typedef struct {
	Cmd	  *items;
	size_t count;
	size_t cap;
} da_Cmd;

#define DA_DEFAULT_CAP 16
#define da_append(xs, x)                                              \
	do {                                                              \
		if (xs.count >= xs.cap) {                                     \
			if (xs.cap == 0)                                          \
				xs.cap = DA_DEFAULT_CAP;                              \
			else                                                      \
				xs.cap *= 2;                                          \
			xs.items = realloc(xs.items, xs.cap * sizeof(*xs.items)); \
		}                                                             \
		xs.items[xs.count++] = x;                                     \
	} while (0)

int execCmd(Cmd *cmd) {
	int status = 0;

	// Empty command
	if (cmd->args.items[0] == NULL) {
		status = 1;
		goto status_found;
	}

	// Launch built-in
	for (int i = 0; i < msh_num_builtins(); ++i) {
		if (strcmp(cmd->args.items[0], builtins[i].name) == 0) {
			status = (*builtins[i].func)(cmd->args.items);
			goto status_found;
		}
	}

	// Launch process
	pid_t pid, wpid;

	pid = fork();
	if (pid == 0) {
		// Child process

		if (cmd->opts.outMode == CMDOUT_FILE) {
			int fd = open(cmd->opts.filePath,
						  O_CREAT | O_CLOEXEC | O_WRONLY | O_TRUNC,
						  S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);  // 0644
			dup2(fd, STDOUT_FILENO);
			close(fd);
		}

		// NOTE: First item of args must be program name
		if (execvp(cmd->args.items[0], cmd->args.items) == -1) {
			perror("msh");
		}

		// NOTE: If execvp() returns, something went wrong
		exit(EXIT_FAILURE);
	} else if (pid < 0) {
		// Error forking
		perror("msh");
	} else {
		do {
			// TODO: Could I let process run on background instead?
			wpid = waitpid(pid, &status, WUNTRACED);
		} while (!WIFEXITED(status) && !WIFSIGNALED(status));
	}

	status = 1;

status_found:
	return status;
}

int main(int argc, char **argv) {
	/*-- LOAD CONFIG FILES START --*/
	/*-- LOAD CONFIG FILES END --*/

	/*-- COMMAND LOOP START --*/
	char *line;
	int	  status;

	da_Cmd cmds = {0};

	char pathBuf[MAX_PATH_SIZE];

	do {
		if (getcwd(pathBuf, MAX_PATH_SIZE)) {
			printf(COOL_BLUE("%s"), strcat(pathBuf, " > "));
		} else {
			printf(COOL_BLUE("Max path size exceeded > "));
		}

		// TODO: Also get next line if current ends in / (slash)
		{ /* Read input line */
			line		   = NULL;
			size_t bufsize = 0;

			if (getline(&line, &bufsize, stdin) == -1) {
				if (feof(stdin)) {
					exit(EXIT_SUCCESS);	 // EOF received
				} else {
					perror("readline");
					exit(EXIT_FAILURE);
				}
			}
		}

		{ /* Split line into args */
			int bufsize = MSH_TOK_BUFSIZE, position = 0;

			char *token;

			char **argsBuf = malloc(bufsize * sizeof(*argsBuf));
			if (!argsBuf) {
				fprintf(stderr, "msh: allocation error\n");
				exit(EXIT_FAILURE);
			}

			// TODO: Respect quotes/escapes when handling delims

			// NOTE: strtok_r updates the address pointed to by the third
			// parameter to the next token's every time it is called.
			// Using tmp to keep original address on line so it can be
			// freed later
			char *tmp_line = line;

			while ((token = strtok_r(tmp_line, MSH_TOK_DELIM, &tmp_line))) {
				if (!argsBuf) {
					char **newArgsBuf = malloc(bufsize * sizeof(*newArgsBuf));
					argsBuf			  = newArgsBuf;
				}

				size_t tokenLen = strlen(token);

				if (tokenLen == 1) {
					switch (token[0]) {
						case '>': {
							argsBuf[position]	   = NULL;
							const char *path	   = strtok_r(tmp_line, MSH_TOK_DELIM, &tmp_line);
							CmdOptions	newCmdOpts = {.outMode = CMDOUT_FILE, .filePath = path};
							Args		newCmdArgs = {.items = argsBuf, .count = position};
							Cmd			newCmd	   = {newCmdOpts, newCmdArgs};
							da_append(cmds, newCmd);

							break;
						}

						case '|': {
							// TODO
							break;
						}

						default:
							goto normal_arg;
					}

					position = 0;
					argsBuf	 = NULL;
				} else if (tokenLen == 2) {
					// TODO
					goto normal_arg;
				} else {
				normal_arg:
					argsBuf[position++] = token;

					if (position >= bufsize) {
						bufsize += MSH_TOK_BUFSIZE;
						argsBuf = realloc(argsBuf, bufsize * sizeof(*argsBuf));

						if (!argsBuf) {
							fprintf(stderr, "msh: allocation error\n");
							exit(EXIT_FAILURE);
						}
					}
				}
			}

			if (argsBuf) {
				argsBuf[position] = NULL;

				CmdOptions newCmdOpts = {.outMode = CMDOUT_NORMAL, .filePath = NULL};
				Args	   newCmdArgs = {.items = argsBuf, .count = position};
				Cmd		   newCmd	  = {.opts = newCmdOpts, .args = newCmdArgs};
				da_append(cmds, newCmd);

				argsBuf = NULL;
			}
		}

		/* Execute commands */
		for (int i = 0; i < cmds.count; ++i) {
			status = execCmd(&cmds.items[i]);
			free(cmds.items[i].args.items);
		}

		cmds.count = 0;

		free(line);
	} while (status);
	/*-- COMMAND LOOP END --*/

	/*-- SHUTDOWN/CLEANUP START --*/
	free(cmds.items);
	/*-- SHUTDOWN/CLEANUP END --*/

	return EXIT_SUCCESS;
}
