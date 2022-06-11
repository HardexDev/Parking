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



int NB_PLACE_ABO = 20;
int NB_PLACE_NON_ABO = 70;
float FAC_DEBORDEMENT = 20; //20% des places sont bloquées le jour
float temps_accel = 2; //1 heure simulée = 2 secondes réelles
int TEMPS_MAX_AVANT_PROCHAINE_VOITURE_SEC = 3600; //temps maximum avant qu'une nouvelle voiture soit créée 
int TEMPS_MAX_DANS_LE_PARKING = 86400; //temps maximum qu'une voiture peut passer dans le parking (1jour)
float PROBA_ABONNE = 0.3;

pthread_mutex_t mutex_cpt = PTHREAD_COND_INITIALIZER;
pthread_cond_t attendre = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex = PTHREAD_COND_INITIALIZER;

int msg_id; //file de message



typedef struct {
    int estAbonne; //1: non abo, 2 : abo    si on met 0 ca plante jcp pq
    int type; //0:entrer, 1:sortir
    pthread_t threadID;
} Usager;


typedef struct {
    pthread_t threadID;
    int action; //0:entrer, 1:pas de place->partir
} Retour;

void Attendre(int temps_simule_secondes){
    usleep((float)temps_simule_secondes*temps_accel/3600.0*1000*1000);
}


