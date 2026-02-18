#include <stdio.h>
#include <unistd.h>

typedef struct {
	char *name;
	char *desc;
	int (*func)(char **);
} Builtin;

int msh_cd(char **args);
int msh_help(char **args);
int msh_exit(char **args);

Builtin builtins[] = {
	{.name = "cd",
	 .desc = "change the working directory",
	 .func = &msh_cd},
	{.name = "help",
	 .desc = "list built-in commands",
	 .func = &msh_help},
	{.name = "exit",
	 .desc = "terminate shell; same as \"quit\"",
	 .func = &msh_exit},
	{.name = "quit",
	 .desc = "terminate shell; same as \"exit\"",
	 .func = &msh_exit}};

int msh_num_builtins() {
	return sizeof(builtins) / sizeof(*builtins);
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
	printf("\nmsh\n\n");
	printf("Following cmds are built in:\n\n");

	for (int i = 0; i < msh_num_builtins(); ++i) {
		printf("\t%s - %s\n", builtins[i].name, builtins[i].desc);
	}

	printf("\nUse the man command for information on other programs.\n\n");

	return 1;
}

int msh_exit(char **args) {
	return 0;
}
