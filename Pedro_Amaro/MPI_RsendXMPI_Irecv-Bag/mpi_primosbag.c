#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "mpi.h"

#define TAMANHO 500000

int primo(int n) {
  int i;
  for (i = 3; i < (int)(sqrt(n) + 1); i += 2) {
    if (n % i == 0) return 0;
  }
  return 1;
}

int main(int argc, char* argv[]) {
  double t_inicial, t_final;
  int cont = 0, total = 0;
  int i, n;
  int meu_ranque, num_procs, inicio, dest, raiz = 0, tag = 1;
  MPI_Status estado;
  MPI_Request req;

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

  MPI_Request *reqs_recv = (MPI_Request*) malloc(num_procs * sizeof(MPI_Request));
  int *cont_recv = (int*) malloc(num_procs * sizeof(int));

  t_inicial = MPI_Wtime();
  int inicio_task = 3;

  if (meu_ranque == 0) {
    reqs_recv[0] = MPI_REQUEST_NULL;
    for (dest = 1; dest < num_procs; dest++) {
      MPI_Irecv(&cont_recv[dest], 1, MPI_INT, dest, MPI_ANY_TAG, MPI_COMM_WORLD, &reqs_recv[dest]);
    }
  } else {
    MPI_Irecv(&inicio, 1, MPI_INT, raiz, MPI_ANY_TAG, MPI_COMM_WORLD, &req);
  }

  MPI_Barrier(MPI_COMM_WORLD);

  if (meu_ranque == 0) {
    int active_workers = num_procs - 1;

    for (dest = 1; dest < num_procs; dest++) {
      if (inicio_task < n) {
        MPI_Rsend(&inicio_task, 1, MPI_INT, dest, tag, MPI_COMM_WORLD);
        inicio_task += TAMANHO;
      } else {
        tag = 99;
        MPI_Rsend(&inicio_task, 1, MPI_INT, dest, tag, MPI_COMM_WORLD);
        active_workers--;
        MPI_Cancel(&reqs_recv[dest]);
        MPI_Wait(&reqs_recv[dest], MPI_STATUS_IGNORE);
      }
    }

    while (active_workers > 0) {
      int index;
      MPI_Waitany(num_procs, reqs_recv, &index, &estado);

      if (index == MPI_UNDEFINED) break;

      total += cont_recv[index];

      if (inicio_task < n) {
        MPI_Irecv(&cont_recv[index], 1, MPI_INT, index, MPI_ANY_TAG, MPI_COMM_WORLD, &reqs_recv[index]);
        MPI_Rsend(&inicio_task, 1, MPI_INT, index, 1, MPI_COMM_WORLD);
        inicio_task += TAMANHO;
      } else {
        tag = 99;
        int dummy = 0; 
        MPI_Rsend(&dummy, 1, MPI_INT, index, tag, MPI_COMM_WORLD);
        active_workers--;
      }
    }
  } else {
    while (1) {
      MPI_Wait(&req, &estado);

      if (estado.MPI_TAG == 99) {
        break;
      }

      cont = 0;
      for (i = inicio; i < (inicio + TAMANHO) && i < n; i += 2) {
        if (primo(i) == 1) cont++;
      }

      MPI_Irecv(&inicio, 1, MPI_INT, raiz, MPI_ANY_TAG, MPI_COMM_WORLD, &req);
      
      MPI_Rsend(&cont, 1, MPI_INT, raiz, 1, MPI_COMM_WORLD);
    }
    t_final = MPI_Wtime();
  }

  if (meu_ranque == 0) {
    t_final = MPI_Wtime();
    total += 1; /* Acrescenta o número 2, que é primo e par */
    printf("Quant. de primos entre 1 e %d: %d \n", n, total);
    printf("Tempo de execucao: %1.3f \n", t_final - t_inicial);
  }

  free(reqs_recv);
  free(cont_recv);

  /* Finaliza o programa */
  MPI_Barrier(MPI_COMM_WORLD);
  MPI_Finalize();
  return (0);
}