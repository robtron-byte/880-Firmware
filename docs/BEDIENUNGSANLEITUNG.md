# 880 Perform – Bedienungsanleitung

Firmware für die **System80 880 Desktop (MK2)**, die live mit einer Hand gespielt werden kann. Basis ist die Original-Firmware 1.1.5 von minisystem.

> **Status:** Alle Funktionen sind umgesetzt, sie lassen sich bauen und sind automatisch getestet, **aber noch nicht am Gerät ausprobiert** (🧪). Vor dem ersten Live-Einsatz bitte die [Test-Checkliste](TEST-CHECKLISTE.md) abarbeiten.
> Interaktive Übersicht mit Klick auf das Panel: https://claude.ai/artifact/YR8iXaFYWAgP2C39WFYmb2

![880 Desktop](img/880-desktop-top.webp)
*Foto: System80, 880 Desktop*

**Tastennamen in dieser Anleitung**
- **Instrument 1–12**: die runden Tasten unter den Reglern. 1 = ACCENT, 2 = BD … 12 = CH.
- **Step 1–16**: die farbigen Tasten unten.
- **SHIFT** = WRITE/NEXT (rechts unten).
- **I/F-VAR** = Taste rechts über TAP. Gedrückt halten heißt auch **ALT**.
- **BASIC-VAR** = BASIC-VARIATION (links über START/STOP).
- **MODE** = große runde Taste oben links.
- **CLEAR** = kleine rote Taste darunter.

**Klick oder halten**
- **Klick** heißt: kurz drücken und loslassen, ohne eine andere Taste dazu.
- **Halten + Taste** heißt: Taste gedrückt halten und die zweite Taste dazu drücken.

---

## Schnellstart

| Was | Wie |
|---|---|
| Zwischen EDIT (Steps programmieren) und PERFORM (live spielen) wechseln | **MODE** |
| Pattern wählen | **BASIC-VAR klicken**, dann **Step 1–12** |
| Bank wählen | **BASIC-VAR halten + Step 1–12** |
| Instrument muten (PERFORM) | **Instrument 2–12** |
| Alle Mutes aufheben (PERFORM) | **Instrument 1** |
| Step-Loop (PERFORM) | **Step halten** (mehrere halten = laufen nacheinander) |
| Fill im nächsten Takt (PERFORM) | **TAP** |
| Live aufnehmen an/aus (PERFORM) | **I/F-VAR halten + TAP** |
| Pattern auf den Stand beim Auswählen zurücksetzen | **SHIFT + TAP** |
| Ratchet / Odds eines oder mehrerer Steps (EDIT) | **Step(s) halten + TAP** / **Step(s) halten + SHIFT** |

---

## Modi

| Funktion | Tasten | Hinweis |
|---|---|---|
| Durchschalten | **MODE** | PERFORM → 1st PART → 2nd PART → PERFORM. Den 2nd PART gibt es dabei nur, wenn er Inhalt hat. Pattern, Takt und Mutes laufen nahtlos weiter, es wird nichts geladen. |
| 2nd PART aktivieren | **SHIFT + MODE** | Geht aus jedem Modus. Ein leerer 2nd PART wird mit dem 1st PART gefüllt. Änderst du nichts und gehst wieder raus, wird die Kopie verworfen. Im 2nd PART bringt SHIFT + MODE dich zum 1st PART. |

**LEDs bei MODE:**
- 1st/2nd PART = EDIT
- MANUAL PLAY = PERFORM
- PLAY = Pattern-Ebene ist aktiv
- COMPOSE = Aufnahme läuft

Der Song-Modus ist entfernt.

---

## Patterns, Banks, Variationen (EDIT und PERFORM)

| Funktion | Tasten | Hinweis |
|---|---|---|
| Pattern-Ebene ein/aus | **BASIC-VAR klicken** | Die Step-Tasten 1–12 sind jetzt Pattern-Tasten. |
| Pattern wählen | Pattern-Ebene: **Step 1–12** | Läuft der Sequencer, wechselt es am Ende des Patterns, sonst sofort. Das wartende Pattern blinkt. |
| Bank wählen | **BASIC-VAR halten + Step 1–12** | Wechselt zusammen mit dem Pattern. |
| Variation A → AB → B | **SHIFT + BASIC-VAR** | |
| Intro/Fill-Variation A/B | **I/F-VAR halten + BASIC-VAR** | |
| Fill-Pattern wählen bzw. Pattern 13–16 bearbeiten | EDIT, Pattern-Ebene: **Step 13–16** | Solange du einen Fill bearbeitest, blinkt in der Step-Reihe sein Step (13–16). |
| Pattern wählen bei Stop | EDIT: **Step 1–16**, PERFORM: **Step 1–12** (13–16 = Fill-Pattern) | |
| Abspielrichtung | **I/F-VAR halten + Step 13 / 14 / 15 / 16** | vorwärts / rückwärts / Ping-Pong / zufällig, wird mit dem Pattern gespeichert |
| Reload | **SHIFT + TAP** (in PERFORM: beim Loslassen von TAP, wenn du dabei nichts gespielt hast) | Stand beim letzten Auswählen. Läuft der Sequencer, passiert das am Taktende. Wirkt nicht während eines Fills. |

