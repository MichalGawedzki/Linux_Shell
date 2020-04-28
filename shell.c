#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <readline/readline.h> 
#include <readline/history.h> 

#define N 64
#define P 10
#define DIR_SIZE 512
#define COM_SIZE 1024

#define clear() printf("\e[1;1H\e[2J")

char *historypath;

void init_shell() {
	clear();
	printf("------------------------------------\n");
	printf("------------------------------------\n");

	printf("\n		SHELL\n");

	printf("\n------------------------------------");
	printf("\n------------------------------------");
	printf("\n------------------------------------");

	sleep(1);
	clear();
}

void print_dir() {
	char cwd[DIR_SIZE];
	getcwd(cwd, sizeof(cwd));
	printf("\033[31m\033[1m%s\033[0m:\033[92m\033[1m %s\033[0m", getlogin(), cwd);
}

int read_his() {
	int f = open(historypath, O_CREAT | O_WRONLY | O_APPEND, 0666);
	if (f < 0) { 
		perror("opening file failed");
		return 1;
	}
	if (read_history(historypath) != 0) { 
		perror("reading history failed");
		return 1;
	}
	if(close(f)!=0){
		perror("close()");
		return 1;
	}
	return 0;
}

int print_his(char* pathfile) {
	int f = open(historypath, O_RDONLY);
	if (f < 0) {
		perror("reading history failed");
		return 1;
	}
	char *buff = (char*)malloc(COM_SIZE * sizeof(char));
	if (read(f, buff, COM_SIZE) < 0) {
		perror("reading from file failed");
		return 1;
	}
	printf("%s", buff);
	if(close(f)!=0){
		perror("close(): ");
		return 1;
	}
	return 0;
}

int take_input(char* input) {

	char* buff;
	buff = readline(" >>> ");

	if (strlen(buff) > 0) {

		strcpy(input, buff);
		add_history(input);
		if (write_history(historypath) != 0) {
			perror("writing history failed");
			return 1;
		}
		if (history_truncate_file(historypath, 20)) {
			perror("saving history failed");
			return 1;
		}
		return 0;
	}
	else {
		return 1;
	}
}

void handler(int signum) {
	char* pathfile = getenv("PWD");
	printf("\n");
	print_his(pathfile);
	print_dir();
	printf(" >>> ");
}

void parse(char *input, char ***args, int *how_many_pipes) {
	char *found_pipe, *found;
	int i, j;

	for (i = 0; (found_pipe = strsep(&input, "|")) != NULL; i++) {
		for (j = 0; (found = strsep(&found_pipe, " ")) != NULL; ) {
			if (strlen(found) > 0) {
				args[i][j] = found;
				j++;
			}
		}
		args[i][j] = NULL;
	}
	args[i] = NULL;

	*how_many_pipes = --i;
}

void flags(char ***args, int how_many_pipes, int *red_flag, char *red_file, int *amp_flag) {
	int i = how_many_pipes;
	*red_flag = 0;
	*amp_flag = 0;
	for (int j = 0; args[i][j] != NULL; j++) {
		if (strcmp(args[i][j], ">>") == 0) {
			args[i][j] = NULL;
			strcpy(red_file, args[i][j + 1]);
			*red_flag = 1;
		}
		else if (strcmp(args[i][j], "&") == 0) {
			args[i][j] = NULL;
			*amp_flag = 1;
		}
	}
}

