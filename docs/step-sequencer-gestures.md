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
  que le step sequencer — pas de mode "free" pour le moment). Échelle
  complète, du plus lent au plus rapide : `4 BARS, 2 BARS, 1 BAR, 1/2, 1/4,
  1/8, 1/8T, 1/16, 1/16T, 1/32` (les rates >1/4 s'appliquent aussi bien au
  step sequencer qu'au LFO, même enum `SeqRate` partagé)
- **Value** : la valeur centrale, même sens que la valeur CC normale — le LFO
  oscille autour
- **Amount** : profondeur de la modulation

Implémenté côté firmware (`src/sequencer/lfo.{h,cpp}`, moteur autonome, même
pattern que `Sequencer` — un `Lfo` par track 0-7, calcule une valeur continue
à chaque tick d'horloge en fonction de la phase dans le cycle, et n'envoie un
CC que si la valeur a changé), **et maintenant branché dans le test boot
standalone** (`sequencerView.{h,cpp}`) : toggle step-seq/LFO fonctionnel,
édition waveform/value/rate/amount en direct. Toujours pas branché à une
vraie page "Encoder Settings" ni aux gestes Shift — le test boot reste isolé
(voir layout ci-dessous, qui documente maintenant l'état réellement
implémenté plutôt qu'un plan).

Affichage : la forme d'onde est dessinée en trait continu (pas des barres)
sur toute la largeur d'un seul écran (`display1`, 256px), échantillonnée à
`LFO_DISPLAY_RES` = 64 points par cycle complet, avec un curseur de lecture
(playhead) qui suit la phase réelle envoyée par le moteur — donc visuellement
réactif en direct aux changements de waveform/value/amount/rate. Le second
écran garde ses labels/boîtes mais pas de courbe **pour l'instant** (à
réévaluer une fois qu'on saura ce qu'on veut y montrer).

## Layout du test boot standalone (step sequencer + LFO)

Actuellement implémenté directement au boot (`SEQUENCER_TEST_BOOT`), sans
geste Shift ni page dédiée — ce layout remplace le brouillon précédent de
la page "Encoder Settings", qu'il préfigure :

| Position | Push (bouton) | Turn (rotation) |
|---|---|---|
| 1 (haut, col1) | Toggle LFO/Step seq | Step select / Waveform (LFO) |
| 2 (haut, col2) | — | Step value / Value (LFO) |
| 3 (haut, col3) | Synced/Free (mode du Rate) — placeholder, non fonctionnel | Rate |
| 4 (haut, col4) | Clear (reset step à 0 / amount à 0) | Length / Amount (LFO) |
| 5 (bas, col1) | Edit MIDI — placeholder | — |
| 6 (bas, col2) | — | — |
| 7 (bas, col3) | — | — |
| 8 (bas, col4) | Clear — placeholder | — |

Précisions :
- **Rate** est un contrôle partagé : une seule valeur par mode (step
  sequencer et LFO ont chacun leur propre `SeqRate`), la molette agit sur
  celui du mode actif.
- **Synced/Free** (position 3) et les deux `Clear`/`Edit MIDI` de la rangée
  du bas (positions 5, 6, 7, 8) sont des placeholders : la boîte s'affiche
  mais rien n'est branché derrière pour l'instant (pas de mode "Free" pour
  le rate, pas de page d'édition MIDI).
- Le toggle (position 1) désarme l'un des deux moteurs et arme l'autre
  (`sequencer.arm`/`lfo.arm`), donc un seul des deux tourne et envoie du
  MIDI à la fois sur la track de test.
- L'affichage LFO réutilise la grille de steps : la forme d'onde est
  échantillonnée sur 16 colonnes (un cycle complet), avec un repère de
  lecture (« playhead ») qui suit la phase réelle envoyée par le moteur —
  donc visuellement réactif en direct aux changements de waveform/value/
  amount/rate.
- La page "Encoder Settings" proper (accessible via Shift+push, avec édition
  MIDI dédiée sur l'encodeur 5, cf. gestes ci-dessus) reste à construire —
  ce layout de test boot en est la base de départ, pas encore le produit
  final.

## Questions ouvertes

1. **Sémantique start/stop du record** : Shift+turn démarre l'enregistrement
   dès la rotation détectée — un deuxième Shift+turn sur le même encodeur
   l'arrête, ou faut-il un geste explicite séparé pour stopper ?
2. **Délai de grâce optionnel** : après relâchement du push, ignorer la
   rotation pendant encore 50-100ms (couvre le cas où le doigt continue de
   glisser juste après avoir relâché) — à ajouter ou pas ?
3. **Seuil double-tap Shift** : réutiliser le même seuil que Latch (300ms) ou
   un seuil différent ?
