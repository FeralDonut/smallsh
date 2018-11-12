#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>

#define MAXLENGTH 2048
#define MAXARG 512
#define BACKGROUND 1
#define FOREGROUND 0
#define YES	1
#define NO 0

int run_bg = YES;

/*
 NAME
    statusCommand
DESCRIPTION
    
REFERENCE:
 	WFEXITSTATUS() & WTERMSIG() Lecture 3.1
*/
void statusCommand(int childExitMethod) 
{
	if (WIFEXITED(childExitMethod)) 
	{
		// If exited by status
		printf("exit value %d\n", WEXITSTATUS(childExitMethod));
	} else {
		// If terminated by signal
		printf("terminated by signal %d\n", WTERMSIG(childExitMethod));
	}
}

/*
 NAME
    
DESCRIPTION
    
RESOURCE
       
*/
void signalStop(int signo) 
{

	if (run_bg == YES) 
	{
		char* message = "Entering foreground-only mode (& is now ignored)\n";
		write(1, message, 49);
		fflush(stdout);
		run_bg = NO;
	}else 
	{
		char* message = "Exiting foreground-only mode\n";
		write (1, message, 29);
		fflush(stdout);
		run_bg = YES;
	}
}


/*
 NAME
    
DESCRIPTION
    
RESOURCE
       fork() lecture 3.1
       dup2() and fcntl() from lecture 3.4
*/
void shellCommand(char *arr[], int *childExitStatus, struct sigaction sa, int *fgbg_status, char input_file[], char output_file[]) 
{	
	int sourceFD, targetFD, result;
	pid_t spawnPid = -5;

	
	spawnPid = fork();
	switch (spawnPid) {
		
		case -1:	
			perror("Hull Breach!\n");
			exit(1);
			break;
		case 0:	//Child
			sa.sa_handler = SIG_DFL;
			sigaction(SIGINT, &sa, NULL);

			if (strcmp(input_file, "")) 
			{
				sourceFD = open(input_file, O_RDONLY);
				if (sourceFD == -1) 
				{
					printf("cannot open %s for input\n", input_file);
					fflush(stdout);
					exit(1);
				}
				result = dup2(sourceFD, 0);
				if (result == -1) {
					perror("dup2()\n");
					exit(2);
				}
				//close on exec
				fcntl(sourceFD, F_SETFD, FD_CLOEXEC);
			}

			if (strcmp(output_file, "")) 
			{
				targetFD = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
				if (targetFD == -1) {
					printf("cannot open %s for output\n", output_file);
					fflush(stdout);
					exit(1);
				}
				result = dup2(targetFD, 1);
				if (result == -1) {
					perror("Unable to assign output file\n");
					exit(2);
				}
				fcntl(targetFD, F_SETFD, FD_CLOEXEC);
			}

			if (execvp(arr[0], (char* const*)arr)) 
			{
				printf("%s: no such file or directory\n", arr[0]);
				fflush(stdout);
				exit(2);
			}
			break;
		
		default:				
			if (*fgbg_status && run_bg) 
			{
				pid_t pid = waitpid(spawnPid, childExitStatus, WNOHANG);
				printf("background pid is %d\n", spawnPid);
				fflush(stdout);
			} else 
			{
				pid_t pid = waitpid(spawnPid, childExitStatus, 0);
			}

			// Check for terminated fgbg_status processes!	
			while (((spawnPid = waitpid(-1, childExitStatus, WNOHANG)) > 0) && run_bg)
			{
				printf("child %d terminated\n", spawnPid);
				statusCommand(*childExitStatus);
				fflush(stdout);
			}
	}
}


/*
 NAME
    
DESCRIPTION
    
RESOURCE
       
*/
void getInput(char* arr[], int* fgbg_status, char inputName[], char outputName[], int pid) 
{
	
	char input[MAXLENGTH];
	int i, j;

	// Get input
	printf(": ");
	fflush(stdout);
	fgets(input, MAXLENGTH, stdin);

	// Remove newline
	int found = 0;
	for (i=0; !found && i<MAXLENGTH; i++) {
		if (input[i] == '\n') {
			input[i] = '\0';
			found = 1;
		}
	}

	// If it's blank, return blank
	if (!strcmp(input, "")) {
		arr[0] = strdup("");
		return;
	}

	// Translate rawInput into individual strings
	const char space[2] = " ";
	char *token = strtok(input, space);

	for (i=0; token; i++) {
		// Check for & to be a fgbg_status process
		if (!strcmp(token, "&")) {
			*fgbg_status = BACKGROUND;
		}
		// Check for < to denote input file
		else if (!strcmp(token, "<")) {
			token = strtok(NULL, space);
			strcpy(inputName, token);
		}
		// Check for > to denote output file
		else if (!strcmp(token, ">")) {
			token = strtok(NULL, space);
			strcpy(outputName, token);
		}
		// Otherwise, it's part of the command!
		else {
			arr[i] = strdup(token);
			// Replace $$ with pid
			// Only occurs at end of string in testscirpt
			for (j=0; arr[i][j]; j++) {
				if (arr[i][j] == '$' &&
					 arr[i][j+1] == '$') {
					arr[i][j] = '\0';
					snprintf(arr[i], 256, "%s%d", arr[i], pid);
				}
			}
		}
		// Next!
		token = strtok(NULL, space);
	}
}



