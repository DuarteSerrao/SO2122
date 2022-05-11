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
#include <signal.h>

//COMMON VALUES
#define BUFF_SIZE 1024
#define MAX_ARGS  20
#define ARG_SIZE  20
#define MAX_OPS   7
#define PIPE_IN   0
#define PIPE_OUT  1


//CUSTOM SIGNALS
#define SIG_SUCC      (SIGRTMIN+3)
#define SIG_GET_STATE (SIGRTMIN+4)
#define SIG_SET_OPS   (SIGRTMIN+5)


//NEW TYPES
int maxOperations[MAX_OPS];
int operations[MAX_OPS];

pid_t *queue;
int queueSize;


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
int           checkOps    (char *args[], int *opsCounter);
pid_t         getFirstElem();
void          gradChild   (int sig);
void          doRequest(char **args, int client, char *execsPath, pid_t ourFather, int fdOperations);
void          freeChild   (int sig);
char**        parseArgs   (char *buff);
bool          parseConfig (char *buffer);
bool          procFileFunc(char **args, char *execsPath);
void          putElem(pid_t pid);
void          sendMessage (int output, char *message);
bool          startUp     (char *configFile, char *execsPath);
void          statusFunc (char *message);
operationType strToOpType (const char *str);
void          terminate   (int signum);
bool          testPath    (char *path);
static void   handler(int sig, siginfo_t *si, void *uap);
void          sendMsgToProc(pid_t pid, char *message);
void          opsToStr(char *operationsMessage, int ops[MAX_OPS]);




