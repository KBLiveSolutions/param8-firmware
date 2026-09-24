# Gestes physiques — step sequencer & loop record

Résumé de la réflexion sur comment intégrer le step sequencer et l'enregistrement
de loops CC dans les gestes existants du controller, sans casser l'existant ni
introduire de collisions. Document de travail — les seuils précis restent à
trancher (voir "Questions ouvertes" en bas).

## Gestes actuels (vérifiés dans le code, presets utilisateur 0-5)

| Geste | Action |
|---|---|
| Shift (hold) | Affiche les noms de preset sur les labels de boutons |
| Shift + push encodeur N | Sélectionne le preset N (occupe les 8 encodeurs) |
| Shift + Latch | Entre en mode Revert |
| Latch (pendant Revert, Shift relâché) | Applique le revert (restaure les valeurs d'origine) |
| Shift (pendant Revert) | Sort du Revert **sans** revert (garde les modifs) |
| Latch seul, hold >300ms | Active le Latch (bufferise les mouvements d'encodeurs, les rejoue au relâchement) |
| Double-tap Latch, release rapide | Arme puis déclenche "assign name" sur le dernier contrôle touché |
| Double-tap Latch + hold ≥1s | "De-assign" (efface le nom) |
| Push encodeur seul (court) | Envoie CC/Note à 127, puis 0 au relâchement |
| Rotation encodeur seule | Change la valeur (relatif ou absolu selon le preset) |

## Gestes envisagés et écartés

| Geste | Pourquoi écarté |
|---|---|
| Long-press push encodeur seul | Doit rester libre pour les boutons momentary (sustain d'une note) |
| Double-tap push encodeur seul | Pas pratique, risque de retrigger une note par accident |
| Push maintenu + rotation du même encodeur | Pas pratique ergonomiquement |
| Latch + push encodeur | Réservé pour une autre feature : latcher un appui bouton (le rendre sustain/toggle) |
| Shift "à retardement" (délai avant que les presets apparaissent) | Remplacé par la solution plus simple ci-dessous — plus besoin d'arbitrer une course entre tap/turn/timeout |

## Nouveaux gestes retenus (version simplifiée)

| Geste | Action |
|---|---|
| Double-tap Shift + hold | Accède aux presets (reprend le pattern double-tap/hold déjà utilisé par Latch) |
| Shift + push encodeur N (simple) | Entre dans "Encoder Settings" pour cet encodeur (step sequencer / LFO / modulation) |
| Shift + turn encodeur N | Démarre l'enregistrement d'une loop CC sur cet encodeur |

Plus simple que la version précédente (pas de délai à régler, pas de course à
arbitrer) : Shift+push et Shift+turn sont deux gestes distincts et immédiats,
et l'accès aux presets passe par un double-tap dédié au lieu de partager le
même déclencheur que "Encoder Settings".

## Protection contre le faux déclenchement (push → micro-rotation)

Problème identifié : appuyer sur un encodeur à poussoir fait presque toujours
vibrer légèrement l'axe. Si "Shift + rotation" déclenche le record au moindre
delta, un simple Shift+push visant "Encoder Settings" pourrait accidentellement
lancer un enregistrement.

Deux filtres à combiner (coût quasi nul, réutilisent l'état déjà tracké) :

1. **Ignorer toute rotation tant que le push de ce même encodeur est
   actuellement enfoncé** (`buttons[idx].currentState`, déjà suivi dans
   `buttons.cpp`). Le twitch parasite arrive pendant que le doigt appuie ;
   une fois le push relâché, la rotation redevient valide pour armer le
   record.
2. **Seuil de magnitude** : n'armer le record que si le delta cumulé dépasse
   ~2-3 crans, pas au premier micro-mouvement — couvre le cas où le push
   serait relâché une frame avant que le twitch soit lu.

Avec les deux, il faut à la fois avoir relâché le push ET tourner franchement
d'au moins 2-3 crans pour déclencher un record — peu probable en visant
juste un tap.

## LFO

En plus du step sequencer, chaque encodeur peut avoir un LFO — l'un ou l'autre,
pas les deux en même temps sur le même encodeur. Le bouton "Clear" (position 1,
haut-gauche de la page Encoder Settings) sert maintenant de toggle step-seq/LFO
pour l'encodeur cible.

Paramètres du LFO :
- **Waveform** : forme d'onde (sine, triangle, square, saw up, saw down pour
  l'instant — `Lfo::waveformLabel`)
- **Rate** : synced au clock MIDI pour l'instant (réutilise le même `SeqRate`
  que le step sequencer — pas de mode "free" pour le moment)
- **Value** : la valeur centrale, même sens que la valeur CC normale — le LFO
  oscille autour
- **Amount** : profondeur de la modulation

Implémenté côté firmware (`src/sequencer/lfo.{h,cpp}`, moteur autonome, même
pattern que `Sequencer` — un `Lfo` par track 0-7, calcule une valeur continue
à chaque tick d'horloge en fonction de la phase dans le cycle, et n'envoie un
CC que si la valeur a changé). **Pas encore branché à l'UI ni au toggle
Clear/step-seq-LFO** — ça viendra avec la page Encoder Settings elle-même.

## Layout de la page "Encoder Settings"

Une fois entré (Shift + push sur l'encodeur N, voir plus haut), les 8 encodeurs
physiques sont repurposés pour éditer les réglages de l'encodeur cible N :
step sequencer / LFO (au choix via Clear) + assignation MIDI + slew. L'édition
CC/Ch dédiée (accès direct sans passer par tout le reste) se fait en appuyant
sur l'encodeur 5.

| Position | Push (bouton) | Turn (rotation) |
|---|---|---|
| 1 (haut, col1) | Clear | Step select |
| 2 (haut, col2) | — | Step value |
| 3 (haut, col3) | Synced/Free (mode du Rate) | Rate |
| 4 (haut, col4) | Exit | Length |
| 5 (bas, col1) | PUSH/TURN editing | Ch |
| 6 (bas, col2) | Note/CC type | Note/CC # |
| 7 (bas, col3) | — | — |
| 8 (bas, col4) | Synced/Free (mode du Slew) | Slew time |

Précisions :
- **Rate** et **Slew** ont chacun leur propre toggle Synced/Free indépendant
  (position 3 pour le rate du step sequencer, position 8 pour le slew).
- **Ch / Note-CC# / Note-CC type** (positions 5-6) éditent soit le mapping
  MIDI du **push** de l'encodeur cible (`controls.getButtonShort(idx)`),
  soit celui de sa **rotation** (`controls.getEncoder(idx)`) — deux structures
  déjà distinctes et indépendantes dans le firmware. Le push de la position 5
  ("PUSH/TURN editing") bascule lequel des deux est actuellement visé par
  ces 3 contrôles.

## Questions ouvertes

1. **Sémantique start/stop du record** : Shift+turn démarre l'enregistrement
   dès la rotation détectée — un deuxième Shift+turn sur le même encodeur
   l'arrête, ou faut-il un geste explicite séparé pour stopper ?
2. **Délai de grâce optionnel** : après relâchement du push, ignorer la
   rotation pendant encore 50-100ms (couvre le cas où le doigt continue de
   glisser juste après avoir relâché) — à ajouter ou pas ?
3. **Seuil double-tap Shift** : réutiliser le même seuil que Latch (300ms) ou
   un seuil différent ?
