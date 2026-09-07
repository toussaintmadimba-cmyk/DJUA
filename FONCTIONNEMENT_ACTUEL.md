# Fonctionnement actuel de l'IoT Box

Ce document décrit le comportement réellement implémenté dans le dépôt. Le système est composé de deux parties :

- le firmware Arduino/ESP32, qui lit les capteurs et publie la télémétrie ;
- l'API FastAPI, qui écoute MQTT et met les données à disposition en HTTP et WebSocket.

## Vue d'ensemble

```text
INA219 ──┐
NEO-6M ──┼──> ESP32 ──Wi-Fi──> broker MQTT public ──> API FastAPI ──> client/dashboard
DS1302 ──┘                  test.mosquitto.org        HTTP + WebSocket
```

Chemin actif des données :

1. L'ESP32 mesure la batterie avec le capteur INA219.
2. Il récupère la dernière position valide décodée depuis le GPS NEO-6M.
3. Il lit la date et l'heure locale conservées par le DS1302.
4. Toutes les 10 secondes environ, il construit un objet de télémétrie.
5. Il publie cet objet en JSON sur MQTT et, si activé, directement en HTTP.
6. L'API s'abonne au topic MQTT, reçoit le JSON et le conserve en mémoire.
7. Un dashboard ou un autre client consulte ensuite les données par HTTP ou WebSocket.

L'envoi direct du firmware vers un backend HTTP est actuellement activé par `ENABLE_HTTP_BACKEND = 1`.

## 1. Firmware ESP32

Le point d'entrée est [`iot_box.ino`](iot_box.ino).

### Démarrage

La fonction `setup()` effectue les opérations suivantes dans cet ordre :

1. ouverture du moniteur série à 115200 bauds ;
2. affichage de l'identifiant du kit ;
3. initialisation du capteur INA219 ;
4. initialisation du GPS NEO-6M ;
5. initialisation de l'horloge RTC DS1302 ;
6. tentative de connexion au Wi-Fi, avec un délai maximal de 15 secondes ;
7. configuration du client MQTT ;
8. démarrage du compteur utilisé pour l'intervalle de télémétrie.

L'identifiant actuellement configuré est `DJUA-KIN-000001`.

### Boucle principale

À chaque passage dans `loop()`, le firmware :

- lit les caractères disponibles sur l'UART du GPS afin d'alimenter TinyGPSPlus ;
- contrôle le Wi-Fi et tente une reconnexion toutes les 10 secondes s'il est coupé ;
- contrôle MQTT et tente une reconnexion toutes les 10 secondes s'il est coupé ;
- attend que l'intervalle de télémétrie de 10 secondes soit écoulé ;
- lit le capteur INA219 ;
- récupère la position GPS disponible ;
- lit la date et l'heure du DS1302 ;
- construit et publie la télémétrie.

Si la mesure INA219 est invalide, le cycle de télémétrie est abandonné : aucun message n'est envoyé pour cet intervalle.

### Capteur de batterie INA219

Fichiers concernés :

- [`src/sensors/power_sensor.cpp`](src/sensors/power_sensor.cpp)
- [`src/sensors/power_sensor.h`](src/sensors/power_sensor.h)

Configuration actuelle :

| Élément | Valeur |
| --- | --- |
| Bus | I2C |
| Adresse INA219 | `0x40` |
| SDA ESP32 | GPIO 33 |
| SCL ESP32 | GPIO 14 |
| Calibration | 32 V / 2 A |

Les valeurs suivantes sont réellement mesurées :

- tension totale en volts : tension du bus + tension du shunt ;
- courant en ampères ;
- puissance en watts : tension × courant.

### GPS NEO-6M

Fichiers concernés :

- [`src/sensors/gps.cpp`](src/sensors/gps.cpp)
- [`src/sensors/gps.h`](src/sensors/gps.h)

Configuration actuelle :

| Élément | Valeur |
| --- | --- |
| Port | UART matériel 2 |
| Vitesse | 9600 bauds |
| RX ESP32 | GPIO 16 |
| TX ESP32 | GPIO 17 |

TinyGPSPlus décode continuellement les données reçues. Si une position est considérée comme valide, sa latitude et sa longitude sont utilisées. Sinon, le firmware envoie `0.0` pour les deux coordonnées.

Le code contrôle la validité de la position, mais pas son âge. Une position précédemment valide peut donc rester utilisée même si elle n'est plus récente.

### Horloge RTC DS1302

Fichiers concernés :

- [`src/sensors/rtc_ds1302.cpp`](src/sensors/rtc_ds1302.cpp)
- [`src/sensors/rtc_ds1302.h`](src/sensors/rtc_ds1302.h)

Configuration actuelle :

| Élément | Valeur |
| --- | --- |
| DAT / IO | GPIO 25 |
| CLK / SCLK | GPIO 26 |
| RST / CE | GPIO 27 |
| Fuseau fixe | `GMT+1` |

Si la date est invalide lors du démarrage, elle est initialisée avec la date et l'heure de compilation. Si l'oscillateur est arrêté, le firmware le démarre. Une horloge déjà valide n'est pas réinitialisée.

