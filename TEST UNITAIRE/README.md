# Tests unitaires et tests de modules

Ce dossier contient des tests autonomes. Il est volontairement séparé du firmware principal : aucun fichier présent ici n'est inclus par `iot_box.ino` et les tests ne modifient donc pas le fonctionnement actuel de l'IoT Box.

## Modules couverts

| Module | État |
| --- | --- |
| Horloge RTC DS1302 | Premier test disponible dans [`DS1302`](DS1302/README.md) |
| GPS NEO-6M | Test matériel disponible dans [`NEO-6M`](NEO-6M/README.md) |

## Organisation retenue

Chaque module possède son propre dossier avec :

- un sketch Arduino autonome contenant sa configuration locale ;
- les instructions de câblage et d'exécution ;
- la liste des contrôles effectués.

Les futurs tests de modules pourront être ajoutés ici sans modifier les sources de production.
