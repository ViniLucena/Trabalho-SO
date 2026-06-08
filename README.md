## Roteiro de Implementação - Trabalho 2 (Gerenciamento de Memória Virtual)

Este roteiro documenta os passos para a extensão do KernelSim com paginação e memória virtual.

### Etapa 1: Atualização das Estruturas Base (`common.h`)

- [x] Definir constantes: tamanho da RAM (32), total de páginas (16) e `IRQ3` (3).
- [x] Criar a `struct` da Tabela de Páginas (PTE) com: `valid`, `frame`, `modifyBit` e `when` (para LRU/FIFO).
- [x] Adicionar no PCB os contadores: `page_faults` e `swap_double_faults`.
- [x] Adicionar uma flag na `SyscallRequest` (ex: `is_mem_access` ou `op = 'M'`) para diferenciar acessos de memória de I/O de disco.

### Etapa 2: Simulando o Disco de Swap (`inter_controller.c`)

- [x] Após gerar o `IRQ0`, adicionar a geração do `IRQ3` com 50% de probabilidade.
- [x] Enviar o `IRQ3` via pipe e sinalizar o Kernel com `SIGUSR2`.

### Etapa 3: A Nova Lógica das Aplicações (`application.c`)

- [x] No loop de execução, adicionar 39% de probabilidade de a aplicação realizar um acesso à memória.
- [x] Sortear o novo índice de página (`mem_acessada` entre 0 e 15).
- [x] Definir a operação (Leitura/Escrita) com base no índice (`mem % 2`).
- [x] Enviar o pedido via `SyscallRequest` pelo `pipe_syscalls`.

### Etapa 4: O Coração da Memória Virtual e Filas FCFS (`kernel_sim.c`)

- [x] Criar globalmente: `RAM[32]`, `RAM_free[32]` e `TPx[5][16]`.
- [x] **Page Hit:** Se `access(mem, op)` e `valid == 1`, atualiza `when`.
- [x] **Page Fault:** Se `valid == 0`, busca em `RAM_free`. Se lotado, aciona algoritmo de substituição.
- [x] **Sw_Queue (FIFO):** Criar uma fila estruturada com `pid`, `pagina_virtual` e `contador_espera` (1 ou 2, baseado no `modifyBit` da página vítima).
- [x] **Tratar IRQ3:** Decrementar o `contador_espera` do primeiro da fila. Se zerar, aloca na RAM e atualiza a TPx.
- [x] _Refatoração:_ Transformar as filas de D1 e D2 em FCFS reais (como feito na Sw_Queue).

### Etapa 5: Algoritmos de Substituição (`kernel_sim.c`)

- [ ] Implementar `global_substitute(RAM)`: escolhe vítimas de qualquer processo.
- [ ] Implementar `local_substitute(RAM, id)`: escolhe vítimas apenas do processo que causou o fault.
- [ ] Ambos os algoritmos devem preferir páginas limpas (`modifyBit == 0`) para evitar o custo duplo de swap (2x IRQ3).

### Etapa 6: Atualização do Monitoramento (Ctrl+Z)

- [ ] Atualizar `handle_display` para imprimir também o total de _Page Faults_ e _Swap double faults_ (quantas vezes esperou 2x IRQ3) por processo.
