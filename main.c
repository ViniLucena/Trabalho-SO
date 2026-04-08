/*
Processo Pai: Responsável por criar os processos KernelSim e InterControllerSim usando fork
e exec. Deve garantir que o sistema inicie corretamente e limpe os processos ao final.
*/
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>

int main(void)
{

    pid_t pid_kernel, pid_controller;
    pid_kernel = fork();
    if (pid_kernel < 0)
    {
        perror("\nerro no fork do kernel\n");
        exit(1);
    }
    else if (pid_kernel == 0)
    {
        /* codigo do KernelSim */
        printf("\nprocesso filho: kernel\n");
        char *args[] = {"./kernel_sim", NULL};
        execv(args[0], args);                   // executa o kernel_sim
        perror("falha ao executar kernel_sim"); // se chegou aqui, execv deu erro
        exit(1);
    }
    pid_controller = fork();
    if (pid_controller < 0)
    {
        perror("\nerro no fork do inter_controller\n");
        kill(pid_kernel, SIGKILL); // se o inter_controller falhar, o kernel deve parar tambem (sem o controlador, o kernel fica desorientado e nao chama outros processos)
        exit(1);
    }
    if (pid_controller == 0)
    {
        /* codigo do InterController */
        printf("\nprocesso filho: eu sou o controlador\n");
        char *args[] = {"./inter_controller", NULL};
        execv(args[0], args);                         // executa o InterController
        perror("falha ao executar inter_controller"); // se chegou aqui, execv deu erro
        exit(1);
    }

    // codigo do pai
    printf("\nprocesso pai: criei o kernelSim (PID %d) e criei o controlador (PID %d)", pid_kernel, pid_controller);
    // espera os filhos terminarem suas execucoes para poder encerrar tudo
    waitpid(pid_kernel, NULL, 0);
    waitpid(pid_controller, NULL, 0);
    return 0;
}