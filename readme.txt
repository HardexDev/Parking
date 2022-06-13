SY40 - Simulation de la gestion d'un parking
Auteurs : Simon NEMO - Alexis ROBIN

Compilation du projet :
Le projet n'est composé que d'un seul fichier : parking.c

La commande "make" produira l'éxécutable "parking" grâce au makefile

Exécution du projet:
Pour éxécuter le projet, simplement lancer l'éxécutable : "./parking"

Paramétrage :
Il est possible modifier à son gré les différents paramètres du projet, voici une liste de ces derniers :
- Nombre total de voitures, entier, valeur par défaut : infini (génération en continu)
- Nombre de places abonnées dans le parking, entier, valeur par défaut : 20
- Nombre de places non abonnées dans le parking, entier, valeur par défaut : 70
- Facteur de débordement (pourcentage de diminution des places à 18h), flottant, entre 0 et 100, valeur par défaut : 20
- Facteur d'accélération du temps (X secondes = 1h), entier, en secondes, valeur par défaut : 2
- Temps maximum avant de générer une nouvelle voiture, entier, en secondes, valeur par défaut : 3600
- Temps maximum qu'une voiture passe dans le parking, entier, en secondes, valeur par défaut : 86400
- Probabilité qu'une voiture soit abonnée, flottant, entre 0 et 1, valeur par défaut : 0.3

Lors du lancement du programme, ces différents paramètres vous seront demandés les uns après les autres si vous 
souhaitez les modifier. Si vous voulez toutefois laisser la valeur par défaut, ne rentrez aucune valeur et appuyez sur entrer
