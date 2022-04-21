/*******************************************************************************
SISTEMAS OPERATIVOS
PROJECT:    SDSTORE-TRANSF
MODULE:     CLIENT
PURPOSE:    Send requests to server and wait for response
DEVELOPERS: a83630, Duarte Serrão
            axxxxx, Renato
            axxxxx, Sebastião
*******************************************************************************/
#include <fcntl.h>
#include <stdio.h>  //Tirar isto mais tarde
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define BUFF_SIZE 1024

void terminate(int signum){
	pid_t p = getpid();
	kill(p, SIGQUIT);
}

int main(int argc, char** argv)
{
    //signal(SIGINT, terminate);
    //signal(SIGTERM, terminate);

    char buff[BUFF_SIZE] = "";
    char *aux_message = "";
    int args_size = 0;
	
    //Opening [Client -> Server] pipe and verifying if the server is ready
    int server = open("tmp/pip", O_WRONLY);
    if (server < 0) 
    {
        aux_message = "Server offline\n";
        write(STDERR_FILENO, aux_message, strlen(aux_message));
		close(server);
        return 1;
    }

    //Iterating through all arguments and concatenating them into a buffer
    //This way, we dont make a lot of system calls and send everything together
    for (int i = 1; i < argc; i++)
    {
        strcat(buff, argv[i]);
        strcat(buff," ");
        args_size += strlen(argv[i]); 
    }

    //sending them through the [Client -> Server] pipe and closing it afterwards
    write(server, buff, args_size);
	close(server);

    //int input = open("tmp/pipCli", O_RDONLY);
	//int i = 0;

    //this is bad :(
    //while(i<2){
    //    char buf[BUFF_SIZE];
    //    int n = read(input, buf, BUFF_SIZE);
    //    if (n > 0)
    //    {
    //    	buf[n] = 0;
	//		n++;
	//		i++;
	//		write(1, buf, n);
	//    }
    //}
    //close(input);

    return 0;
}
