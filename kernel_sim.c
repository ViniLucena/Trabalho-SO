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

// trab 2: variáveis globais para o kernel
int RAM[RAM_SIZE];
int RAM_free[RAM_SIZE];
int RAM_page[RAM_SIZE];
PageTableEntry TPx[N_PROCESSOS][PAGE_TOTAL];

// trab 2: estrutura para o disco de SWAP
typedef struct
{
    int pid;
    int idx;             // indice no PCB
    int pagina_virtual;  // pagina que gerou o fault
    int contador_espera; // 1 para fault normal, 2 se precisar gravar (double fault)
    int quadro_alvo;     // quadro reservado na RAM
} SwapRequest;

// trab 2: fila para o disco de SWAP (Sw_queue)
SwapRequest sw_queue[N_PROCESSOS];
int head_sw = 0, tail_sw = 0, count_sw = 0;

int global_substitute(void);
int local_substitute(int req_idx);
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

    // trab 2: inicializando a RAM e a tabela de páginas
    for (int i = 0; i < RAM_SIZE; i++)
    {
        RAM_free[i] = 1; // livre
        RAM[i] = -1;     // vazio
        RAM_page[i] = -1;
    }
    for (int i = 0; i < N_PROCESSOS; i++)
    {
        for (int j = 0; j < PAGE_TOTAL; j++)
        {
            TPx[i][j].valid = 0;
            TPx[i][j].frame = -1;
            TPx[i][j].modifyBit = 0;
            TPx[i][j].when = 0;
        }
    }

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

// trab 2: usa substituição global para escolher o melhor frame analisando todos os frames da RAM
int global_substitute(void)
{
    int oldest_clean_frame = -1;
    int oldest_dirty_frame = -1;
    int min_when_clean = 9999999;
    int min_when_dirty = 9999999;

    for (int i = 0; i < RAM_SIZE; i++)
    {
        int p_idx = RAM[i];
        // só analisa o frame se ele nao estiver vazio
        if (p_idx != -1)
        {
            int page = RAM_page[i];
            int pag_suja = TPx[p_idx][page].modifyBit;
            int when = TPx[p_idx][page].when;

            if (pag_suja == 0)
            {
                if (when < min_when_clean)
                {
                    min_when_clean = when;
                    oldest_clean_frame = i;
                }
            }
            else
            {
                if (when < min_when_dirty)
                {
                    min_when_dirty = when;
                    oldest_dirty_frame = i;
                }
            }
        }
    }
    if (oldest_clean_frame != -1)
    {
        return oldest_clean_frame;
    }

    return oldest_dirty_frame;
}

