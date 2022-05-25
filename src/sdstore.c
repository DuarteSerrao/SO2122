/*******************************************************************************
SISTEMAS OPERATIVOS
PROJECT:    SDSTORE-TRANSF
MODULE:     CLIENT
PURPOSE:    Send requests to server and wait for response
DEVELOPERS: a83630, Duarte Serrão
            a84696, Renato Gomes
            a71074, Sebastião Freitas
*******************************************************************************/
#include <fcntl.h>
#include <stdio.h>  
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <sys/time.h>
#include <time.h>

#define BUFF_SIZE 1024

void terminate(int signum){

	pid_t p = getpid();

    char pidStr[BUFF_SIZE] = "";
    sprintf(pidStr, "%d", getpid());
    unlink(pidStr);
	
    kill(p, SIGQUIT);
}

void loader();

int main(int argc, char** argv)
{

    loader();

    signal(SIGINT, terminate);
    signal(SIGQUIT, terminate);
    signal(SIGTERM, terminate);

    char buff[BUFF_SIZE] = "";
    strcpy(buff, "");
    char *auxMessage = "";
    int argsSize = 0;

    

    sprintf(buff, "%d", getpid());
    char fifo[30];
    strcpy(fifo, "tmp/");
    strcat(fifo, buff);

    argsSize += strlen(buff)+1;

    //Iterating through all arguments and concatenating them into a buffer
    //This way, we dont make a lot of system calls and send everything together
    
    for (int i = 1; i < argc; i++)
    {
        argsSize += strlen(argv[i])+1;
        strcat(buff," ");
        strcat(buff, argv[i]);
    }

    buff[strlen(buff)] = '\0';

    //Opening [Client -> Server] pipe and verifying if the server is ready
    int server = open("tmp/pipCliServ", O_WRONLY | O_TRUNC);
    if (server < 0)
    {
        auxMessage = "Server offline\n";
        write(STDERR_FILENO, auxMessage, strlen(auxMessage));
        close(server);
        return 1;
    }

    mkfifo(fifo, 0644);


    //sending them through the [Client -> Server] pipe and closing it afterwards
    write(server, buff, argsSize);

	close(server);

    int fd = open(fifo, O_RDONLY | O_NONBLOCK);



    //SETTING TIMEOUT

    // Initialize file descriptor sets
    fd_set read_fds, write_fds, except_fds;
    FD_ZERO(&read_fds);
    FD_ZERO(&write_fds);
    FD_ZERO(&except_fds);
    FD_SET(fd, &read_fds);

   

    
    bool listening = true;
    while(listening)
    {   
         // Set timeout to 30 seconds
        struct timeval timeout;
        timeout.tv_sec = 30;
        timeout.tv_usec = 0;
        
        int p = select(fd +1, &read_fds, &write_fds, &except_fds, &timeout);
        if (p > 0)
        {
            int n = read(fd, buff, BUFF_SIZE);
            buff[n] = '\0';
            write(STDOUT_FILENO, buff, n);
            if(access(fifo, F_OK) != 0) listening = false;
        }
        else
        {
            auxMessage = "Connection to server not possible at the time.\n";
            write(STDERR_FILENO, auxMessage, strlen(auxMessage));
            close(fd);
            unlink(fifo);
            return 1;
        }
    }

    close(fd);
    
    return 0;
}


void loader()
{

    if(fork() == 0)
    {
        exit(0);
    }
    int status;
    waitpid(0, &status, 0);

    srand(time(NULL));  
    int randomnumber;
    randomnumber = rand() % 10 + 1;    // Returns a pseudo-random integer between 0 and RAND_MAX.
    usleep(randomnumber);
}
