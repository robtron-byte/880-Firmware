# Maße für Knöpfe und Transport-Abdeckung (System80 880)

Stand: 28.11.2026. Alle Maße in Millimetern.

## Quellen

| Kürzel | Datei | Repository |
|---|---|---|
| **DXF** | `Panel/Panel_2.0_mechanical_r12.dxf` (Löcher aus Bögen zusammengesetzt) | github.com/minisystem/880 (Branch `master`, identisch in `split`) |
| **PCB** | `TR-808-Clone.kicad_pcb` (Bauteilpositionen und Footprints) | github.com/minisystem/880 (`master` und `split` gleich) |
| **BOM** | `TR-808-Clone.csv` | github.com/minisystem/880 |
| **TPL** | `Panel/Panel 2.0 alignment template.dxf`, `Panel Template for import into Kicad.dxf` | github.com/minisystem/880 |
| **CASE** | `880_case.kicad_pcb`, `880_enclosure_*.kicad_pcb` | github.com/minisystem/880-case |
| **MX** | `TR808-Start-Stop.stl`, `TR808-Tap-Tempo.stl` | github.com/minisystem/TR-808-Cherry-MX |

**Lizenz der Originaldaten:** Die 880-Hardwaredaten von minisystem (System80) stehen unter
**CC BY-NC-SA 4.0** (Namensnennung, nicht kommerziell, Weitergabe unter gleichen Bedingungen).
Daraus abgeleitete Modelle in diesem Ordner dürfen deshalb nur nicht kommerziell genutzt und
nur unter derselben Lizenz weitergegeben werden. Das Repository TR-808-Cherry-MX steht unter CC BY-SA 3.0.

## Frontplatte (gefunden)

- Außenmaß: **304,3 × 128,5** (Eurorack 3U, 60 TE). Quelle: DXF.
- Hauptplatine dahinter: 300 × 105. Quelle: TPL.
- Umrechnung Platine → Frontplatte (Ursprung oben links): `x_Platte = x_PCB − 65,85`, `y_Platte = y_PCB − 39,25`.
  Geprüft an allen 23 Poti-Löchern, den Buchsen und den Tastern.
- Plattendicke: **nicht in den Daten.**

### Alle Löcher (Quelle: DXF, zugeordnet über PCB)

| Loch | Anzahl | Bauteil | Footprint/BOM |
|---|---|---|---|
| Ø 6,5 | 23 | Potis | R0904N-SONG-HUEI (9-mm-Serie) |
| Ø 7,5 | 1 | Poti VR18 (Tempo, links) | RD901F-ALPHA-POT |
| Ø 6,1 | 15 | 3,5-mm-Klinkenbuchsen | PJ-301B |
| Ø 5,3 | 15 | Kleine Taster (Instrument, CLEAR, BASIC VAR, I/F VAR) | B3F-1050-OMRON |
| Ø 3,3 | 56 | 3-mm-LEDs | 3MM-LED |
| Ø 9,7 | 3 | Drehschalter | BF-4050-PCB-ADAPT (SW4, SW5, SW11) |
| Ø 16,5 | 1 | MIDI-Buchse | SD-50BV-5DIN |
| Ø 3,2 | 3 | Abstandsbolzen Platte ↔ Platine (M3) | SNAP_HOLE |
| □ 9,5 × 9,5 | 16 | Step-Tasten 1–16 | Step-Platine (B3W-4050) |
| □ 19 × 19 | 2 | START/STOP und TAP | CHERRY-MX |
| Langloch 6,2 × 3,2 | 8 | Eurorack-Befestigung (Ecken und Mitte, oben und unten) | – |

### Potis

- **Mittenabstand: 18,0** (waagerecht und senkrecht). Quelle: DXF und PCB stimmen überein.
  Die frühere Angabe „17,4 mm“ kam aus der SVG-Datei. Diese SVG ist nur eine Beschriftungsvorlage,
  ihre 22 Kreise mit Ø 5,93 sind keine Bohrungen.
- Größter Knopf-Ø bei 18 mm Abstand: **höchstens 16**, besser **15** (damit man zwei Nachbarn noch greifen kann).
- Engste Nachbarn außer den Potis: MIDI-Buchse 22 mm neben dem ersten Poti (passt bis Ø 16).
- Bohrung in der Platte Ø 6,5 → Schaft vermutlich **Ø 6**. Die Form (gerändelt, D, rund) ist nirgends angegeben.

Poti-Lochmitten auf der Frontplatte (von oben links gemessen):

| Reihe (y) | x-Positionen |
|---|---|
| 19,75 | 71,15 · 89,15 · 107,15 · 125,15 · 143,15 · 161,15 · 179,15 · 197,15 · 215,15 · 233,15 · 251,15 · 269,15 |
| 37,75 | 89,15 · 107,15 · 125,15 · 143,15 · 161,15 · 233,15 |
| 55,75 | 89,15 · 107,15 · 233,15 · 251,15 |
| 95,75 | 290,65 (VR1, rechts unten bei den Buchsen) |
| 72,25 | 18,65 (VR18, Alpha, Ø 7,5) |

**Bassdrum-Level** ist vermutlich VR4 (x 89,15 / y 19,75), **Snare-Level** VR7 (x 107,15 / y 19,75).
Das ergibt sich aus der Spaltenfolge der TR-808 (Accent, BD, SD …), ist aber nicht beschriftet.

### Sonstiges

- Desktop-Gehäuse: Im Repository 880-case liegt nur eine Platine für die Gehäuse-Rückseite
  (279,5 × 38, mit 22 Buchsen und 2 MIDI-Buchsen). Das deutet auf **ca. 38 mm Innenhöhe hinten** hin.
  Außenmaße, Seitenteile und Befestigung des Gehäuses stehen **nicht** in den Daten.
- Originale START/STOP-Kappe für Cherry MX (TR-808-Version, nicht unbedingt die der 880): 33,5 × 15,3, Höhe 8,95 über dem Taster. Quelle: MX.

## Was noch fehlt (muss gemessen werden)

1. Poti-Schaft: Durchmesser, Form (gerändelt mit wie vielen Rillen / D-Fläche / glatt), Länge über der Frontplatte.
2. Originalknopf: Außendurchmesser, Höhe, Bohrungstiefe, ggf. Form (Rock, Kegel, Zylinder).
3. Frontplattendicke.
4. Desktop-Gehäuse: Außenmaße (Breite, Tiefe, Höhe vorn und hinten), wie weit die Seitenteile
   über die Frontplatte ragen, wie das Gehäuse verschraubt ist.
5. Höchster Punkt über der Frontplatte: Knöpfe, START/STOP-Taste, Drehschalter, MIDI-Buchse, eingesteckte Kabel.
