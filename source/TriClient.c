/************************************
*VR457792
*Nicola Tomasoni
*23.01.2025
*************************************/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <signal.h>
#include <pthread.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/sem.h>

#define key 12345

typedef struct {
    char nomeGiocatore[100];
    char simbolo;
    int turno;
} giocatore;

giocatore g;
int semid;
int N;

void handle_sigint(int sig) {    
    if (g.turno == 0)
        kill(getppid(), SIGUSR1);
    else
        kill(getppid(), SIGUSR2); 
    exit(0);  
}

void disegnaGriglia(char *mem) 
{
    system("clear");
    printf("È il turno di %s (Simbolo %c)\n\n", g.nomeGiocatore, g.simbolo);
    for (int i = 0; i < N; i++) 
    {
        for (int j = 0; j < N; j++) 
        {            
            printf(" %c ", mem[i * N + j]);
            if (j < N-1)
                printf("|");
        }
        if (i < N-1) {
            printf("\n");
            for(int i=0; i<N-1; i++)
                printf("-");
            for(int i=0; i<N; i++)
                printf("---");
            printf("\n");
        }
    }
    printf("\n\n\n");
}

bool posizioneOccupata(char *mem, int riga, int colonna) {
    return mem[(riga-1)*N + (colonna-1)] != ' ';
}

int main (int argc, char *argv[])
{
	setbuf(stdout, NULL);           //Disabilita buffer di stdout, evita errori stampa
    signal(SIGINT, handle_sigint);    

    // Importa giocatore corrente
    strcpy(g.nomeGiocatore, argv[1]);
    g.simbolo = argv[2][0];
    g.turno = *(argv[3])-'0';
    N = *(argv[4])-'0';

    // Importa memoria condivisa
    int shmid = shmget(key, sizeof(char[N][N]), S_IRUSR | S_IWUSR);
    if (shmid == -1) {
        perror("shmgetClient");
        return -1;
    }
    char *mem = (char *) shmat (shmid, NULL, 0);
    if (mem == (char *) -1) {
        perror("shmat");
        return -1;
    }

    // Importa semafori
    semid = semget(key, 3, S_IRUSR | S_IWUSR);
    if (semid == -1) {
        perror("semget");
        exit(1);
    }

    while (true) {
        struct sembuf p = {g.turno, -1, 0}; // Decrementa il semaforo del processo corrispondente
        semop(semid, &p, 1);

        // SEZIONE CRITICA        
        disegnaGriglia(mem);

        int riga, colonna;
        bool mossaValida = false;
        while (!mossaValida)
        {
            printf("Scegli la riga dove mettere il simbolo (1, 2, 3): ");
            if (scanf("%d", &riga) != 1 || riga < 1 || riga > N) {
                disegnaGriglia(mem);
                printf("Mossa non valida.\n");
                while(getchar() != '\n');
                    continue;
            }       

            printf("Scegli la colonna dove mettere il simbolo (1, 2, 3): ");
            if (scanf("%d", &colonna) != 1 || colonna < 1 || colonna > N || posizioneOccupata(mem, riga, colonna)) {
                disegnaGriglia(mem);
                printf("Mossa non valida.\n");
                while(getchar() != '\n');
                    continue;
            }             

            mossaValida = true;
        }        

        mem[(riga-1)*N + (colonna-1)] = g.simbolo;
        disegnaGriglia(mem);        //viene sovrascritto a meno che qualcuno vinca
        struct sembuf v = {2, 1, 0};
        semop(semid, &v, 1);           
    }

    return 0;
}