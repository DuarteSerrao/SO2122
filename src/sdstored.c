/*******************************************************************************
SISTEMAS OPERATIVOS
PROJECT:    SDSTORE-TRANSF
MODULE:     SERVER
PURPOSE:    Get request(s) from client(s) and process them
DEVELOPERS: a83630, Duarte Serrão
            a84696, Renato Gomes
            a71074, Sebastião Freitas
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
#define MAX_OPS   7


//NEW TYPES
int maxOperations[MAX_OPS];
int operations[MAX_OPS];

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
bool          checkOps    (char *args[], int *opsCounter);
char**        parseArgs   (char *buff);
bool          parseConfig (char *buffer);
void          procFileFunc(char **args, char *execsPath);
void          sendMessage (int output, char *message);
bool          startUp     (char *configFile, char *execsPath);
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

    //We need to get absolute path for execv
    char *execsPath = malloc((strlen(getenv("PWD")) + strlen(argv[2]) + 2) * sizeof(char));
    strcpy(execsPath, getenv("PWD"));
    strcat(execsPath, "/");
    strcat(execsPath, argv[2]);

    //validates input before starting server
    if((argc != 3) || (!startUp(argv[1], execsPath))) {
        sendMessage(STDERR_FILENO, "Input not valid. Needs a valid configuration file and a relative path to executables\n");
        return 1;
    }



    sendMessage(STDOUT_FILENO, "Server starting...\n");


    //Making pipes
    mkfifo("tmp/pipServCli", 0644);

    sendMessage(STDOUT_FILENO, "Recieving pipe created...\nListening...\n---------------------------------------\n");

    //Loop that will constantly listen for new requests
    while(1)
    {
        //Opening pipe [Client -> Server]
        mkfifo("tmp/pipCliServ", 0644);
        int input = open("tmp/pipCliServ", O_RDONLY);
        if(input < 0)
        {
            sendMessage(STDERR_FILENO, "Couldn't open pipe Client->Server.\n---------------------------------------\n");
            return 2;
        }

        char buff[BUFF_SIZE] = "";

        //Getting the size of what was actually read
        ssize_t n = read(input, buff, BUFF_SIZE);
        buff[n] = '\0';
        sendMessage(STDOUT_FILENO, "Request received from client\n");        

        char **args = parseArgs(buff);

        close(input);

        //Opening pipe [Server -> Client]
        int client = open("tmp/pipServCli", O_WRONLY);
        if(client < 0)//In case we can't communicate back with client, we choose not to continue the request
        {
            sendMessage(STDERR_FILENO, "Couldn't open pipe Server->Client.\n");
            continue;
        }

        
        if(!strcmp(args[TYPE], "proc-file"))
        {
            sendMessage(STDOUT_FILENO, "Request type: PROCESS FILE\n");
            sendMessage(client, "Your request will be processed now\n");

            int opsCounter[MAX_OPS];
            if(!checkOps(args+ARGS, opsCounter))
            {
                sendMessage(client, "Full Capacity. Try again :)\n");
                sendMessage(STDOUT_FILENO, "---------------------------------------\n");
            }
            else
            {
                for(int i = 0; i < MAX_OPS; i++) operations[i] += opsCounter[i];

                //Since procFile will mess with STDIN and STDOUT
                if(fork() == 0) procFileFunc(args, execsPath);

                int status;
                waitpid(0, &status, 0);

                for(int i = 0; i < MAX_OPS; i++) operations[i] -= opsCounter[i];

                sendMessage(STDOUT_FILENO, "Request processed\n---------------------------------------\n");
                sendMessage(client, "Your request was processed\n");
            }

            
        }
        else if(!strcmp(args[TYPE], "status"))
        {
            sendMessage(STDOUT_FILENO, "Request type: STATUS\n---------------------------------------\n");
            sendMessage(client, statusFunc ());
        }
        else 
        {
            sendMessage(STDOUT_FILENO, "Request type: ERROR\n---------------------------------------\n");
            sendMessage(client, "Options available: 'proc_file' or 'status'\n");
        }
        close(client);
        unlink("tmp/pipServCli");
    }
  return 0;
}

/*******************************************************************************
FUNCTION: Send message from a literal string to an output
*******************************************************************************/
void sendMessage(int output, char *message)
{
    write(output, message, strlen(message));
}


/*******************************************************************************
FUNCTION: 
*******************************************************************************/
bool checkOps(char *args[], int *opsCounter)
{

    for(int i = 0; i < MAX_OPS; i++) opsCounter[i] = operations[i];

    for(int i = 0; args[i] != NULL; i++)
    {
        operationType type = strToOpType(args[i]);
        opsCounter[type]++;
    }

    for(int i = 0; i<MAX_OPS; i++) 
        if(opsCounter[i] > maxOperations[i])
        {
            opsCounter = NULL;
            return false;
        }

    return true;
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
void procFileFunc(char **args, char* execsPath)
{    
    int i = ARGS;
    int fd[2];
    if(pipe(fd) == -1)
    {
        sendMessage(STDERR_FILENO, "pipe failed :/\n");
        return;
    }

    int input = open(args[SRC_FILE], O_RDONLY);
    if(input < 0) return;

    dup2(input, STDIN_FILENO);
    close(input);


    for(i = ARGS; args[i+1]!=NULL; i++)
    {
        
        //Preparing command to send through the exec
        char path[BUFF_SIZE] = "";
        strcpy(path, execsPath);
        strcat(path, "/");
        strcat(path,args[i]);

        //Since exec will substitute this process if successful, we need to encase it in an new process
        int child_pid = fork();
        if(child_pid == 0)
        {
           
            dup2(fd[1], STDOUT_FILENO);
            close(fd[0]);
            close(fd[1]);
            execl(path, path, NULL);
            //execlp("ls", "ls", NULL);
            sendMessage(STDERR_FILENO, "Failed to execute\n");
            exit(0);
        }
        else
        {

            int status;
            waitpid(child_pid, &status, 0);
            printf("here aahhh\n");
            dup2(fd[0], STDIN_FILENO);
            close(fd[1]);
            close(fd[0]);
        }
        
    }
    close(fd[0]);
    close(fd[1]);
    
    int output = open(args[DEST_FILE], O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if(output < 0) return;

    dup2(output, STDOUT_FILENO);

    char path[BUFF_SIZE] = "";
    strcpy(path, execsPath);
    strcat(path, "/");
    strcat(path,args[i]);
    execl(path, path, NULL);
    //execlp("ls", "ls", NULL);
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
bool parseConfig(char *buffer)
{
    char *token =  strtok(buffer," ");

    while (token != NULL)
    {
        operationType type = strToOpType(token);
        if(type != NONE)
        {
            maxOperations[type] = atoi(strtok(NULL,"\n"));
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
bool startUp(char *configFile, char *execsPath)
{
    char buffer[128];
    int fd;

    if((fd = open(configFile, O_RDONLY)) >= 0)
    {
        ssize_t n = read(fd,buffer,sizeof(buffer));

        char *auxBuff = malloc(n*sizeof(char));
        strncpy(auxBuff, buffer, n);

        if(!parseConfig(auxBuff)) return false;

        free(auxBuff);
    }
    else return false; 

    return testPath(execsPath);
}