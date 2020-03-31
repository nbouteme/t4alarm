Réveil implémenté sur teensy4
=============================

Compiler nécéssite teensyduino d'installé.
Nécéssite de modifier le makefile si non-installé dans /usr/share/arduino

Compiler
========

`make` produit un fichier flashable sur teensy
`make prog` flash le fichier sur une teensy4 branchée

`make emu.so` produit un module chargeable sur ["l'émulateur" PC](https://github.com/nbouteme/teensy-emu).

tip: ajouter  `-jN` où N  est le nombre  de threads disponible  sur la
machine pour compiler en parallèle.

Manuel
======

Au démarrage, l'écran affiche la date.

La télécommande est nécessaire pour  accéder au menu de configuration,
avec la touche power. Les touches  autour du cercle de la télécommande
sont utilisées pour se déplacer, la touche au centre et à droite, pour
valider, à  gauche pour retourner  en arrière. Les  touches numériques
sont utilisées pour régler des heures. La touche 5 est utilisé lors du
réglage  d'alarme pour  activer  et désactiver  certains  jours de  la
semaine.

Le    menu     permet    de     configurer    la     date    actuelle,
d'ajouter/supprimer/modifier  des   alarmes,  de  leur   assigner  une
musique.

Lorsqu'une  alarme  se déclenche,  le  son  est  joué jusqu'à  ce  que
l'utilisateur intéragit avec le  réveil d'une quelconque manière, soit
à travers la télécommande, soit avec le bouton physique du montage.

Un fichier  de configuration est  sauvegardé sur  la carte SD  pour la
persistance.

