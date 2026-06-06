/*Processo Ax: O código que os processos A1-A5 executarão.
Contém o loop de MAX iterações, o Program Counter (PC),
acessos aleatórios à memória e as requisições de syscall.*/
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include "common.h"

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        exit(1);
    }
    int id = atoi(argv[1]); // ID do processo (A1, A2, ...)
    int pc = 0;
    int mem;

    // inicializar semente aleatoria para cada processo
    srand(time(NULL) + id);
    while (pc < MAX)
    {
        pc++;
        mem = rand() % MEM_SIZE; // escolhe m00-m15
        // simula o processamento
        usleep(500000);

        // logica de syscall aleatoria
        if ((rand() % 100) < 5) // 5% de chance de syscall
        {
            int device = (rand() % 2 == 0) ? DEV_D1 : DEV_D2;
            char op;
            int r = rand() % 3;
            if (r == 0)
            {
                op = 'R';
            }
            else if (r == 1)
            {
                op = 'W';
            }
            else
            {
                op = 'X';
            }
            SyscallRequest req;
            req.id = id;
            req.device = device;
            req.op = op;
            req.mem_address = mem;
            req.pc = pc;
            // escrevendo o pedido do pipe
            int pipe_fd = atoi(argv[2]);
            write(pipe_fd, &req, sizeof(SyscallRequest));

            kill(getppid(), SIGUSR1);
        }
    }
    SyscallRequest fin;
    fin.id = id;
    fin.op = MSG_FINISHED;
    fin.pc = pc;
    fin.mem_address = mem;
    int pipe_fd = atoi(argv[2]);
    write(pipe_fd, &fin, sizeof(SyscallRequest));
    kill(getppid(), SIGUSR1); // acorda o kernel para receber a mensagem

    printf("\nprocesso A%d terminado (PC =%d)\n", id, pc);
    return 0;
}