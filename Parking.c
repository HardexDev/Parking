#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <stdbool.h>
#include <sys/sem.h>
#include <errno.h>
#include <pthread.h>



#define PROBA_ABONNE 0.2

int NB_PLACE_ABO = 10;
int NB_PLACE_NON_ABO = 10;
float FAC_DEBORDEMENT = 0;
float temps_accel = 0;


pthread_mutex_t mutex_cpt = PTHREAD_COND_INITIALIZER;
pthread_cond_t attendre = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex = PTHREAD_COND_INITIALIZER;

int msg_id; //file de message



typedef struct {
    int estAbonne; //1: non abo, 2 : abo    si on met 0 ca plante jcp pq
    int type; //0:entrer, 1:sortir
    pthread_t threadID;
} Usager;


void Attendre(int temps_simule_secondes){
    usleep((float)temps_simule_secondes*temps_accel/3600.0*1000*1000);
}


void *timer_function(void* arg){
    int heure = 0;
    int nb_place_non_abo_init = NB_PLACE_NON_ABO;
    int aug = (int)NB_PLACE_NON_ABO*(FAC_DEBORDEMENT/4)/100;
    bool premier_passage = true;
    while(true){
        Attendre(3600);
        if(heure == 18 || heure == 19 || heure == 20 || heure == 21){
            printf("Heure : %d       zone de débordement augmentation de %d places\n", heure, aug);
            pthread_mutex_lock(&mutex_cpt);
            NB_PLACE_NON_ABO += aug;
            //printf("Nombre de places non abonnées : %d\n", NB_PLACE_NON_ABO);
            pthread_mutex_unlock(&mutex_cpt);
        }
        if(heure == 8 && !premier_passage){
            pthread_mutex_lock(&mutex_cpt);
            NB_PLACE_NON_ABO -= 4*aug; //peut être négatif -> personne ne peut rentrer
            //printf("Nombre de places non abonnées : %d\n", NB_PLACE_NON_ABO);
            pthread_mutex_unlock(&mutex_cpt);
            printf("Heure : %d        fin zone de débordement nombre maximal de place non abo dispo : %d\n", heure, nb_place_non_abo_init);
        }
        if(heure == 24){
            heure = 0;
            premier_passage = false;
        }
        heure++;
    }
}



void traitantSIGUSR2(int num) {
    if (num!=SIGUSR2)
        printf("erreur SIGUSR2\n");
}
void traitantSIGUSR1(int num) {
    if (num!=SIGUSR1)
        printf("erreur SIGUSR1\n");
    printf("L'usager avance vers la barrière\n");
    printf("le feu passe au rouge\n");
}

void voiture(int arg){
    int est_abonne = 1;
    if(rand()%100 < PROBA_ABONNE*100){
        est_abonne = 2;
    }
    Attendre(rand()%3600);
    printf("La Voiture %d arrivee vers le parking et envoie un message              voiture abonnée ? : %s\n", (int)arg, est_abonne == 1 ? "non" : "oui");
    Usager usager;
    usager.estAbonne = est_abonne;
    usager.type = 0;
    usager.threadID = pthread_self();

    // Ajout de l'usager dans la file de message
    if(msgsnd(msg_id, &usager, sizeof(Usager) - sizeof(long), 0) == -1) {
        perror("Erreur de envoie requete \n");
        exit(1);
    }
    
    sigset_t new_mask, old_mask;
    sigemptyset(&new_mask);
    sigaddset(&new_mask, SIGUSR1); //ajoute sigusr1 dans le masque
    sigprocmask(SIG_BLOCK, &new_mask, &old_mask); //si sigusr1 arrive, le programme bloque

    signal(SIGUSR2,traitantSIGUSR2);
    signal(SIGUSR1,traitantSIGUSR1);
    sigsuspend(&old_mask); //attente d'une réponse
    printf("La Voiture %d entre dans le parking (verrouille la ressource critique)\n", (int)arg);
    Attendre(rand()%5);
    printf("La Voiture %d est garée (déverrouille la ressource critique)\n\n", (int)arg);
    pthread_cond_signal(&attendre);


    Attendre(rand()%86400);
    printf("La Voiture %d veut sortir du parking donc envoye un message\n", (int)arg);
    usager.type = 1;
    if(msgsnd(msg_id, &usager, sizeof(Usager) - sizeof(long), 0) == -1) {
        perror("Erreur de envoie requete \n");
        exit(1);
    }
    sigsuspend(&old_mask); //attente d'une réponse
    printf("La Voiture %d sort du parking (verrouille la ressource critique)\n", (int)arg);
    pthread_cond_signal(&attendre);
    printf("La Voiture %d est sortie du parking et part (déverrouille la ressource critique)\n\n", (int)arg);
}


