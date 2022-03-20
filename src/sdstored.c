#include <fcntl.h>

#include <stdio.h>

#include <sys/stat.h>

#include <sys/wait.h>

#include <unistd.h>

#include <string.h>

#include <stdlib.h>



// server



void terminate(int signum) {

    unlink("tmp/pipCli");

    unlink("tmp/pip");

    pid_t p = getpid();

    kill(p, SIGQUIT);

}









int main(int argc, char** argv)

{

    signal(SIGINT, terminate);

    signal(SIGTERM, terminate);



    /*if (argv[1] == NULL){

      char* tutorial = "tutorial \n";

      write(1,tutorial, strlen(tutorial));

      return 1;

    }*/







    printf("Starting server...\n");

    mkfifo("tmp/pipCli", 0644);

    mkfifo("tmp/pip", 0644);



    while (1) {

        int input = open("tmp/pip", O_RDONLY);

        char buf[1024];

        int n = read(input, buf, 1024);



        int cliente = open("tmp/pipCli", O_WRONLY);

        char* resposta = "O seu pedido esta a ser processado\n";

        write(cliente, resposta, strlen(resposta));

        close(cliente);

    }

    return 0;

}