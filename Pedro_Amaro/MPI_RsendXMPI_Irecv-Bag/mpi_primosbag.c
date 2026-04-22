#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "mpi.h"

#define TAMANHO 500000
#define TAG_READY 1
#define TAG_TASK 2
#define TAG_RESULT 3
#define TAG_STOP 99

int primo(int n) {
  int i;
  for (i = 3; i < (int)(sqrt(n) + 1); i += 2) {
    if (n % i == 0) return 0;
  }
  return 1;
}

int main(int argc, char* argv[]) {
  double t_inicial, t_final;
  int total = 0;
  int i, n;
  int meu_ranque, num_procs, inicio, dest;
  
  /* Verifica o número de argumentos passados */
  if (argc < 2) {
    printf("Entre com o valor do maior inteiro como parâmetro para o programa.\n");
    return 0;
  } else {
    n = strtol(argv[1], (char**)NULL, 10);
  }
  
  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &meu_ranque);
  MPI_Comm_size(MPI_COMM_WORLD, &num_procs);
  
  /* Se houver menos que dois processos aborta */
  if (num_procs < 2) {
    printf("Este programa deve ser executado com no mínimo dois processos.\n");
    MPI_Abort(MPI_COMM_WORLD, 1);
    return (1);
  }
  
  /* Registra o tempo inicial de execução do programa */
  t_inicial = MPI_Wtime();
  
  if (meu_ranque == 0) { // PROCESSO MESTRE
    int* conts = (int*)malloc(num_procs * sizeof(int));
    MPI_Request* reqs = (MPI_Request*)malloc(num_procs * sizeof(MPI_Request));
    
    // Anula o request do processo 0 para o MPI_Waitany ignorá-lo
    reqs[0] = MPI_REQUEST_NULL; 

    // 1. Pré-posta a recepção de todos os escravos simultaneamente
    for (dest = 1; dest < num_procs; dest++) {
      MPI_Irecv(&conts[dest], 1, MPI_INT, dest, MPI_ANY_TAG, MPI_COMM_WORLD, &reqs[dest]);
    }

    // 2. Sincroniza para garantir que todos os nós já postaram seus Irecvs antes dos Rsends iniciarem
    MPI_Barrier(MPI_COMM_WORLD);

    int workers_active = num_procs - 1;
    inicio = 3;

    while (workers_active > 0) {
      int idx;
      MPI_Status status_wait;
      
      // Aguarda qualquer escravo se comunicar (seja um envio de PRONTO ou RESULTADO)
      MPI_Waitany(num_procs, reqs, &idx, &status_wait);

      if (idx == MPI_UNDEFINED || idx == 0) continue;

      if (status_wait.MPI_TAG == TAG_RESULT) {
        total += conts[idx];
      }

      if (inicio < n) {
        // Pré-posta o Irecv para a PRÓXIMA comunicação deste escravo específico antes de enviar
        MPI_Irecv(&conts[idx], 1, MPI_INT, idx, MPI_ANY_TAG, MPI_COMM_WORLD, &reqs[idx]);
        
        // Envia a nova tarefa usando Rsend (Seguro, pois o escravo pré-postou Irecv lá na ponta)
        MPI_Rsend(&inicio, 1, MPI_INT, idx, TAG_TASK, MPI_COMM_WORLD);
        inicio += TAMANHO;
      } else {
        int dummy = 0;
        // Envia parada definitiva via Rsend
        MPI_Rsend(&dummy, 1, MPI_INT, idx, TAG_STOP, MPI_COMM_WORLD);
        workers_active--;
      }
    }
    
    free(conts);
    free(reqs);
    
    t_final = MPI_Wtime();
    total += 1; /* Acrescenta o 2, que é primo */
    printf("Quant. de primos entre 1 e %d: %d \n", n, total);
    printf("Tempo de execucao: %1.3f \n", t_final - t_inicial);
    
  } else { // PROCESSOS ESCRAVOS
    int start_val;
    MPI_Request req;
    MPI_Status status_wait;

    // 1. Pré-posta a recepção do pacote da primeira tarefa que virá do Mestre
    MPI_Irecv(&start_val, 1, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &req);

    // 2. Sincroniza para garantir que todos pré-postaram seus recebimentos
    MPI_Barrier(MPI_COMM_WORLD);

    int my_cont = 0;
    
    // Manda aviso de "Estou Pronto" pro mestre (Seguro: Mestre já pré-postou Irecv antes da barreira)
    MPI_Rsend(&my_cont, 1, MPI_INT, 0, TAG_READY, MPI_COMM_WORLD);

    while (1) {
      // Bloqueia apenas até a tarefa efetivamente chegar
      MPI_Wait(&req, &status_wait);

      if (status_wait.MPI_TAG == TAG_STOP) {
        break; // Mestre esgotou o bag of tasks
      }

      my_cont = 0;
      for (i = start_val; i < (start_val + TAMANHO) && i < n; i += 2) {
        if (primo(i) == 1) my_cont++;
      }

      // Pré-posta o recebimento da PRÓXIMA tarefa ANTES de enviar o resultado atual
      // Assim permitimos overlap de redes enquanto computamos futuramente
      MPI_Irecv(&start_val, 1, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &req);

      // Descarrega o resultado parcial para o Mestre via Ready Send
      MPI_Rsend(&my_cont, 1, MPI_INT, 0, TAG_RESULT, MPI_COMM_WORLD);
    }
    
    t_final = MPI_Wtime();
  }
  
  /* Finaliza o programa */
  MPI_Barrier(MPI_COMM_WORLD);
  MPI_Finalize();
  return (0);
}