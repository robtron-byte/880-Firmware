# Test-Checkliste 880 Perform (eine Seite)

Erst die Grundfunktionen, dann die neuen Funktionen. Was nicht klappt, bitte mit Nummer melden.
Rückweg jederzeit: [Original-Firmware aufspielen](UPDATE-ANLEITUNG.md#wenn-etwas-schiefgeht).

## A. Grundfunktionen (vor allem anderen)
- [ ] A1 Nach dem Update sind deine Patterns noch da (einige Patterns in verschiedenen Banks anhören).
- [ ] A2 START/STOP und Tempo-Regler funktionieren wie gewohnt, alle Patterns laufen in 16teln (Pre-Scale fest).
- [ ] A3 Sync: MIDI Master (MIDI-Clock raus), MIDI Slave, DIN-Sync Master/Slave – so wie du es nutzt.
- [ ] A4 Shuffle (SHIFT + Step 1–6) und Variation A/AB/B (SHIFT + BASIC-VAR).
- [ ] A5 Aus- und wieder einschalten: letztes Pattern/Bank wird wieder geladen.
- [ ] A6 Version: SHIFT + MODE, dann SHIFT + TAP halten zeigt 1.1.6. Mit MODE zurück.

## B. EDIT
- [ ] B1 Steps setzen/löschen, Accent, Instrument wählen (Tom/Conga umschalten).
- [ ] B2 TAP-Einspielen landet auf dem erwarteten Step (knapp davor und knapp danach probieren).
- [ ] B3 CLEAR klicken löscht nur das Instrument, CLEAR zweimal das ganze Pattern.
- [ ] B4 Länge: CLEAR + Step. I/F-VAR + MODE: 2nd PART ist eine Kopie, ohne Änderung wieder raus = weg.
- [ ] B5 Ratchet: Step halten + TAP (1×–4×), Odds: Step halten + SHIFT – hörbar richtig? Zwei Steps gleichzeitig halten + TAP/SHIFT ändert beide. Nur halten zeigt beide Werte im Wechsel. In der Step-Reihe flackern solche Steps einmal pro Sekunde: 1× = Odds, 2× = Ratchet, 3× = beides.
- [ ] B6 SHIFT + CLEAR kopiert Variation A nach B. PRE-SCALE-LEDs zählen beim Abspielen 1–2–3–4.
- [ ] B7 Änderungen sind nach Pattern-Wechsel und nach Aus/Ein gespeichert.

## C. Wechsel und Patterns
- [ ] C1 MODE mehrfach während des Spielens: Pattern und Takt ändern sich **nicht**.
- [ ] C2 BASIC-VAR klicken, Step 1–12: Pattern wechselt am Pattern-Ende.
- [ ] C3 BASIC-VAR halten + Step: Bank wechselt.
- [ ] C4 SHIFT + TAP (Reload) bringt das Pattern auf den Stand beim Auswählen.

## D. PERFORM
- [ ] D1 Instrument 2–12 muten sofort, Instrument 1 hebt alles auf, CLEAR + Instrument = Solo.
- [ ] D2 I/F-VAR klicken → Auswahl, Instrument wählen; SHIFT klicken → Steps setzen funktioniert.
- [ ] D3 Step-Loop: ein Step gehalten wiederholt 16tel, mehrere laufen nacheinander, Loslassen läuft nahtlos weiter.
- [ ] D4 TAP spielt ein Fill (Fill-Pattern 13–16 vorher in EDIT gewählt/programmiert).
- [ ] D5 Pattern-Kette: Pattern halten + zweites drücken.
- [ ] D6 Mute-Szenen: SHIFT + Step 13–16 speichern, Step 13–16 abrufen (auch nach Aus/Ein).
- [ ] D7 Kopieren (SHIFT + Quelle, SHIFT + Ziel) und Löschen (CLEAR + Pattern 2×).
- [ ] D8 Toms (I/F-VAR + 4/5/6), Congas (SHIFT + 4/5/6).
- [ ] D9 Aufnahme (I/F-VAR + TAP): Einspielen landet auf dem richtigen 16tel, kein Doppelschlag.
- [ ] D10 Abspielrichtung: I/F-VAR + Step 13–16.

## E. Sonstiges
- [ ] E1 TRIGGER OUT 1 = Low Tom, OUT 2 = Low Conga, gemutet = stumm.
- [ ] E2 MIDI-Noten von außen lösen Sounds aus (Sequencer gestoppt), Program Change wechselt Pattern.
- [ ] E3 Eine Stunde laufen lassen mit Pattern-Wechseln: keine Aussetzer, kein Neustart.