---

## PERFORM

### Instrument-Tasten
**I/F-VAR klicken** wechselt zwischen **A** (LED A neben I/F-VAR: Step-Tasten = LOOP/STEPS SETZEN) und **B** (LED B: Step-Tasten = Pads und Mute-Szenen).

Die Instrument-Tasten **muten immer** – in A und in B. Nur wenn I/F-VAR auf A steht **und** die Step-Tasten auf STEPS SETZEN (SHIFT-Klick), wählen sie stattdessen das Instrument, dessen Steps du setzt.

| Funktion | Tasten |
|---|---|
| Mute an/aus (Tom und Conga, RS und CL, CP und MA immer zusammen) | **Instrument 2–12** (gemutete leuchten) |
| Alle Mutes und Solo aufheben | **Instrument 1** |
| Instrument wählen (zum Steps setzen) | A + STEPS SETZEN: **Instrument 1–12** |
| Solo an/aus | **CLEAR halten + Instrument** |
| Low/Mid/Hi **Tom** spielen | **I/F-VAR halten + Instrument 4 / 5 / 6** |
| Low/Mid/Hi **Conga**, Claves, Maracas spielen | **SHIFT halten + Instrument 4 / 5 / 6 / 7 / 8** |

### Step-Tasten
**SHIFT klicken** wechselt zwischen **LOOP** und **STEPS SETZEN** (gilt, solange I/F-VAR auf A steht; bei STEPS SETZEN wählen die Instrument-Tasten das Instrument). Beim Wechsel nach PERFORM ist LOOP aktiv.

**Steht I/F-VAR auf B, werden die Step-Tasten zu Pads und Mute-Szenen:**

| Funktion | Tasten | Hinweis |
|---|---|---|
| Instrument spielen | **Step 2–12** = BD, SD, LT, MT, HT, RS, CP, CB, CY, OH, CH | Anschlagen spielt sofort bzw. auf dem nächsten 16tel. **Halten = 16tel-Wirbel im Takt.** Mehrere Pads gleichzeitig gehen. Spielt auch, wenn das Instrument gemutet ist. |
| Congas, Claves, Maracas | **SHIFT + Step 4–8** | Tom/Conga, Rimshot/Claves und Clap/Maracas teilen sich je eine Stimme: Spielt das Pattern auf demselben 16tel das Gegenstück, klingt nur eins von beiden. |
| Pads aufnehmen | **SHIFT + TAP halten** + Pads | Schläge und Wirbel werden ins Pattern geschrieben (siehe „Aufnahme halten“). Pad 4–8 bleiben dabei Toms/Rimshot/Clap. |
| Mit Accent | **Step 1 halten** + Pads | Der Accent gilt beim 808-Schaltkreis für den ganzen Schlag, also auch für Pattern-Instrumente auf diesem 16tel. |
| Mute-Szenen | **Step 13–16** | wie in der Pattern-Ebene (abrufen, speichern, bearbeiten, CLEAR + Szene = löschen) |
| Fill-In wählen | **SHIFT + Step 13–16** | |

Anzeige: Gehaltene Pads und gerade gespielte Instrumente leuchten. Mute-Szenen mit Inhalt leuchten, leere blitzen alle 2 Sekunden kurz auf. Mit gehaltenem SHIFT zeigt 13–16 den gewählten Fill-In. Steps setzen geht in PERFORM nur über STEPS SETZEN (I/F-VAR auf A).

