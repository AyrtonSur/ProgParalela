#include <stdio.h>
#include <stdlib.h>
#include "mpi.h"
#include <math.h>

#define TAMANHO 500000

int primo(int n) {
    int i;
    if (n < 2) return 0;
    if (n % 2 == 0 && n > 2) return 0;
    for (i = 3; i < (int)(sqrt(n) + 1); i += 2) {
        if (n % i == 0) return 0;
    }
    return 1;
}

int main(int argc, char *argv[]) {
    double t_inicial, t_final;
    int cont = 0, total = 0;
    int i, n;
    int inicio_atual, proximo_inicio;
    int meu_ranque, num_procs, inicio, dest, raiz = 0, tag = 1, stop = 0;
    MPI_Status estado;
    MPI_Request requisicao;

    if (argc < 2) {
        printf("Entre com o valor do maior inteiro como parâmetro.\n");
        return 0;
    }
    n = strtol(argv[1], (char **)NULL, 10);

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &meu_ranque);
    MPI_Comm_size(MPI_COMM_WORLD, &num_procs);

    if (num_procs < 2) {
        if (meu_ranque == 0) printf("Este programa exige pelo menos 2 processos.\n");
        MPI_Abort(MPI_COMM_WORLD, 1);
        return (1);
    }

    t_inicial = MPI_Wtime();

    if (meu_ranque == 0) {
        /* MESTRE */
        // Distribuição inicial síncrona
        for (dest = 1, inicio = 3; dest < num_procs && inicio < n; dest++, inicio += TAMANHO) {
            MPI_Ssend(&inicio, 1, MPI_INT, dest, tag, MPI_COMM_WORLD);
        }

        // Mata escravos excedentes imediatamente
        for (; dest < num_procs; dest++) {
            MPI_Ssend(&inicio, 1, MPI_INT, dest, 99, MPI_COMM_WORLD);
            stop++;
        }

        while (stop < (num_procs - 1)) {
            // Recebe resultado
            MPI_Irecv(&cont, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &requisicao);
            MPI_Wait(&requisicao, &estado);
            total += cont;
            dest = estado.MPI_SOURCE;

            if (inicio > n) {
                tag = 99;
                stop++;
            }

            // Envio síncrono da nova tarefa (ou sinal de morte)
            MPI_Ssend(&inicio, 1, MPI_INT, dest, tag, MPI_COMM_WORLD);
            inicio += TAMANHO;
        }
    } 
    else {
        /* ESCRAVO */
        // Recebe a primeira tarefa
        MPI_Irecv(&proximo_inicio, 1, MPI_INT, raiz, MPI_ANY_TAG, MPI_COMM_WORLD, &requisicao);
        MPI_Wait(&requisicao, &estado);

        while (estado.MPI_TAG != 99) {
            inicio_atual = proximo_inicio;

            // Prefetching: Pede a próxima enquanto calcula a atual
            MPI_Irecv(&proximo_inicio, 1, MPI_INT, raiz, MPI_ANY_TAG, MPI_COMM_WORLD, &requisicao);

            // Cálculo usando a variável local correta
            for (i = inicio_atual, cont=0; i < (inicio_atual + TAMANHO) && i < n; i+=2) {
                if (primo(i) == 1)
                    cont++;
            }
            
            // O escravo só avançará quando o mestre der o Irecv/Recv
            MPI_Ssend(&cont, 1, MPI_INT, raiz, 1, MPI_COMM_WORLD);

            // Espera a próxima tarefa chegar
            MPI_Wait(&requisicao, &estado);
        }
    }

    if (meu_ranque == 0) {
        t_final = MPI_Wtime();
        total += 1; // Soma o número 2
        printf("Quant. de primos entre 1 e %d: %d \n", n, total);
        printf("Tempo de execucao: %1.3f \n", t_final - t_inicial);
    }

    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Finalize();
    return (0);
}