int execute(char ***args, int how_many_pipes, int red_flag, char *red_file, int amp_flag) {
	int fd[2 * how_many_pipes], status;
	int i, j, k;
	pid_t pid, wpid;
	for (i = 0; i < (how_many_pipes); i++) {
		if (pipe(fd+i*2) < 0) {
			perror("error with piping");
			return 1;
		}
	}

	for (k = 0; k <= how_many_pipes; k++) {

		j = 2*k;

		if (strcmp(args[k][0], "exit") == 0)
			exit(EXIT_SUCCESS);

		pid = fork();
		if(pid<0){
			perror("fork()");
			return 1;
		}
		if (pid == 0) {
			if (red_flag == 1) {
				int rfile;
				rfile = open(red_file, O_CREAT | O_TRUNC | O_WRONLY, 0777);
				if(rfile<0){
					perror("open()");
					exit(EXIT_FAILURE);
				}
				if(dup2(rfile, STDOUT_FILENO)<0){
					perror("dup2()");
					exit(EXIT_FAILURE);
				}
				if(close(rfile)!=0){
					perror("close()");
					exit(EXIT_FAILURE);
				}
			}
			if (k < how_many_pipes) {
				if (dup2(fd[j+1],1) < 0) {
					perror("dup2()");
					exit(EXIT_FAILURE);
				}
			}
			if (j != 0) {
				if (dup2(fd[j-2],0) < 0) {
					perror("dup2()");
					exit(EXIT_FAILURE);
				}
			}
			for (i = 0; i < 2*how_many_pipes; i++)
				if(close(fd[i])<0){
					perror("close(fd[i])");
					exit(EXIT_FAILURE);
				}

			if (strcmp(args[k][0],"cd") == 0) {
				if (chdir(args[k][1]) < 0)
					perror("cd");
					exit(EXIT_FAILURE);
			}
			else if (execvp(args[k][0], args[k]) < 0) {
				perror(args[k][0]);
				exit(EXIT_FAILURE);
			}
		}
		else if (pid < 0) {
			perror("fork(): ");
			return 1;
		}
	}

	for (i = 0; i < 2*how_many_pipes; i++)
		if(close(fd[i])<0){
			perror("close(fd[i]): ");
			return 1;
		}

	for (i = 0; i <= how_many_pipes; i++) {
		if (amp_flag == 0) {
			waitpid(pid, &status, 0);
		}
	}
	return 0;
}

int execute_script(char *arg) {
	int how_many_pipes, amp_flag, red_flag;
	char *red_file = (char*)malloc(COM_SIZE * sizeof(char*));
	char *input = (char*)malloc(COM_SIZE * sizeof(char*));

	int fscript = open(arg, O_RDONLY);
	if (fscript < 0) {
		perror("reading script failed: ");
	}
	else {
		FILE * script = fdopen(fscript, "r");
		if (script == NULL) {
			close(fscript);
			perror("reading form file failed");
		}
		else {
			printf("Rozpoczynam wykonywanie skryptu.\n");
			fgets(input, COM_SIZE, script);
			while (fgets(input, COM_SIZE, script)) {
				char ***parsed = malloc(P * sizeof(char**));
				for (int i = 0; i < P; i++) {
					parsed[i] = malloc(N * sizeof(char*));
					for (int j = 0; j < N; j++)
						parsed[i][j] = malloc(N * sizeof(char));
				}
				input[strlen(input) - 1] = NULL;
				parse(input, parsed, &how_many_pipes);
				flags(parsed, how_many_pipes, &red_flag, red_file, &amp_flag);
				execute(parsed, how_many_pipes, red_flag, red_file, amp_flag);
				sleep(1);
			}
			printf("Wykonywanie skryptu zakończone.\n");
			close(fscript);
			fclose(script);
		}
	}
}

int main(int argc, char* argv[]) {

	if (argc > 2) {
		printf("Too many arguments. Usage: ./shell || ./shell <filename>\n");
		return 1;
	}

	historypath = strcat(getenv("HOME"), "/history.txt");
	init_shell();
	read_his();
	signal(SIGQUIT, handler);
	signal(SIGCHLD, SIG_IGN);

	int how_many_pipes, amp_flag, red_flag;
	char *red_file = (char*)malloc(COM_SIZE * sizeof(char*));

	if (argc == 2) {
		execute_script(argv[1]);
	}

	while (1) {
		char *input = (char*)malloc(COM_SIZE * sizeof(char*));
		char ***parsed = malloc(P * sizeof(char**));
		for (int i = 0; i < P; i++) {
			parsed[i] = malloc(N * sizeof(char*));
			for (int j = 0; j < N; j++)
				parsed[i][j] = malloc(N * sizeof(char));
		}
		print_dir();
		if (take_input(input)==0) {
			parse(input, parsed, &how_many_pipes);
			flags(parsed, how_many_pipes, &red_flag, red_file, &amp_flag);
			execute(parsed, how_many_pipes, red_flag, red_file, amp_flag);
		}
	}
	return 0;
}
