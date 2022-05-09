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
#define PIPE_IN   0
#define PIPE_OUT  1


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
    ID,
    TYPE,
    SRC_FILE,
    DEST_FILE,
    ARGS
};


//FUNCTIONS INDEX
bool          checkOps    (char *args[], int *opsCounter);
void          doRequest   (char **args, int client, char *execsPath);
char**        parseArgs   (char *buff);
bool          parseConfig (char *buffer);
bool          procFileFunc(char **args, char *execsPath);
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
    mkfifo("tmp/pipCliServ", 0644);

    sendMessage(STDOUT_FILENO, "Recieving pipe created...\nListening...\n---------------------------------------\n");

    

    

    //Loop that will constantly listen for new requests
    while(1)
    {
        int fd[2];
        if(pipe(fd) == -1)
        {
            sendMessage(STDERR_FILENO, "pipe failed :/\n");
            return 3;
        }

        char buff[BUFF_SIZE] = "";
        
        int listener = open("tmp/pipCliServ", O_RDONLY);
        if(listener < 0)
        {
            sendMessage(STDERR_FILENO, "Couldn't open pipe Client->Server.\n---------------------------------------\n");
            return 2;
        }

        //Getting the size of what was actually read
        ssize_t n = read (listener, buff, BUFF_SIZE);
            
        sendMessage(STDOUT_FILENO, "Request received from client\n"); 
        printf("%s\n", buff);

        pid_t pid = fork();

        if(pid < 0)
        {
            sendMessage(STDERR_FILENO, "Couldn't open fork for my son\n");     
        }
        else if (pid == 0) //Son
        {
            close(fd[1]);

            //Opening pipe [Client -> Server]
            
            char buff[BUFF_SIZE] = "";
            int n = read(fd[0], buff, BUFF_SIZE);
            buff[n] = '\0';

            close(fd[0]);

            //printf("%s\n", buff);
            char **args = parseArgs(buff);

            char fifo[15];
            strcpy(fifo, "tmp/");
            strcat(fifo, args[ID]);
            strcat(fifo, "\0");

            

            //Opening pipe [Server -> Client]
            int client = open(fifo, O_WRONLY); //| O_NONBLOCK

            printf("%s\n", fifo);

            if(client < 0)//In case we can't communicate back with client, we choose not to continue the request
            {
                sendMessage(STDERR_FILENO, "Couldn't open pipe Server->Client.\n");
                continue;
            }

            doRequest(args, client, execsPath);

            close(client);
            unlink(fifo);
            
            exit(0);
        }
        else //FATHER
        {
            close(fd[0]);

            write(fd[1], buff, n);
            buff[0] = '\0';

            close(fd[1]);
            close(listener);

            //int status;
            //waitpid(0, &status, 0);
        }
        
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
void doRequest(char **args, int client, char *execsPath)
{
    if(!strcmp(args[TYPE], "proc-file"))
    {
        sendMessage(STDOUT_FILENO, "Request type: PROCESS FILE\n");
        sendMessage(client, "Your request will be processed now\n");

        int opsCounter[MAX_OPS];
        switch(checkOps(args+ARGS, opsCounter))
        {
        // It's impossible to ever do this request
        case -1:
            sendMessage(client, "Full Capacity. Try again :)\n");
            sendMessage(STDOUT_FILENO, "---------------------------------------\n");
            break;
        //It's possible, but not RIGHT NOW
        case 0:
            //Wait here for signal, i guess
            //No break, since we want to do the rest too
        default:
            for(int i = 0; i < MAX_OPS; i++) operations[i] += opsCounter[i];

            //Since procFile will mess with STDIN and STDOUT
            procFileFunc(args, execsPath);


            for(int i = 0; i < MAX_OPS; i++) operations[i] -= opsCounter[i];

            sendMessage(STDOUT_FILENO, "Request processed\n---------------------------------------\n");
            sendMessage(client, "Your request was processed\n");
            break;
        }
        else
        {
            
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
    
}


/*******************************************************************************
FUNCTION: 
*******************************************************************************/
int checkOps(char *args[], int *opsCounter)
{
    int retVal = 1;

    //
    for(int i = 0; i < MAX_OPS; i++) opsCounter[i] = 0;

    //
    for(int i = 0; args[i] != NULL; i++)
    {
        operationType type = strToOpType(args[i]);
        opsCounter[type]++;

        if(opsCounter[i] > maxOperations[i])
        {
            opsCounter = NULL;
            return -1;
        }
        if((operations[i] + opsCounter[i]) > maxOperations[i])
        {
            retVal = 0;
        }
    }
        
    return retVal;
}



/*******************************************************************************
FUNCTION: Function for a 'status' request
*******************************************************************************/
char* statusFunc ()
{
    
    return "STATUS\n";
}


/*******************************************************************************
FUNCTION: Function for a 'process file' request
*******************************************************************************/
bool procFileFunc(char **args, char* execsPath)
{    
    bool retVal = true;
    int **pipes = NULL; //We need n-1 pipes
    
    int input = open(args[SRC_FILE], O_RDONLY);
    if(input < 0) return false;

    //Saving copies of the original std I/O
    const int dupIN  = dup(STDIN_FILENO);
    const int dupOUT = dup(STDOUT_FILENO);

    //Starting comands with the input from file
    dup2(input, STDIN_FILENO);
    close(input);


    for(int i = ARGS; args[i]!=NULL && retVal; i++)
    {
        int j = i-ARGS;

        // Last iteration must write on file and it wont create a new pipe
        if(args[i+1] == NULL) 
        {
            int output = open(args[DEST_FILE], O_CREAT | O_WRONLY | O_TRUNC, 0644);
            if(output < 0)
            {
                sendMessage(STDERR_FILENO, "Couldn't open output file.\n");
                retVal = false;
                break;
            }
            dup2(output, STDOUT_FILENO);
            close(output);
        }
        //Creating new pipe with STDOUT as the output
        else
        {
            pipes = realloc(pipes, (j + 1)*sizeof(*pipes));
            pipes[j] = malloc(2*sizeof(int));

            if(pipe(pipes[j]) == -1)
            {
                sendMessage(STDERR_FILENO, "pipe failed :/\n");
                retVal = false;
                break;
            }
            dup2(pipes[j][PIPE_OUT], STDOUT_FILENO);
            close(pipes[j][PIPE_OUT]);
        }


        //Preparing command to send through the exec
        char path[BUFF_SIZE] = "";
        strcpy(path, execsPath);
        strcat(path,args[i]);


        int pid = fork();

        switch(pid)
        {
        //A FAILED CHILD
        case -1:
            sendMessage(STDERR_FILENO, "Failed to create fork()\n");
            retVal = false;
            break;
        //DAUGHTER
        case 0:
            //close(pipes[j][PIPE_IN]);
            execl(path, path, NULL);
            sendMessage(STDERR_FILENO, "Failed to execute\n");
            exit(0);
            break;
        //MOTHER
        default:
            if(args[i+1] != NULL) 
            {
                sendMessage(STDERR_FILENO, "heeere1/\n");
                dup2(pipes[j][PIPE_IN], STDIN_FILENO);    
                close(pipes[j][PIPE_IN]);
            }    
            
            //Waiting for exec to complete
            int status;
            waitpid(pid, &status, 0);
            break;
        }
    }

    free(pipes);

    dup2(dupIN,  STDIN_FILENO);
    close(dupIN);

    dup2(dupOUT, STDOUT_FILENO);
    close(dupOUT);

    return retVal;
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