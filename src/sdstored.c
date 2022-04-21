/*******************************************************************************
SISTEMAS OPERATIVOS
PROJECT:    SDSTORE-TRANSF
MODULE:     SERVER
PURPOSE:    Get request(s) from client(s) and process them
DEVELOPERS: a83630, Duarte Serrão
            axxxxx, Renato
            axxxxx, Sebastião
*******************************************************************************/
#include <fcntl.h>
#include <stdio.h>      //Tirar isto mais tarde
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#define BUFF_SIZE 1024
#define MAX_ARGS  20
#define ARG_SIZE  20

void terminate(int signum)
{
    unlink("tmp/pipCli");
    unlink("tmp/pip");
    pid_t p = getpid();
    kill(p, SIGQUIT);
}


int main(int argc, char** argv)
{
    //signal(SIGINT, terminate);
    //signal(SIGTERM, terminate);


    const char *execs_path = "./SDStore-transf/";
    char *aux_message = "";

    
    aux_message = "Server starting...\n";
    write(STDOUT_FILENO, aux_message, strlen(aux_message));


    //Making pipes
    mkfifo("tmp/pipCli", 0644);
    mkfifo("tmp/pip", 0644);

    aux_message = "Pipes created...\n";
    write(STDOUT_FILENO, aux_message, strlen(aux_message));


    aux_message = "Listening...\n";
    write(STDOUT_FILENO, aux_message, strlen(aux_message));

    //Loop that will constantly listen for new requests
    while(1)
    {
        //Opening pipe [Client -> Server]
        int input = open("tmp/pip", O_RDONLY);
        char buff[BUFF_SIZE] = "";
        char **args = NULL;
        unsigned int num_args = 0;
        char *token = "";

        //Getting the size of what was actually read and copying it to an aux
        ssize_t n = read(input, buff, BUFF_SIZE);
        char *aux_buff = malloc(n*sizeof(char));
        strncpy(aux_buff, buff, n);

        token = strtok(aux_buff," ");  

        while (token != NULL) {
            num_args++;
        
            //Reallocate space for your argument list
            args = realloc(args, num_args * sizeof(*args));
        
            //Copy the current argument
            args[num_args - 1] = malloc(strlen(token) + 1); /* The +1 for length is for the terminating '\0' character */
            snprintf(args[num_args - 1], strlen(token) + 1, "%s", token);
        
            printf("%s\n", token);
            token = strtok(NULL, " ");
        }
        
        // Store the last NULL pointer
        num_args++;
        args = realloc(args, num_args * sizeof(*args));
        args[num_args - 1] = NULL;
        //strcat(execs_path,args[0]);

        //execv(sdstore,args);
        execvp("ls", args);
        
        free(aux_buff);
        close(input);


        //Opening pipe [Server -> Client]
        int cliente = open("tmp/pipCli", O_WRONLY);
        aux_message = "O seu pedido foi processado\n";
        write(cliente, aux_message, strlen(aux_message));
        close(cliente);
    }
  return 0;
}
