/*
 * ============================================================================
 * DOCUMENTAÇÃO DE GANHO DE PERFORMANCE (SPEEDUP)
 * ============================================================================
 * Mudanças realizadas:
 * 1. Substituição de MPI_Send por MPI_Bsend (Envio em buffer).
 * 2. Substituição de MPI_Recv por MPI_Irecv + MPI_Wait (Recebimento não-bloqueante).
 * * Como o ganho de performance é gerado?
 * O ganho principal (Latency Hiding / Ocultação de Latência) ocorre devido ao 
 * desacoplamento da sincronização entre remetente e destinatário.
 * * - Com MPI_Send (padrão): Dependendo da implementação do MPI e do tamanho da 
 * mensagem, o mestre pode ficar bloqueado esperando o trabalhador (worker) 
 * estar pronto para receber a tarefa, e o trabalhador bloqueia esperando o 
 * mestre receber a contagem. Isso desperdiça ciclos de CPU.
 * * - Com MPI_Bsend: O mestre (rank 0) empacota as tarefas (chunks) no buffer 
 * local e retorna IMEDIATAMENTE para sua rotina, disparando as tarefas para
 * os trabalhadores muito mais rápido. Da mesma forma, o trabalhador calcula
 * o resultado, joga no buffer com Bsend e já pede a próxima tarefa, sem ficar
 * ocioso aguardando o mestre ler o resultado anterior.
 * * - Com MPI_Irecv: Prepara o hardware/camada de rede para receber a mensagem 
 * antes do bloqueio estrito. Embora o MPI_Wait bloqueie até a mensagem 
 * chegar, postar a requisição assíncrona ajuda a biblioteca MPI a gerenciar
 * o recebimento em background.
 * ============================================================================
 */

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
  int meu_ranque, num_procs, inicio, dest, raiz = 0, tag = 1, stop = 0;
  
  MPI_Status estado;
  MPI_Request requisicao;

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

  /* * ALOCAÇÃO DO BUFFER PARA O MPI_Bsend 
   * O tamanho deve acomodar o dado + overhead interno do MPI para múltiplas mensagens.
   */
  int buf_size = 10000 * (sizeof(int) + MPI_BSEND_OVERHEAD);
  void *buffer = malloc(buf_size);
  MPI_Buffer_attach(buffer, buf_size);

  /* Registra o tempo inicial de execução do programa */
  t_inicial = MPI_Wtime();

  if (meu_ranque == 0) {
    /* MESTRE: Envia pedaços com TAMANHO números para cada processo */
    for (dest = 1, inicio = 3; dest < num_procs && inicio < n; dest++, inicio += TAMANHO) {
      MPI_Bsend(&inicio, 1, MPI_INT, dest, tag, MPI_COMM_WORLD);
    }

    /* Fica recebendo as contagens parciais de cada processo */
    while (stop < (num_procs - 1)) {
      /* Recebimento não-bloqueante seguido de espera (Substitui o MPI_Recv) */
      MPI_Irecv(&cont, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &requisicao);
      MPI_Wait(&requisicao, &estado);
      
      total += cont;
      dest = estado.MPI_SOURCE;

      if (inicio > n) {
        tag = 99; // Sinal de parada
        stop++;
      }
      
      /* Envia um novo pedaço (ou sinal de parada) com Bsend */
      MPI_Bsend(&inicio, 1, MPI_INT, dest, tag, MPI_COMM_WORLD);
      inicio += TAMANHO;
    }
  } else {
    /* TRABALHADOR: Fica num loop aguardando blocos até receber a tag 99 */
    while (1) {
      /* Recebimento não-bloqueante da próxima tarefa */
      MPI_Irecv(&inicio, 1, MPI_INT, raiz, MPI_ANY_TAG, MPI_COMM_WORLD, &requisicao);
      MPI_Wait(&requisicao, &estado);

      /* Verifica se é o sinal de parada */
      if (estado.MPI_TAG == 99) {
        break;
      }

      for (i = inicio, cont = 0; i < (inicio + TAMANHO) && i < n; i += 2) {
        if (primo(i) == 1) cont++;
      }

      /* Envia a contagem parcial para o processo mestre usando Bsend */
      MPI_Bsend(&cont, 1, MPI_INT, raiz, tag, MPI_COMM_WORLD);
    }
  }

  /* Registra o tempo final de execução apenas no mestre (opcional em todos, mas printa no 0) */
  if (meu_ranque == 0) {
    t_final = MPI_Wtime();
    total += 1; /* Acrescenta o 2, que é o único primo par */
    printf("Quant. de primos entre 1 e %d: %d \n", n, total);
    printf("Tempo de execucao: %1.3f \n", t_final - t_inicial);
  }

  /* Desanexa e libera o buffer do MPI */
  MPI_Buffer_detach(&buffer, &buf_size);
  free(buffer);

  /* Finaliza o programa */
  MPI_Barrier(MPI_COMM_WORLD);
  MPI_Finalize();
  return (0);
}