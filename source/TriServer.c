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
#include <errno.h>

#define key 12345

typedef struct {
	char nomeGiocatore[100];
	char simbolo;
	int turno;
} giocatore;

giocatore primo, secondo;
giocatore *vincitore; 
bool quit = false;
int N;
int semid, shmid, turno;
char *mem;
pid_t client1, client2;	

void cleanup() {    	
	kill(client1, SIGTERM);
    kill(client2, SIGTERM); 
    if (shmdt(mem) == -1) 
        perror("shmdt"); 
    if (shmctl(shmid, IPC_RMID, NULL) == -1) 
        perror("shmctl");
    if (semctl(semid, 0, IPC_RMID) == -1) 
        perror("semctl IPC_RMID");      
	exit(0);
}

void handle_sigint(int sig) {
	if(quit) 
		cleanup();  
    printf("\nSegnale di terminazione ricevuto.\nPremere di nuovo per uscire.\n");
    quit = true;
}

void handle_sigusr1(int sig) {
    printf("\n%s ha abbandonato la partita.\n%s vince a tavolino!\n\n", primo.nomeGiocatore, secondo.nomeGiocatore);
    cleanup();
}

void handle_sigusr2(int sig) {
    printf("\n%s ha abbandonato la partita.\n%s vince a tavolino!\n\n", secondo.nomeGiocatore, primo.nomeGiocatore);
    cleanup();
}

void sigHandler(int sig)            //effetua catch timer esaurito
{ 
    printf("\n\nTempo esaurito! La partita è vinta a tavolino.\n");
    if(turno%2 == 0)
    	printf("\n%s ha vinto!\n\n\n", primo.nomeGiocatore);
    else
    	printf("\n%s ha vinto!\n\n\n", secondo.nomeGiocatore);
    cleanup();
}

union semun {
    int val;
    struct semid_ds * buf;
    unsigned short * array;
};

giocatore* checkLine(char linea[]) {
	for (int i = 1; i < N; i++) 
        if (linea[i] != linea[0]) 
            return NULL; 				// Non tutti gli elementi sono uguali        
    
    // Se tutti gli elementi sono uguali, verifica a quale giocatore appartiene 
    if (linea[0] == primo.simbolo)
        return &primo;
    else if (linea[0] == secondo.simbolo)
        return &secondo;
    return NULL;
}

giocatore* checkTris(char *mem) {
    // Controllo orizzontale e verticale
	char riga[N];
	char colonna[N];
    for (int i = 0; i < N; i++) {
	    for (int j = 0; j < N; j++) {
	    	riga[j] = mem[i * N + j];
	    	colonna[j] = mem[i + j * N];
	    } 
		
		if((sizeof(riga) / sizeof(riga[0])) != N) 
			return NULL;
	    
	   	vincitore = checkLine(riga);	   	
	    if (vincitore) return vincitore;   
	    vincitore = checkLine(colonna);	   	
	    if (vincitore) return vincitore;  
	}

	// Controllo diagonale
	char diagonale1[N];
	char diagonale2[N];
	for(int i=0; i<N; i++)
		diagonale1[i] = mem[N*i + i];	
	for(int i=0; i<N; i++)
		diagonale2[i] = mem[(N-1) + (N-1)*i];
	
    giocatore *vincitore = checkLine(diagonale1);
    if (vincitore) return vincitore;
    return checkLine(diagonale2);	
}

