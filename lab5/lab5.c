#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

void signaler(pid_t* pid_ary, int size, int signal);

int main(int argc, char* argv[])
{
	

	if (argc < 2){

		printf("Input: ./lab5 <num_processes>\n");
		exit(-1);

	}

	// initialization of pid like lab 4

	int size = atoi(argv[1]);

	pid_t *pid_ary = (pid_t *)malloc(sizeof(pid_t) * size);

	char *args[] = {"./iobound", "-seconds", "4", 0};

	// initialize sigset
	sigset_t sigset;
	int sig;

	// create an empty sigset_t

	// use sigaddset() to add the SIGUSR1 signal to the set

	// use sigprocmask() to add the signal set in the sigset for blocking

	
	for(int i = 0; i < size; i++)
	{

		pid_ary[i] = fork();

		if(pid_ary[i] == 0)
		{
			// print: Child Process: <pid> - Waiting for SIGUSR1…

			// wait for the signal
			
			// print: Child Process: <pid> - Received signal: SIGUSR1 - Calling exec().

			// call execvp with ./iobound like in lab 4

			exit(0);

		}
		else if (pid_ary[i] < 0) {
			// fork failed error
			exit(-1);
		}
	}
	
	// send SIGUSR1 

	// send SIGSTOP 

	// send SIGCONT

	// send SIGINT



	free(pid_ary);
	
	return 0;
}

void signaler(pid_t* pid_ary, int size, int signal)
{
	// sleep for three seconds

	for(int i = 0; i < size; i++)
	{
		// print: Parent process: <pid> - Sending signal: <signal> to child process: <pid>

		// send the signal


	}
}