| Funktion | Tasten | Hinweis |
|---|---|---|
| Step-Loop | LOOP: **Step halten** | wiederholt in 16teln; mehrere gehaltene Steps laufen in Step-Reihenfolge; Loslassen = nahtlos weiter |
| Step setzen / löschen | STEPS SETZEN: **Step 1–16** | für das gewählte Instrument, im gerade spielenden Part (gestoppt: 1st PART) |
| Shuffle | **SHIFT halten + Step 1–6** | |
| Fill im nächsten Takt | **TAP** | Stopp: TAP schaltet das Intro um |

### Pattern-Ebene (BASIC-VAR klicken)

| Funktion | Tasten | Hinweis |
|---|---|---|
| Pattern wählen | **Step 1–12** | beendet eine Kette |
| Kette | **Pattern halten + zweites Pattern drücken** | spielt vom ersten bis zum zweiten Pattern im Kreis, nur in PERFORM |
| Mute-Szene abrufen | **Step 13–16 drücken** | sofort beim Drücken |
| Neue Mute-Szene speichern | **leere Szene 1 s halten** | Die aktuellen Mutes werden übernommen, der Step blinkt schnell. Solange du hältst, kannst du weitere muten oder entmuten. Gespeichert wird beim Loslassen. |
| Gespeicherte Szene bearbeiten | **Szene halten + Instrumente muten/entmuten** | Die Szene ist sofort aktiv; was du beim Halten änderst, wird beim Loslassen in ihr gespeichert. Nimmst du alle Mutes heraus, ist die Szene danach leer. |
| Mute-Szene löschen | **CLEAR halten + Step 13–16** | Danach ist sie leer und kann neu gespeichert werden. Leere Szenen blitzen in I/F-VAR B alle 2 s auf. |
| Fill-In wählen (auch während es läuft) | **SHIFT halten + Step 13–16** | Bei gehaltenem SHIFT leuchtet der gewählte Fill wie ein Pattern. TAP spielt ihn im nächsten Takt. |
| Pattern kopieren | **SHIFT halten + Quelle**, dann **SHIFT halten + Ziel** | Quelle blinkt; Bankwechsel dazwischen erlaubt; Quelle nochmal = Abbruch |
| Pattern löschen | **CLEAR halten + Pattern zweimal drücken** | |

### Aufnahme

| Funktion | Tasten | Hinweis |
|---|---|---|
| Aufnahme an/aus | **I/F-VAR halten + TAP** | COMPOSE-LED blinkt |
| Aufnahme halten | **SHIFT + TAP halten** (Sequencer läuft) | Alles, was du dabei spielst, wird auf die 16tel gerundet ins Pattern geschrieben: Pads und Wirbel (I/F-VAR auf B) und die Instrument-Tasten (Taste 2–12, mit SHIFT Taste 4–8 = Congas/Claves/Maracas). Am besten erst SHIFT, dann TAP drücken. Spielst du nichts, ist es beim Loslassen ein Reload. |
| Einspielen | **Instrument 1–12** (auch Toms/Congas wie oben) | gerundet auf den nächsten 16tel; während der Aufnahme spielen die Instrument-Tasten immer, egal welche Ebene |

Gespeichert wird **immer automatisch am Taktende**.

---

## EDIT

Steps setzen, löschen und Odds/Ratchets einstellen geht in EDIT, **während der Sequencer läuft**. Bei Stop wählen die Step-Tasten Patterns.

| Funktion | Tasten | Hinweis |
|---|---|---|
| Instrument wählen | **Instrument-Taste** | wie bisher (inkl. Tom/Conga-Umschaltung durch erneutes Drücken) |
| Step setzen | **Step drücken** | sofort |
| Step löschen | **gesetzten Step drücken und loslassen** | gelöscht wird beim Loslassen |
| Accent | **Instrument 1**, dann Steps | |
| Odds und Ratchet **ansehen** | **gesetzten Step halten** | PRE-SCALE-LEDs zeigen im Wechsel (je ½ s): Ratchet = LED leuchtet, Odds = LED blinkt. Nichts wird verändert. |
| Ratchet 1× → 2× → 3× → 4× | **gesetzten Step halten + TAP** (auch mehrere Steps gleichzeitig halten: alle bekommen denselben Wert) | danach zeigen die LEDs nur noch den Ratchet (leuchtet) |
| Odds: jeder 1./2./3./4. Durchlauf | **gesetzten Step halten + SHIFT** (auch mehrere Steps gleichzeitig: alle bekommen denselben Wert) | danach zeigen die LEDs nur noch die Odds (blinkt) |
| Tap-Write | **TAP** im Takt | wird auf den nächsten 16tel gerundet |
| Pattern-Länge (gilt für beide Parts) | **CLEAR halten + Step** im 1st PART | |
| 2nd PART entfernen | **CLEAR halten + Step 1** im 2nd PART | |
| Aktuelles Instrument löschen | **CLEAR klicken** | nur im Part, den du gerade bearbeitest (1st oder 2nd PART) |
| Ganzes Pattern löschen | **CLEAR zweimal kurz** | |
| Variation A nach B kopieren | **SHIFT + CLEAR** | beide Parts, inkl. Odds/Ratchets; Steps blinken kurz |
| Shuffle | **SHIFT halten + Step 1–6** | |
| Mute an/aus | **SHIFT halten + Instrument** | |
| Alle Mutes aufheben | **CLEAR + Instrument 1** | |
| Solo | **CLEAR + Instrument** | wie bisher |

