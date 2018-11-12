/*
 Name: Jose-Antonio D. Rubio
 OSUID: 932962915
 Class: 344-400
 Program 3 smallsh.c
 Comment: 
*/

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
    takes in integer and will determine if the it was an exit value or a termination signal and print it to the screen
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
 	signalStop    
DESCRIPTION
    Checks to see if background processing is allowed or no and will start
    or finish foreground-only and write to stdout
RESOURCE
       Lecture 3.3 Signals
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
    shellCOmmand
DESCRIPTION
    takes in the command given to the program 
    forks
    child processes will open a file to read from and open a file to write to and then close file
    parent will wait for child process to finish and report out
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
    commandLine
DESCRIPTION
    takes in the command passed to the program
    will see if redirection is needed, if background process will run, or if a name with PID is needed
RESOURCE
    https://www.geeksforgeeks.org/snprintf-c-library/
    https://stackoverflow.com/questions/2693776/removing-trailing-newline-character-from-fgets-input
*/
void commandLine(char* arr[], int* fgbg_status, char read_file[], char write_file[], int pid) 
{
	
	char input[MAXLENGTH];
	char *source_buffer;
	int i, j;
	const char space[2] = " ";


	printf(": ");
	fflush(stdout);
	fgets(input, MAXLENGTH, stdin);

	//remove the newline added by fgets
	input[strcspn(input, "\n")] = '\0';

	char *curr_arg = strtok(input, space);

	for (i=0; curr_arg; i++) 
	{
		if (strcmp(curr_arg, "<") == 0) 
		{
			//< denotes input
			curr_arg = strtok(NULL, space);
			strcpy(read_file, curr_arg);
		}else if (strcmp(curr_arg, ">") == 0) 
		{
			//> denotes output
			curr_arg = strtok(NULL, space);
			strcpy(write_file, curr_arg);
		} else 	if (strcmp(curr_arg, "&") == 0) 
		{
			//set to background 
			*fgbg_status = BACKGROUND;
		}else 
		{
			arr[i] = strdup(curr_arg);
			source_buffer = strdup(curr_arg);
		
			for (j=0; arr[i][j]; j++) 
			{
				if (arr[i][j] == '$' && arr[i][j+1] == '$') 
				{
					source_buffer[j] = '\0';
					snprintf(arr[i], MAXLENGTH, "%s%d", source_buffer, pid);
				}
			}
		}
		curr_arg = strtok(NULL, space);
	}
}



/*
			MAIN
							*/

int main() 
{

	int pid = getpid();
	int i, fgbg_status, exitStatus = 0;
	char input_file[MAXLENGTH];
	char output_file[MAXLENGTH];
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
	
		commandLine(command_line, &fgbg_status, input_file, output_file, pid);

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
		}else if (strcmp(command_line[0], "status") == 0) 
		{
			statusCommand(exitStatus);
		}else 
		{
			shellCommand(command_line, &exitStatus, sa_sigint, &fgbg_status, input_file, output_file);
		}

	
		fgbg_status = FOREGROUND;
		input_file[0] = '\0';
		output_file[0] = '\0';

	}
	
	return 0;
}
