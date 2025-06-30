# Param8 - Contrôleur MIDI 8 paramètres

Un contrôleur MIDI polyvalent avec 8 faders motorisés et écrans OLED pour le contrôle de paramètres DAW.

## Caractéristiques

- 8 faders motorisés avec contrôle MIDI bidirectionnel
- 2 écrans OLED 256x64 pour l'affichage des paramètres
- 8 boutons rotatifs avec encodeurs
- LEDs RGB pour le feedback visuel
- Communication USB MIDI native
- Gestion des messages SysEx pour l'affichage de texte

## Matériel

- **Microcontrôleur** : Raspberry Pi Pico (RP2040)
- **Écrans** : 2x SSD1322 OLED 256x64
- **Faders** : 8x faders motorisés
- **Encodeurs** : 8x encodeurs rotatifs avec boutons
- **LEDs** : WS2812B (NeoPixel)
- **Expansion I/O** : PCF8574 pour les boutons

## Architecture du code

```
src/
├── core/           # Logique métier principale
│   ├── actions.cpp/h     # Gestion des actions utilisateur
│   ├── controls.cpp/h    # Contrôle des faders et paramètres
│   └── jsonManager.cpp/h # Gestion de la configuration JSON
├── input/          # Gestion des entrées
│   ├── buttons.cpp/h     # Boutons et encodeurs
│   └── encoders.cpp/h    # Logique des encodeurs rotatifs
├── midi/           # Communication MIDI
│   └── midi.cpp/h        # Messages MIDI et SysEx
├── view/           # Interface utilisateur
│   ├── display.cpp/h     # Gestion des écrans OLED
│   ├── faderWidget.cpp/h # Widgets d'affichage des faders
│   └── leds.cpp/h        # Contrôle des LEDs RGB
└── param8_firmware.ino   # Fichier principal
```

## Configuration PlatformIO

Le projet utilise PlatformIO avec les dépendances suivantes :

- **Framework** : Arduino pour RP2040
- **Bibliothèques** :
  - U8g2 (écrans OLED)
  - Adafruit TinyUSB Library (USB MIDI)
  - MIDI Library
  - Adafruit NeoPixel (LEDs RGB)
  - RotaryEncoder
  - Adafruit PCF8574 (expansion I/O)
  - ArduinoJson (configuration)
  - LittleFS (stockage)

## Compilation et téléchargement

1. Installer PlatformIO Core ou PlatformIO IDE
2. Cloner ce repository
3. Ouvrir le projet dans PlatformIO
4. Compiler avec `pio run`
5. Télécharger avec `pio run --target upload`

```bash
# Installation des dépendances
pio lib install

# Compilation
pio run

# Téléchargement sur le Pico
pio run --target upload
```

## Configuration MIDI

Le contrôleur utilise les messages MIDI suivants :

- **Control Change (CC)** : Contrôle des paramètres DAW
- **Program Change (PC)** : Actions sur appui long des boutons
- **SysEx** : Mise à jour des noms de paramètres et affichage de texte

### Format SysEx pour l'affichage

```
F0 [constructor] [status] [param] [data...] F7
```

- `status 0` : Nom du paramètre du fader
- `status 1` : Titre du fader  
- `status 2` : Texte boîte gauche
- `status 3` : Texte boîte droite

## Utilisation

1. Connecter le contrôleur via USB
2. Le périphérique MIDI "param8" apparaît dans votre DAW
3. Mapper les faders aux paramètres souhaités
4. Les écrans affichent automatiquement les informations des paramètres

## Développement

Le projet est structuré de manière modulaire pour faciliter les modifications :

- Ajout de nouveaux types de contrôles dans `input/`
- Personnalisation de l'affichage dans `view/`
- Extension des fonctionnalités MIDI dans `midi/`

## Licence

[Indiquez ici votre licence]

## Auteur

[Votre nom]
