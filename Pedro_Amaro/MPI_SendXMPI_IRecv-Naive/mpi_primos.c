/*
 * ============================================================================
 * DOCUMENTAÇÃO DE DESEMPENHO E AVALIAÇÃO
 * ============================================================================
 * Estratégia de Ganho de Performance:
 * A função MPI_Reduce foi substituída por uma combinação de MPI_Irecv e MPI_Send.
 * O ganho de performance é obtido através do "Overlap" (sobreposição) de 
 * comunicação e computação. O processo mestre (ranque 0) posta antecipadamente 
 * todas as requisições de recebimento (MPI_Irecv) ANTES de iniciar a sua 
 * própria carga de trabalho matemática (o laço for). 
 * * Isso permite que a biblioteca MPI gerencie o recebimento das mensagens dos 
 * processos trabalhadores em background enquanto a CPU do mestre foca na 
 * verificação dos primos. Reduz-se drasticamente o gargalo de sincronização no
 * final da execução.
 * ============================================================================
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "mpi.h"

/* Mantida intacta conforme solicitado */
int primo(long int n) { 
  int i;
  for (i = 3; i < (int)(sqrt(n) + 1); i += 2) {
    if (n % i == 0) return 0;
  }
  return 1;
}

int main(int argc, char* argv[]) {
  double t_inicial, t_final;
  int cont = 0, total = 0;
  long int i, n;
  int meu_ranque, num_procs, inicio, salto;
  
  // Variáveis para a comunicação não-bloqueante
  int *recv_counts = NULL;
  MPI_Request *requests = NULL;

  if (argc < 2) {
    printf("Valor inválido! Entre com um valor do maior inteiro\n");
    return 0;
  } else {
    n = strtol(argv[1], (char**)NULL, 10);
  }

  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &meu_ranque);
  MPI_Comm_size(MPI_COMM_WORLD, &num_procs);
  
  t_inicial = MPI_Wtime();
  
  // O processo mestre prepara os recebimentos assíncronos ANTES do cálculo
  if (meu_ranque == 0 && num_procs > 1) {
    recv_counts = (int*) malloc((num_procs - 1) * sizeof(int));
    requests = (MPI_Request*) malloc((num_procs - 1) * sizeof(MPI_Request));
    
    // Posta os receives não-bloqueantes. A rede já fica aguardando dados.
    for (int p = 1; p < num_procs; p++) {
      MPI_Irecv(&recv_counts[p - 1], 1, MPI_INT, p, 0, MPI_COMM_WORLD, &requests[p - 1]);
    }
  }

  // Divisão cíclica de tarefas
  inicio = 3 + meu_ranque * 2;
  salto = num_procs * 2;
  
  // Computação principal: ocorre simultaneamente com os receives postados no mestre
  for (i = inicio; i <= n; i += salto) {
    if (primo(i) == 1) cont++;
  }

  // Resolução da Comunicação
  if (num_procs > 1) {
    if (meu_ranque != 0) {
      // Trabalhadores usam MPI_Send para enviar seu resultado final
      MPI_Send(&cont, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
    } else {
      // Mestre garante que todas as mensagens não-bloqueantes chegaram
      MPI_Waitall(num_procs - 1, requests, MPI_STATUSES_IGNORE);
      
      total = cont; // Soma local do mestre
      // Adiciona os resultados recebidos dos trabalhadores
      for (int p = 1; p < num_procs; p++) {
        total += recv_counts[p - 1];
      }
      
      free(recv_counts);
      free(requests);
    }
  } else {
    total = cont;
  }

  t_final = MPI_Wtime();
  
  if (meu_ranque == 0) {
    total += 1; /* Acrescenta o dois, que também é primo */
    printf("Quant. de primos entre 1 e n: %d \n", total);
    printf("Tempo de execucao: %1.3f \n", t_final - t_inicial);
  }
  
  MPI_Finalize();
  return 0;
}