// trab 2: usa substituição local para escolher o melhor frame analisando todos os frames do próprio processo
int local_substitute(int req_idx)
{
    int oldest_clean_frame = -1;
    int oldest_dirty_frame = -1;
    int min_when_clean = 9999999;
    int min_when_dirty = 9999999;

    for (int i = 0; i < RAM_SIZE; i++)
    {
        int p_idx = RAM[i];
        if (p_idx == req_idx)
        {
            int page = RAM_page[i];
            int pag_suja = TPx[p_idx][page].modifyBit;
            int when = TPx[p_idx][page].when;
            if (pag_suja == 0)
            {
                if (when < min_when_clean)
                {
                    min_when_clean = when;
                    oldest_clean_frame = i;
                }
            }
            else
            {
                if (when < min_when_dirty)
                {
                    min_when_dirty = when;
                    oldest_dirty_frame = i;
                }
            }
        }
    }
    if (oldest_clean_frame != -1)
    {
        return oldest_clean_frame;
    }
    if (oldest_dirty_frame != -1)
    {
        return oldest_dirty_frame;
    }
    // fallback: caso o processo nao tenha paginas na RAM e tenha causado fault
    return global_substitute();
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
        // trab 2: caso a operacao seja 'U'
        if (req.op == 'U')
        {
            // como o PC e a memoria foram alterados nas linhas de cima, encerra a funcao sem bloquear a aplicacao
            return;
        }
        // trab2: tratamento da memoria virtual
        if (req.is_mem_access == 1)
        {
            int page = req.mem_address;
            // caso do page hit
            if (TPx[idx][page].valid == 1)
            {
                printf("kernel: PAGE HIT para A%d na pagina %d\n", req.id, page);
                TPx[idx][page].when = req.pc;
                if (req.op == 'W')
                {
                    TPx[idx][page].modifyBit = 1;
                }
                return;
            }
            // caso do page fault
            printf("kernel: PAGE FAULT para A%d na pagina %d\n", req.id, page);
            pcb_table[idx].page_faults++;

            int free_frame = -1;
            // procura um quadro livre na RAM
            for (int i = 0; i < RAM_SIZE && free_frame == -1; i++)
            {
                if (RAM_free[i] == 1)
                {
                    free_frame = i;
                }
            }
            int custo_espera = 1;

            if (free_frame != -1)
            {
                RAM_free[free_frame] = 0;
                RAM[free_frame] = idx;
                RAM_page[free_frame] = page;

                custo_espera = 1;
            }
            else
            {
                // caso da RAM cheia
                free_frame = global_substitute();
                int victim_idx = RAM[free_frame];
                int victim_page = RAM_page[free_frame];

                // verifica se a vitima estava suja (double fault)
                if (TPx[victim_idx][victim_page].modifyBit == 1)
                {
                    custo_espera = 2; // custo duplo: salvar no disco ler
                    pcb_table[idx].swap_double_faults++;
                    printf("kernel: [SWAP] vitima A%d(pag %d) SUJA. Custo=2 IRQ3\n", pcb_table[victim_idx].id, victim_page);
                }
                else
                {
                    custo_espera = 1; // apenas sobrescrever
                    printf("kernel: [SWAP] vitima A%d(pag %d) LIMPA. Custo=1 IRQ3\n", pcb_table[victim_idx].id, victim_page);
                }
                // remove a vitima da tabela do dono antigo
                TPx[victim_idx][victim_page].valid = 0;

                // atribuindo o frame ao processo atual
                RAM[free_frame] = idx;
                RAM_page[free_frame] = page;
            }
            // coloca o pedido na fila FIFO do swap
            sw_queue[tail_sw].pid = pcb_table[idx].pid;
            sw_queue[tail_sw].idx = idx;
            sw_queue[tail_sw].pagina_virtual = page;
            sw_queue[tail_sw].contador_espera = custo_espera;
            sw_queue[tail_sw].quadro_alvo = free_frame;

            tail_sw = (tail_sw + 1) % N_PROCESSOS;
            count_sw++;

            // suspende o processo enquanto o disco de swap trabalha
            pcb_table[idx].state = BLOCKED;
            kill(pcb_table[idx].pid, SIGSTOP);
            if (idx == running_process_idx)
            {
                running_process_idx = -1;
                scheduler();
            }
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
        // trab 2: trata o caso pro IRQ3 (retorno do disco)
        else if (irq_type == IRQ3)
        {
            if (count_sw > 0)
            {
                sw_queue[head_sw].contador_espera--;

                if (sw_queue[head_sw].contador_espera == 0)
                {
                    // carrega a página e acorda o processo
                    int idx = sw_queue[head_sw].idx;
                    int page = sw_queue[head_sw].pagina_virtual;
                    int frame = sw_queue[head_sw].quadro_alvo;

                    TPx[idx][page].valid = 1;
                    TPx[idx][page].frame = frame;
                    TPx[idx][page].modifyBit = 0;
                    TPx[idx][page].when = pcb_table[idx].pc;

                    pcb_table[idx].state = READY;

                    // remove da fila
                    head_sw = (head_sw + 1) % N_PROCESSOS;
                    count_sw--;

                    printf("kernel: page %d carregada para A%d no quadro %d. (READY)\n", page, pcb_table[idx].id, frame);
                }
                else
                {
                    // remove da head e reinsere no final da fila
                    SwapRequest temp = sw_queue[head_sw];

                    head_sw = (head_sw + 1) % N_PROCESSOS;
                    count_sw--;

                    sw_queue[tail_sw] = temp;
                    tail_sw = (tail_sw + 1) % N_PROCESSOS;
                    count_sw++;

                    printf("kernel: swap write concluido para A%d, reinserido na fila para leitura.\n", pcb_table[temp.idx].id);
                }
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
        // trab 2: exibe os dados do trab 2
        printf("Page Faults: %d - Double Faults: %d\n", pcb_table[i].page_faults, pcb_table[i].swap_double_faults);
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
