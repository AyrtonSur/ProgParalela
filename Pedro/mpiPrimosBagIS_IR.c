#include <stdio.h>
#include <stdlib.h>
#include "mpi.h"
#include <math.h>

#define TAMANHO 500000

int primo(int n) {
    int i;
    for (i = 3; i < (int)(sqrt(n) + 1); i += 2) {
        if (n % i == 0) return 0;
    }
    return 1;
}

int main(int argc, char *argv[]) {
    double t_inicial, t_final;
    int cont = 0, total = 0, i, n;
    int meu_ranque, num_procs, inicio, dest, raiz = 0, stop = 0;
    int inicio_atual, proximo_inicio;
    
    MPI_Status estado;
    MPI_Request req_send, req_recv;

    if (argc < 2) {
        if (meu_ranque == 0) printf("Entre com o valor de N como parâmetro.\n");
        return 0;
    }
    n = strtol(argv[1], (char **) NULL, 10);

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &meu_ranque);
    MPI_Comm_size(MPI_COMM_WORLD, &num_procs);

    if (num_procs < 2) {
        if (meu_ranque == 0) printf("Este programa exige pelo menos 2 processos.\n");
        MPI_Abort(MPI_COMM_WORLD, 1);
        return(1);
    }

    t_inicial = MPI_Wtime();

    if (meu_ranque == 0) { 
        /* --- MESTRE --- */
        // Distribuição inicial (usando < n conforme o original)
        for (dest = 1, inicio = 3; dest < num_procs && inicio < n; dest++, inicio += TAMANHO) {
            MPI_Isend(&inicio, 1, MPI_INT, dest, 1, MPI_COMM_WORLD, &req_send);
            MPI_Wait(&req_send, MPI_STATUS_IGNORE);
        }

        // Encerra processos excedentes
        for (; dest < num_procs; dest++) {
            MPI_Isend(&inicio, 1, MPI_INT, dest, 99, MPI_COMM_WORLD, &req_send);
            MPI_Wait(&req_send, MPI_STATUS_IGNORE);
            stop++;
        }

        while (stop < (num_procs - 1)) {
            MPI_Irecv(&cont, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &req_recv);
            MPI_Wait(&req_recv, &estado);
            
            total += cont;
            dest = estado.MPI_SOURCE;

            int tag_envio = 1;
            if (inicio >= n) { // Se o próximo início já passou do limite
                tag_envio = 99;
                stop++;
            }

            MPI_Isend(&inicio, 1, MPI_INT, dest, tag_envio, MPI_COMM_WORLD, &req_send);
            MPI_Wait(&req_send, MPI_STATUS_IGNORE);
            inicio += TAMANHO;
        }
    } 
    else { 
        /* --- ESCRAVO --- */
        MPI_Irecv(&proximo_inicio, 1, MPI_INT, raiz, MPI_ANY_TAG, MPI_COMM_WORLD, &req_recv);
        MPI_Wait(&req_recv, &estado);

        while (estado.MPI_TAG != 99) {
            inicio_atual = proximo_inicio;
            
            // Pre-fetch da próxima tarefa
            MPI_Irecv(&proximo_inicio, 1, MPI_INT, raiz, MPI_ANY_TAG, MPI_COMM_WORLD, &req_recv);

            cont = 0;
            // Limite < n exatamente como solicitado
            for (i = inicio_atual; i < (inicio_atual + TAMANHO) && i < n; i += 2) {
                if (primo(i)) cont++;
            }

            MPI_Isend(&cont, 1, MPI_INT, raiz, 1, MPI_COMM_WORLD, &req_send);
            MPI_Wait(&req_send, MPI_STATUS_IGNORE);

            MPI_Wait(&req_recv, &estado);
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
    return(0);
}