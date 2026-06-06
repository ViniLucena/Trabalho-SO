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
#define IRQ3 3 // termino D3 (termino de swap)

// definicoes de memoria e dispositivos
#define MEM_SIZE 16 // m00 a m15
#define DEV_D1 0
#define DEV_D2 1
#define MSG_FINISHED 'F' // mensagem de fim de processo

#define RAM_SIZE 32   // quantidade total de quadros na memória física
#define PAGE_TOTAL 16 // total de paginas virtuais por processo

typedef enum
{
    READY,
    RUNNING,
    BLOCKED,
    FINISHED
} ProcessState; // tipo que diz qual estado do processo esta correndo

typedef struct
{
    int valid;
    int frame;
    int modifyBit;
    int when;
} PageTableEntry; // trab 2: estrutura da entrada da tabela de paginas (PTE)

typedef struct
{
    int id;          // quem esta pedindo
    int device;      // DEV_D1 ou DEV_D2
    char op;         //'R','W','X' ou 'F'
    int mem_address; // endereco de memoria acessado
    int pc;          // para o kernel saber onde a aplicacao esta

    int is_mem_access; // trab 2: 1 se for tentativa de acesso a memoria, 0 se for I/O de D1/D2
} SyscallRequest;

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

    // trab 2:
    int page_faults;        // quantas vezes a pagina nao estava na RAM
    int swap_double_faults; // quantas vezes foi preciso escrever no swap antes de ler
} ProcessControlBlock;

#endif