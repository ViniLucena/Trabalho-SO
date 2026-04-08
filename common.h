/*
PCB: estrutura que guarda as informações dos processos para trocar entre eles, e também possui
as definições compartilhadas: constantes tipo MAX, N_PROCESSOS;
frequências de interrupção; definições de estados de processos
e a estrutura do contexto de processo.
*/
#ifndef COMMON_H
#define COMMON_H // torna o common.h importavel por outros arquivos

#include <sys/types.h>
#include <signal.h>
#include <unistd.h>

#define MAX 3000      // limite de iteracoes
#define N_PROCESSOS 5 // total de processos

// definicoes de interrupcoes
#define IRQ0 0 // time-slice
#define IRQ1 1 // termino D1
#define IRQ2 2 // termino D2

// definicoes de memoria e dispositivos
#define MEM_SIZE 16 // m00 a m15
#define DEV_D1 0
#define DEV_D2 1

typedef enum
{
    READY,
    RUNNING,
    BLOCKED,
    FINISHED
} ProcessState; // tipo que diz qual estado do processo esta correndo

typedef struct
{
    pid_t pid;          // PID do processo
    int id;             // ID simulado (1 a 5)
    int pc;             // program counter (0 ate max)
    int last_mem;       // ultima memoria acessada (m00 a m15)
    ProcessState state; // ready, running, blocked...
    int block_device;   // d1 ou d2 (se estiver bloqueado)
    char block_op;      // r, w ou x
    int io_count[2];    // contador de acessos: io_count[0] para D1 e io_count[1] para D2
} ProcessControlBlock;

#endif