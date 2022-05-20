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
#define PIPE_RD   0
#define PIPE_WR  1


//CUSTOM SIGNALS
#define SIG_SUCC    (SIGRTMIN+3)
#define SIG_FAIL    (SIGRTMIN+4)
#define SIG_SET_OPS (SIGRTMIN+5)


//GLOBAL VARS
int maxOperations[MAX_OPS];
int operations[MAX_OPS];

pid_t *queue;
int queueSize;

int fdOperations[2];

int client;
bool childContinues;


//NEW TYPES
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
} opType;

const static struct {
    opType
  val;
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
int         checkOps     (char *args[], int *opsCounter);
void        doRequest    (char **args, int client, char *execsPath, pid_t ourFather);
void        freeChild    (int sig);
pid_t       getFirstElem ();
void        gradChild    (int sig);
static void handlerChild (int sig);
static void handlerFather(int sig, siginfo_t *si, void *uap);
void        opsToStr     (char *opsMSG, int ops[MAX_OPS]);
char**      parseArgs    (char *buff);
bool        parseConfig  (char *buffer);
bool        procFileFunc (char **args, char *execsPath);
void        putElem      (pid_t pid);
void        sendMessage  (int output, char *message);
bool        setOps       (char *opsMSG);
bool        startUp      (char *configFile, char *execsPath);
void        statusFunc   (char *message);
opType      strToOpType  (const char *str);
void        terminate    (int signum);
bool        testPath     (char *path);




/*******************************************************************************
FUNCTION: Main function that will control the whole flow of the server
*******************************************************************************/
int main(int argc, char **argv)
{

    //------------------------------SIGNAL STUFF-----------------------------//

    signal(SIGINT, terminate);
    signal(SIGTERM, terminate);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = handlerFather;
    sa.sa_flags = SA_SIGINFO | SA_RESTART | SA_NOCLDSTOP;



    sigaction(SIGCHLD, &sa, NULL);
    sigaction(SIG_SET_OPS, &sa, NULL);

    signal(SIG_SUCC, handlerChild);
    signal(SIG_FAIL, handlerChild);


    pipe(fdOperations);
    



    //---------------------------------START UP------------------------------//

    queue = NULL;
    queueSize = 0;
    childContinues = false;

    pid_t ourFather = getpid();

    //Getting absolute path for execv
    char *execsPath = malloc(strlen(getenv("PWD")) + strlen(argv[2]) + 2);
    strcpy(execsPath, getenv("PWD"));
    strcat(execsPath, "/");
    strcat(execsPath, argv[2]);

    //validates input before starting server
    if((argc != 3) || (!startUp(argv[1], execsPath))) {
        sendMessage(STDERR_FILENO, "Input not valid. Needs a valid configuration file and a relative path to executables\n");
        return 1;
    }

    sendMessage(STDOUT_FILENO, "Server starting...\n");

    //Making named Client->Server pipe
    mkfifo("tmp/pipCliServ", 0644);

    sendMessage(STDOUT_FILENO, "Recieving pipe created...\nListening...\n---------------------------------------\n");



    //---------------------------------LISTENER------------------------------//

    while(1)
    {
        //sleep(0.1);
        
        //Opening pipe
        int listener = open("tmp/pipCliServ", O_RDONLY);
        if(listener < 0)
        {
            sendMessage(STDERR_FILENO, "Couldn't open pipe Client->Server.\n---------------------------------------\n");
            return 2;
        }

        char buff[BUFF_SIZE] = "";

        //Getting the size of what was actually read
        ssize_t n = read (listener, buff, BUFF_SIZE);


        printf("Current client input -> %s\n", buff);
            
        sendMessage(STDOUT_FILENO, "Request received from client\n---------------------------------------\n"); 

        buff[n] = '\0';

        pid_t pid = fork();

        if(pid < 0)
        {
            sendMessage(STDERR_FILENO, "Couldn't open fork for my son\n");     
        }
        else if (pid == 0) //Son
        {
            close(fdOperations[PIPE_RD]);
            //Opening pipe [Client -> Server]

            //printf("%s\n", buff);
            char **args = parseArgs(buff);

            char fifo[15];
            strcpy(fifo, "tmp/");
            strcat(fifo, args[ID]);
            strcat(fifo, "\0");


            //Opening pipe [Server -> Client]
            client = open(fifo, O_WRONLY); //| O_NONBLOCK

            if(client < 0)//In case we can't communicate back with client, we choose not to continue the request
            {
                sendMessage(STDERR_FILENO, "Couldn't open pipe Server->Client.\n");
                continue;
            }

            doRequest(args, client, execsPath, ourFather);

            close(client);
            unlink(fifo);

            kill(ourFather, SIGCHLD);
            
            exit(0);
        }
        
        

        close(listener);   
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
void doRequest(char **args, int client, char *execsPath, pid_t ourFather)
{
    if(!strcmp(args[TYPE], "proc-file"))
    {
        sendMessage(STDOUT_FILENO, "Request type: PROCESS FILE\n");
        sendMessage(client, "Your request will be processed now\n");

        int opsCounter[MAX_OPS];
        char opsMSG [BUFF_SIZE];
        pid_t imAChild = getpid();

        switch(checkOps(args+ARGS, opsCounter))
        {
        // It's impossible to ever do this request
        case -1:
            sendMessage(client, "Full Capacity. Try again :)\n");
            sendMessage(STDOUT_FILENO, "---------------------------------------\n");
            break;
        default:
            strcpy(opsMSG,"+");
            opsToStr(opsMSG, opsCounter);


            printf("pid antes do pipe-> %d\n", imAChild);

            if(queue != NULL) putElem(imAChild);

            while(!childContinues)
            {
                kill(ourFather, SIG_SET_OPS);
                write(fdOperations[PIPE_WR], opsMSG, strlen(opsMSG));
                kill(imAChild, SIGSTOP);
            }


            printf("opsmessage  depois do stop-> %s\n", opsMSG);

            //Since procFile will mess with STDIN and STDOUT
            procFileFunc(args, execsPath);


            opsMSG[0] = '-';


            
            kill(ourFather, SIG_SET_OPS);
            write(fdOperations[PIPE_WR], opsMSG, strlen(opsMSG));
            

            sendMessage(STDOUT_FILENO, "Request processed\n---------------------------------------\n");
            sendMessage(client, "Your request was processed\n");
            break;
        }
    }
    else if(!strcmp(args[TYPE], "status"))
    {
        sendMessage(STDOUT_FILENO, "Request type: STATUS\n---------------------------------------\n");
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
        opType
     type = strToOpType(args[i]);
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
    //Operations part
    char aux[3];
    for(int i = 0; i < MAX_OPS; i++)
    {
        strcat(message, "transf ");
        strcat(message, conversion[i].str);
        strcat(message, ": ");
        sprintf(aux, "%d", operations[i]);
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
    sleep(1); //TIRAR DEPOIS

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
            dup2(pipes[j][PIPE_WR], STDOUT_FILENO);
            close(pipes[j][PIPE_WR]);
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
            //close(pipes[j][PIPE_RD]);
            execl(path, path, NULL);
            sendMessage(STDERR_FILENO, "Failed to execute\n");
            exit(0);
            break;
        //MOTHER
        default:
            if(args[i+1] != NULL) 
            {
                dup2(pipes[j][PIPE_RD], STDIN_FILENO);    
                close(pipes[j][PIPE_RD]);
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
        snprintf(args[numArgs - 1], strlen(token) + 1, "%s", token);
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
FUNCTION: From a string, it check if it is part of the enum opType
*******************************************************************************/
opType strToOpType (const char *str)
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
        opType
     type = strToOpType(token);
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






/*******************************************************************************
FUNCTION: 
*******************************************************************************/
void opsToStr(char *opsMSG, int ops[MAX_OPS])
{
    for(int i = 0; i < MAX_OPS; i++) 
    {
        char aux[2];
        sprintf(aux,"%d",ops[i]);
        strcat(opsMSG,aux);
    }
}


/*******************************************************************************
FUNCTION: 
*******************************************************************************/
bool setOps(char *opsMSG)
{

    char auxMessage[2];
    for (int i = 1; i <= MAX_OPS; i++)
    {
        //Geting each number and seperating them in different strings
        auxMessage[0] = opsMSG[i];
        auxMessage[1] = '\0';

        int value = atoi(auxMessage);
        
        
        if (opsMSG[0] == '+')
            if(operations[i-1] + value <= maxOperations[i-1])
                operations[i-1] += value; 
            else return false;
      
        else if (opsMSG[0] == '-')
            if(operations[i-1] - value >= 0)
                operations[i-1] -= value; 
            else return false;
      
        else
        {
            sendMessage(STDERR_FILENO, "Garbage collected\n");
            return false;
        }
    }

    return true;
}


/*******************************************************************************
FUNCTION: Handler function for signals sent by the user
*******************************************************************************/
static void handlerFather(int sig, siginfo_t *si, void *uap)
{
    
    if(sig == SIG_SET_OPS && si->si_pid != getpid())
    {   

        char opsMSG[MAX_OPS+2];
        read(fdOperations[PIPE_RD], opsMSG, MAX_OPS+2);

        if(setOps(opsMSG))
        {
            //If the process tried to set up operations, was successful and was
            //still in the queue, then we need to take it out
            if(queue != NULL && si->si_pid == queue[0]) getFirstElem();

            sleep(0.1);
            kill(si->si_pid, SIGCONT);
            kill(si->si_pid, SIG_SUCC);

            if(queue != NULL) kill(queue[0], SIGCONT);
        }
        else
        {
            sendMessage(STDERR_FILENO, "Setting up operations failed\n");
            kill(si->si_pid, SIGCONT);
            kill(si->si_pid, SIG_FAIL);
            if(queue != NULL && si->si_pid != queue[0]) putElem(si->si_pid);
        }
    }

    //When the parent process recieves signal that a child process finished,
    //it will try to force a "continue process" on the first pendent request
    if(si->si_code == SI_USER && sig == SIGCHLD && queue != NULL)
        kill(queue[0], SIGCONT);
}


/*******************************************************************************
FUNCTION: Handler function for signals sent to children
*******************************************************************************/
void handlerChild(int sig)
{
    if(sig == SIG_FAIL)
    {
        sendMessage(client, "Request pending...\n");
        kill(getpid(), SIGSTOP);
    }
    if(sig == SIG_SUCC)
    {
        //sendMessage(client, "Request is being processed...\n");
        childContinues = true;
    }
}