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

        // envia a atualizacao de contexto pro kernel
        SyscallRequest ctx;
        ctx.id = id;
        ctx.op = 'U'; // U de update
        ctx.pc = pc;
        ctx.mem_address = mem;
        ctx.is_mem_access = 0;
        ctx.device = 0; // irrelevante para operacao de U

        int pipe_fd = atoi(argv[2]);
        write(pipe_fd, &ctx, sizeof(SyscallRequest));

        kill(getppid(), SIGUSR1); // acorda o kernel para ler a atualização
        // simula o processamento
        usleep(500000);

        int probabilidade = rand() % 100;

        if (probabilidade < 5) // 5% de chance de syscall
        {
            /* code */
            int device;
            if (rand() % 2 == 0)
            {
                device = DEV_D1;
            }
            else
            {
                device = DEV_D2;
            }
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

            // garantindo que o kernel saiba que é em disco, nao em memoria
            req.is_mem_access = 0;
            int pipe_fd = atoi(argv[2]);
            write(pipe_fd, &req, sizeof(SyscallRequest));
            kill(getppid(), SIGUSR1);
        }
        else if (probabilidade >= 5 && probabilidade <= 43) // 39% de prob de acessar a memória (5 a 43)
        {
            // sorteia a pagina a ser acessada (0 a 15)
            int mem_acessada = rand() % 16;
            char op_mem;
            if (mem_acessada % 2 == 0)
            {
                op_mem = 'R';
            }
            else
            {
                op_mem = 'W';
            }
            SyscallRequest req_mem;
            req_mem.id = id;
            req_mem.device = 0;
            req_mem.op = op_mem;
            req_mem.mem_address = mem_acessada;
            req_mem.pc = pc;

            req_mem.is_mem_access = 1;

            int pipe_fd = atoi(argv[2]);
            write(pipe_fd, &req_mem, sizeof(SyscallRequest));
            kill(getppid(), SIGUSR1);
        }
    }
    SyscallRequest fin;
    fin.id = id;
    fin.op = MSG_FINISHED;
    fin.pc = pc;
    fin.is_mem_access = 0;

    int pipe_fd = atoi(argv[2]);
    write(pipe_fd, &fin, sizeof(SyscallRequest));
    kill(getppid(), SIGUSR1);
    printf("\nprocesso A%d terminado (PC =%d)\n", id, pc);
    return 0;
}