void *parking(void* arg){
    pthread_t Abo_sur_place_non_abo[NB_PLACE_NON_ABO];
    for(int i = 0; i < NB_PLACE_NON_ABO; i++){
        Abo_sur_place_non_abo[i] = 0;
    }
    printf("Le Parking est ouvert\n");
    printf("Nombre de places non abonnées : %d\n", NB_PLACE_NON_ABO);
    printf("Nombre de places abonnées : %d\n", NB_PLACE_ABO);
    printf("Nombre de places total : %d\n", NB_PLACE_NON_ABO+NB_PLACE_ABO);
    while (1) {
        // Le médiateur lit continuellement dans la file de messages
        Usager response;
        if (msgrcv(msg_id, &response, sizeof(Usager)- sizeof(long), 0, 0) == -1) {
            perror("Erreur de lecture requete \n");
            exit(1);
        }

        pthread_mutex_lock(&mutex_cpt);
        if(response.type == 0){ //entrer
            if(response.estAbonne == 1){
                if(NB_PLACE_NON_ABO > 0){
                    NB_PLACE_NON_ABO--;
                    printf("\nNombre de places non abonnées disponible: %d\nle feu passe au vert\n", NB_PLACE_NON_ABO);
                    pthread_kill(response.threadID, SIGUSR1);
                    pthread_cond_wait(&attendre, &mutex);//il se met en attente
                }
                else{
                    printf("Nombre de places non abonnées disponible: %d\nPlus de place non abonnées disponibles\nsignal SIGUSR2\n", NB_PLACE_NON_ABO);
                    pthread_kill(response.threadID, SIGUSR2);
                }
            }
            else{
                if(NB_PLACE_ABO > 0){
                    NB_PLACE_ABO--;
                    printf("Nombre de places abonnées disponible: %d\nsle feu passe au vert\n", NB_PLACE_ABO);
                    pthread_kill(response.threadID, SIGUSR1);
                    pthread_cond_wait(&attendre, &mutex);//il se met en attente
                }
                else{
                    printf("Plus de place abonnées disponibles\n");
                    if(NB_PLACE_NON_ABO > 0){
                        //retenir son  ID
                        for(int i = 0; i < NB_PLACE_NON_ABO; i++){
                            if(Abo_sur_place_non_abo[i] == 0){
                                Abo_sur_place_non_abo[i] = response.threadID;
                                break;
                            }
                        }
                        NB_PLACE_NON_ABO--;
                        printf("Nombre de places non abonnées disponible: %d\nle feu passe au vert\n", NB_PLACE_NON_ABO);
                        pthread_kill(response.threadID, SIGUSR1);
                        pthread_cond_wait(&attendre, &mutex);//il se met en attente
                    }
                    else{
                        printf("Nombre de places non abonnées disponible : %d\nPlus de place abonnées ou non abonnées disponibles\nsignal SIGUSR2\n", NB_PLACE_NON_ABO);
                        pthread_kill(response.threadID, SIGUSR2);
                    }
                    
                }
            }
        }
        else{ //sortir
            if(response.estAbonne == 1){ //verif sa place pour decrémenter
                bool done = false;
                for(int i = 0; i < NB_PLACE_NON_ABO; i++){
                    if(Abo_sur_place_non_abo[i] == response.threadID){
                        Abo_sur_place_non_abo[i] = 0;
                        NB_PLACE_NON_ABO++;
                        done = true;
                        break;
                    }
                }
                if(!done){
                    NB_PLACE_ABO++;
                }
                pthread_kill(response.threadID, SIGUSR2);
                printf("le feu passe au vert \n");
                pthread_cond_wait(&attendre, &mutex);//il se met en attente
            }
            else{
                NB_PLACE_NON_ABO++;
                pthread_kill(response.threadID, SIGUSR2);
                printf("le feu passe au vert \n");
                pthread_cond_wait(&attendre, &mutex);//il se met en attente
            }
        }
        pthread_mutex_unlock(&mutex_cpt);
    }
}

void generateur_voiture(int arg){
    long num = 0;
    while(true){
        pthread_t voiture_i;
        pthread_create(&voiture_i, NULL, voiture, num);
        Attendre(rand()%60);
        num++;
    }
    
}

int main(int argc, char *argv[]) // nb place abo - nb place non abo - facteur temps
{
    if (argc-1 != 4) {
        fprintf(stderr,"Nombre de place abonées : <nb places>  Nombre de place non abonées : <nb places>  Taille zone de debordement <facteur %%> Facteur temps : <1 heure simulé = X secondes réel>  \n");
        return 1;
    }
    NB_PLACE_ABO=atoi(argv[1]);
    NB_PLACE_NON_ABO=atoi(argv[2]);
    FAC_DEBORDEMENT=atof(argv[3]);
    temps_accel=atof(argv[4]);


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

    //creer le parking
    pthread_t parking_thread;
    pthread_create(&parking_thread, NULL, parking, NULL);

    //creer le thread timer 
    pthread_t timer;
    pthread_create(&timer, NULL, timer_function, NULL);
    


    //creer les threads voiture
    pthread_t generateur;
    pthread_create(&generateur, NULL, generateur_voiture, NULL);

    pthread_join(parking_thread, NULL);
    pthread_join(timer, NULL);
    msgctl(msg_id, IPC_RMID, NULL);
    return 0;
}