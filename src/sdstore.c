/******************************************************************
* PROJECT:	SDStore
* MODULE:	CLIENT
* PURPOSE:	
* GROUP:	xx
* STUDENTS:	- Duarte Serrão, a83630
*			- Renato
*			- Sebastião
*******************************************************************/
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define BUFF_SIZE 1024

//client

void terminate(int signum);

int main(int argc, char** argv)
{
    signal(SIGINT, terminate);
    signal(SIGTERM, terminate);

    // Opening server pipeline for writing
    int server = open("tmp/pip", O_WRONLY);

    // If the server pipeline fails, then the program ends
    if (server < 0) {
        printf("Server offline\n");
        close(server);
        return 2;
    }

    // wot?
    if (argv[1] == NULL){
        char* tutorial = "Mensagem de tutorial\n";
        write(1,tutorial, strlen(tutorial));
        close(server);
        return 1;
    }

    //For each new argument, we need to send it through the pipeline
    for (int i = 1; i < argc; i++){
      write(server, argv[i], strlen(argv[i]));
    }

    //Closing server now, since we wont send anything more
    close(server);

    // Opening pipeline for recieving messages from the server
    int input = open("tmp/pipCli", O_RDONLY);
    
    // If the server pipeline fails, then the program ends
    if (input < 0) {
        printf("Couldn't make connection for recieving\n");
        close(input);
        return 2;
    }
    
    int i = 0;

    //this is bad :frowning:
    while(i<2){
          char buf[BUFF_SIZE];
          int n = read(input, buf, BUFF_SIZE);
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

void terminate(int signum){
    pid_t p = getpid();
    kill(p, SIGQUIT);
}