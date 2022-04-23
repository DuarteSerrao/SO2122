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

//COMMON VALUES
#define BUFF_SIZE 1024
#define MAX_ARGS  20
#define ARG_SIZE  20


//NEW TYPES
typedef int operations[7];

typedef enum
{
  NOP,
  BCOMPRESS,
  BDECOMPRESS,
  GCOMPRESS,
  GDECOMPRESS,
  ENCRYPT,
  DECRYPT,
  NONE
} operationType;

const static struct {
    operationType  val;
    const char    *str;
} conversion [] = {
    {NOP,         "nop"},
    {BCOMPRESS,   "bcompress"},
    {BDECOMPRESS, "bdecompress"},
    {GCOMPRESS,   "gcompress"},
    {GDECOMPRESS, "gdecompress"},
    {ENCRYPT,     "encrypt"},
    {GDECOMPRESS, "gdecompress"},
    {DECRYPT,     "decrypt"}
};


//FUNCTIONS INDEX
char**        parseArgs   (char *buff);
bool          parseConfig (char *buffer, operations operations);
bool          startUp     (char *configFile, char *execPath, operations operations);
operationType strToOpType (const char *str);
void          terminate   (int signum);
bool          testPath    (char *path);

/*******************************************************************************
FUNCTION: Main function that will control the whole flow of the server
*******************************************************************************/
int main(int argc, char **argv)
{
    signal(SIGINT, terminate);
    signal(SIGTERM, terminate);

    char *auxMessage = "";
    operations operations;
    //validates input before starting server
    if((argc != 3) || (!startUp(argv[1], argv[2], operations))) {
      auxMessage = "Input not valid.\n";
      write(STDERR_FILENO, auxMessage, strlen(auxMessage));
      return 1;
    }

     char *execsPath = argv[2];

    auxMessage = "Server starting...\n";
    write(STDOUT_FILENO, auxMessage, strlen(auxMessage));


    //Making pipes
    mkfifo("tmp/pipServCli", 0644);
    mkfifo("tmp/pipCliServ", 0644);

    auxMessage = "Pipes created...\n";
    write(STDOUT_FILENO, auxMessage, strlen(auxMessage));

    auxMessage = "Listening...\n";
    write(STDOUT_FILENO, auxMessage, strlen(auxMessage));

    //Loop that will constantly listen for new requests
    while(1)
    {
        //Opening pipe [Client -> Server]
        int input = open("tmp/pipCliServ", O_RDONLY);
        char buff[BUFF_SIZE] = "";

        

        //Getting the size of what was actually read and copying it to an aux
        ssize_t n = read(input, buff, BUFF_SIZE);
        char *auxBuff = malloc(n*sizeof(char));
        strncpy(auxBuff, buff, n);

        char **args = parseArgs(auxBuff);

        strcat(execsPath,args[0]);

        //execv(sdstore,args);

        free(auxBuff);
        close(input);

        /*char *srcFile = args[0];
        char *destFile = args[1];
        testPath(char *path);
        for(i){
        path = concat
        exec(path, {arg[i], scr, dest})}*/

        //Opening pipe [Server -> Client]
        int cliente = open("tmp/pipServCli", O_WRONLY);
        auxMessage = "O seu pedido foi processado\n";
        write(cliente, auxMessage, strlen(auxMessage));
        close(cliente);
    }
  return 0;
}

/*******************************************************************************
FUNCTION: Gets all the arguments from the request
*******************************************************************************/
char** parseArgs(char *buff)
{
    char **args = NULL;
    char *token = "";
    unsigned int numArgs = 0;
    token = strtok(buff," ");
        //if(!strcmp())

        while (token != NULL)
        {
            numArgs++;

            //Reallocate space for your argument list
            args = realloc(args, numArgs * sizeof(*args));

            //Copy the current argument
            args[numArgs - 1] = malloc(strlen(token) + 1); /* The +1 for length is for the terminating '\0' character */
            snprintf(args[numArgs - 1], strlen(token) + 1, "%s", token);// O DUARTE NAO GOSTA

            printf("%s\n", token);
            token = strtok(NULL, " ");
        }

        // Store the last NULL pointer
        numArgs++;
        args = realloc(args, numArgs * sizeof(*args));
        args[numArgs - 1] = NULL;
        return args;
}


/*******************************************************************************
FUNCTION: Gracefully exists the program
*******************************************************************************/
void terminate(int signum)
{
    unlink("tmp/pipCli");
    unlink("tmp/pip");
    pid_t p = getpid();
    kill(p, SIGQUIT);
}


/*******************************************************************************
FUNCTION: Checks if a path really existes and is accessable.
*******************************************************************************/
bool testPath(char *path)
{
    int fd;
    bool ret = ((fd = open(path,O_RDONLY)) >= 0);
    close(fd);
    return ret;
}

/*******************************************************************************
FUNCTION: From a string, it check if it is part of the enum operationType
*******************************************************************************/
operationType strToOpType (const char *str)
{
    for (int i = 0;  i < sizeof (conversion) / sizeof (conversion[0]);  ++i)
        if (!strcmp (str, conversion[i].str))
        {
            return conversion[i].val;   
        } 
    return NONE;
}


/*******************************************************************************
FUNCTION: Parses the configuration file and saves the max number of every opera-
          tion type occurences
*******************************************************************************/
bool parseConfig(char *buffer, operations operations)
{
    char *token =  strtok(buffer," ");

    while (token != NULL)
    {
        operationType type = strToOpType(token);
        if(type != NONE)
        {
            operations[type] = atoi(strtok(NULL,"\n"));
        }
        else return false;

      token = strtok(NULL," ");
    }

    return true;
}


/*******************************************************************************
FUNCTION: Prepares basic server startup information and checks if everything is
          as it should
*******************************************************************************/
bool startUp(char *configFile, char *execPath, operations operations)
{
    char buffer[128];
    int fd;

    if((fd = open(configFile, O_RDONLY)) >= 0)
    {
        ssize_t n = read(fd,buffer,sizeof(buffer));

        char *auxBuff = malloc(n*sizeof(char));
        strncpy(auxBuff, buffer, n);

        if(!parseConfig(auxBuff, operations)) return false;

        free(auxBuff);
    }
    else return false; 

    return testPath(execPath);
}