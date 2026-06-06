/*Núcleo do SO: O "gerente". Deve conter a lógica de escalonamento Round-Robin,
as filas de processos (prontos/bloqueados) e o tratamento de sinais SIGSTOP/SIGCONT.*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include "common.h"

// variaveis globais para o kernel
ProcessControlBlock pcb_table[N_PROCESSOS];
int fd_syscalls, fd_irqs;
int running_process_idx = -1;

int blocked_queue_d1[N_PROCESSOS];
int blocked_queue_d2[N_PROCESSOS];
int head_d1 = 0, tail_d1 = 0, count_d1 = 0;
int head_d2 = 0, tail_d2 = 0, count_d2 = 0;

void handle_syscall(int sig);
void handle_irq(int sig);
void scheduler(void);
void handle_display(int sig);

int main(int argc, char *argv[])
{
    if (argc < 4)
    {
        printf("erro: o kernel precisa dos FDs dos pipes\n");
        exit(1);
    }
    fd_syscalls = atoi(argv[1]);
    fd_irqs = atoi(argv[2]);
    int fd_syscalls_write = atoi(argv[3]);
    signal(SIGUSR1, handle_syscall);
    signal(SIGUSR2, handle_irq);
    signal(SIGTSTP, handle_display); // handler para ctrl+Z

    printf("kernel inicializado. FDs: Sys=%d, IRQ=%d\n", fd_syscalls, fd_irqs);
    // loop de criacao de processos
    for (int i = 0; i < N_PROCESSOS; i++)
    {
        pid_t pid = fork();
        if (pid == 0)
        {
            /* codigo do filho: se transforma na aplicacao */
            char id_str[2], fd_str[10];
            sprintf(id_str, "%d", i + 1);             // IDs de 1 a 5 ao inves do intervalo de 0 a 4
            sprintf(fd_str, "%d", fd_syscalls_write); // aplicacao precisa escrever no pipe de syscalls

            char *args[] = {"./application", id_str, fd_str, NULL};
            execv(args[0], args);
            exit(1);
        }
        else
        {
            /* codigo do kernel: registra o processo */
            pcb_table[i].pid = pid;
            pcb_table[i].id = i + 1;
            pcb_table[i].pc = 0;
            pcb_table[i].state = READY;
            pcb_table[i].io_count[0] = 0;
            pcb_table[i].io_count[1] = 0;

            // como as aplicacoes comecam paradas, sao suspensas imediatamente e deixadas
            // prontas para o escalonador
            kill(pid, SIGSTOP);
        }
    }
    printf("todos os processos foram criados e suspensos. Iniciando o escalonador\n");
    // inicia o escalonador
    scheduler();
    while (1)
    {
        pause(); // espera por qualquer sinal, IRQ ou Syscall
    }

    return 0;
}

void handle_syscall(int sig)
{
    SyscallRequest req;
    // lendo a estrutura do pedido vinda do pipe de syscall
    if (read(fd_syscalls, &req, sizeof(SyscallRequest)) > 0)
    {
        // identifica quem enviou a mensagem
        int idx = req.id - 1;
        printf("kernel: syscall de A%d para disp %d (Op %c)\n", req.id, req.device, req.op);

        // atualiza o contexto
        pcb_table[idx].pc = req.pc;
        pcb_table[idx].last_mem = req.mem_address;

        // caso seja uma syscall de termino
        if (req.op == MSG_FINISHED)
        {
            pcb_table[idx].state = FINISHED;
            printf("kernel: processo A%d terminou (estado=FINISHED)\n", req.id);
            if (idx == running_process_idx)
            {
                running_process_idx = -1;
                scheduler();
            }
            return;
        }
        if (req.op == 'U')
        {
            // como o PC e a memoria foram alterados nas linhas de cima, encerra a funcao sem bloquear a aplicacao
            return;
        }

        pcb_table[idx].state = BLOCKED;
        pcb_table[idx].block_device = req.device;
        pcb_table[idx].block_op = req.op;
        // incrementa os dados de uso do dispositivo
        pcb_table[idx].io_count[req.device]++;

        // insere na fila FIFO correta
        if (req.device == DEV_D1)
        {
            blocked_queue_d1[tail_d1] = idx;
            tail_d1 = (tail_d1 + 1) % N_PROCESSOS; // avança de forma circular
            count_d1++;
        }
        else if (req.device == DEV_D2)
        {
            blocked_queue_d2[tail_d2] = idx;
            tail_d2 = (tail_d2 + 1) % N_PROCESSOS;
            count_d2++;
        }

        // pausa a execucao da aplicacao para que ela nao gaste CPU enquanto espera pelo hardware
        kill(pcb_table[idx].pid, SIGSTOP);
        // chama o escalonador somente se o processo que acabou de se bloquear for o processo que estava na CPU naquele momento
        if (idx == running_process_idx)
        {
            scheduler();
        }
    }
}

