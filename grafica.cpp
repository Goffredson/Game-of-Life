#include <stdio.h>
#include <unistd.h>
#include <allegro5/allegro.h>
#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_ttf.h>
#include "openmpi/mpi.h"

#define n 60
#define generazioni 10000
#define rest 10000 // in microsecondi

void stampaMatrice(bool *, unsigned, ALLEGRO_FONT *, int);
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

    //variabili ALLEGRO
    ALLEGRO_DISPLAY *display;
    ALLEGRO_FONT *font;
    ALLEGRO_EVENT_QUEUE *queue;
    int size = 10;

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

    // inizializzazioni ALLEGRO
    if (idProc == idMaster)
    {
        al_init();
        al_init_primitives_addon();
        al_init_font_addon();
        al_init_ttf_addon();
        queue = al_create_event_queue();
        font = al_load_ttf_font("font.ttf", 20, 0);
        display = al_create_display(n * size, n * size);
        al_register_event_source(queue, al_get_display_event_source(display));
    }

    // ciclo di simulazione
    bool ok = true;
    for (unsigned gen = 1; gen <= generazioni && ok; gen++)
    {
        if (idProc == idMaster)
        {
            ALLEGRO_EVENT ev;
            ALLEGRO_TIMEOUT timeout;
            al_init_timeout(&timeout, 0.06);
            if (al_wait_for_event_until(queue, &ev, &timeout) && ev.type == ALLEGRO_EVENT_DISPLAY_CLOSE)
                ok = false;
            for (int i = 1; i < nProcs; i++)
                MPI_Send(&ok, 1, MPI_CXX_BOOL, i, 0, MPI_COMM_WORLD);
        }
        if (idProc != idMaster)
            MPI_Recv(&ok, 1, MPI_CXX_BOOL, idMaster, 0, MPI_COMM_WORLD, &status);

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

        if (idProc == idMaster)
        {
            stampaMatrice(mainMatrix, gen, font, size);
        }

        delete[] upRowToSend;
        delete[] downRowToSend;
    }

    // deallocazioni ALLEGRO
    if (idProc == idMaster)
    {
        al_destroy_display(display);
    }

    // deallocazioni MPI
    MPI_Finalize();

    return 0;
}

void stampaMatrice(bool *m, unsigned g, ALLEGRO_FONT *font, int size)
{
    system("clear");
    printf("Generazione: %d\nPremi ctrl+C per interrompere\n", g);

    al_clear_to_color(al_map_rgb(255, 255, 255));
    for (int i = 0; i < n; i++)
    {
        for (int j = 0; j < n; j++)
        {
            if (m[i * n + j])
                al_draw_filled_rectangle(i * size, j * size, i * size + size, j * size + size, al_map_rgb(0, 255, 0));
        }
    }
    al_draw_textf(font, al_map_rgb(0, 0, 0), 0, 0, 0, "Generazione: %d", g);
    al_draw_textf(font, al_map_rgb(0, 0, 0), 0, 20, 0, "Premi Alt+F4 o chiudi la finestra per uscire");
    al_flip_display();
    usleep(rest);
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
