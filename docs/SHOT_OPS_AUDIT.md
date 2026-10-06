# Audit delle operazioni sugli shot — Animatic, Board, Monitor, modello

**Fase 1 del report `BUG_ANIMATIC_ADDSHOT_NAVIGATOR_CRASH` (v2).** Solo analisi: nessuna modifica al
codice. Righe riferite al ramo `fix/navigator-stale-shot-index` del 2026-10-05 (master + `afc2b8365`).
Serve da base per la discussione della fase 2.

Legenda della classificazione:
- **INTENZIONALE** — comportamento proprio del pannello, da conservare (anche dopo la fase 2, come
  reazione del pannello e non come logica dell'operazione);
- **DERIVA** — le due versioni dovrebbero fare la stessa cosa e non la fanno: bug in attesa;
- **DA VERIFICARE** — sospetto letto nel codice, non provato.

---

## 1. Tabella 3.1 completata

| Operazione | Animatic (`ztoryanimatic.cpp`) | Board (`storyboardpanel.cpp`) | Altri | Modello (`ztorymodel.cpp`) |
|---|---|---|---|---|
| Add | `onAddShot` 7285 | `onAddShot` 6044, `newShotAfterCurrent` 10320 | — | `addShot` 2790 (**nessun chiamante**), `addShotNamed` 2809 (startuppopup 1426, 1433), `addShotFromRasters` 2853 (ztorythumbnailpanel 909) |
| Delete | `onDeleteShots` 6574 | `onDeleteShot` 5951 | **`ZtoryMonitorPanel::doDeleteShots`** (ztorymonitorpanel.cpp 617) | `removeShot` 3005 (**nessun chiamante**) |
| Copy | `onCopyShots` 6461 | `onCopyShot` 5491 | — | `setSharedClip` |
| Cut | `onCutShots` 6487 | `onCutShot` 5517 (→ `onDeleteShot`) | — | `setSharedClip` |
| Paste | `onPasteShots` 6539 | `onPasteShot` 5589 | — | — (entrambi usano `ZtoryShotOps::pasteSharedClip`) |
| Clone | `onCloneShots` 6611 | `onCloneShot` 5560 | — | `setSharedClip` |
| Merge | `onMergeShots` 7165, `onMergeWithNext` 7341 | `onMergeShots` 4722 | — | — |
| Split | `onRazorRequested` 7442 | **non esiste** | — | — |
| Move / riordino | **non esiste** (`onShotMoved` 6875 è vuota: il segnale nasce dal ripple trim, non da un riordino) | `onMoveShot` 6463 | — | `moveShot` 3015 (**nessun chiamante**) |
| Undo di tutte | `UndoBoardState` solo **se c'è un Board** | `UndoBoardState` | Monitor: idem Animatic | — |
| Riallineamento | — | `onModelResequenced` 4500 → `onShotInserted` 4985 / rimozione incrementale / `reconcileShotsWithScene` / `refreshFromScene` | — | `syncShotPanels` 2761 (allunga `m_shots`), `setShotsFrom` (solo da `saveZtoryc`) |

Correzioni al report:
- **`addShotNamed` e `addShotFromRasters` scrivono anche lo xsheet** (`insertColumn` 2832 e 2987),
  non solo `m_shots`. Sono due implementazioni complete di Add in più, con un loro comportamento:
  colonna **in coda** a tutte (`xsh->getColumnCount()`), poi `resequenceXsheet()` **e** un
  `emit modelReset()` esplicito — due reset di fila.
- Esiste una **quarta implementazione di Delete**, nel Monitor.
- `addShot`, `removeShot`, `moveShot` del modello non hanno chiamanti in tutto il repo:
  codice morto, che in più emette `shotAdded`/`shotRemoved`/`shotMoved` (la regola di AGENTS.md
  vieta proprio quei segnali dopo un resequence).

---

## 2. Il meccanismo che rende il modello stantio (confermato in riproduzione)

Il modello si allinea al Board in due soli punti:
1. `syncShotPanels(i, …)` — chiamato da `refreshFromScene()` per ogni shot; **allunga** `m_shots`.
2. `setShotsFrom(...)` — dentro `saveZtoryc()`, **solo se** la scena è collegata al tracker
   (`!m_suppressProjectPublication`, storyboardpanel.cpp 3354) e superate le guardie di
   `saveZtoryc` (`m_savingZtoryc`, scena-shot, scena-personaggio, percorso `.ztoryc` vuoto).

Dopo un Add dell'Animatic il Board prende quasi sempre il **percorso incrementale**
(`onModelResequenced` → `onShotInserted`), che **non chiama nessuno dei due**: legge dal modello
(4954, 5041) ma non ci scrive. Risultato:

| scena | Board | modello dopo il «+» dell'Animatic |
|---|---|---|
| collegata al tracker | presente | in pari (via `setShotsFrom` nel salvataggio) — prova A/A′ |
| **scollegata** | presente | **indietro di uno** — prova B, qWarning `stale` |
| qualsiasi | assente | indietro (nessuno lo riallinea) — non provato |
| non ancora salvata (`.ztoryc` vuoto) | presente | indietro (`saveZtoryc` esce subito) — non provato |

---

## 3. Confronto operazione per operazione

### 3.1 Add

| Aspetto | Animatic `onAddShot` | Board `onAddShot` | Classe |
|---|---|---|---|
| Dentro una sotto-scena | delega a `board->newShotAfterCurrent()` (che chiude, aggiunge, **rientra**); senza Board non fa niente | chiude tutte le sotto-scene e aggiunge; `newShotAfterCurrent` rientra nello shot nuovo | INTENZIONALE (rientrare per continuare a disegnare); **DERIVA**: l'Animatic dipende dal Board per un caso che sa fare da sé |
| Punto d'inserimento | dopo la colonna più a destra della selezione (traccia, poi selezione condivisa); senza selezione **in coda a tutte le colonne**, audio compreso | dopo lo shot selezionato **usando l'indice del Board come indice di colonna** (`m_selectedShotIndex + 1`); senza selezione `m_shots.size()` | **DERIVA**. Il Board confonde indice di shot e indice di colonna: con colonne audio intercalate inserisce nel posto sbagliato (stessa famiglia di `996f4ea64` e del bug di `onMoveShot` corretto il 18/09). I due «in coda» finiscono in posti diversi se ci sono colonne audio |
| Durata | 24 fissa | 24 fissa | uguale |
| Camera della sotto-scena | `syncChildCameraToMain` | `syncChildCameraToMain` | uguale |
| Uuid | nessuno; lo riceve dopo, se e quando il Board salva (`ensureShotUuids` in `saveZtoryc`) | subito (`makeSourcedUuid`), prima del primo rendering, per la miniatura del tracker | **DERIVA** (miniatura del tracker vuota per gli shot creati dall'Animatic finché non si salva) |
| Numerazione | nessuna diretta: `onShotInserted` del Board → `renumberAll()`; niente `assignKeepNumbers` | `updateNumberingLock()` + `assignKeepNumbers(insertAt)` + `renumberAll()` | **DERIVA**: con la numerazione «Keep» (shot già su Kitsu) l'Add dell'Animatic può rinumerare shot esistenti, cioè proprio quello che il lock serve a impedire |
| Selezione dopo | nessuna | `selectShot(insertAt)` | INTENZIONALE (reazione del pannello) |
| Undo | `UndoBoardState` solo se c'è un Board | `UndoBoardState` | **DERIVA**: senza Board l'Add dell'Animatic non è annullabile |
| Segnali | `notifyXsheetChanged` + `resequenceXsheet` (→ `modelReset`) | idem | uguale |
| `.ztoryc` | nessun salvataggio proprio (lo fa il Board reagendo) | `saveZtoryc()` | **DERIVA** (senza Board non si salva) |
| Modello / DB / Kitsu | modello aggiornato solo indirettamente (§2); pubblicazione nel DB solo via `saveZtoryc` del Board; Kitsu mai (sync manuale) | modello via `setShotsFrom` (se collegata); pubblicazione sì; Kitsu mai | **DERIVA** (è la causa del crash) |

`addShotNamed` (avvio di progetto) e `addShotFromRasters` (Send to Board): stessa struttura, colonna
**in coda a tutte**, nessun uuid, nessun undo, due reset. Sono percorsi di creazione e non di
editing, quindi l'undo mancante è ragionevole (INTENZIONALE); il doppio reset è DERIVA minore.

### 3.2 Delete

| Aspetto | Animatic | Board | Monitor | Classe |
|---|---|---|---|---|
| Dentro una sotto-scena | chiude e cancella | **non chiude**, e cancella con `ColumnCmd::deleteColumns` sulla **xsheet corrente** | si rifiuta (`assertMainXsheet(false)`, senza avviso) | **DERIVA — CONFERMATA (§7.2), perde lavoro**: dentro uno shot il Delete del Board cancella colonne della sotto-scena e toglie dal cast il livello del disegno; l'undo non lo restituisce |
| Colonne da cancellare | dalla selezione (colonne) | da `m_shots[idx].data.xsheetColumn` | dalla selezione | uguale nella sostanza |
| Livelli rimasti senza uso | **restano nel cast** | tolti dal cast (tenuti vivi dall'undo) | restano | **DERIVA**: è la famiglia dei «livelli orfani» (`2bdb3d19e`) che bloccava l'export-to-board |
| `xsheetChanged` durante l'op | collegato | scollegato e ricollegato | collegato | INTENZIONALE (ottimizzazione del Board) |
| Selezione | traccia | azzerata | traccia | INTENZIONALE |
| `.ztoryc` | via reazione del Board | `saveZtoryc()` | via reazione | DERIVA (come Add) |
| Undo | se c'è un Board | sì, con i livelli rimossi | se c'è un Board | DERIVA |

### 3.3 Copy / Clone

| Aspetto | Animatic | Board | Classe |
|---|---|---|---|
| Dentro una sotto-scena | **non** esce (legge la top xsheet: «non-destructive») | **esce** dalla sotto-scena | **DERIVA**: una copia non dovrebbe far uscire l'animatore dallo shot; il commento dell'Animatic lo dice esplicitamente |
| Durata nella clip | `ZtoryShotOps::colDuration(xsh, col)` (lunghezza vera della colonna) | `panels[0].duration` — **la durata del primo pannello** | **DERIVA**: con uno shot a più pannelli la clip del Board ha la durata sbagliata |
| Ordine | per colonna | per indice del Board | uguale se gli indici coincidono |

### 3.4 Cut

| Aspetto | Animatic | Board | Classe |
|---|---|---|---|
| Durata | `colDuration` | `panels[0].duration` | **DERIVA** (come Copy) |
| Cancellazione | propria, `deleteColumns` | **riusa `onDeleteShot`** | INTENZIONALE (riuso) ma con un effetto da verificare ↓ |
| Livello tagliato | `cutLevel` nella clip, livello lasciato nel cast | `cutLevel` nella clip, ma `onDeleteShot` **toglie il livello dal cast** | **CONFERMATO (§7.3), perde lavoro**: il Paste rimette il `cutLevel` nelle celle (`pasteSharedClip`, ztoryshotops 266-270) ma niente lo riaggiunge al cast. Una sotto-scena esposta da celle ma assente dal cast rischia di non essere salvata con la scena |
| Undo | «Cut Shot» se c'è un Board | l'undo registrato è quello di `onDeleteShot`: la cronologia dice **«Delete Shot»** | DERIVA minore |

### 3.5 Paste

| Aspetto | Animatic | Board | Classe |
|---|---|---|---|
| Operazione sullo xsheet | `ZtoryShotOps::pasteSharedClip` | `ZtoryShotOps::pasteSharedClip` | uguale — è **già** l'operazione unica |
| Punto d'inserimento | solo la selezione della **traccia** (ignora la selezione condivisa, a differenza di tutte le altre op dell'Animatic); senza selezione in coda | `xsheetColumn + 1` dello shot selezionato | **DERIVA** |
| Clip dopo il paste | rimuove le voci cut/clone, tiene le copie | idem | uguale |
| Allineamento | `resequenceXsheet` + `refreshFromScene` della traccia | `resequenceXsheet` + `refreshFromScene` del Board solo se non combacia | INTENZIONALE |
| Uuid dei cloni | assegnati dal Board al salvataggio | idem | uguale (ma vedi §4, punto 3) |

### 3.6 Merge

| Aspetto | Animatic `onMergeShots` | Board `onMergeShots` | Classe |
|---|---|---|---|
| Dentro una sotto-scena | chiude e procede | **si rifiuta con un avviso** | DERIVA (stesso comando, due risposte) |
| Lunghezze | `getRange(…, ignoreLastStop=true)`: esclude il fermo-immagine finale (SFH) messo dal resequence | `getRange(r0, r1)` **senza** `ignoreLastStop` | **DERIVA (probabile bug)**: ogni shot conta un fotogramma in più e la cella SFH finisce dentro lo shot unito; l'Animatic era stato corretto, il Board no |
| Contenuto | `materializeCells` + `trimChildXsheetTo` + `mergeChildXsheetContent` | identico | uguale (codice duplicato riga per riga) |
| Segnali dopo il resequence | nessuno | **`emit shotRemovedAt(...)`** con `m_updating=true` | **DERIVA**: viola la regola di AGENTS.md. `m_updating` protegge solo il Board che emette; **le altre istanze del Board** (Board room, Shot board, flottante) ricevono `shotRemovedAt` **dopo** essersi già riallineate con `onModelResequenced`, e tolgono uno shot di troppo |
| Selezione | traccia | azzerata | INTENZIONALE |

`onMergeWithNext` (solo Animatic): come `onMergeShots`, ma rifiuta dentro una sotto-scena. DERIVA
interna all'Animatic.

### 3.7 Split (rasoio)

Esiste solo nell'Animatic (`onRazorRequested`, più il ramo audio-only). Non c'è un confronto da fare;
per la fase 2 i punti da portare nel modulo unico sono: `cloneChild` + `popUndo(1)`, chiavi di
confine (`addRazorKeyframes`), taglio dell'audio collegato (`splitAudioColumn`), ripristino dello
scroll. Rifiuta dentro una sotto-scena, coerente con Merge-with-next.

### 3.8 Move / riordino

Esiste solo nel Board (`onMoveShot`). Dopo la correzione del 18/09 legge le colonne dalla scena
(`ztoryShotColumns`) e si ferma se il conteggio non torna: è l'implementazione più prudente del
gruppo, e il modello da seguire. Rifiuta dentro una sotto-scena con avviso. Riscrive le **celle**
invece di spostare le colonne: per questo il rilevamento del riordino in `onModelResequenced` si
basa sul nome della colonna (4648-4660).

---

## 4. Derive trasversali (valgono per più operazioni)

1. **Undo dipendente dal Board.** Animatic e Monitor registrano l'undo solo se trovano un Board
   (`findBoardPanel()`): senza Board nessuna operazione strutturale è annullabile.
2. **Salvataggio e pubblicazione dipendenti dal Board.** Lo stesso: il `.ztoryc` e il DB di
   progetto si aggiornano solo se un Board reagisce.
3. **`ensureShotUuids` scrive gli uuid nel modello per indice** (2849: `if (i < n) m->shot(i).uuid = bu`
   con `n = min(board, modello)`). Se il modello è stantio e lo shot nuovo sta in mezzo, gli uuid del
   modello scivolano di un posto rispetto al contenuto. **CONFERMATO (§7.1)**: dopo un Add
   dall'Animatic tecnica e task finiscono sullo shot sbagliato.
4. **Indice di shot usato come indice di colonna** nel Board (Add 6058, `onShotInserted` che lo dà
   per vero e lo controlla solo nel percorso incrementale). Con colonne audio intercalate è
   sbagliato.
5. **Ingresso in sotto-scena incoerente**: chiudere (Animatic: Delete/Cut/Paste/Merge; Board:
   Add/Copy/Cut/Clone/Paste), rifiutare con avviso (Board: Merge, Move; Animatic: Merge-with-next,
   Razor), rifiutare in silenzio (Monitor: Delete), non accorgersene (Board: Delete).
6. **Più istanze del Board** si riallineano ognuna per conto suo (nel log: tre `onModelResequenced`
   per ogni operazione, tre `saveZtoryc`). Funziona finché nessuno emette segnali incrementali dopo
   il resequence (§3.6).

---

## 5. Cosa non è deriva

- La durata predefinita (24), la sincronizzazione della camera e l'operazione di Paste sullo xsheet
  (`pasteSharedClip`) sono già condivise.
- La clip condivisa (`m_sharedClip`) e la selezione condivisa (`m_sharedSelection`) funzionano come
  descritto in AGENTS.md.
- `onModelResequenced`, con i suoi tre livelli (incrementale, riconciliazione per identità,
  ricostruzione completa), è robusto **per il Board**: il difetto è solo che non aggiorna il modello.

---

## 6. Domande per Franco prima della fase 2

1. **Proprietà dei metadati dei pannelli** (dialoghi, azioni, note, luci, durate parziali): restano
   del Board o passano al modello? Oggi esistono in due copie (`m_shots[i].data` del Board e
   `ZtoryModel::m_shots`), e il Navigator scrive direttamente in quella del modello.
2. **Undo**: un undo unico basato sullo xsheet (riconciliazione al resequence) o lo snapshot del
   Board? Con lo snapshot, Animatic e Monitor senza Board restano senza undo.
3. **Ingresso in sotto-scena**: una regola sola per tutte le operazioni strutturali? La più usata è
   «chiudi e procedi»; Copy/Clone dovrebbero invece non uscire.
4. **Codice morto del modello** (`addShot`, `removeShot`, `moveShot`): rimuoverlo subito (nessun
   chiamante) o tenerlo come scheletro della fase 2?
5. Le tre derive marcate **DA VERIFICARE** (Cut e cast, Delete del Board dentro una sotto-scena, uuid
   del modello per indice) vanno provate prima della fase 2: se sono vere, sono bug da correggere
   anche nella 0.16 indipendentemente dal refactor.

---

## 7. Verifica dei tre sospetti (2026-10-05, Mac, progetto di prova `navcrash_test`)

Scena `navtest1`, tre shot, **non collegata al tracker** (`productionTracker="off"`), build del
ramo `fix/navigator-stale-shot-index` (cioè con la guardia del Navigator).

### 7.1 Uuid del modello per indice — CONFERMATO

Tecniche impostate a mano: `sub`=Generic, `sub_1`=Live, `sub_2`=Traditional. Selezionato sh010
nell'Animatic, premuto «+». Il Board passa per il percorso incrementale (nel log, tre volte:
`onModelResequenced: shot inserted at 1 -> incremental`). Il `.ztoryc` salvato:

| indice | livello | tecnica |
|---|---|---|
| 0 | `sub` | Generic |
| 1 | `sub_3` (nuovo) | — |
| 2 | `sub_1` | **Traditional** (era Live) |
| 3 | `sub_2` | Traditional |

Catena: il modello resta con 3 shot mentre il Board ne ha 4; in `saveZtoryc`
`pushTrackingToBoard` (2853) riporta i dati del modello sul Board per uuid, poi `ensureShotUuids`
(2849) riscrive gli uuid nel modello **per posizione**. Lo shot nuovo in mezzo fa scivolare di uno
gli uuid, e il giro successivo copia tecnica e task dello shot sbagliato. Su una scena collegata
al tracker finisce anche nel DB di progetto.

### 7.2 Delete del Board dentro una sotto-scena — CONFERMATO, perde lavoro

Entrato in sh010 (doppio clic sul pannello del Board), disegnato un tratto nella colonna `A8`
della sotto-scena, mostrato il Board con il pulsante BOARD | XSHEET **senza uscire**, selezionato
sh010, premuto Delete.

- Lo shot nel main resta (l'Animatic mostra ancora 4 shot).
- Sparisce la colonna `A8` **dentro la sotto-scena**: `onDeleteShot` (5951) non chiude la
  sotto-scena e passa a `ColumnCmd::deleteColumns` l'indice di colonna del main
  (`data.xsheetColumn` = 0) sulla xsheet **corrente**.
- La pulizia del cast raccoglie i livelli interni dello shot (prima della cancellazione) e
  toglie quelli che `top->isLevelUsed()` non trova più. `isLevelUsed` entra nelle sotto-scene
  (`TXsheet::getUsedLevels` è ricorsiva), quindi la pulizia in sé è giusta: il livello del
  disegno viene tolto perché la sua unica colonna è appena stata cancellata per errore.
- **L'undo non recupera il disegno.** Il primo ⌘Z ha annullato una voce registrata dopo il
  Delete; tornando dalla History a prima del Delete, la colonna `A8` ricompare con la cella ma
  **senza il disegno**, e l'undo aggiunge una voce «Close Sub-Scene» che tronca il redo.

Il caso non è raro: nella room Ztoryc X il Board e lo xsheet condividono lo stesso pannello, e si
passa dall'uno all'altro con un clic restando dentro lo shot. Nota a margine: un clic singolo
sulla miniatura dentro una sotto-scena mostra l'avviso «solo al livello principale» di
`onMoveShot` (6464), cioè il clic viene preso per l'inizio di un trascinamento.


### 7.3 Cut → Paste e cast — CONFERMATO, perde lavoro

Prova del 2026-10-06 sulla stessa scena (tre shot, livello raster `AB` disegnato dentro sh020).

1. **Dopo Cut e Paste dal Board** (selezione del pannello, ⌘X, selezione di sh010, ⌘V) e un
   salvataggio, il `.tnz` ha nella cartella Cast solo `sub` e `sub_2`: **né la sotto-scena di sh020
   né `AB` stanno più nel `levelSet`**. Il contenuto non è perso, perché il salvataggio scrive la
   sotto-scena per intero dentro la colonna che la espone.
2. **Ricaricando la scena** (Revert Scene) il caricamento rimette in cast tutti i livelli che
   incontra; risalvando, la cartella Cast torna completa. Da solo, quindi, il difetto si ripara
   alla prima riapertura.
3. **Il caso che perde lavoro: si disegna nello shot incollato prima di riaprire la scena.**
   Cut e Paste di sh020, ingresso nello shot, secondo tratto sul disegno, ⌘S (Save All). Il `.tnz`
   viene riscritto, ma `AB.0001.png` resta identico a prima (stessa data, stesso MD5): **Save All
   salva solo i livelli che trova nel `levelSet`** (`IoCmd::saveAll` → `SceneResources::getResources`,
   toonzlib/sceneresources.cpp:491, che scorre `scene->getLevelSet()`), e `AB` non c'è più. Nessun avviso. Alla
   riapertura sh020 mostra solo il primo tratto.

Catena: `onCutShot` → `onDeleteShot` (5951) toglie dal cast la sotto-scena e i livelli interni
rimasti senza uso, che è giusto per un Delete; poi `pasteSharedClip` (ztoryshotops 266-270) rimette
`ce.cutLevel` nelle celle **senza riaggiungere al `levelSet` né lui né i livelli interni**.
Correzione minima per la 0.16 (da decidere): al Paste di un taglio, riaggiungere al `levelSet` il
`cutLevel` e i livelli che usa (`getUsedLevels` sulla sua xsheet). In alternativa, il Cut non
deve togliere nulla dal cast: lo tolgono la chiusura della scena o un Delete vero.

Nota sulla prova: Ztoryc si è aperto col progetto **sandbox** anche con `CurrentProject` sul
progetto di prova (titolo `navtest1 [sandbox]`), quindi `+extras` puntava a
`Ztoryc.app/ztorycstuff/sandbox/extras/`, dove è finito `AB.0001.png`. Non cambia il risultato.

### 7.5 Testo scritto nel Navigator — CONFERMATO, perde lavoro

Prova del 2026-10-06, room Ztoryc T, pannello «Shot Board» (`ZtoryPanelNavigator`) su sh010.
Scritta una battuta nel campo Dialog, uscito dal campo, ⌘S: il `.tnz` viene salvato e il titolo
perde l'asterisco, ma il `.ztoryc` resta quello di prima. Chiudendo Ztoryc non compare nessun
avviso; la battuta non è su disco, né nel `.ztoryc` né nel `.tnz`.

Catena: il Navigator scrive nel modello (`ztoryanimatic.cpp:4436`) ed emette `shotDataChanged`;
ogni Board copia il testo nel proprio `m_shots` **per indice** (`storyboardpanel.cpp:1832`) ma salva
il `.ztoryc` solo se è cambiata la luce (lo dice il commento a 1858: «text fields are saved by
their own Board flows; the navigator has no other path to the .ztoryc»). Il ⌘S della scena non
scrive il `.ztoryc` (`iocommand.cpp` non lo chiama mai): lo scrivono solo gli eventi dei Board (39
punti in `storyboardpanel.cpp`). La battuta sopravvive solo se un Board salva per un altro motivo
prima della chiusura.

Non provato: con il modello indietro di uno shot (§7.1) lo stesso gestore copierebbe il testo
sullo shot sbagliato del Board.

### 7.6 Cut → Paste dal Board perde i testi dello shot — CONFERMATO

Prova del 2026-10-06: sh010 con una battuta nel primo pannello; Cut dal Board, Paste dopo sh020.
Lo shot incollato ha i pannelli vuoti e il `.ztoryc` salvato subito dopo non contiene più la
battuta. Lo shot incollato prende anche l'uuid di un altro shot: nel file tre shot finiscono
con lo stesso uuid.

Catena:
- `onCutShot` dice «save metadata», ma la clip (`ZtoryClipEntry`, `ztorymodel.h:45`) porta solo
  colonna, durata e sotto-scena: **nessun testo, uuid, tecnica, task, luce**. Poi `onDeleteShot`
  toglie lo shot da `m_shots` e salva il `.ztoryc` senza di lui.
- `onPasteShot` reinserisce la sotto-scena e lascia che il Board crei uno shot nuovo, vuoto.
- L'uuid doppio viene dal ripiego per posizione di `pushTrackingToBoard`
  (`else if (sameLength) mi = i;`): lo shot incollato non ha uuid, le due liste hanno la stessa
  lunghezza, e riceve l'uuid (con tecnica e task) dello shot che il modello ha a quella
  posizione. Ragionato su un caso pulito di tre shot: succede sia con il codice di prima sia
  con la correzione `43e40b7c5`, che cambia solo quale shot viene doppiato.

È del codice rilasciato (0.16.1), non delle correzioni di questa sessione.

**Correzione (per la 0.16.2)**: la clip porta i dati dello shot (`ZtoryClipEntry::shot`); il Board
che costruisce la colonna incollata li riprende (`StoryboardPanel::adoptCutShot`) nei tre punti
dove nasce uno shot (inserimento incrementale, riconciliazione, ricostruzione completa); il
Paste del Board toglie il taglio dalla clip solo dopo il riallineamento; `pushTrackingToBoard`
non ripiega più sulla posizione. Cut + Paste vale come uno spostamento (decisione di Franco,
2026-10-06): lo shot torna con etichetta, ordine e sequenza, e poi `renumberAll` decide come
dopo `onMoveShot` — in Auto rinumera per posizione, in Keep gli shot conservano il numero (e
con lui l'aggancio a Kitsu). Se l'originale è ancora nel Board (Cut, ⌘Z, ⌘V), o il numero è
stato preso nel frattempo, lo shot incollato riceve uuid ed etichetta nuovi. Anche la durata: il Cut del Board usava il primo pannello, quello dell'Animatic contava
il fotogramma di chiusura; ora entrambi `shotTrueSpan`.

### 7.7 Testi slittati fra Board dopo un Paste — CONFERMATO, corretto per la 0.16.2

Prova del 2026-10-06: Cut, ⌘Z, ⌘V (due colonne della stessa sotto-scena). Nel `.ztoryc` salvato
gli shot dopo il punto d'inserimento avevano il dialogo di quello prima.

Catena: un Board ricostruisce in blocco («scene has 5 shot columns, panel has 4 → full rebuild»)
e riscrive il modello shot per shot (`syncShotPanels` → `shotDataChanged`). Gli altri due Board,
ancora con la lista vecchia, ricevono l'avviso e copiano i testi **per indice** (gestore di
`shotDataChanged`, `storyboardpanel.cpp`); poi inseriscono il loro shot e salvano per ultimi.
Indice e colonna scivolano insieme, quindi un confronto su lunghezza e colonna non basta (provato).

Correzione: il Board copia il testo dal modello solo se la colonna a cui punta la voce del modello
espone **ora** la stessa sotto-scena del suo shot (`Shot::childLevel`). Provato: dopo Cut, ⌘Z, ⌘V
tutti i testi sono al loro posto; la copia dal Navigator ai Board funziona ancora.

### 7.4 Riepilogo

| sospetto | esito | effetto |
|---|---|---|
| uuid del modello per indice (`ensureShotUuids`) | confermato | tecnica e task sullo shot sbagliato dopo un Add dall'Animatic |
| Delete del Board dentro una sotto-scena | confermato | colonna del disegno cancellata, undo che non lo restituisce |
| Cut → Paste e cast | confermato | disegno fatto nello shot incollato non salvato fino alla riapertura |
| testo scritto nel Navigator | confermato, corretto nel passo 0 di `feature/shot-document` | battuta persa: il ⌘S non scriveva il `.ztoryc` |
| Cut → Paste (Board e Animatic) | confermato, corretto per la 0.16.2 | testi dello shot persi, uuid doppio, durata sbagliata |
| testi slittati fra Board dopo un Paste | confermato, corretto per la 0.16.2 | dialoghi sullo shot sbagliato |

Tutti e tre si possono correggere in modo locale, senza aspettare la fase 2.
