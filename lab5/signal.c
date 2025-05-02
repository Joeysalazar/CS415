#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <signal.h>

void alarm_handler(int sig);

pid_t child_process;

int main(){

	child_process = fork();		// creates a child

	char *args[] = {"ls", "-a", NULL};
	
	if (child_process < 0){
			fprintf(stderr, "fork failed\n");
			exit(-1);
		}
	else if (child_process == 0){		// child process

			sigset_t sig_set;			// define sig_set
			sigemptyset(&sig_set);		// init empty set
			sigaddset(&sig_set, SIGUSR1);	// add SIGUSR 1 to the set

			sigprocmask(SIG_BLOCK, &sig_set, NULL);		// block (delay acting on) all signals in the set. prevents signal from being acted
														// on before we get to sigwait().

			int sig;
			printf("CHILD WAITING ON SIGUSR1 SIGNAL...\n");
			sigwait(&sig_set, &sig);		// wait until it receives a signal in the set
			execvp(args[0], args);
			exit(0);

		}
	else {		// parent process

		signal(SIGALRM, alarm_handler);		// registers the alarm signal w the alarm handler function

		alarm(3);
	}

	// while (wait(NULL) > 0);		// waits for all children to finish

	// waitpid(pid_ary[i], NULL, 0);	// waits on specific process

	wait(NULL);					// waits on first child to finish

}

void alarm_handler(int sig){

		printf("ALARM WENT OFF. PARENT SENDING SIGUSR1 SIGNAL...\n");
		printf("[%d] - ALARM SIG NUM: %d\n", sig, SIGALRM);
		
		kill(child_process, SIGUSR1);

		alarm(1); // restart the alarm

}










