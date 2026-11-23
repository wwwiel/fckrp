# fckrp

Eine komplette Vocal-Kette in einem Plugin, für FL Studio und andere DAWs (VST3 für Windows und Mac, zusätzlich AU auf dem Mac).

![fckrp Oberfläche](docs/fckrp-ui.png)

## Was drin ist

Das Signal läuft in dieser Reihenfolge durch die Module. Jedes Modul lässt sich mit dem Punkt oben rechts an- und ausschalten.

| Modul | Was es macht |
|---|---|
| **Gate** | Senkt Rauschen und Atmer zwischen den Phrasen sanft ab, statt hart abzuschneiden. |
| **EQ** | Low Cut gegen Rumpeln, „Mud“ bei 300 Hz, „Presence“ bei 3,5 kHz für Verständlichkeit, „Air“ ab 10 kHz für Glanz. |
| **De-Ess** | Macht scharfe S- und Zischlaute leiser, nur im Höhenbereich. |
| **Opto** | Eigener optischer Röhrenkompressor: sehr weiche Kennlinie, zweistufiges Release (schneller Anteil plus langsamer Ausklang) und „Gedächtnis“, das nach langer Kompression langsamer loslässt. Regler: Threshold, Ratio, Attack, Release, Gain. |
| **Tube** | Warme, leicht asymmetrische Röhrensättigung mit 2-fach-Oversampling. |
| **Delay** | Echo, das zum trockenen Signal dazugemischt wird. Die Wiederholungen sind gefiltert, damit sie hinter der Stimme bleiben. |
| **Hall** | Raum, der dazugemischt wird, mit Low Cut am Halleingang gegen Matsch. |

**Makro-Regler:**
- **Polish** macht die Stimme mit einem Dreh „fertiger“: mehr Kompression, mehr Presence und Air, etwas mehr De-Essing und Wärme.
- **Space** gibt mehr Delay und Hall dazu.

**Presets:** Init, Clean Pop, Rap Upfront, Airy R&B, Lo-Fi Warm, Big Hook, Natural Leveler.

## In FL Studio installieren

1. Auf GitHub unter **Actions** den neuesten erfolgreichen Lauf von „Build“ öffnen und unten bei *Artifacts* `fckrp-Windows` oder `fckrp-macOS` herunterladen und entpacken.
2. `fckrp.vst3` kopieren nach
   - **Windows:** `C:\Program Files\Common Files\VST3\`
   - **Mac:** `/Library/Audio/Plug-Ins/VST3/` (die AU-Version `fckrp.component` nach `/Library/Audio/Plug-Ins/Components/`)
3. **Nur Mac:** Das Plugin ist noch nicht bei Apple beglaubigt. Einmal im Terminal ausführen:
   `xattr -cr /Library/Audio/Plug-Ins/VST3/fckrp.vst3`
4. In FL Studio: *Options → Manage plugins → Find installed plugins*. Danach erscheint fckrp unter den Effekten und kann in einen Mixer-Insert geladen werden.

## Selbst bauen

Voraussetzungen: CMake 3.22+ und ein C++17-Compiler (Visual Studio 2022, Xcode oder GCC/Clang). JUCE wird automatisch heruntergeladen.

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target fckrp_VST3
```

Offline-Tests (prüfen alle Presets bei mehreren Sample-Raten, Kompressor, De-Esser, Ausklang, Mono und Speichern/Laden):

```
cmake -B build -DCMAKE_BUILD_TYPE=Release -DFCKRP_BUILD_TESTS=ON
cmake --build build --target fckrp_tests
./build/fckrp_tests_artefacts/Release/fckrp_tests
```

## Geplant

- Version 2: Echtzeit-Tonhöhenkorrektur (Auto-Tune-Stil)
- Eigene Presets speichern und teilen
- Tempo-Sync für das Delay
- Überarbeitetes Design

## Lizenz-Hinweis

fckrp basiert auf dem JUCE-Framework. JUCE ist entweder unter AGPLv3 (dann muss der Quellcode offen sein) oder mit einer JUCE-Lizenz nutzbar; die kostenlose „Starter“-Lizenz gilt bis zu einem bestimmten Jahresumsatz. Vor einem Verkauf bitte die aktuellen Bedingungen auf juce.com prüfen.
