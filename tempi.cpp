#include <stdio.h>
#include "openmpi/mpi.h"

#define n 500
#define generazioni 100

void update(bool *, bool *, bool *, bool *, int, int);
unsigned contaVicini(bool *, bool *, bool *, unsigned, unsigned, unsigned, unsigned);
void drawMatrix(bool *);

int main(int argc, char *argv[])
{
    // variabili MPI
    int idProc; // id del processo
    int nProcs; // numero di processi
    int idMaster = 0;
    MPI_Status status;
    MPI_Request request;

    // variabili LOGICHE
    bool *mainMatrix;   // matrice principale
    bool *localMatrix;  // matrice locale ad ogni processo
    bool *localSupport; //matrice locale di supporto
    bool *upRow;        // riga inferiore del processo precedente
    bool *downRow;      // riga superiore del processo successivo
    double start;       // tempo di inizio
    double end;         // tempo di fine

    // inizializzazioni MPI
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &idProc);
    MPI_Comm_size(MPI_COMM_WORLD, &nProcs);
    int nRows = n / nProcs; // numero di righe della localMatrix
    int nCols = n;          // numero di colonne della localMatrix

    // inizializzazioni LOGICHE
    if (idProc == idMaster)
    {
        mainMatrix = new bool[n * n]();
        drawMatrix(mainMatrix);
    }
    localMatrix = new bool[nRows * nCols]();
    upRow = new bool[nCols]();
    downRow = new bool[nCols]();

    // ciclo di simulazione
    if (idProc == idMaster)
        start = MPI_Wtime();
    for (unsigned gen = 1; gen <= generazioni; gen++)
    {
        MPI_Scatter(mainMatrix, nRows * nCols, MPI_CXX_BOOL, localMatrix, nRows * nCols, MPI_CXX_BOOL, idMaster, MPI_COMM_WORLD);

        bool *upRowToSend = new bool[nCols];   // riga superiore da mandare al processo precedente
        bool *downRowToSend = new bool[nCols]; // riga inferiore da mandare al processo successivo
        for (unsigned i = 0; i < nCols; i++)
        {
            upRowToSend[i] = localMatrix[i];
            downRowToSend[i] = localMatrix[nRows * nCols - nCols + i];
        }
        // se sono un processo pari
        if (idProc % 2 == 0)
        {
            if (idProc - 1 >= 0)
                MPI_Send(upRowToSend, nCols, MPI_CXX_BOOL, idProc - 1, 0, MPI_COMM_WORLD);
            if (idProc + 1 < nProcs)
                MPI_Send(downRowToSend, nCols, MPI_CXX_BOOL, idProc + 1, 0, MPI_COMM_WORLD);
            if (idProc - 1 >= 0)
                MPI_Recv(upRow, nCols, MPI_CXX_BOOL, idProc - 1, 0, MPI_COMM_WORLD, &status);
            if (idProc + 1 < nProcs)
                MPI_Recv(downRow, nCols, MPI_CXX_BOOL, idProc + 1, 0, MPI_COMM_WORLD, &status);
        }
        // se sono un processo dispari
        if (idProc % 2 == 1)
        {
            if (idProc - 1 >= 0)
                MPI_Recv(upRow, nCols, MPI_CXX_BOOL, idProc - 1, 0, MPI_COMM_WORLD, &status);
            if (idProc + 1 < nProcs)
                MPI_Recv(downRow, nCols, MPI_CXX_BOOL, idProc + 1, 0, MPI_COMM_WORLD, &status);
            if (idProc - 1 >= 0)
                MPI_Send(upRowToSend, nCols, MPI_CXX_BOOL, idProc - 1, 0, MPI_COMM_WORLD);
            if (idProc + 1 < nProcs)
                MPI_Send(downRowToSend, nCols, MPI_CXX_BOOL, idProc + 1, 0, MPI_COMM_WORLD);
        }

        // lavoro di ogni processo
        localSupport = new bool[n / nProcs * n]();
        update(localMatrix, localSupport, upRow, downRow, nRows, nCols);
        bool *temp = localMatrix;
        localMatrix = localSupport;
        localSupport = localMatrix;

        MPI_Gather(localMatrix, n / nProcs * n, MPI_CXX_BOOL, mainMatrix, n / nProcs * n, MPI_CXX_BOOL, idMaster, MPI_COMM_WORLD);

        delete[] upRowToSend;
        delete[] downRowToSend;
    }
    if (idProc == idMaster)
    {
        double end = MPI_Wtime();

        printf("Generazioni: %d\nCellule: %d\nProcessi: %d\nTempo: %f\n", generazioni, n * n, nProcs, end - start);
    }
    // deallocazioni MPI
    MPI_Finalize();

    return 0;
}

void update(bool *a, bool *b, bool *up, bool *down, int r, int c)
{
    for (unsigned i = 0; i < r; i++)
    {
        for (unsigned j = 0; j < c; j++)
        {
            unsigned vicini = contaVicini(a, up, down, r, c, i, j);
            if (a[i * c + j] && (vicini == 2 || vicini == 3))
                b[i * c + j] = true;
            else if (a[i * c + j] && (vicini < 2 || vicini > 3))
                b[i * c + j] = false;
            else if (!a[i * c + j] && vicini == 3)
                b[i * c + j] = true;
            else
                b[i * c + j] = false;
        }
    }
}

unsigned contaVicini(bool *a, bool *up, bool *down, unsigned r, unsigned c, unsigned i, unsigned j)
{
    unsigned cont = 0;

    // all'interno della piccola matrice
    if (i > 0 && j > 0 && a[(i - 1) * c + j - 1])
        cont++;
    if (i > 0 && a[(i - 1) * c + j])
        cont++;
    if (i > 0 && j < c - 1 && a[(i - 1) * c + j + 1])
        cont++;
    if (j > 0 && a[i * c + j - 1])
        cont++;
    if (j < c - 1 && a[i * c + j + 1])
        cont++;
    if (i < r - 1 && j > 0 && a[(i + 1) * c + j - 1])
        cont++;
    if (i < r - 1 && a[(i + 1) * c + j])
        cont++;
    if (i < r - 1 && j < c - 1 && a[(i + 1) * c + j + 1])
        cont++;

    // bordo superiore
    if (i == 0 && j == 0 && up[0])
        cont++;
    if (i == 0 && j == 0 && up[1])
        cont++;
    if (i == 0 && j == c - 1 && up[c - 1])
        cont++;
    if (i == 0 && j == c - 1 && up[c - 2])
        cont++;
    if (i == 0 && j > 0 && j < c - 1 && up[j - 1])
        cont++;
    if (i == 0 && j > 0 && j < c - 1 && up[j])
        cont++;
    if (i == 0 && j > 0 && j < c - 1 && up[j + 1])
        cont++;

    // bordo inferiore
    if (i == r - 1 && j == 0 && down[0])
        cont++;
    if (i == r - 1 && j == 0 && down[1])
        cont++;
    if (i == r - 1 && j == c - 1 && down[c - 1])
        cont++;
    if (i == r - 1 && j == c - 1 && down[c - 2])
        cont++;
    if (i == r - 1 && j > 0 && j < c - 1 && down[j - 1])
        cont++;
    if (i == r - 1 && j > 0 && j < c - 1 && down[j])
        cont++;
    if (i == r - 1 && j > 0 && j < c - 1 && down[j + 1])
        cont++;

    return cont;
}

void drawMatrix(bool *a)
{
    unsigned seed = 1324324;
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            a[i * n + j] = rand_r(&seed) % 2;
}
