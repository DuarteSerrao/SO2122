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

enum requestArgIndex
{
    TYPE,
    SRC_FILE,
    DEST_FILE,
    ARGS
};


//FUNCTIONS INDEX
char**        parseArgs   (char *buff);
bool          parseConfig (char *buffer, operations operations);
void          procFileFunc(char **args);
bool          startUp     (char *configFile, char *execPath, operations operations);
char*         statusFunc  ();
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
        if(input < 0)
        {
            auxMessage = "Couldn't open pipe Client->Server.\n";
            write(STDERR_FILENO, auxMessage, strlen(auxMessage));
            return 2;
        }

        char buff[BUFF_SIZE] = "";

        //Getting the size of what was actually read and copying it to an aux
        ssize_t n = read(input, buff, BUFF_SIZE);

        auxMessage = "Request received from client\n";
        write(STDOUT_FILENO, auxMessage, strlen(auxMessage));

        char *auxBuff = malloc(n*sizeof(char));
        strncpy(auxBuff, buff, n);

        

        char **args = parseArgs(auxBuff);

        free(auxBuff);
        close(input);

        //Opening pipe [Server -> Client]
        int client = open("tmp/pipServCli", O_WRONLY);
        if(client < 0)//In case we can't communicate back with client, we choose not to continue the request
        {
            auxMessage = "Couldn't open pipe Server->Client.\n";
            write(STDERR_FILENO, auxMessage, strlen(auxMessage));
            continue;
        }

        
        if(!strcmp(args[TYPE], "proc_file"))
        {
            auxMessage = "Request type: PROCESS FILE\n";
            write(STDOUT_FILENO, auxMessage, strlen(auxMessage));
            procFileFunc(args);
            auxMessage = "O seu pedido foi processado\n";
        }
        else if(!strcmp(args[TYPE], "status"))
        {
            auxMessage = "Request type: STATUS\n";
            write(STDOUT_FILENO, auxMessage, strlen(auxMessage));
            auxMessage = statusFunc (); 
        }
        else 
        {
            auxMessage = "Request type: ERROR\n";
            write(STDOUT_FILENO, auxMessage, strlen(auxMessage));
            auxMessage = "Options available: 'proc_file' or 'status'\n";
        }

        write(client, auxMessage, strlen(auxMessage));
        close(client);
    }
  return 0;
}


/*******************************************************************************
FUNCTION: Function for a 'status' request
*******************************************************************************/
char* statusFunc ()
{
    //code here :D
    return "STATUS\n";
    
}


/*******************************************************************************
FUNCTION: Function for a 'process file' request
*******************************************************************************/
void procFileFunc(char **args)
{
    char *destFile = malloc((strlen(args[DEST_FILE])+3)*sizeof(char));
    strcpy(destFile, "<");
    strcat(destFile, args[DEST_FILE]);
    strcat(destFile, ">");

    //for(int i = ARGS; args[i]!=NULL; i++)
    //{
    //    char *path = "";
    //    strcpy(path, execsPath);
    //    strcat(path,args[i]);
    //    char *argsExec[3] = {args[i], args[SRC_FILE], destFile};
    //    if(fork() == 0) execv(path, argsExec);
    //    //else wait(&status);
    //}
    free(destFile);
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
    unlink("tmp/pipServCli");
    unlink("tmp/pipCliServ");
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