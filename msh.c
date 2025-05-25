// TODO: Make "quit" an alias for "exit"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define MSH_RL_BUFSIZE 1024

/*char* old_msh_read_line(void) {
	int bufsize = MSH_RL_BUFSIZE;
	int position = 0;
	char *buffer = malloc(sizeof(*buffer) * bufsize);
	int c;

	if (!buffer) {
		fprintf(stderr, "msh: allocation error\n");
		exit(EXIT_FAILURE);
	}

	while (1) {
		// Read a character
		c = getchar();

		// Replace with null terminator if it hits EOF or newline
		if (c == EOF || c == '\n') {
			buffer[position] = '\0';
			return buffer;
		} else {
			buffer[position] = c;
		}

		++position;
		
		{ // Reallocate if buffer is exceeded
			if (position >= bufsize) {
				bufsize += MSH_RL_BUFSIZE;
				buffer = realloc(buffer, bufsize);

				if (!buffer) {
					fprintf(stderr, "msh: allocation error\n");
					exit(EXIT_FAILURE);
				}
			}
		}
	}
}*/

char* msh_read_line(void) {
	char *line = NULL;
	size_t bufsize = 0;

	if (getline(&line, &bufsize, stdin) == -1) {
		if (feof(stdin)) {
			exit(EXIT_SUCCESS); // EOF received
		} else {
			perror("readline");
			exit(EXIT_FAILURE);
		}
	}

	return line;
}

#define MSH_TOK_BUFSIZE 64
#define MSH_TOK_DELIM " \t\r\n\a"

char** msh_split_line(char *line) {
	int bufsize = MSH_TOK_BUFSIZE, position = 0;

	char *token;
	char **tokens = malloc(bufsize * sizeof(*tokens));

	if (!tokens) {
		fprintf(stderr, "msh: allocation error\n");
		exit(EXIT_FAILURE);
	}

	while ((token = strtok_r(line, MSH_TOK_DELIM, &line))) {
		tokens[position] = token;
		++position;

		if (position >= bufsize) {
			bufsize += MSH_TOK_BUFSIZE;
			tokens = realloc(tokens, bufsize * sizeof(*tokens));

			if (!tokens) {
				fprintf(stderr, "msh: allocation error\n");
				exit(EXIT_FAILURE);
			}
		}
	}

	tokens[position] = NULL;
	return tokens;
}

int msh_launch(char **args) {
	pid_t pid, wpid;
	int status;

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

	return 1;
}

/*-- CODE FOR BUILT IN COMMANDS --*/

int msh_cd(char **args);
int msh_help(char **args);
int msh_exit(char **args);

char *builtin_str[] = {
	"cd",
	"help",
	"exit"
};

int (*builtin_func[]) (char **) = {
	&msh_cd,
	&msh_help,
	&msh_exit
};

int msh_num_builtins() {
	return sizeof(builtin_str) / sizeof(*builtin_str);
}

int msh_cd(char **args) {
	if (args[1] == NULL) {
		fprintf(stderr, "msh: expected argument to \"cd\"\n");
	} else if (chdir(args[1]) != 0) {
		perror("lsh");
	}

	return 1;
}

int msh_help(char **args) {
	printf("\nMyShell\n\n");
	printf("Following cmds are built in:\n\n");

	for (int i = 0; i < msh_num_builtins(); ++i) {
		printf("  %s\n", builtin_str[i]);	
	}

	printf("\nUse the man command for information on other programs.\n\n");
	return 1;
}

int msh_exit(char **args) {
	return 0;
}

/*-- END --*/

int msh_execute(char **args) {
	if (args[0] == NULL) {
		// Empty command
		return 1;
	}

	for (int i = 0; i < msh_num_builtins(); ++i) {
		if (strcmp(args[0], builtin_str[i]) == 0) {
			return (*builtin_func[i])(args); 
		}
	}

	return msh_launch(args);
}

void msh_loop(void) {
	char *line;
	char **args;
	int status;

	do {
		printf("> ");

		line = msh_read_line();
		args = msh_split_line(line);
		status = msh_execute(args);

		free(line);
		free(args);
	} while (status);
}

int main(int argc, char **argv) {
	// Load config files
	
	// Command loop
	msh_loop();

	// Shutdown/cleanup
	
	return EXIT_SUCCESS;
}
