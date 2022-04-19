#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>

//client

void terminate(int signum){
	pid_t p = getpid();
	kill(p, SIGQUIT);
}

int main(int argc, char** argv)
{
    signal(SIGINT, terminate);
    signal(SIGTERM, terminate);
		int server = open("tmp/pip", O_WRONLY);

    if (server < 0) {
        printf("Server offline\n");
				close(server);
        return 1;
    }

/*		if (argv[1] == NULL){
			char* tutorial = "Mensagem de tutorial\n";
			write(1,tutorial, strlen(tutorial));
			close(server);
			return 1;
		}*/

    for (int i = 1; i < argc; i++){
      write(server, argv[i], strlen(argv[i]));
    }
		close(server);

    int input = open("tmp/pipCli", O_RDONLY);
		int i = 0;

//this is bad :(
    while(i<2){
	      char buf[1024];
	      int n = read(input, buf, 1024);
	      if (n > 0){
						buf[n] = 0;
						n++;
						i++;
						write(1, buf, n);
	      }
    }
    close(input);

    return 0;
}