/*******************************************************************************
FUNCTION: Main function that will control the whole flow of the server
*******************************************************************************/
int main(int argc, char **argv)
{
    queue = NULL;
    queueSize = 0;

    pid_t ourFather = getpid();

    signal(SIGINT, terminate);
    signal(SIGTERM, terminate);



    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = handler;
    sa.sa_flags = SA_SIGINFO | SA_RESTART | SA_NOCLDSTOP;



    sigaction(SIGCHLD, &sa, NULL);
    sigaction(SIG_SUCC, &sa, NULL);

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

    int fdOperations[2];
    char readbuffer[BUFF_SIZE];

    if(pipe(fdOperations) == -1){
        return 3; //error
    }

    //Loop that will constantly listen for comunication from children
    if(fork() == 0)
    {
        close(fdOperations[PIPE_OUT]);
        char auxMessage[2];
        
        while(true)
        {
            
            int n = read(fdOperations[PIPE_IN],readbuffer, BUFF_SIZE);
            printf("%d\n", n);
            for (int i = 1; i < n; i++)
            {
                auxMessage[0] = readbuffer[i];
                auxMessage[1] = '\0';

                sendMessage(STDERR_FILENO, auxMessage);
              
                if (readbuffer[0] == '+')       operations[i-1] += atoi(auxMessage); 
              
                else if (readbuffer[0] == '-')  operations[i-1] -= atoi(auxMessage); 
              
                else sendMessage(STDERR_FILENO, "Garbage collected\n");
            }
        }
    }

    //Loop that will constantly listen for new requests
    while(1)
    {
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

        buff[n] = '\0';



        pid_t pid = fork();

        if(pid < 0)
        {
            sendMessage(STDERR_FILENO, "Couldn't open fork for my son\n");     
        }
        else if (pid == 0) //Son
        {
            close(fdOperations[PIPE_IN]);
            //Opening pipe [Client -> Server]

            //printf("%s\n", buff);
            char **args = parseArgs(buff);

            char fifo[15];
            strcpy(fifo, "tmp/");
            strcat(fifo, args[ID]);
            strcat(fifo, "\0");

            

            //Opening pipe [Server -> Client]
            int client = open(fifo, O_WRONLY); //| O_NONBLOCK

            if(client < 0)//In case we can't communicate back with client, we choose not to continue the request
            {
                sendMessage(STDERR_FILENO, "Couldn't open pipe Server->Client.\n");
                continue;
            }

            doRequest(args, client, execsPath, ourFather, fdOperations[PIPE_OUT]);

            close(client);
            unlink(fifo);

            kill(ourFather, SIGCHLD);
            
            exit(0);
        }
        else //FATHER
        {
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
void doRequest(char **args, int client, char *execsPath, pid_t ourFather, int fdOperations)
{
    if(!strcmp(args[TYPE], "proc-file"))
    {
        sendMessage(STDOUT_FILENO, "Request type: PROCESS FILE\n");
        sendMessage(client, "Your request will be processed now\n");

        int opsCounter[MAX_OPS];
        char operationsMessage[BUFF_SIZE];
        pid_t imAChild = getpid();

        switch(checkOps(args+ARGS, opsCounter))
        {
        // It's impossible to ever do this request
        case -1:
            sendMessage(client, "Full Capacity. Try again :)\n");
            sendMessage(STDOUT_FILENO, "---------------------------------------\n");
            break;
        //It's possible, but not RIGHT NOW
        case 0:
            putElem(imAChild);

            sendMessage(STDERR_FILENO, "I failed :(\n");

            do{  kill(imAChild, SIGSTOP);  } while(!checkOps(args+ARGS, opsCounter));

            kill(ourFather, SIG_SUCC); //we can continue

        default:
            strcpy(operationsMessage,"+");
            opsToStr(operationsMessage, opsCounter);

            write(fdOperations, operationsMessage, strlen(operationsMessage));

            //Since procFile will mess with STDIN and STDOUT
            procFileFunc(args, execsPath);


            operationsMessage[0] = '-';
            write(fdOperations, operationsMessage, strlen(operationsMessage));

            sendMessage(STDOUT_FILENO, "Request processed\n---------------------------------------\n");
            sendMessage(client, "Your request was processed\n");
            break;
        }
    }
    else if(!strcmp(args[TYPE], "status"))
    {
        sendMessage(STDOUT_FILENO, "Request type: STATUS\n---------------------------------------\n");
        kill(ourFather, SIG_GET_STATE);
        char message[BUFF_SIZE];
        statusFunc (message);
        sendMessage(client, message);
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

    //Inicializing opsCounter
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
void statusFunc (char *message)
{
    int n = read(STDIN_FILENO, message, BUFF_SIZE);
    message[n]='\0';


    //Operations part
    char aux[3];
    for(int i = 0; i < MAX_OPS; i++)
    {
        strcat(message, "transf ");
        strcat(message, conversion[i].str);
        strcat(message, ": ");
        sprintf(aux, "%d", message[i]);
        strcat(message, aux);
        strcat(message, "/");
        sprintf(aux, "%d", maxOperations[i]);
        strcat(message, aux);
        strcat(message, " (running/max)\n");
    }
}


/*******************************************************************************
FUNCTION: Function for a 'process file' request
*******************************************************************************/
bool procFileFunc(char **args, char* execsPath)
{    
    sleep(3); //TIRAR DEPOIS

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
    free(queue);
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


/*******************************************************************************
FUNCTION: Takes first element of queue and returnes it
*******************************************************************************/
pid_t getFirstElem()
{
    if (queue == NULL) return -1;

    pid_t pid = queue[0];

    queue = &queue[1];
    queue = realloc(queue, sizeof(queue)-sizeof(pid_t));

    return pid;
}

/*******************************************************************************
FUNCTION: Adds element to end of queue
*******************************************************************************/
void putElem(pid_t pid)
{
    queueSize++;

    queue = realloc(queue, queueSize*sizeof(pid_t));

    queue[queueSize-1] = pid;
}


static void handler(int sig, siginfo_t *si, void *uap)
{
    if(si->si_code != SI_USER ) return;

    //When the parent process recieves signal that a child process was
    //successful in finding space for available execs, it will remove the 
    //first element of the queue (the one who succeeded) and try to
    //"continue process" the next request.
    if(sig == SIG_SUCC) getFirstElem();



    //When the parent process recieves signal that a child process finished,
    //it will try to force a "continue process" on the first pendent request
    if((sig == SIGCHLD || sig == SIG_SUCC)&& queue != NULL)
        kill(queue[0], SIGCONT);




    if(sig == SIG_GET_STATE && si->si_pid != getpid())
    {
        //The first MAX_OPS bytes are reserved for the operations availability
        char opsMSG[MAX_OPS];
        opsToStr(opsMSG, operations);
        sendMsgToProc(si->si_pid, opsMSG);
    }

}




void sendMsgToProc(pid_t pid, char *message)
{
    //pit to string
    char mypid[6];
    sprintf(mypid, "%d", pid);

    //Defining path of the stdin of process
    char path[40]="/proc/";
    strcat(path, mypid);
    strcat(path, "/fd/0");

    //Opening STDIN of process
    int procFD = open(path, O_WRONLY | O_TRUNC);

    sendMessage(procFD, message);
}

void opsToStr(char *operationsMessage, int ops[MAX_OPS])
{
    for(int i = 0; i < MAX_OPS; i++) 
    {
        char aux[2];
        sprintf(aux,"%d",ops[i]);
        strcat(operationsMessage,aux);
    }
}

void strToOps(char *operationsMessage, int ops[MAX_OPS])
{

}