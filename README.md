# Game Of Life
**Inizio sviluppo: Lug. 2019**

**Fine sviluppo: Lug. 2019**

Progetto per il corso di Algoritmi Paralleli e Sistemi Distribuiti

## Introduzione
Il progetto rappresenta la versione parallela dell’automa cellulare “Il Gioco della Vita”.

Il Gioco della Vita è un automa cellulare, nel quale ogni cellula può appartenere a due stati differenti, _VIVA_ o _MORTA_ , seguendo delle semplici regole:

1. Una cellula viva continua a vivere se ha esattamente due o tre cellule vicine vive;
2. Una cellula morta torna in vita se ha esattamente tre cellule vicine vive;
3. Una cellula viva muore per spopolamento se ha un numero di cellule vicine vive minore o uguale ad uno;
4. Una cellula viva muore per sovrappopolamento se ha un numero di cellule vicine vive maggiore o uguale a quattro.

Seguendo queste semplici regole, ogni nuova generazione dell’automa cellulare, dipendente solo dalla precedente generazione, diventa, appunto, il nuovo modello sulla quale si basa la simulazione della generazione successiva.

## Rappresentazione dell’automa

L’automa viene rappresentato tramite un array dinamico monodimensionale di tipo booleano, dove ogni cella può essere:

- <code>false</code>, che rappresenta una cellula _MORTA_;
- <code>true</code>, che rappresenta una cellula _VIVA_.

La scelta di utilizzare array monodimensionali ricade sul fatto che in questo modo le celle sono memorizzate in modo lineare in memoria. Questo concetto è fondamentale per poter utilizzare con facilità le funzioni della libreria <code>MPI</code>.

## Due versioni

Sono stati prodotti due codici, il primo (<code>grafica.cpp</code>), ovvero il più completo, prevede una parte grafica realizzata con la libreria <code>Allegro5</code>, mentre la seconda versione (<code>tempi.cpp</code>), è identica alla prima, con la differenza che sono state rimosse tutte le funzioni che si occupano della grafica e sono state aggiunte un paio di variabili con lo scopo di fornire in output i tempi, lasciando però l’algoritmo intatto.

## L’algoritmo e l’implementazione

Il progetto è stato scritto utilizzando il linguaggio C++ e la libreria <code>MPI</code> che ha permesso la parallelizzazione dell’algoritmo. L’uso di un ambiente di sviluppo basato sul _message passing_ prevede l’uso di funzioni di <code>send</code> e <code>receive</code> tra i vari processi.

L’architettura è del tipo _Master/Slave_ , quindi prevede che un processo _Master_ si occupa della distribuzione dei dati, della ricezione dei dati una volta lavorati dagli _Slaves_ e della grafica, mentre i processi _Slave_ (incluso il _Master_) ricevono i dati, li lavorano e successivamente vengono rispediti al mittente.

Nel progetto, solo il processo _Master_ ha la matrice principale, che viene inizializzata. Ad ogni generazione il master invia a tutti gli altri processi singole porzioni di matrice, attraverso la funzione <code>MPI_Scatter</code>, in modo che ogni processo modifichi la sua matrice locale e poi la rispedisca al processo _Master_ , attraverso la funzione <code>MPI_Gather</code>, che si occuperà poi della stampa.

Lavorare semplicemente la propria sottomatrice locale non basta però! Si deve gestire accuratamente il caso in cui un processo abbia bisogno, solo in lettura, delle celle adiacenti ad una determinata cella, che però sono possedute da un altro processo. Se non venisse gestito questo caso, ogni processo lavorerebbe solo con la sua sottomatrice locale e il risultato sarebbe che la simulazione non sarebbe per niente corretta!

Nel progetto, il problema è stato gestito grazie all’uso di array di supporto che memorizzano, per ogni processo, l’orlo esterno superiore e quello inferiore, ovvero l’ultima riga del processo precedente e la prima riga del processo successivo, quando esistono. Questo ci permette di accedere alle celle adiacenti a quelle che si trovano sul bordo superiore e su quello inferiore, risolvendo il problema. Ad ogni generazione, quindi, ogni processo invia la propria riga superiore al processo precedente e la propria riga inferiore al processo successivo (se il processo è il primo, quindi non ha nessun processo precedente, non invierà la propria riga superiore, stesso discorso per l’ultimo processo che non invierà la propria riga inferiore). Questo meccanismo però provoca sicuramente un _deadlock_ tra i vari processi, perché tutti inviano e si bloccano, aspettando la ricezione degli altri, che non avverrà mai!

È possibile risolvere questo ulteriore problema grazie ad un trucchetto: i processi con <code>idProc</code> pari faranno prima le due <code>MPI_Send</code> e poi le due <code>MPI_Receive</code>, invece i processi con <code>idProc</code> dispari faranno prima le due <code>MPI_Receive</code> e poi le due <code>MPI_Send</code>. Questo ci permette di evitare il _deadlock_ tra i processi e risolve definitivamente il problema di gestione delle cosiddette _Ghost Cells_.

Ogni processo ora si occupa di aggiornare la propria sottomatrice, usando una sottomatrice di supporto per preservare la copia originale di ogni generazione. La matrice aggiornata viene mandata indietro al processo _Master_ che si occuperà della stampa.