void *timer_function(void* arg){
    int heure = 17; //commencer avant 8h pour décrémenter le nombre de place non abonnées suite a la zone de débordement
    int nb_place_non_abo_init = NB_PLACE_NON_ABO;
    int aug = (int)NB_PLACE_NON_ABO*(FAC_DEBORDEMENT/6)/100;
    while(true){
        Attendre(3600);
        if(heure == 18 || heure == 19 || heure == 20 || heure == 21 || heure == 22 || heure == 23 || heure == 24){
            pthread_mutex_lock(&mutex_cpt);
            if(heure == 19 || heure == 20 || heure == 21 || heure == 22 || heure == 23 || heure == 24){
                printf("\n\nHeure : %d       zone de débordement augmentation de %d places\n", heure, aug);
                printf("NB places anciennement disponible : %d\n", NB_PLACE_NON_ABO);
                NB_PLACE_NON_ABO += aug;
                printf("NB places maintenant disponible: %d\n\n\n", NB_PLACE_NON_ABO);
                
            }
            if(heure == 18){
                printf("\n\nHeure : %d        début de la zone de débordement réduction du nombre de place disponible : nombre maximal de place non abo dispo : %d\n", heure, nb_place_non_abo_init - 6*aug);
                printf("NB places anciennement disponible : %d\n", NB_PLACE_NON_ABO);
                NB_PLACE_NON_ABO -= 6*aug; //peut être négatif -> personne ne peut rentrer
                printf("NB places maintenant disponible: %d\n\n\n", NB_PLACE_NON_ABO);
                
            }
            pthread_mutex_unlock(&mutex_cpt);
        }
        if(heure == 24){
            heure = 0;
        }
        heure++;
    }
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
    
    Retour rep;
    if(msgrcv(msg_id,&rep,sizeof(Retour),pthread_self(),0)==-1){
        perror("Erreur de reception reponse \n");
        exit(1);
    };
    if(rep.action == 1){
        printf("La Voiture %d n'a pas trouver de place et part\n\n", (int)arg);
        return(NULL);
    }
    printf("La Voiture %d entre dans le parking (verrouille la ressource critique)\n", (int)arg);
    printf("le feu passe au rouge\n");
    if(est_abonne == 1){
        printf("La Voiture %d prend un ticket\n", (int)arg);
        Attendre(rand()%5);
        printf("La barrière s'ouvre\n");
        Attendre(rand()%5);
        printf("La barrière se ferme\n");
    }
    else{
        printf("La Voiture %d est abonnée la barrière s'ouvre automatiquement\n", (int)arg);
        Attendre(rand()%5);
        printf("La barrière se ferme\n");
    }
    
    Attendre(rand()%5);

    printf("La Voiture %d est garée (déverrouille la ressource critique)\n\n", (int)arg);
    pthread_cond_signal(&attendre);


    Attendre(rand()%TEMPS_MAX_DANS_LE_PARKING);
    printf("La Voiture %d veut sortir du parking donc envoye un message\n", (int)arg);
    usager.type = 1;
    if(msgsnd(msg_id, &usager, sizeof(Usager) - sizeof(long), 0) == -1) {
        perror("Erreur de envoie requete \n");
        exit(1);
    }
    if(msgrcv(msg_id,&rep,sizeof(Retour),pthread_self(),0)==-1){
        perror("Erreur de reception reponse \n");
        exit(1);
    };
    Attendre(rand()%5);
    printf("La Voiture %d sort du parking (verrouille la ressource critique)\n", (int)arg);
    printf("le feu passe au rouge\n");
    printf("La Voiture %d est sortie du parking et part (déverrouille la ressource critique)\n\n", (int)arg);
    pthread_cond_signal(&attendre);
    return(NULL);
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
        Usager msg;
        if (msgrcv(msg_id, &msg, sizeof(Usager)- sizeof(long), 0, 0) == -1) {
            perror("Erreur de lecture requete \n");
            exit(1);
        }

        pthread_mutex_lock(&mutex_cpt);

        Retour reponse;
        reponse.threadID = msg.threadID;
        if(msg.type == 0){ //entrer
            if(msg.estAbonne == 1){ //non abonné
                if(NB_PLACE_NON_ABO > 0){
                    NB_PLACE_NON_ABO--;
                    printf("\nNombre de places non abonnées restantes: %d\nle feu passe au vert\n", NB_PLACE_NON_ABO);
                    reponse.action = 0;
                    if(msgsnd(msg_id, &reponse, sizeof(Retour) - sizeof(long), 0) == -1) {
                        perror("Erreur de envoie requete \n");
                        exit(1);
                    }
                    pthread_cond_wait(&attendre, &mutex);//il se met en attente
                }
                else{
                    printf("Nombre de places non abonnées disponible: %d\nPlus de place non abonnées disponibles\n", NB_PLACE_NON_ABO);
                    reponse.action = 1;
                    if(msgsnd(msg_id, &reponse, sizeof(Retour) - sizeof(long), 0) == -1) {
                        perror("Erreur de envoie requete \n");
                        exit(1);
                    }
                }
            }
            else{ //abonné
                if(NB_PLACE_ABO > 0){
                    NB_PLACE_ABO--;
                    printf("Nombre de places abonnées restantes: %d\nle feu passe au vert\n", NB_PLACE_ABO);
                    reponse.action = 0;
                    if(msgsnd(msg_id, &reponse, sizeof(Retour) - sizeof(long), 0) == -1) {
                        perror("Erreur de envoie requete \n");
                        exit(1);
                    }
                    pthread_cond_wait(&attendre, &mutex);//il se met en attente
                }
                else{
                    printf("Plus de place abonnées disponibles\n");
                    if(NB_PLACE_NON_ABO > 0){
                        //retenir son  ID
                        for(int i = 0; i < NB_PLACE_NON_ABO; i++){
                            if(Abo_sur_place_non_abo[i] == 0){
                                Abo_sur_place_non_abo[i] = msg.threadID;
                                break;
                            }
                        }
                        NB_PLACE_NON_ABO--;
                        printf("Nombre de places non abonnées restantes: %d\nle feu passe au vert\n", NB_PLACE_NON_ABO);
                        reponse.action = 0;
                        if(msgsnd(msg_id, &reponse, sizeof(Retour) - sizeof(long), 0) == -1) {
                            perror("Erreur de envoie requete \n");
                            exit(1);
                        }
                        pthread_cond_wait(&attendre, &mutex);//il se met en attente
                    }
                    else{
                        printf("Nombre de places non abonnées disponible : %d\nPlus de place abonnées ou non abonnées disponibles\n", NB_PLACE_NON_ABO);
                        reponse.action = 1;
                        if(msgsnd(msg_id, &reponse, sizeof(Retour) - sizeof(long), 0) == -1) {
                            perror("Erreur de envoie requete \n");
                            exit(1);
                        }
                    }
                    
                }
            }
        }
        if(msg.type == 1){ //sortir
            if(msg.estAbonne == 2){ //abonné verif sa place pour decrémenter
                bool done = false;
                for(int i = 0; i < NB_PLACE_NON_ABO; i++){
                    if(Abo_sur_place_non_abo[i] == msg.threadID){
                        Abo_sur_place_non_abo[i] = 0;
                        NB_PLACE_NON_ABO++;
                        done = true;
                        break;
                    }
                }
                if(!done){
                    NB_PLACE_ABO++;
                }
                reponse.action = 0;
                if(msgsnd(msg_id, &reponse, sizeof(Retour) - sizeof(long), 0) == -1) {
                    perror("Erreur de envoie requete \n");
                    exit(1);
                }
                printf("le feu passe au vert\n");
                pthread_cond_wait(&attendre, &mutex);//il se met en attente
            }
            else{
                NB_PLACE_NON_ABO++;
                reponse.action = 0;
                if(msgsnd(msg_id, &reponse, sizeof(Retour) - sizeof(long), 0) == -1) {
                    perror("Erreur de envoie requete \n");
                    exit(1);
                }
                printf("le feu passe au vert\n");
                pthread_cond_wait(&attendre, &mutex);//il se met en attente
            }
            printf("\nNombre de places non abonnées restantes: %d  Nombre de places abonnées restantes: %d\n\n", NB_PLACE_NON_ABO, NB_PLACE_ABO);
        }
        if(msg.type != 0 && msg.type != 1){
            printf("Erreur de type\n");
            printf("type : %d \n", msg.type);
            exit(0);
        }
        pthread_mutex_unlock(&mutex_cpt);
    }
}