Le timestamp produit suit le format ISO 8601, par exemple `2026-09-04T15:42:05+01:00`. Si le DS1302 devient invalide ou indisponible, le firmware continue à fonctionner et utilise le timestamp historique basé sur `millis()` comme secours pour l'envoi HTTP.

### Champs de télémétrie

La structure complète est définie dans [`src/telemetry/telemetry.h`](src/telemetry/telemetry.h) et assemblée dans [`src/telemetry/telemetry.cpp`](src/telemetry/telemetry.cpp).

| Groupe | État actuel |
| --- | --- |
| Horodatage | Réel depuis le DS1302 avec indication `GMT+1`, avec repli si invalide |
| GPS | Réel si un fix est disponible, sinon latitude/longitude à `0.0` |
| Batterie | Mesure réelle du INA219 |
| Solaire | Non mesuré, toutes les valeurs sont à `0.0` |
| Charge AC | Non mesurée, toutes les valeurs sont à `0.0` |
| Intervalle | Temps écoulé depuis le cycle précédent, en secondes |

Exemple de charge utile MQTT :

```json
{
  "kit_id": "DJUA-KIN-000001",
  "timestamp_ms": 123456,
  "timestamp": "2026-09-04T15:42:05+01:00",
  "timezone": "GMT+1",
  "interval_seconds": 10,
  "latitude": 0.0,
  "longitude": 0.0,
  "battery": {
    "voltage_v": 12.4,
    "current_a": 0.42,
    "power_w": 5.21
  },
  "solar": {
    "voltage_v": 0.0,
    "current_a": 0.0,
    "power_w": 0.0,
    "energy_interval_wh": 0.0
  },
  "ac_load": {
    "voltage_v": 0.0,
    "current_a": 0.0,
    "apparent_power_va": 0.0,
    "energy_interval_vah": 0.0
  }
}
```

`timestamp_ms` reste présent pour la compatibilité et correspond au nombre de millisecondes écoulées depuis le démarrage de l'ESP32. Le champ `timestamp` contient la date du DS1302 avec le décalage GMT configuré.

## 2. Communication réseau

### Wi-Fi

La gestion du Wi-Fi se trouve dans [`src/communication/internet.cpp`](src/communication/internet.cpp).

- Le mode utilisé est `WIFI_STA`.
- Le SSID et le mot de passe sont actuellement compilés dans le firmware via [`src/config.h`](src/config.h).
- La première tentative attend au maximum 15 secondes.
- En cas de coupure, une nouvelle tentative est déclenchée toutes les 10 secondes.

### MQTT actif

La gestion MQTT se trouve dans [`src/communication/mqtt.cpp`](src/communication/mqtt.cpp).

Configuration active par défaut :

| Élément | Valeur |
| --- | --- |
| Broker | `test.mosquitto.org` |
| Port | `1883` |
| Chiffrement TLS | Non |
| Authentification | Non |
| Topic télémétrie | `djua/test/DJUA-KIN-000001/telemetry` |
| Topic état | `djua/test/DJUA-KIN-000001/status` |
| Taille du buffer MQTT | 1024 octets |

Lors d'une connexion réussie, l'ESP32 publie `online` sur le topic d'état avec conservation du message. Un Last Will MQTT publiera `offline` sur ce même topic si la connexion disparaît de manière anormale.

Les télémétries sont publiées sans conservation et avec le QoS par défaut de PubSubClient, soit QoS 0. Si MQTT est déconnecté au moment de l'envoi, le message est perdu : le firmware ne possède actuellement ni file d'attente locale ni mécanisme de renvoi.

### Backend HTTP optionnel

Le code sait aussi envoyer un `POST` JSON vers `BACKEND_URL`, avec la clé configurée dans l'en-tête `x-device-token`. Cette voie est compilée seulement si `ENABLE_HTTP_BACKEND` vaut `1`.

Dans la configuration actuelle, elle vaut `1` : un POST HTTP est effectué après chaque télémétrie valide. Le timestamp provient du DS1302 lorsqu'il est valide ; sinon, le format historique basé sur la durée de fonctionnement est utilisé comme secours.

### GSM et SMS

Les fichiers `gsm.cpp`, `gsm.h`, `sms.cpp` et `sms.h` sont actuellement vides. Les broches du futur SIM800C ne sont pas définies. Il n'existe donc, à ce stade, aucun envoi GSM ou SMS fonctionnel.

## 3. API FastAPI

Le service est implémenté dans [`api/app/main.py`](api/app/main.py).

### Démarrage

Depuis le dossier `api` :

```powershell
uv sync
uv run uvicorn app.main:app --reload
```

L'interface Swagger est ensuite disponible à l'adresse <http://127.0.0.1:8000/docs>.

Au démarrage, l'API :

1. crée un client MQTT Paho ;
2. se connecte de manière asynchrone au broker ;
3. s'abonne par défaut à `djua/test/+/telemetry` ;
4. accepte ainsi les télémétries de plusieurs identifiants de kit.

