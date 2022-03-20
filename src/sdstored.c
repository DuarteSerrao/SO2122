#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

 // server

void terminate(int signum){
    unlink("tmp/pipCli");
    unlink("tmp/pip");
    pid_t p = getpid();
    kill(p, SIGQUIT);
}




int main(int argc, char** argv)
{
    signal(SIGINT, terminate);
    signal(SIGTERM, terminate);

    if (argv[1] == NULL){
      char* tutorial = "tutorial \n";
      write(1,tutorial, strlen(tutorial));
      return 1;
    }

    printf("Starting server...\n");
    mkfifo("tmp/pipCli", 0644);
    mkfifo("tmp/pip", 0644);

    printf("Loading config..\n");
    loadConfig(argv[1]);
    while(1){

        printf("Forking..\n");
        if (!fork()) {

            printf("Opening pipe\n");
            int input = open("tmp/pip", O_RDONLY);

            printf("Reading\n");

            char iteratedInput[1024];
            int tag = 0;
            int i = 0;

            for (; tag==0; i++){

              char buf[1024];
              int n = read(input, buf, 1024);

              if(n>0){
                buf[n] = 0;
                strcat(iteratedInput,buf);
                strcat(iteratedInput," ");

              }else{
                tag = 1;
              }
            }
            close(input);

      int cliente = open("tmp/pipCli", O_WRONLY);
      char* resposta = "O seu pedido esta a ser processado\n";
      write(cliente, resposta, strlen(resposta));
      close(cliente);

      char* comando = readInput(iteratedInput, argv[2]);
      char** comandos = stringToStringArray(comando);

      int in  = open(comandos[2], O_RDONLY, 0666);
      int out = open(comandos[3], O_WRONLY | O_CREAT, 0666);

      dup2(in, fileno(stdin));
      dup2(out, fileno(stdout));

      close(in);
      close(out);

      cliente = open("tmp/pipCli", O_WRONLY);
      resposta = "O seu pedido foi iniciado\n";
      write(cliente, resposta, strlen(resposta));
      close(cliente);

      if(!fork()){

        int error = execlp(comandos[1], comandos[0], NULL);
        exit(error);

      }else{
        wait(NULL);
        cliente = open("tmp/pipCli", O_WRONLY);
        resposta = "O seu pedido foi finalizado\n";
        write(cliente, resposta, strlen(resposta));
        close(cliente);
      }

      } else {
        wait(NULL);
      }
  }
  return 0;
}