int main (int argc, char *argv[])
{
	setbuf(stdout, NULL);
	if(argc != 4) {
		printf("\nParametri mancanti.\nPer avviare correttamente la partita"
		" specificare il tempo massimo per ogni turno\n(in secondi) e i "
		"simboli dei due giocatori.\n\nEs:\n./TriServer 10 X O\n\n"
		"Nota: per disabilitare il timer inserire 0 secondi\n\n\n");
		exit(0);
	}

	// SEGNALI
	signal(SIGINT, handle_sigint);
	signal(SIGUSR1, handle_sigusr1);
	signal(SIGUSR2, handle_sigusr2);

	// IMPORTO PARAMETRI
	primo.simbolo = *argv[2];
	secondo.simbolo = *argv[3];
	primo.turno = 0;
	secondo.turno = 1;

	// TIMER
    system("clear");
	int timer = atoi(argv[1]);
	if (timer == 0)
		printf("Timer disabilitato.\n");
	else
		printf("Timer impostato su %d secondi.\n", timer);

	// CREAZIONE GRIGLIA
	printf("Inserisci un numero per le righe e le colonne della griglia da gioco (minimo 3): ");
    int input = scanf("%d", &N);
    while ((input != 1) || (N < 3))
    {
        printf("Numero non valido (minimo 3): ");
        while (getchar() != '\n');

        input = scanf("%d", &N);
    } 

	char griglia[N][N];
	for(int i=0; i<N; i++)
		for (int j=0; j<N; j++)
			griglia[i][j] = ' ';	//Da inizializzare vuota
	
	// MEMORIA CONDIVISA
	shmid = shmget(key, 0, 0); 	// Controlla se esiste già
	if (shmid != -1) 			// Stessa chiave ma dimensione diversa porta a Invalid Argument
	    shmctl(shmid, IPC_RMID, NULL); 
	
	size_t memSize = N * N * sizeof(char);
	shmid = shmget(key, memSize, IPC_CREAT | S_IRUSR | S_IWUSR);
	if (shmid == -1) {
		perror("shmgetServer");
		//printf("Errno: %d\n", errno);
		return -1;
	}
	mem = (char *) shmat (shmid, NULL, 0);
	if (mem == (char *) -1) {
		perror("shmat");
		return -1;
	}
	memcpy(mem, griglia, sizeof(griglia));

	// INIZIALIZZAZIONE SEMAFORI
	semid = semget(key, 3, IPC_CREAT | S_IRUSR | S_IWUSR);
	if (semid == -1) {
        perror("semget");
        exit(1);
    }
    unsigned short semInitVal[] = {1, 0, 0};	//client1, client2, server
    union semun arg;
    arg.array = semInitVal;    
    semctl(semid, 0, SETALL, arg);
	
	// PTHREAD	
	printf("Inserisci il nome del primo giocatore (Simbolo %c): ", primo.simbolo);
	scanf("%s", primo.nomeGiocatore);
	printf("Inserisci il nome del secondo giocatore (Simbolo %c): ", secondo.simbolo);
	scanf("%s", secondo.nomeGiocatore);
	char n = N + '0';
	client1 = fork();
	if(client1 == 0) {
		char c = primo.turno + '0';
		execl("./TriClient", "./TriClient", primo.nomeGiocatore, &primo.simbolo, &c, &n, NULL);
		perror("giocatore1");
		return -1;
	}
	else
	{	
		client2 = fork();
		if(client2 == 0)
		{
			char c = secondo.turno + '0';
			execl("./TriClient", "./TriClient", secondo.nomeGiocatore, &secondo.simbolo, &c, &n, NULL);	//execl accetta solo *char
			perror("giocatore2");
			return -1;
		}
		signal(SIGALRM, sigHandler);
    	alarm(timer);
	}

	giocatore* vincitore = NULL;
	turno = 1;							//Tiene traccia del turno corrente

	while (vincitore == NULL)
	{
		struct sembuf p = {2, -1, 0};	//Definisce operazione che decrementa di 1 il semaforo n.2
        semop(semid, &p, 1);			//Aspetta di poter eseguire l'operazione
        alarm(0);

        //SEZIONE CRITICA
		if((vincitore = checkTris(mem)) != NULL)
			printf("\n%s ha vinto!\n\n\n", vincitore->nomeGiocatore);	
		else if (turno < (N*N)) {
			struct sembuf v2 = {turno%2, 1, 0}; 	//Se la partita non è ancora finita permette al prossimo giocatore
			semop(semid, &v2, 1);					//di continuare sbloccando il suo semaforo
			turno++;								//turni pari significa che %2 = 0 e quindi tocca primo giocatore
			alarm(timer);
		}
		else {
			printf("La partita finisce in parità!\n\n\n");
			break;
		}		
	}

	cleanup();
}