/*
			MAIN
							*/

int main() 
{

	int pid = getpid();
	int i, fgbg_status, exitStatus = 0;
	char input_file[256];
	char output_file[256];
	char *command_line[MAXARG];
	struct sigaction sa_sigint = {0};
	struct sigaction sa_sigtstp = {0};

	//Signal handler for CTRL-C
	//Reference lecture 3.2
	sa_sigint.sa_handler = SIG_IGN;
	sigfillset(&sa_sigint.sa_mask);
	sa_sigint.sa_flags = 0;
	sigaction(SIGINT, &sa_sigint, NULL);

	
	//Signal handler for CTRL-Z
	//Reference lecture 3.2
	//SIGTSTP lecture 3.3
	sa_sigtstp.sa_handler = signalStop;
	sigfillset(&sa_sigtstp.sa_mask);
	sa_sigtstp.sa_flags = 0;
	sigaction(SIGTSTP, &sa_sigtstp, NULL);

	while(1)
	{
		memset(input_file, '\0', sizeof(input_file));
		memset(output_file, '\0', sizeof(output_file));
		memset(command_line, '\0', sizeof(command_line));
	
		getInput(command_line, &fgbg_status, input_file, output_file, pid);

		//determine the first argument from command line
		if (*command_line[0] == '#' || *command_line[0] == '\0') 
		{
			;	
		}else if (strcmp(command_line[0], "exit") == 0) 
		{
			exit(0);
		}else if (strcmp(command_line[0], "cd") == 0) 
		{
			if (command_line[1])
			{
				if (chdir(command_line[1]) == -1)
				{
					printf("Directory not found.\n");
					fflush(stdout);
				}
			} else 
			{
				chdir(getenv("HOME"));
			}
		}
		else if (strcmp(command_line[0], "status") == 0) {
			statusCommand(exitStatus);
		}

		// Anything else
		else {
			shellCommand(command_line, &exitStatus, sa_sigint, &fgbg_status, input_file, output_file);
		}

	
		fgbg_status = FOREGROUND;
		input_file[0] = '\0';
		output_file[0] = '\0';

	}
	
	return 0;
}

/***************************************************************
 * 			void getInput(char*[], int, char*, char*, int)
 *
 * Prompts the user and parses the input into an array of words
 *
 * INPUT:
 *		char*[512] arr		The output array
 *		int* fgbg_status	Is this a fgbg_status process? boolean
 *		char* inputName	Name of an input file
 *		char* outputName	Name of an output file
 *		int pid				PID of smallsh
 *
 *	OUTPUT:
 *		void - n/a
 ***************************************************************/

/***************************************************************
 * 			void shellCommand(char*[], int*, struct sigaction, int*, char[], char[])
 *
 * Executes the command parsed into arr[]
 *
 * INPUT:
 * 	char* arr[]				The array with command information
 *		int* childExitStatus	The success status of the command
 *		struct sigaction sa	The sigaction for SIGINT
 *		int* fgbg_status		Is it a fgbg_status process? boolean
 *		char inputName[]		The name of the input file
 *		char outputName[]		The name of the output file
 *
 * OUTPUT:
 *		void - n/a
***************************************************************/

/***************************************************************
 * 			void catchSIGTSTP(int)
 *
 * When SIGTSTP is called, toggle the run_bg boolean.
 *
 * I didn't know how to pass my own variables into this, so I made
 * run_bg a global variable. I know that's bad form.
 *
 * INPUT:
 * 	int signo		Required, according to the homework. Isn't used.
 *
 * OUTPUT:
 * 	void - n/a
***************************************************************/


/***************************************************************
 * 			void statusCommand(int)
 *
 * Calls the exit status
 *
 * INPUT:
 * 	int childExitMethod	Child Exit Method
 *
 * OUTPUT:
 * 	void - n/a
***************************************************************/



/***************************************************************
 * Filename: smallsh.c
 * Author: Brent Irwin
 * Date: 21 May 2017
 * Description: Main file for smallsh, CS 344 Program 3
***************************************************************/