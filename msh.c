#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define MSH_RL_BUFSIZE 1024
#define MSH_TOK_BUFSIZE 64
#define MSH_TOK_DELIM " \t\r\n\a"

/*-- BUILT-IN COMMANDS START --*/

typedef struct {
	char *name;
	char *desc;
	int (*func)(char **);
} Builtin;

int msh_cd(char **args);
int msh_help(char **args);
int msh_exit(char **args);

Builtin builtins[] = {
	{.name = "cd", .desc = "change the working directory", .func = &msh_cd},
	{.name = "help", .desc = "list built-in commands", .func = &msh_help},
	{.name = "exit",
	 .desc = "terminate shell; same as \"quit\"",
	 .func = &msh_exit},
	{.name = "quit",
	 .desc = "terminate shell; same as \"exit\"",
	 .func = &msh_exit}};

int msh_num_builtins() { return sizeof(builtins) / sizeof(*builtins); }

int msh_cd(char **args) {
	if (args[1] == NULL) {
		fprintf(stderr, "msh: expected argument to \"cd\"\n");
	} else if (chdir(args[1]) != 0) {
		perror("lsh");
	}

	return 1;
}

int msh_help(char **args) {
	printf("\nmsh\n\n");
	printf("Following cmds are built in:\n\n");

	for (int i = 0; i < msh_num_builtins(); ++i) {
		printf("\t%s - %s\n", builtins[i].name, builtins[i].desc);
	}

	printf("\nUse the man command for information on other programs.\n\n");

	return 1;
}

int msh_exit(char **args) { return 0; }

/*-- BUILT-IN COMMANDS END --*/

int main(int argc, char **argv) {
	/*-- LOAD CONFIG FILES START --*/
	/*-- LOAD CONFIG FILES END --*/

	/*-- COMMAND LOOP START --*/
	char *line;
	char **args;
	int status;

	do {
		printf("> ");

		{ /* Read input line */
			line = NULL;
			size_t bufsize = 0;

			if (getline(&line, &bufsize, stdin) == -1) {
				if (feof(stdin)) {
					exit(EXIT_SUCCESS); // EOF received
				} else {
					perror("readline");
					exit(EXIT_FAILURE);
				}
			}
		}

		{ /* Split line into args */
			int bufsize = MSH_TOK_BUFSIZE, position = 0;

			char *token;
			args = malloc(bufsize * sizeof(*args));

			if (!args) {
				fprintf(stderr, "msh: allocation error\n");
				exit(EXIT_FAILURE);
			}

			// NOTE: strtok_r updates the address pointed by the third
			// parameter to the next token's every time it is called.
			// Use tmp to keep original address on line so it can be
			// freed later
			char *tmp_line = line;
			while ((token = strtok_r(tmp_line, MSH_TOK_DELIM, &tmp_line))) {
				args[position] = token;
				++position;

				if (position >= bufsize) {
					bufsize += MSH_TOK_BUFSIZE;
					args = realloc(args, bufsize * sizeof(*args));

					if (!args) {
						fprintf(stderr, "msh: allocation error\n");
						exit(EXIT_FAILURE);
					}
				}
			}

			args[position] = NULL;
		}

		/* Execute command */

		// Empty command
		if (args[0] == NULL) {
			status = 1;
			goto status_found;
		}

		// Launch built-in
		for (int i = 0; i < msh_num_builtins(); ++i) {
			if (strcmp(args[0], builtins[i].name) == 0) {
				status = (*builtins[i].func)(args);
				goto status_found;
			}
		}

		// Launch process
		pid_t pid, wpid;

		pid = fork();
		if (pid == 0) {
			// Child process

			// NOTE: First item of args must be program name
			if (execvp(args[0], args) == -1) {
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
		free(line);
		free(args);
	} while (status);
	/*-- COMMAND LOOP END --*/

	/*-- SHUTDOWN/CLEANUP START --*/
	/*-- SHUTDOWN/CLEANUP END --*/

	return EXIT_SUCCESS;
}
