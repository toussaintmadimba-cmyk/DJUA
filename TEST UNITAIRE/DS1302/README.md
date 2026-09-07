# Test du module RTC DS1302

Ce test vérifie un module d'horloge temps réel DS1302 directement sur un ESP32. Il reste autonome, mais utilise maintenant le même brochage que le pilote intégré au firmware principal.

> Ce sketch est un test matériel du module. Il doit être téléversé séparément du firmware principal.

## Contenu

- [`ds1302_test/ds1302_test.ino`](ds1302_test/ds1302_test.ino) : fichier unique contenant le brochage, les options et tous les tests avec rapport série `PASS`, `FAIL` et `SKIP`.

## Dépendance

Installer la bibliothèque **Rtc by Makuna** depuis le gestionnaire de bibliothèques de l'IDE Arduino. Le sketch utilise son pilote `RtcDS1302` et son bus `ThreeWire`.

Documentation officielle : <https://github.com/Makuna/Rtc>

## Câblage de test par défaut

| DS1302 | ESP32 |
| --- | --- |
| `VCC` | `3V3` |
| `GND` | `GND` |
| `DAT` ou `IO` | GPIO 25 |
| `CLK` ou `SCLK` | GPIO 26 |
| `RST` ou `CE` | GPIO 27 |

Ces trois GPIO sont ceux retenus pour l'intégration dans le firmware principal. Ils évitent le GPS sur GPIO 16/17 et l'INA219 sur GPIO 33/14. Le même câblage peut donc être conservé entre ce test autonome et le firmware complet.

Ne jamais appliquer un signal logique de 5 V directement sur une entrée de l'ESP32. Vérifier également le type de pile ou d'accumulateur monté sur la carte DS1302 avant d'utiliser un éventuel circuit de charge.

## Initialisation automatique

Avant les tests, le sketch contrôle la date et l'état de l'oscillateur :

- si la date est invalide, elle est réglée une seule fois avec `__DATE__` et `__TIME__`, c'est-à-dire la date et l'heure de compilation du sketch ;
- si l'oscillateur est arrêté, il est démarré ;
- si la date est déjà valide et l'oscillateur actif, aucune écriture n'est effectuée.

Ce comportement est commandé directement dans `ds1302_test.ino` par :

```cpp
#define DS1302_INITIALIZE_IF_INVALID 1
```

Après une première initialisation réussie, vérifier que l'ordinateur utilisé pour compiler possède une heure correcte. La pile du DS1302 doit ensuite conserver l'horloge lorsque l'ESP32 est hors tension.

## Tests exécutés par défaut

Après cette initialisation éventuelle, le sketch vérifie :

1. validité de la date et de l'heure lues ;
2. plages des champs année, mois, jour, heure, minute et seconde ;
3. état actif de l'oscillateur ;
4. progression de l'heure après environ 2,2 secondes.

Si la date reste invalide après la tentative d'initialisation, vérifier en priorité le câblage, l'alimentation, la pile et le module.

## Tests d'écriture optionnels

Dans `ds1302_test.ino`, remplacer :

```cpp
#define DS1302_ENABLE_WRITE_TESTS 0
```

par :

```cpp
#define DS1302_ENABLE_WRITE_TESTS 1
```

Le sketch testera alors :

- la désactivation temporaire de la protection en écriture ;
- l'écriture puis la relecture d'une date connue ;
- l'écriture puis la relecture d'un octet de RAM du DS1302 ;
- la restauration de l'heure, de l'octet RAM, de l'état de l'oscillateur et de la protection d'origine.

Les tests d'écriture sont ignorés si la date initiale est invalide, car le sketch ne pourrait pas restaurer fidèlement l'état temporel d'origine. Une coupure d'alimentation pendant un test d'écriture pourrait aussi empêcher la restauration.

## Exécution

1. Ouvrir `ds1302_test/ds1302_test.ino` dans l'IDE Arduino.
2. Sélectionner la carte ESP32 et son port série.
3. Installer `Rtc by Makuna` si nécessaire.
4. Téléverser le sketch.
5. Ouvrir le moniteur série à **115200 bauds**.

Exemple de résultat :

```text
=== TEST MODULE DS1302 ===
[PASS] Date/heure valide
[PASS] Champs calendaires dans les plages attendues
[PASS] Oscillateur actif
[PASS] Progression de l'horloge
[SKIP] Tests d'ecriture desactives dans ds1302_test.ino

=== RESUME ===
PASS: 4
FAIL: 0
SKIP: 1
[RESULTAT] SUCCES
```

Le sketch exécute la campagne une seule fois dans `setup()`. La fonction `loop()` reste inactive afin que le rapport ne soit pas répété.
