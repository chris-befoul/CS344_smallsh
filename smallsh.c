#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <string.h>
#include <fcntl.h>
#define	MAXchar	2049
#define	ARGlimit 513

int foregroundMode = 0;		// Global variable used to track if smallsh is in foreground only mode

/*
* Structure to represent command line calls
*/
struct commandLine
{
	char* command;
	char* argument[ARGlimit];
	char* inputFile;
	char* outputFile;
	int	background;
	char* execArg[ARGlimit];
};


/*
* Function creates command line structure to be used for smallsh calls
*/
struct commandLine* readCommand(void)
{
	char userInput[MAXchar];
	fgets(userInput, MAXchar, stdin);
	userInput[strcspn(userInput, "\n")] = 0;			// Removes "\n" character from userInput
	struct commandLine* currCall = malloc(sizeof(struct commandLine));
	char expansion[MAXchar];

	// For use with strtok_r
	char* saveptr;

	// The first token is the command
	char* token = strtok_r(userInput, " ", &saveptr);
	if (token == NULL)
	{
		return currCall;
	}
	currCall->command = calloc(strlen(token) + 1, sizeof(char));
	strcpy(currCall->command, token);
	currCall->execArg[0] = calloc(strlen(token) + 1, sizeof(char));
	strcpy(currCall->execArg[0], token);

	int i = 0;
	int x = 1;

	// Loops through userInput/command line call
	while (1)
	{
		char* token = strtok_r(NULL, " ", &saveptr);
		if (token == NULL)
		{
			break;
		}
		else if (strcmp(token, "<") == 0)
		{
			char* token = strtok_r(NULL, " ", &saveptr);
			currCall->inputFile = calloc(strlen(token) + 1, sizeof(char));
			strcpy(currCall->inputFile, token);
		}
		else if (strcmp(token, ">") == 0)
		{
			char* token = strtok_r(NULL, " ", &saveptr);
			currCall->outputFile = calloc(strlen(token) + 1, sizeof(char));
			strcpy(currCall->outputFile, token);
		}
		else if (strcmp(token, "&") == 0 && strtok_r(NULL, " ", &saveptr) == NULL) 
		{
			currCall->background = 1;
		}
		else
		{
			if (strstr(token, "$$") != NULL)
			{
				int addPID = getpid();
				//convert the $$ to the shell Pid.
				strcpy(expansion, token);
				newName(expansion, addPID);
				token = expansion;
				currCall->argument[i] = calloc(strlen(token) + 1, sizeof(char));
				strcpy(currCall->argument[i], token);
				i = i++;
				currCall->execArg[x] = calloc(strlen(token) + 1, sizeof(char));
				strcpy(currCall->execArg[x], token);
				x = x++;
			}
			currCall->argument[i] = calloc(strlen(token) + 1, sizeof(char));
			strcpy(currCall->argument[i], token);
			i++;
			currCall->execArg[x] = calloc(strlen(token) + 1, sizeof(char));
			strcpy(currCall->execArg[x], token);
			x++;
		}
	}
	currCall->execArg[x] = NULL;			// NULLs end of execArg to allow for appropriate execvp call
	return currCall;
};


/*
* Function used to replace $$ characters using pid
*/
void newName(char* token, int addPID) {

	char stringPid[250];
	char temp[MAXchar];
	char* tokenCopy = token;

	sprintf(stringPid, "%d", addPID);

	while ((tokenCopy = strstr(tokenCopy, "$$"))) {
		strncpy(temp, token, tokenCopy - token);
		temp[tokenCopy - token] = '\0';

		strcat(temp, stringPid);
		strcat(temp, tokenCopy + strlen("$$"));

		strcpy(token, temp);
		tokenCopy++;
	}

}

/*
* Built in command used to for change directory call "cd"
*/
void changeDirectory(struct commandLine* call) 
{
	char* path;
	if (call->argument[0] == NULL)
	{
		path = getenv("HOME");
		chdir(path);
	}
	else if (call->argument[0] != NULL)
	{
		if (chdir(call->argument[0]) != 0)
		{
			printf("Invalid directory name.\n");
			fflush(stdout);
		}
	}
}

/*
* Function used to dislay status be it an exit value or terminated
*/
void dislayStatus(int status)
{
	if (WIFEXITED(status)) {          //Taken from lecture
		printf("exit value %d\n", WEXITSTATUS(status));
		fflush(stdout);
	}
	else {
		printf("terminated by signal %d\n", status);
		fflush(stdout);
	}
}

