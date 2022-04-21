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
#include <stdbool.h>

#define BUFF_SIZE 1024
#define MAX_ARGS  20
#define ARG_SIZE  20

void terminate(int signum);

typedef int operations[7];

typedef enum{
  NOP,
  BCOMPRESS,
  BDECOMPRESS,
  GCOMPRESS,
  GDECOMPRESS,
  ENCRYPT,
  DECRYPT
} operationType;

bool parseConfig(char* buffer, operations operations)
{
    char* token =  strtok(buffer," ");

    while (token != NULL)
    {
      int tokenNumber = atoi(strtok(NULL,"\n"));
      if(!strcmp(token,"nop"))
      {
          operations[NOP] = tokenNumber;
      }
      else if(!strcmp(token,"bcompress"))
      {
          operations[BCOMPRESS] = tokenNumber;
      }
      else if(!strcmp(token,"bdecompress"))
      {
        operations[BDECOMPRESS] = tokenNumber;
      }
      else if(!strcmp(token,"gcompress"))
      {
          operations[GCOMPRESS] = tokenNumber;
      }
      else if(!strcmp(token,"gdecompress"))
      {
          operations[GDECOMPRESS] = tokenNumber;
      }
      else if(!strcmp(token,"encrypt"))
      {
          operations[ENCRYPT] = tokenNumber;
      }
      else if(!strcmp(token,"decrypt"))
      {
          operations[DECRYPT] = tokenNumber;
      } else return false;

      token = strtok(NULL," ");
    }

    return true;
}

bool startUp(char* config, char* execPath, operations operations)
{
    bool ret = false;
    char buffer[128];
    int fd;
    if((fd = open(config, O_RDONLY)) >= 0)
    {
        ssize_t n = read(fd,buffer,sizeof(buffer));

        char *aux_buff = malloc(n*sizeof(char));
        strncpy(aux_buff, buffer, n);

        ret = parseConfig(aux_buff, operations);

        free(aux_buff);
    }
    else ret = false;

    ret = ((fd = open(execPath,O_RDONLY)) >= 0);
    close(fd);

    return ret;
}

int main(int argc, char** argv)
{
    signal(SIGINT, terminate);
    signal(SIGTERM, terminate);

    char *aux_message = "";
    operations operations;
    //validates input before starting server
    if(!startUp(argv[1],argv[2], operations)) {
      aux_message = "Input not valid.\n";
      write(STDERR_FILENO, aux_message, strlen(aux_message));
      return 1;
    }

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

        while (token != NULL) 
        {
            num_args++;

            //Reallocate space for your argument list
            args = realloc(args, num_args * sizeof(*args));

            //Copy the current argument
            args[num_args - 1] = malloc(strlen(token) + 1); /* The +1 for length is for the terminating '\0' character */
            snprintf(args[num_args - 1], strlen(token) + 1, "%s", token);// O DUARTE NAO GOSTA

            printf("%s\n", token);
            token = strtok(NULL, " ");
        }

        // Store the last NULL pointer
        num_args++;
        args = realloc(args, num_args * sizeof(*args));
        args[num_args - 1] = NULL;
        //strcat(execs_path,args[0]);

        //execv(sdstore,args);
        execvp(args[0], args);

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

void terminate(int signum)
{
    unlink("tmp/pipCli");
    unlink("tmp/pip");
    pid_t p = getpid();
    kill(p, SIGQUIT);
}
