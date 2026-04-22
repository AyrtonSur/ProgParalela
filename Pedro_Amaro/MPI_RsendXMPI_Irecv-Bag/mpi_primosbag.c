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
  
  if (num_procs < 2) {
    printf("Este programa deve ser executado com no mínimo dois processos.\n");
    MPI_Abort(MPI_COMM_WORLD, 1);
    return (1);
  }
  
  t_inicial = MPI_Wtime();
  
  if (meu_ranque == 0) {
    int* conts = (int*)malloc(num_procs * sizeof(int));
    MPI_Request* reqs = (MPI_Request*)malloc(num_procs * sizeof(MPI_Request));
    
    reqs[0] = MPI_REQUEST_NULL; 

    for (dest = 1; dest < num_procs; dest++) {
      MPI_Irecv(&conts[dest], 1, MPI_INT, dest, MPI_ANY_TAG, MPI_COMM_WORLD, &reqs[dest]);
    }

    MPI_Barrier(MPI_COMM_WORLD);

    int workers_active = num_procs - 1;
    inicio = 3;

    while (workers_active > 0) {
      int idx;
      MPI_Status status_wait;
      
      MPI_Waitany(num_procs, reqs, &idx, &status_wait);

      if (idx == MPI_UNDEFINED || idx == 0) continue;

      if (status_wait.MPI_TAG == TAG_RESULT) {
        total += conts[idx];
      }

      if (inicio < n) {
        MPI_Irecv(&conts[idx], 1, MPI_INT, idx, MPI_ANY_TAG, MPI_COMM_WORLD, &reqs[idx]);
        
        MPI_Rsend(&inicio, 1, MPI_INT, idx, TAG_TASK, MPI_COMM_WORLD);
        inicio += TAMANHO;
      } else {
        int dummy = 0;
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
    
  } else {
    int start_val;
    MPI_Request req;
    MPI_Status status_wait;

    MPI_Irecv(&start_val, 1, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &req);

    MPI_Barrier(MPI_COMM_WORLD);

    int my_cont = 0;
    
    MPI_Rsend(&my_cont, 1, MPI_INT, 0, TAG_READY, MPI_COMM_WORLD);

    while (1) {
      MPI_Wait(&req, &status_wait);

      if (status_wait.MPI_TAG == TAG_STOP) {
        break; // Mestre esgotou o bag of tasks
      }

      my_cont = 0;
      for (i = start_val; i < (start_val + TAMANHO) && i < n; i += 2) {
        if (primo(i) == 1) my_cont++;
      }


      MPI_Irecv(&start_val, 1, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &req);

      MPI_Rsend(&my_cont, 1, MPI_INT, 0, TAG_RESULT, MPI_COMM_WORLD);
    }
    
    t_final = MPI_Wtime();
  }
  
  /* Finaliza o programa */
  MPI_Barrier(MPI_COMM_WORLD);
  MPI_Finalize();
  return (0);
}