/*
* Function to process Ctrl + Z call to enter/exit foreground only mode
*/
void ctrlZ(void)
{
	//If user is in standard mode program prompts user they are about to go into foreground only mode
	if (foregroundMode == 0) {
		foregroundMode = 1;
		printf("Entering foreground-only mode (& is now ignored)\n: ");
		fflush(stdout);

	}
	//If user is in foreground mode program will prompt user of exiting foreground mode
	else {
		foregroundMode = 0;
		printf("Exiting foreground-only mode\n: ");
		fflush(stdout);

	}
}

int main(void)
{
	int status = 0;
	int readFile;
	int writeFile;
	pid_t childPid = -5;

	// Create signal handler to ignore user input of ctrl + c
	struct sigaction ctrlc_action = { 0 };
	ctrlc_action.sa_handler = SIG_IGN;
	sigaction(SIGINT, &ctrlc_action, NULL);

	// Create signal handler to process user input of ctrl + z for entering/exiting foreground mode
	struct sigaction ctrlz_action = { 0 };
	ctrlz_action.sa_handler = &ctrlZ;
	sigfillset(&ctrlz_action.sa_mask);
	ctrlz_action.sa_flags = SA_RESTART;
	sigaction(SIGTSTP, &ctrlz_action, NULL);

	printf("$ smallsh\n");
	fflush(stdout);
	// Loop that process commands provided by user that exits upon command call of "exit"
	while (1)
	{
		printf(": ");
		fflush(stdout);
		struct commandLine* call = readCommand();
		if (call->command == NULL)
		{
			continue;
		}
		if (call->command[0] == '#')
		{
			continue;
		}
		else if (strcmp(call->command, "exit") == 0)
		{
			printf("\n$");
			fflush(stdout);
			exit(0);
		}
		else if (strcmp(call->command, "cd") == 0)
		{
			changeDirectory(call);
		}
		else if (strcmp(call->command, "status") == 0)
		{
			dislayStatus(status);
		}
		else
		{
			childPid = fork();
			if (childPid == -1)
			{
				perror("Fork Error\n");
				status = 1;
				exit(1);
			}
			else if (childPid == 0)
			{
				if (call->background != 1)
				{
					ctrlc_action.sa_handler = SIG_DFL;
					sigaction(SIGINT, &ctrlc_action, NULL);
				}
				if (call->inputFile != NULL)
				{
					readFile = open(call->inputFile, O_RDONLY);

					if (readFile == -1)
					{
						printf("cannot open %s for input\n", call->inputFile);
						fflush(stdout);
						status = 1;
						exit(1);
					}
					if (dup2(readFile, 0) == -1)
					{
						perror("dup error");
						status = 1;
						exit(1);
					}
					close(readFile);
				}
				if (call->outputFile != NULL)
				{
					writeFile = open(call->outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
					if (writeFile == -1)
					{
						printf("cannot open %s for output\n", call->outputFile);
						fflush(stdout);
						status = 1;
						exit(1);
					}
					if (dup2(writeFile, 1) == -1)
					{
						perror("dup error");
						status = 1;
						exit(1);
					}
					close(writeFile);
				}
				if (call->background == 1)
				{
					if (call->inputFile == NULL)
					{
						readFile = open("/dev/null", O_RDONLY);
						if (readFile == -1)
						{
							printf("cannot open /dev/null for input\n");
							fflush(stdout);
							status = 1;
							exit(1);
						}
						else if (dup2(readFile, 0) == -1)
						{
							perror("dup error");
							status = 1;
							exit(1);
						}
						close(readFile);
					}
					if (call->outputFile == NULL)
					{
						writeFile = open("/dev/null", O_WRONLY);
						if (writeFile == -1) {
							printf("cannot open /dev/null for output\n");
							fflush(stdout);
							status = 1;
							exit(1);
						}
						else if (dup2(writeFile, 1) == -1) {
							perror("dup error");
							status = 1;
							exit(1);
						}
						close(writeFile);
					}
				}
				if (execvp(call->execArg[0], call->execArg))
				{
					printf("%s: no such file or directory\n", call->command);
					fflush(stdout);
					status = 1;
					exit(1);
				}
			}
				
			else 
			{
				if (call->background == 1)
				{
					printf("background pid is %d\n", childPid);
					fflush(stdout);
				}
				else
				{
					waitpid(childPid, &status, 0);
				}
			}
		}
		childPid = waitpid(-1, &status, WNOHANG);
		// Loop that will print status when background process is done and the status be it exit or terminated
		while (childPid > 0) {                                            
			printf("background process, %i, is done: ", childPid);
			fflush(stdout);
			if (WIFEXITED(status)) {
				printf("exit value %i\n", WEXITSTATUS(status));
				fflush(stdout);
			}
			else {
				printf("terminated by signal %i\n", status);
				fflush(stdout);
			}
			childPid = waitpid(-1, &status, WNOHANG);
		}
	}
}