void scheduler(void)
{
    // se houver um processo rodando, pausa ele
    if (running_process_idx != -1 && pcb_table[running_process_idx].state == RUNNING)
    {
        pcb_table[running_process_idx].state = READY;
        kill(pcb_table[running_process_idx].pid, SIGSTOP);
    }
    // procura o proximo processo READY
    int next_idx = (running_process_idx + 1) % N_PROCESSOS;
    int count = 0;
    while (pcb_table[next_idx].state != READY && count < N_PROCESSOS)
    {
        next_idx = (next_idx + 1) % N_PROCESSOS;
        count++;
    }
    // se encontrou um processo READY, roda ele
    if (pcb_table[next_idx].state == READY)
    {
        running_process_idx = next_idx;
        pcb_table[running_process_idx].state = RUNNING;
        printf("kernel: escalonando A%d (PID %d)\n", pcb_table[running_process_idx].id, pcb_table[running_process_idx].pid);
        kill(pcb_table[running_process_idx].pid, SIGCONT);
    }
    else
    {
        running_process_idx = -1;
        printf("kernel: nenhum processo pronto para rodar \n");
    }
}

void handle_irq(int sig)
{
    int irq_type;
    if (read(fd_irqs, &irq_type, sizeof(int)) > 0)
    {
        if (irq_type == IRQ0)
        {
            printf("kernel: recebido IRQ0 (timeslice). Trocando processo\n");
            scheduler();
        }
        else if (irq_type == IRQ1)
        {
            // desbloqueia o primeiro da fila D1
            if (count_d1 > 0) // fila nao vazia
            {
                int idx = blocked_queue_d1[head_d1];
                head_d1 = (head_d1 + 1) % N_PROCESSOS;
                count_d1--;
                pcb_table[idx].state = READY;
                printf("kernel: processo A%d desbloqueado (D1)\n", pcb_table[idx].id);
            }
        }
        else if (irq_type == IRQ2)
        {
            // desbloqueia o primeiro da fila D2
            if (count_d2 > 0)
            {
                int idx = blocked_queue_d2[head_d2];
                head_d2 = (head_d2 + 1) % N_PROCESSOS;
                count_d2--;
                pcb_table[idx].state = READY;
                printf("kernel: processo A%d desbloqueado (D2)\n", pcb_table[idx].id);
            }
        }
    }
}

void handle_display(int sig)
{
    // pausa o processo que estava rodando
    if (running_process_idx != -1 && pcb_table[running_process_idx].state == RUNNING)
        kill(pcb_table[running_process_idx].pid, SIGSTOP);
    printf("\n-----------ESTADO DOS PROCESSOS-----------\n");
    for (int i = 0; i < N_PROCESSOS; i++)
    {
        char *estado;
        if (pcb_table[i].state == READY)
        {
            estado = "PRONTO";
        }
        else if (pcb_table[i].state == RUNNING)
        {
            estado = "EXECUTANDO";
        }
        else if (pcb_table[i].state == BLOCKED)
        {
            estado = "BLOQUEADO";
        }
        else
        {
            estado = "TERMINADO";
        }
        printf("A%d -- PC=%d -- mem=m%02d -- estado = %s\n", pcb_table[i].id, pcb_table[i].pc, pcb_table[i].last_mem, estado);

        if (pcb_table[i].state == BLOCKED)
        {
            printf("\ndisp=D%d op=%c\n", pcb_table[i].block_device + 1, pcb_table[i].block_op);
        }
        printf("\nD1=%d D2=%d\n", pcb_table[i].io_count[0], pcb_table[i].io_count[1]);
    }
    printf("\nsistema retomando em 5 segundos\n");
    // ignora os sinais durante o sleep para nao ser acordado antes da hora
    signal(SIGUSR1, SIG_IGN);
    signal(SIGUSR2, SIG_IGN);
    sleep(5);
    // reinstala os handlers normais
    signal(SIGUSR1, handle_syscall);
    signal(SIGUSR2, handle_irq);
    printf("\nretomando\n");
    signal(SIGTSTP, handle_display); // reinstala o handler
    // retoma o processo que estava rodando
    if (running_process_idx != -1 && pcb_table[running_process_idx].state == RUNNING)
        kill(pcb_table[running_process_idx].pid, SIGCONT);
}
