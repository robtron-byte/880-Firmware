# Firmware aufspielen (per MIDI, ohne Programmer)

Die 880 hat einen eingebauten Update-Modus (Bootloader). Er wird beim Update **nicht** überschrieben, du kommst also immer wieder zurück.

## Was du brauchst
- Ein **USB-MIDI-Interface mit 5-poligem MIDI-OUT** (class compliant) und ein MIDI-Kabel
- Einen Computer mit **Chrome oder Edge**. Alternativ SysEx Librarian (Mac) oder MIDI-OX (Windows).
- Die Firmware-Datei:
  - **880 Perform:** [`firmware/880-perform.syx`](../firmware/880-perform.syx)
  - **Original zum Zurückspielen:** [`firmware/official/880-1.1.4-MK2.syx`](../firmware/official/880-1.1.4-MK2.syx)

Deine Patterns liegen auf einem eigenen Speicherchip und bleiben beim Update erhalten.

## Schritt für Schritt (Web-Updater)
1. 880 ausschalten. MIDI-Kabel vom **MIDI-OUT des Interfaces** in die **MIDI-Buchse der 880** stecken.
2. **START/STOP gedrückt halten** und die 880 einschalten. Halten, bis die **SYNC-LED rot** leuchtet.
3. Im Browser https://system80.net/firmware/ öffnen und MIDI erlauben.
4. Den MIDI-Ausgang deines Interfaces wählen.
5. Bei „Firmware“ **Load .SYX File** wählen und `880-perform.syx` laden.
6. Senden. Die SYNC-LED blinkt **gelb**, solange Daten ankommen. Nicht ausschalten und nicht das Kabel ziehen.
7. Danach startet die 880 neu. Zur Kontrolle: SHIFT + MODE, dann SHIFT + TAP halten zeigt die Version **1.1.6**.

Wenn das Interface Daten verschluckt, erhöhst du im Web-Updater den **Delay**, z. B. auf 250 ms.

## Wenn etwas schiefgeht
- **SYNC-LED bleibt rot:** Es kommt nichts an. Prüfe Kabel und MIDI-Ausgang und sende erneut. Ausschalten ohne START/STOP startet die alte Firmware.
- **Abgebrochen, während sie gelb blinkte:** Die 880 startet nicht mehr normal. Das ist nicht schlimm: Mit START/STOP beim Einschalten kommst du wieder in den Update-Modus, dann sendest du die Datei erneut.
- **Zurück zum Original:** Spiele genauso `firmware/official/880-1.1.4-MK2.syx` auf.

## Selbst bauen (für Entwickler)
```
sudo apt install gcc-avr avr-libc binutils-avr
make dfp      # einmalig: Microchip-Gerätepaket für den ATmega328PB
make          # -> build/880-firmware.hex und build/880-firmware.syx
make test     # Unit-Tests und Selbsttest des Sysex-Konverters
```
`make` bricht ab, wenn die Firmware größer als 28.672 Byte wird (Platz des Bootloaders) oder weniger als 500 Byte RAM für den Stack frei bleiben.
