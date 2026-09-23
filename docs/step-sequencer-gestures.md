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
| Double-tap Shift | Faisable techniquement, mais remplacé par une solution plus simple ci-dessous |

## Nouveaux gestes retenus

Le principe : transformer Shift en modificateur **à retardement**, arbitré par le
premier des trois événements suivants qui se produit pendant le hold :

| Geste | Action |
|---|---|
| Shift tenu, tap rapide (push+release) d'un encodeur **avant** le seuil | Entre dans "encoder settings" pour cet encodeur (step sequencer inclus) |
| Shift tenu, rotation d'un encodeur **avant** le seuil | Démarre l'enregistrement d'une loop CC sur cet encodeur |
| Shift tenu **au-delà** du seuil, sans tap ni rotation | Labels de preset apparaissent (comportement actuel, juste retardé) |
| Shift (après le seuil) + push encodeur N | Sélectionne le preset N — inchangé dans le fond |

Avantages : aucune collision avec l'existant, pas de nouveau geste à mémoriser
au-delà de "tenir Shift et voir ce qui se passe", et le state-machine de
Latch (double-tap/hold) n'a pas besoin d'être dupliqué pour Shift.

## Protection contre le faux déclenchement (push → micro-rotation)

Problème identifié : appuyer sur un encodeur à poussoir fait presque toujours
vibrer légèrement l'axe. Si "Shift + rotation" déclenche le record au moindre
delta, un simple tap visant "encoder settings" pourrait accidentellement
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

## Questions ouvertes

1. **Seuil du délai Shift** : 300 ou 500ms avant que les labels de preset
   apparaissent ?
2. **Sémantique start/stop du record** : Shift+turn démarre l'enregistrement
   dès la rotation détectée — un deuxième Shift+turn sur le même encodeur
   l'arrête, ou faut-il un geste explicite séparé pour stopper ?
3. **Délai de grâce optionnel** : après relâchement du push, ignorer la
   rotation pendant encore 50-100ms (couvre le cas où le doigt continue de
   glisser juste après avoir relâché) — à ajouter ou pas ?
