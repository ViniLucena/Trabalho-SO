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
    int pipe_fd = atoi(argv[2]);
    int pc = 0;

    // inicializar semente aleatoria para cada processo
    srand(time(NULL) + id);
    int mem = rand() % 16;

    // trab 2: loop atualizado para o novo escopo do trbalho
    while (pc < MAX)
    {
        pc++;
        char op;
        // atualizacao
        SyscallRequest update;
        update.id = id;
        update.op = 'U';
        update.pc = pc;
        update.mem_address = mem;
        update.is_mem_access = 0;
        write(pipe_fd, &update, sizeof(SyscallRequest));
        kill(getppid(), SIGUSR1);
        // 40% de chance de acessar nova página de memoria
        if (rand() % 100 < 40)
        {
            mem = rand() % 16;
            if (mem % 2 != 0)
            {
                op = 'R';
            }
            else
            {
                op = 'W';
            }

            SyscallRequest req; // instanciando o request
            req.id = id;
            req.pc = pc;
            req.op = op;
            req.is_mem_access = 1;
            req.mem_address = mem;

            write(pipe_fd, &req, sizeof(SyscallRequest));
            kill(getppid(), SIGUSR1);
        }
        // 15% de chance de fazer I/O
        int d = rand() % 100 + 1;
        if (d < 15)
        {
            // define o dispositivo
            int device;
            if (d % 2 != 0)
            {
                device = DEV_D1;
            }
            else
            {
                device = DEV_D2;
            }
            // define a operacao
            if (d % 3 == 1)
            {
                op = 'R';
            }
            else if (d % 3 == 2)
            {
                op = 'W';
            }
            else
            {
                op = 'X';
            }

            SyscallRequest req;
            req.id = id;
            req.pc = pc;
            req.device = device;
            req.op = op;
            req.is_mem_access = 0;
            req.mem_address = mem;

            write(pipe_fd, &req, sizeof(SyscallRequest));
            kill(getppid(), SIGUSR1);
        }
        usleep(500000);
    }
    SyscallRequest fin;
    fin.id = id;
    fin.op = MSG_FINISHED;
    fin.pc = pc;
    fin.is_mem_access = 0;

    write(pipe_fd, &fin, sizeof(SyscallRequest));
    kill(getppid(), SIGUSR1);
    printf("\nprocesso A%d terminado (PC =%d)\n", id, pc);
    return 0;
}