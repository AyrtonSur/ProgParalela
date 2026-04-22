/*
=======================================================================================
DOCUMENTAÇÃO DE OTIMIZAÇÃO E REATORAÇÃO (BAG OF TASKS)
=======================================================================================

Estratégia de Ganho de Performance:
A substituição do `MPI_Send` (bloqueante) por `MPI_Isend` (não-bloqueante) aliada ao
`MPI_Recv` reduz o overhead de sincronização de rede ("handshake"), permitindo o 
sobreposicionamento (overlap) de comunicação. 

1. No Mestre (Processo 0): 
Ao invés de esperar que a rede confirme o envio do pacote de trabalho para um nó 
escravo (o que poderia causar atrasos caso o buffer do destinatário estivesse cheio), 
o mestre utiliza o `MPI_Isend` para apenas despachar o trabalho e retornar 
imediatamente para o `MPI_Recv` à espera de resultados de QUALQUER outro processo 
(`MPI_ANY_SOURCE`). Isso maximiza a responsividade do mestre na distribuição de tarefas.
Para evitar condição de corrida (onde a variável sobrescreve o valor antes de ser enviada),
foi alocado um vetor de buffers de envio `inicio_buf[num_procs]`.

2. Nos Escravos:
Ao calcular os números primos do seu chunk, o escravo envia o resultado via `MPI_Isend`. 
Ele só aguarda a conclusão (via `MPI_Wait`) no ciclo seguinte, garantindo que o tempo de
cálculo do próximo bloco mascare o tempo de transferência da resposta anterior.
=======================================================================================
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

  if (meu_ranque == 0) {
    /* Arrays para Requests e Buffers individuais por processo para os envios não-bloqueantes */
    MPI_Request *reqs = (MPI_Request*) malloc(num_procs * sizeof(MPI_Request));
    int *inicio_buf = (int*) malloc(num_procs * sizeof(int));
    
    for (int j = 0; j < num_procs; j++) {
        reqs[j] = MPI_REQUEST_NULL;
    }

    /* Envia pedaços com TAMANHO números para cada processo */
    for (dest = 1, inicio = 3; dest < num_procs && inicio < n; dest++, inicio += TAMANHO) {
      inicio_buf[dest] = inicio;
      MPI_Isend(&inicio_buf[dest], 1, MPI_INT, dest, tag, MPI_COMM_WORLD, &reqs[dest]);
    }

    /* Fica recebendo as contagens parciais de cada processo */
    while (stop < (num_procs - 1)) {
      MPI_Recv(&cont, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &estado);
      total += cont;
      dest = estado.MPI_SOURCE;
      
      /* Aguarda conclusão do último Isend para este destino específico antes de reutilizar o buffer */
      if (reqs[dest] != MPI_REQUEST_NULL) {
          MPI_Wait(&reqs[dest], MPI_STATUS_IGNORE);
      }

      if (inicio > n) {
        tag = 99;
        stop++;
      }
      
      /* Envia um novo pedaço com TAMANHO números para o mesmo processo */
      inicio_buf[dest] = inicio;
      MPI_Isend(&inicio_buf[dest], 1, MPI_INT, dest, tag, MPI_COMM_WORLD, &reqs[dest]);
      inicio += TAMANHO;
    }
    
    /* Cleanup mestre */
    free(reqs);
    free(inicio_buf);

  } else {
    MPI_Request req_envio = MPI_REQUEST_NULL;
    int meu_cont = 0; /* Buffer estável para o envio assíncrono */

    /* Cada processo escravo recebe o início do espaço de busca */
    while (1) {
      MPI_Recv(&inicio, 1, MPI_INT, raiz, MPI_ANY_TAG, MPI_COMM_WORLD, &estado);
      
      if (estado.MPI_TAG == 99) {
        break;
      }

      for (i = inicio, cont = 0; i < (inicio + TAMANHO) && i < n; i += 2) {
        if (primo(i) == 1) cont++;
      }

      /* Garante que o envio não-bloqueante da iteração anterior terminou antes de atualizar o buffer */
      if (req_envio != MPI_REQUEST_NULL) {
        MPI_Wait(&req_envio, MPI_STATUS_IGNORE);
      }
      
      meu_cont = cont; 
      /* Envia a contagem parcial para o processo mestre via Isend */
      MPI_Isend(&meu_cont, 1, MPI_INT, raiz, tag, MPI_COMM_WORLD, &req_envio);
    }
    
    /* Garante finalização de qualquer mensagem pendente antes de sair */
    if (req_envio != MPI_REQUEST_NULL) {
        MPI_Wait(&req_envio, MPI_STATUS_IGNORE);
    }

    /* Registra o tempo final de execução nos escravos se necessário */
    t_final = MPI_Wtime();
  }

  if (meu_ranque == 0) {
    t_final = MPI_Wtime();
    total += 1; /* Acrescenta o 2, que é primo */
    printf("Quant. de primos entre 1 e %d: %d \n", n, total);
    printf("Tempo de execucao: %1.3f \n", t_final - t_inicial);
  }

  /* Finaliza o programa */
  MPI_Barrier(MPI_COMM_WORLD);
  MPI_Finalize();
  return (0);
}