Die **PRE-SCALE-LEDs 1–4 zählen die Durchläufe** des Patterns, solange es läuft. Daran siehst du, wann Steps mit Odds „jeder 2./3./4. Durchlauf“ dran sind: Sie spielen bei LED 1, „jeder 2.“ auch bei LED 3. **Steps mit Odds oder Ratchet** erkennst du in der Step-Reihe daran, dass sie einmal pro Sekunde ganz kurz flackern – unabhängig vom Tempo, auch während das Pattern läuft: **1× kurz aus = Odds**, **2× kurz aus = Ratchet**, **3× kurz aus = Odds und Ratchet**. Den Rest der Sekunde leuchten sie normal (gilt für das gewählte Instrument, in EDIT und in PERFORM bei STEPS SETZEN). Odds und Ratchets: höchstens **31 Steps pro Pattern** (beide Parts und Variationen zusammen). Ist die Liste voll, blinken die Steps kurz.

---

## Einstellungen (Sequencer gestoppt)

| Funktion | Tasten |
|---|---|
| Sync-Modus | **SHIFT + AUTO FILL IN** (wie bisher) |
| Sync-Modus speichern | **I/F-VAR + AUTO FILL IN** |
| MIDI-Kanal | EDIT: **SHIFT + I/F-VAR halten + Step 1–16** |
| DIN-Sync-Teiler (DIN Slave) | EDIT: **SHIFT + Step 1–6** |
| DIN-Reset an/aus | EDIT: **SHIFT + Step 8** (bei gehaltenem SHIFT leuchtet Step 8, wenn er an ist) |

**TRIGGER OUT 1** feuert mit dem **Low Tom**, **TRIGGER OUT 2** mit der **Low Conga**. Mute, Odds und Ratchets gelten auch für die Ausgänge.

---

## Was gegenüber der Original-Firmware weggefallen ist

- Song-Modus (PLAY/COMPOSE RHYTHM, INS/DEL); Ersatz ist die Kette
- 32tel-Zwischenschläge (2nd PART ohne Länge); der 2nd PART ist jetzt immer so lang wie der 1st PART
- Roll-Modus (SHIFT + Step 9–14); Ersatz ist der Step-Loop
- AUD Live-Hits; Ersatz ist die PERFORM-Aufnahme
- Trigger-Eingänge (TII) und frei wählbare Trigger-Zuweisung
- Pre-Scale-Wahl (SHIFT + CLEAR): Die Auflösung ist fest auf PRE-SCALE 3 (16tel im 4/4-Takt). Patterns mit anderem Pre-Scale spielen in dieser Auflösung und werden beim nächsten Speichern so abgelegt.
- Shift-Lock (SHIFT + ACCENT)
- Step-Fill (SHIFT + TAP), Instrument löschen mit CLEAR + TAP; Variation A→B kopieren liegt jetzt auf SHIFT + CLEAR
- Pattern-Kopieren bei Stop mit CLEAR + Step; Ersatz ist das neue Kopieren in PERFORM
- Der Modus PATTERN CLEAR mit „alle Patterns löschen“ und der Versionsanzeige; einzelne Patterns löschst du mit CLEAR in der Pattern-Ebene

**Achtung beim Zurückspielen der Original-Firmware:**
- **Songs:** Gespeicherte Songs sind nicht mehr da. Der Reload-Speicher liegt im ehemaligen Song-Bereich.
- **Patterns:** Patterns mit 32tel-Zwischenschlägen oder anderer 2nd-PART-Länge werden beim nächsten Speichern angepasst. Das ist dauerhaft.
- **Odds, Ratchets, Abspielrichtung:** Sie liegen in einem eigenen Speicherbereich, die Original-Firmware ignoriert sie.
