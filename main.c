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
#include <string.h>

int main(void)
{

    pid_t pid_kernel, pid_controller;
    // variaveis para os pipes (arrays de dois ints)
    int pipe_syscalls[2];
    int pipe_irqs[2];
    // arrays de char para conversao dos numeros para strings (10 espaços para dar margem de segurança)
    char str_fd_syscalls[10], str_fd_irqs[10], str_kernel_pid[10];
    // criando pipes antes do for
    if (pipe(pipe_syscalls) == -1 || pipe(pipe_irqs) == -1)
    {
        perror("erro ao criar pipes");
        exit(1);
    }
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
        // Converte o FD de LEITURA (índice 0) dos dois pipes para string
        sprintf(str_fd_syscalls, "%d", pipe_syscalls[0]);
        sprintf(str_fd_irqs, "%d", pipe_irqs[0]);
        // passa os FDs como argumentos: argv[1] e argv[2]
        char *args[] = {"./kernel_sim", str_fd_syscalls, str_fd_irqs, NULL};
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
        printf("\nprocesso filho: controlador\n");
        // converte o PID do Kernel para string
        sprintf(str_kernel_pid, "%d", pid_kernel);
        // converte a parte de escrita do pipe de IRQs (index 1) para string
        sprintf(str_fd_irqs, "%d", pipe_irqs[1]);
        char *args[] = {"./inter_controller", str_kernel_pid, str_fd_irqs, NULL};
        execv(args[0], args);                         // executa o InterController
        perror("falha ao executar inter_controller"); // se chegou aqui, execv deu erro
        exit(1);
    }

    // codigo do pai
    printf("\nprocesso pai: criei o kernelSim (PID %d) e criei o controlador (PID %d)", pid_kernel, pid_controller);
    // fecha os pipes pois o processo pai nao os usa
    close(pipe_syscalls[0]);
    close(pipe_syscalls[1]);
    close(pipe_irqs[0]);
    close(pipe_irqs[1]);
    // espera os filhos terminarem suas execucoes para poder encerrar tudo
    waitpid(pid_kernel, NULL, 0);
    waitpid(pid_controller, NULL, 0);
    return 0;
}