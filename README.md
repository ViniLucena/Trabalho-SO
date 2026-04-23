# Trabalho 1 — INF 1316

## Simulando escalonamento preemptivo com chamadas de sistema

---

## O que o trabalho faz

Simula um kernel de SO que gerencia 5 processos de aplicação (A1–A5) usando:

- Escalonamento **Round-Robin preemptivo** via SIGSTOP/SIGCONT
- **Chamadas de sistema** (syscalls) para dois dispositivos de E/S simulados: D1 e D2
- **Interrupções** geradas por um controlador de hardware simulado (IRQ0, IRQ1, IRQ2)

Cada componente do sistema é um **processo Unix real**. A comunicação entre eles usa pipes e sinais — exatamente o que foi ensinado nos Labs 3 e 4.

---

## Arquivos e responsabilidade de cada um

**`main.c`**
Processo pai. Só serve para inicializar o sistema: cria os dois pipes (`pipe_syscalls` e `pipe_irqs`), lança o `kernel_sim` e o `inter_controller` via fork/exec, passa os file descriptors corretos para cada um como argumentos, e fica esperando com `waitpid`.

**`kernel_sim.c`**
O "cérebro" do sistema. Faz três coisas:

1. Cria os 5 processos de aplicação via fork/exec, já os suspendendo com SIGSTOP
2. Escalona os processos em Round-Robin quando recebe IRQ0 (timeslice)
3. Bloqueia processos que pedem syscall e os desbloqueia quando o dispositivo termina (IRQ1/IRQ2)

**`inter_controller.c`**
Simula o hardware. A cada 500ms gera sempre um IRQ0, e com probabilidade 10% um IRQ1 e 5% um IRQ2. Escreve o tipo da IRQ no pipe e avisa o kernel com SIGUSR2.

**`application.c`**
Código que todos os processos A1–A5 executam (via exec). Loop de MAX=3000 iterações com: incremento do PC, acesso aleatório à memória virtual (m00–m15), e 5% de chance de fazer uma syscall aleatória (dispositivo D1 ou D2, operação R/W/X).

**`common.h`**
Definições compartilhadas entre todos os arquivos:

- Constantes: MAX, N_PROCESSOS, IRQ0/1/2, DEV_D1/D2, MEM_SIZE
- Enum `ProcessState`: READY, RUNNING, BLOCKED, FINISHED
- Struct `SyscallRequest`: o que a aplicação manda pelo pipe ao fazer syscall (id, device, op, mem_address, pc)
- Struct `ProcessControlBlock`: o que o kernel guarda sobre cada processo (pid, id, pc, last_mem, state, block_device, block_op, io_count)

---

## Arquitetura de comunicação

```
main
├── fork/exec ──→ kernel_sim
│                  └── fork/exec × 5 ──→ application (A1–A5)
└── fork/exec ──→ inter_controller

Pipes:
  pipe_syscalls: application ──escrita──→ kernel_sim (leitura)
  pipe_irqs:     inter_controller ──escrita──→ kernel_sim (leitura)

Sinais:
  application      ──SIGUSR1──→ kernel_sim   (tem syscall no pipe)
  inter_controller ──SIGUSR2──→ kernel_sim   (tem IRQ no pipe)
  kernel_sim       ──SIGSTOP──→ application  (pausa o processo)
  kernel_sim       ──SIGCONT──→ application  (retoma o processo)
  usuário Ctrl+Z   ──SIGTSTP──→ kernel_sim   (exibe estado)
```

A lógica de dois passos (escreve no pipe + envia sinal) existe porque o sinal apenas acorda o kernel, mas o dado real (qual IRQ ou qual syscall) vem pelo pipe — o sinal sozinho não carrega informação suficiente.

---

## Decisões de design que tomamos

**Por que SIGUSR1 para syscall e SIGUSR2 para IRQ?**
Os dois sinais de usuário disponíveis no material (Lab 3) são SIGUSR1 e SIGUSR2. Usamos SIGUSR1 para syscall (processo → kernel) e SIGUSR2 para IRQ (controlador → kernel) para separar claramente as duas fontes de interrupção.

**Por que o kernel passa o FD de escrita do pipe_syscalls para as aplicações?**
As aplicações precisam escrever a SyscallRequest no pipe. O kernel recebe esse FD do main como argv[3] e o repassa para cada aplicação filha via execv. Assim as aplicações não precisam saber nada sobre como o pipe foi criado.

**Por que usamos a flag `dev_liberado` no handle_irq?**
O enunciado exige que um IRQ libere apenas o primeiro processo bloqueado naquele dispositivo (FIFO). A flag impede que o loop continue liberando outros processos depois do primeiro.

**Por que o SIGALRM no handle_display?**
Quando o usuário aperta Ctrl+Z, o sistema exibe o estado e precisa ficar pausado até retomar. Usamos `alarm(5)` + `pause()` para retomar automaticamente após 5 segundos, usando o SIGALRM ensinado no Lab 3. O `alarm(0)` após o `pause()` cancela qualquer alarme residual (também ensinado no Lab 3 — "Se alarm = 0, desarma o alarme").

**Limitação conhecida — PC e last_mem desatualizados no timeslice:**
O PC e a última memória do PCB só são atualizados quando o processo faz uma syscall (porque aí ele manda esses dados pelo pipe). Em trocas de contexto por IRQ0, o kernel não tem como saber o PC atual da aplicação sem memória compartilhada. Para o escopo do trabalho isso é aceitável — vale mencionar na apresentação para mostrar que entendemos a limitação.

---

## Como compilar e rodar

```bash
gcc -o main main.c
gcc -o kernel_sim kernel_sim.c
gcc -o inter_controller inter_controller.c
gcc -o application application.c
./main
```

## Como usar o Ctrl+Z

Com o sistema rodando, Ctrl+Z no terminal exibe o estado de todos os processos:

- PC e última memória acessada
- Estado: PRONTO / EXECUTANDO / BLOQUEADO / TERMINADO
- Se bloqueado: qual dispositivo e qual operação
- Contagem total de acessos a D1 e D2

O sistema retoma automaticamente após 5 segundos.