void generateur_voiture(int arg){
    long num = 0;
    while(true){
        pthread_t voiture_i;
        pthread_create(&voiture_i, NULL, voiture, num);
        Attendre(rand()%TEMPS_MAX_AVANT_PROCHAINE_VOITURE_SEC);
        num++;
    }
    
}

int main(int argc, char *argv[]) // nb place abo - nb place non abo - facteur temps
{
    printf("%d\n", argc-1);
    if(argc-1 != 0){
        if (argc-1 < 4 || argc-1 > 7) {
            fprintf(stderr,"Nombre de place abonées : <nb places> \nNombre de place non abonées : <nb places>\nTaille zone de debordement <facteur %%>\nFacteur temps : <1 heure simulé = X secondes réel>\noptionnel :\nTemps max avant génération de la prochaine voiture <temps en secondes> default : 3600\nTemps max qu'une voiture peut passer dans le parking <temps en secondes> default : 86400\nProbabilité de génération d'une voiture abonnée <probabilité> default : 0.3\n");
            return 1;
        }
        else{
            switch (argc-1)
            {
            case 4:
                NB_PLACE_ABO=atoi(argv[1]);
                NB_PLACE_NON_ABO=atoi(argv[2]);
                FAC_DEBORDEMENT=atof(argv[3]);
                temps_accel=atof(argv[4]);
                break;
            case 5:
                NB_PLACE_ABO=atoi(argv[1]);
                NB_PLACE_NON_ABO=atoi(argv[2]);
                FAC_DEBORDEMENT=atof(argv[3]);
                temps_accel=atof(argv[4]);
                TEMPS_MAX_AVANT_PROCHAINE_VOITURE_SEC = atoi(argv[5]);
                break;
            case 6:
                NB_PLACE_ABO=atoi(argv[1]);
                NB_PLACE_NON_ABO=atoi(argv[2]);
                FAC_DEBORDEMENT=atof(argv[3]);
                temps_accel=atof(argv[4]);
                TEMPS_MAX_AVANT_PROCHAINE_VOITURE_SEC = atoi(argv[5]);
                TEMPS_MAX_DANS_LE_PARKING = atoi(argv[6]);
                break;
            default:
                NB_PLACE_ABO=atoi(argv[1]);
                NB_PLACE_NON_ABO=atoi(argv[2]);
                FAC_DEBORDEMENT=atof(argv[3]);
                temps_accel=atof(argv[4]);
                TEMPS_MAX_AVANT_PROCHAINE_VOITURE_SEC = atoi(argv[5]);
                TEMPS_MAX_DANS_LE_PARKING = atoi(argv[6]);
                PROBA_ABONNE = atof(argv[6]);
                break;
            }
        }            
    }
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