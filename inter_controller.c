/*Simulador de Hardware: Gera as interrupções IRQ0 (Time-slice), IRQ1 e IRQ2 (I/O finalizado)
em loops infinitos com as probabilidades e tempos solicitados.*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include "common.h"

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        printf("\nErro: %s <pid_kernel> <pipe_irq_fd>\n", argv[0]);
        exit(1);
    }
    pid_t kernel_pid = atoi(argv[1]);
    int pipe_irq_fd = atoi(argv[2]);
    int irq;
    srand(time(NULL));
    printf("[Controlador] iniciado. Avisando Kernel (PID %d)\n", kernel_pid);

    while (1)
    {
        usleep(500000); // espera o tempo do ciclo
        irq = IRQ0;     // gera o time-slice
        write(pipe_irq_fd, &irq, sizeof(int));
        kill(kernel_pid, SIGUSR2); // SIGUSR2 para interrupcoes de hardware

        if ((rand() % 100) < 10) // probabilidade de terminar no disp1 (10%)
        {
            irq = IRQ1;
            write(pipe_irq_fd, &irq, sizeof(int));
            kill(kernel_pid, SIGUSR2);
        }

        if ((rand() % 100) < 5) // probabilidade de terminar no disp2 (5%)
        {
            irq = IRQ2;
            write(pipe_irq_fd, &irq, sizeof(int));
            kill(kernel_pid, SIGUSR2);
        }
    }

    return 0;
}