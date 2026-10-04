# DARK BLOOD – Animation Pipeline (UE 5.8)

## Ziel
Natürliche Bewegung für Laufen, Sprinten, Stoppen, Drehen, Springen, Dodge, Katana-Kampf, Trefferreaktionen, Mimik und Cinematics.

## Kostenlose Basis 1 – Game Animation Sample Project 5.8
Epic stellt für UE 5.8 das Game Animation Sample Project bereit. Es enthält über 500 game-ready Animationen und ein modernes Motion-Matching-System.

Nutzen für DARK BLOOD:
- Idle
- Locomotion
- Start/Stop
- Richtungswechsel
- Sprint
- Traversal
- Ledge/Vaulting
- natürliche Übergänge

Nicht blind kopieren: auf DARK-BLOOD-Character, Kamera und Combat-Geschwindigkeit anpassen.

## Kostenlose Basis 2 – MetaHuman Animator 5.8
UE 5.8 kann Markerless Motion Capture aus normalem Videomaterial erzeugen. Eine Webcam oder Smartphone-Aufnahme kann für Gesicht und Körper genutzt werden.

Damit kann der Nutzer ohne Mocap-Anzug eigene Bewegungen aufnehmen, z. B.:
- Gespräch/Mimik
- Verbeugung
- Wachen-Idle
- Händlergesten
- langsames Ziehen eines Schwerts
- Schmerzreaktionen
- Story-Cinematics

Die Body-Capture-Funktion ist in UE 5.8 experimentell und muss vor Produktion getestet werden.

## Combat
Kostenloser Start:
- Game Animation Sample für Locomotion.
- kostenlose Fab-Mocap-Packs nur nach Lizenzprüfung.
- eigene Anpassung mit Control Rig / Animation Layers / Motion Warping.
- für Katana-spezifische Moves bei Bedarf eigene Video-Mocap-Aufnahmen und manuelle Cleanup-Pässe.

## Pflicht-Animationen Player
### Movement
- Idle variants
- Walk 8-way
- Jog 8-way
- Sprint
- Start/Stop
- Turn 90/180
- Jump Start/Loop/Land
- Double Jump
- Hard Landing
- Slope Up/Down

### Combat
- Weapon Draw
- Weapon Sheathe
- Combat Idle
- Combat Strafe 8-way
- Light Attack chain
- Heavy Attack
- Charged Attack
- Sprint Attack
- Jump Attack
- Air Attack
- Dodge 4/8-way
- Roll
- Dash
- Block
- Perfect Parry
- Counter
- Hit reactions directional
- Knockdown/Getup
- Finisher
- Death variants

## NPC
- conversation gestures
- sitting
- eating/drinking
- blacksmith work
- farming
- carrying
- walking with goods
- guard patrol
- bowing/greeting
- sleep/rest
- panic/run

## Pferde/Kutschen
- mount/dismount
- horse idle/walk/trot/gallop
- reins/driver animation
- seated passengers
- attack/panic reaction

## Boss/Vasall
Jeder Vasall braucht eigene Signature-Animationen. Keine reine Wiederverwendung eines generischen Sword Set.

## Technischer Stack
- Animation Blueprint
- Motion Matching / Pose Search
- Choosers
- Motion Warping
- Root Motion für definierte Angriffe
- IK / Foot Placement
- Control Rig
- Animation Montages
- Sync Markers
- Gameplay Tags für States
- serverautoritatives Combat Timing

## Regel
Animation entscheidet nie allein über Schaden. Damage Window/Hitbox muss serverseitig kontrolliert werden.
