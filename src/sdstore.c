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
    signal(SIGINT, terminate);
    signal(SIGTERM, terminate);

    char buff[BUFF_SIZE] = "";
    char *auxMessage = "";
    int argsSize = 0;

    //Opening [Client -> Server] pipe and verifying if the server is ready
    int server = open("tmp/pipCliServ", O_WRONLY);
    if (server < 0)
    {
        auxMessage = "Server offline\n";
        write(STDERR_FILENO, auxMessage, strlen(auxMessage));
		close(server);
        return 1;
    }

    //Iterating through all arguments and concatenating them into a buffer
    //This way, we dont make a lot of system calls and send everything together
    for (int i = 1; i < argc; i++)
    {
        strcat(buff, argv[i]);
        strcat(buff," ");
        argsSize += strlen(argv[i])+1;
    }

    //sending them through the [Client -> Server] pipe and closing it afterwards
    write(server, buff, argsSize);
	close(server);

    int input = open("tmp/pipServCli", O_RDONLY);
    char buf[BUFF_SIZE];
    int n = read(input, buf, BUFF_SIZE);
    write(STDOUT_FILENO, buf, n);
    close(input);

    return 0;
}