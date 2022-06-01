#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <stdbool.h>
#include <sys/sem.h>
#include <errno.h>
#include <pthread.h>

#define NB_ABONNEMENTS 1000

pthread_t tid[10];
pthread_t pMediateur;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t attendre = PTHREAD_COND_INITIALIZER;

typedef struct {
    bool estAbonne;
    int code;
} Usager;

int msg_id;
int abonnements[NB_ABONNEMENTS];

void error(char* message) 
{
    fprintf(stderr, "%s\n", message);
}

bool booleen_aleatoire()
{
    return rand() & 1;
}

int nombre_aleatoire(int min, int max)
{
  return(min + (rand() % (max - min)));
}

int creerUsager()
{
    srand(time(NULL) ^ pthread_self());
    bool rand_abonne = booleen_aleatoire();
    int code;

    if (rand_abonne) {
        code = nombre_aleatoire(2, 1001);
    } else {
        code = 1;
    }

    // Création de l'usager
    Usager usager;
    usager.estAbonne = rand_abonne;
    usager.code = code;

    printf("Usager créé\nEst abonné : %d\nCode Abonné : %d\n", usager.estAbonne, usager.code);

    // Ajout de l'usager dans la file de message
    if (msgsnd(msg_id, &usager, sizeof(Usager) - sizeof(long), 0) == -1) {
        perror("Erreur de lecture requete \n");
        exit(1);
    }

    

    printf("Thread usager créé !\n");
}


int creerMediateur()
{

    while (1) {
        // Le médiateur lit continuellement dans la file de messages
        Usager response;
        if (msgrcv(msg_id, &response, sizeof(Usager)- sizeof(long), 0, 0) == -1) {
            perror("Erreur de lecture requete \n");
            exit(1);
        }

        printf("Le médiateur lit dans la file de messages : \n");
        printf("Usager lu dans la file\nEst abonné : %d\nCode Abonné : %d\n", response.estAbonne, response.code);
    }
}

void* fonc_mediateur()
{
    creerMediateur();
}

void* fonc_usager()
{
    creerUsager();
}


int main(int argc, char* argv[])
{
    //Seed aléatoire
    srand(time(NULL));

    // Générer clé aléatoire pour la file de message
    key_t key;
    if ((key = ftok("./", 'A')) == -1) {
        perror("Erreur de creation de la clé \n");
        exit(1);
    }

    // Créer la file de message
    if ((msg_id = msgget(key, 0750 | IPC_CREAT)) == -1) {
        perror("Erreur de creation de la file\n");
        exit(1);
    }
    
    // On initialise les abonnements
    for (int i=2; i<=NB_ABONNEMENTS+1; i++) {
        abonnements[i-2] = i;
    }

    for (int i = 0; i<10; i++) {
        pthread_create(tid + i,NULL,fonc_usager,NULL);
    }
    
    pthread_create(&pMediateur,NULL,fonc_mediateur,NULL);


    for (int i = 0; i<10; i++) {
        pthread_join(tid[i], NULL);
    }

    pthread_join(pMediateur, NULL);

    while (wait(NULL) > 0);
    msgctl(msg_id, IPC_RMID, NULL);
    
    return EXIT_SUCCESS;
}