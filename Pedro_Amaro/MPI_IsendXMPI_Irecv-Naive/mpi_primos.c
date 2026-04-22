#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "mpi.h"

int primo(long int n) {
    int i;
    for (i = 3; i < (int)(sqrt(n) + 1); i += 2) {
        if (n % i == 0) return 0;
    }
    return 1;
}

int main(int argc, char* argv[]) {
    double t_inicial, t_final;
    long int i, n;
    int meu_ranque, num_procs;
    long int inicio, salto;
    int cont = 0; 
    int total = 0;

    if (argc < 2) {
        printf("Valor inválido! Entre com um valor do maior inteiro\n");
        return 1;
    }
    n = strtol(argv[1], (char**)NULL, 10);

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &meu_ranque);
    MPI_Comm_size(MPI_COMM_WORLD, &num_procs);

    t_inicial = MPI_Wtime();

    inicio = 3 + (long int)meu_ranque * 2;
    salto  = (long int)num_procs * 2;

    int *buf_workers  = NULL;
    MPI_Request *reqs = NULL;

    if (meu_ranque == 0 && num_procs > 1) {
        buf_workers = (int*)malloc((num_procs - 1) * sizeof(int));
        reqs        = (MPI_Request*)malloc((num_procs - 1) * sizeof(MPI_Request));

        for (int src = 1; src < num_procs; src++) {
            MPI_Irecv(&buf_workers[src - 1], 1, MPI_INT,
                      src, 0, MPI_COMM_WORLD, &reqs[src - 1]);
        }
    }

    for (i = inicio; i <= n; i += salto) {
        if (primo(i) == 1) cont++;
    }
    if (num_procs > 1) {
        if (meu_ranque != 0) {
            MPI_Request req_send;
            MPI_Isend(&cont, 1, MPI_INT,
                      0, 0, MPI_COMM_WORLD, &req_send);
            MPI_Wait(&req_send, MPI_STATUS_IGNORE);
        } else {
            MPI_Waitall(num_procs - 1, reqs, MPI_STATUSES_IGNORE);

            total = cont;
            for (int src = 1; src < num_procs; src++) {
                total += buf_workers[src - 1];
            }

            free(buf_workers);
            free(reqs);
        }
    } else {
        total = cont;
    }

    t_final = MPI_Wtime();

    if (meu_ranque == 0) {
        total += 1;
        printf("Quant. de primos entre 1 e %ld: %d\n", n, total);
        printf("Tempo de execucao: %1.3f s\n", t_final - t_inicial);
        printf("Processos utilizados: %d\n", num_procs);
    }

    MPI_Finalize();
    return 0;
}