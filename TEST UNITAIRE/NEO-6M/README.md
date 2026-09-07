# Test du module GPS NEO-6M

Ce test vérifie un module GPS NEO-6M directement sur un ESP32. Il est autonome et utilise le même port série, le même brochage et le même débit que le firmware principal.

> Ce sketch est un test matériel du module. Il doit être téléversé séparément du firmware principal.

## Contenu

- [`neo6m_test/neo6m_test.ino`](neo6m_test/neo6m_test.ino) : sketch autonome avec rapport série `PASS`, `FAIL` et `SKIP`.

## Dépendance

Installer la bibliothèque **TinyGPSPlus by Mikal Hart** depuis le gestionnaire de bibliothèques de l’IDE Arduino.

Documentation officielle : <https://github.com/mikalhart/TinyGPSPlus>

## Câblage de test

| NEO-6M | ESP32 |
| --- | --- |
| `GND` | `GND` |
| `TX` | GPIO 16 (`RX2`) |
| `RX` | GPIO 17 (`TX2`, optionnel pour ce test en lecture seule) |
| `VCC` | Alimentation prévue par la carte GPS utilisée |

Le test lit uniquement les données émises par le GPS. La liaison indispensable est donc `TX` du NEO-6M vers GPIO 16 de l’ESP32, avec une masse commune. Vérifier la tension acceptée par la carte GPS : le module NEO-6M lui-même et les entrées de l’ESP32 utilisent une logique 3,3 V. Ne jamais appliquer directement un signal logique de 5 V à l’ESP32.

## Contrôles effectués

Pendant 60 secondes, le sketch lit en continu l’UART2 à 9600 bauds, puis vérifie :

1. la réception d’octets sur l’UART ;
2. la réception d’un volume minimal de caractères ;
3. la présence d’au moins une trame NMEA avec un checksum valide ;
4. l’absence de trames NMEA corrompues ;
5. l’obtention éventuelle d’un FIX GPS ;
6. les plages de latitude et longitude lorsqu’une position existe ;
7. la cohérence du nombre de satellites ;
8. la disponibilité de la date et de l’heure GPS.

L’absence de FIX est signalée par `SKIP` par défaut, car une antenne placée à l’intérieur peut recevoir des trames NMEA valides sans voir suffisamment de satellites. Le contrôle essentiel du module et du câblage est la réception de trames dont le checksum est valide.

Pour exiger un FIX et transformer son absence en échec, remplacer dans le sketch :

```cpp
#define NEO6M_REQUIRE_FIX 0
```

par :

```cpp
#define NEO6M_REQUIRE_FIX 1
```

Effectuer alors le test dehors, antenne céramique orientée vers le ciel. Un premier démarrage à froid peut nécessiter plusieurs minutes ; augmenter `NEO6M_TEST_DURATION_MS` si nécessaire.

Pour afficher également les trames NMEA brutes dans le moniteur série, activer :

```cpp
#define NEO6M_ECHO_NMEA 1
```

## Exécution

1. Ouvrir `neo6m_test/neo6m_test.ino` dans l’IDE Arduino.
2. Sélectionner la carte ESP32 et son port série.
3. Installer `TinyGPSPlus` si nécessaire.
4. Téléverser le sketch.
5. Ouvrir le moniteur série à **115200 bauds**.
6. Attendre la fin de la campagne de 60 secondes.

Exemple de résultat avec communication correcte mais sans visibilité satellite :

```text
=== CONTROLES ===
[PASS] Reception de donnees sur UART2
[PASS] Flux NMEA suffisamment long
[PASS] Au moins une trame NMEA valide
[PASS] Integrite des checksums NMEA
[SKIP] Obtention d'un FIX GPS - Aucun FIX; ce controle est optionnel dans la configuration actuelle
[SKIP] Coordonnees dans les plages attendues - Aucune position valide disponible
[PASS] Nombre de satellites coherent
[PASS] Date et heure GPS valides

=== RESUME ===
PASS: 6
FAIL: 0
SKIP: 2
[RESULTAT] SUCCES
```

En cas de `Aucun octet recu`, contrôler en priorité l’alimentation, la masse commune et le croisement `TX GPS -> RX ESP32`. En cas d’octets reçus mais sans checksum valide, vérifier le débit de 9600 bauds et la qualité des connexions.
