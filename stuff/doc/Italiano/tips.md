# Benvenuto in Ztoryc!

Ztoryc è uno strumento per storyboard e animatic che vive dentro un programma di animazione: i pannelli che disegni sono già gli shot che animerai. Le novità di ogni versione sono nella [pagina delle release](https://github.com/matitanimata/ztoryc/releases).

### Com'è fatta una scena di Ztoryc

- Lo Xsheet principale ha **una colonna per shot**, e ogni shot è una sotto-scena.
- Room **BOARD**: si disegnano e si ordinano i pannelli dello storyboard.
- Room **ANIMATIC**: si danno i tempi agli shot, si aggiungono il sonoro e le dissolvenze.
- Dentro uno shot si lavora come in qualsiasi scena Toonz: layout, pose, animazione.
- Il sonoro sta nello Xsheet principale (l'animatic), mai nelle sotto-scene.
- **Copy** crea uno shot che condivide la stessa sotto-scena; **Clone** ne crea uno del tutto indipendente.

-----

## Se vieni da OpenToonz o da Tahoma2D

Ztoryc è basato su Tahoma2D, quindi alcune cose sono diverse da OpenToonz:
- Celle e colonne non hanno la barra di trascinamento: si clicca una cella esposta o una colonna non vuota e la si trascina. Per tornare al comportamento di prima: `Preferences` -> `Scene` -> `Show Column and Cell Drag Bars`.
- `Alt` (`⌥ Option` su macOS) seleziona fotogrammi e chiavi di animazione; `Ctrl` (`⌘ Cmd` su macOS) avvia una selezione di più celle o colonne.
- Tenute implicite: un fotogramma esposto resta visibile fino al successivo. `Stop Frame Hold` interrompe la tenuta.
- Alcune funzioni e impostazioni avanzate sono nascoste: si attivano con `Preferences` -> `General` -> `Show Advanced Preferences and Options`, poi si riavvia.

-----

## Consigli

- Scene e disegni stanno in file separati dentro la cartella di un progetto: crea un progetto per ogni produzione invece di usare il `sandbox` predefinito.
- Usa spesso `Save All`, che salva scena e disegni. `Save Scene` salva solo la scena.
- Per i file esterni usa `Import`: gli originali restano intatti.
- Perché più immagini raster diventino un solo livello, chiamale `nomelivello.####.ext` o `nomelivello_####.ext` (per esempio ABC.0001.png, ABC.0002.png).
- L'animatic si esporta come video e come timeline per il montaggio: `.fcpxml` (DaVinci Resolve, Final Cut Pro) e `_premiere.xml` (Adobe Premiere Pro).
- La barra di stato in basso dà altre informazioni sullo strumento in uso.
- Ti serve aiuto o hai trovato un difetto? Entra nel [Discord di Ztoryc](https://discord.gg/ZP2gqQwmDb).
- Ztoryc è libero e open source. Se è utile a te o al tuo studio, puoi [sostenerlo](https://github.com/sponsors/matitanimata).