Le fichier `api/.env` n'est pas chargé automatiquement par l'application. Pour l'utiliser directement avec Uvicorn, il faut lancer :

```powershell
uv run uvicorn app.main:app --reload --env-file .env
```

Il est également possible de définir les variables dans le shell ou dans la configuration de déploiement.

### Traitement d'un message MQTT

Lorsqu'un message arrive, l'API :

1. décode le contenu en UTF-8 puis en JSON ;
2. ignore le message si le JSON est invalide ou si sa racine n'est pas un objet ;
3. utilise `kit_id` comme identifiant, ou l'extrait du topic si `kit_id` est absent ;
4. ajoute l'heure UTC de réception calculée par le serveur ;
5. ajoute un numéro de séquence interne ;
6. met à jour la dernière télémétrie du kit ;
7. ajoute le message à l'historique global en mémoire.

L'accès à cet état partagé est protégé par un verrou entre le thread MQTT et les requêtes de l'API.

### Endpoints disponibles

| Interface | Fonction |
| --- | --- |
| `GET /health` | Retourne l'état général, l'état MQTT, le topic et le nombre de kits connus |
| `GET /devices` | Liste les kits ayant déjà transmis depuis le dernier démarrage |
| `GET /devices/{device_id}/telemetry/latest` | Retourne la dernière télémétrie du kit, ou 404 si elle n'existe pas |
| `GET /devices/{device_id}/telemetry?limit=100` | Retourne les derniers messages du kit |
| `WS /ws/telemetry` | Envoie l'historique présent puis les nouveaux messages, avec vérification toutes les 500 ms |

Le champ `status` de `/health` reste actuellement égal à `ok`, même si la sous-partie `mqtt.connected` vaut `false`. Il faut donc contrôler explicitement `mqtt.connected` pour connaître l'état de la liaison au broker.

### Stockage

Les messages ne sont pas enregistrés dans une base de données.

- L'historique est une file en mémoire limitée à 500 messages au total par défaut.
- La limite est globale à tous les appareils, pas 500 messages par appareil.
- Un redémarrage de l'API efface l'historique et la liste des appareils connus.
- La variable `TELEMETRY_HISTORY_LIMIT` permet de changer cette limite.

## 4. Configuration importante

### Firmware

Les constantes principales se trouvent dans [`src/config.h`](src/config.h) :

- identifiant du kit ;
- fuseau GMT et initialisation du DS1302 ;
- identifiants Wi-Fi ;
- adresse du backend HTTP et jeton associé ;
- broker, port et préfixe MQTT ;
- intervalles de télémétrie et de reconnexion.

### API

Les variables disponibles sont présentées dans [`api/.env.example`](api/.env.example) :

- `MQTT_BROKER_HOST` ;
- `MQTT_BROKER_PORT` ;
- `MQTT_TOPIC` ;
- `MQTT_USERNAME` ;
- `MQTT_PASSWORD` ;
- `TELEMETRY_HISTORY_LIMIT`.

## 5. Dépendances nécessaires

### Firmware Arduino/ESP32

Le code utilise les bibliothèques suivantes :

- framework Arduino pour ESP32 ;
- `Adafruit INA219` ;
- `TinyGPSPlus` ;
- `Rtc by Makuna` pour le DS1302 ;
- `ArduinoJson` ;
- `PubSubClient` ;
- les bibliothèques ESP32 intégrées `WiFi`, `HTTPClient` et `Wire`.

Le dépôt ne contient actuellement ni `platformio.ini` ni liste Arduino formelle des versions. La reproductibilité de la compilation du firmware dépend donc des bibliothèques installées dans l'environnement Arduino utilisé.

### API

Les dépendances Python sont déclarées dans [`api/pyproject.toml`](api/pyproject.toml) et verrouillées dans `api/uv.lock` :

- FastAPI ;
- Paho MQTT ;
- Uvicorn avec ses dépendances standard.

Python 3.11 ou plus récent est requis.

## 6. Limites et précautions actuelles

Avant une utilisation en production, les points suivants doivent être traités :

- le broker MQTT est public, sans TLS ni authentification ;
- les topics publics peuvent être lus ou alimentés par des tiers ;
- les identifiants Wi-Fi et le jeton HTTP sont écrits dans `config.h` et doivent être considérés comme exposés s'ils ont été partagés ou versionnés ;
- l'API n'a aucune authentification ;
- CORS accepte actuellement toutes les origines ;
- l'API ne valide pas la structure métier détaillée du JSON reçu ;
- les messages ne sont pas persistés ;
- les données perdues pendant une coupure MQTT ne sont pas renvoyées ;
- le fuseau du DS1302 est fixe et ne gère pas automatiquement un changement saisonnier ;
- les mesures solaire et AC ne sont pas encore implémentées ;
- le GPS ne vérifie pas la fraîcheur du dernier fix ;
- GSM et SMS ne sont pas implémentés.

Ces éléments décrivent l'état actuel ; ils ne signifient pas que le flux de test est inutilisable. Pour les essais, le chemin ESP32 → MQTT → FastAPI est déjà cohérent et opérationnel dès que l'ESP32 et l'API ont accès au broker.
