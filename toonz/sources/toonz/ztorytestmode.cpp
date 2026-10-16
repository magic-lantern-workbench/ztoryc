// Ztoryc's test modes, armed only by environment variables (none of them is
// set in a user's session): ZTORYC_ROUNDTRIP (the safety net of the shot
// document rework, with its dialog watchdog and ZTORYC_DIALOG_HOLD_MS) and
// ZTORYC_SCENETEST (shot, character and plain scenes; docs/SHOT_DOCUMENT_PLAN.md).
// They used to live in ztorymodel.cpp, which included the mouth and IoCmd
// headers only for them (review 0.17, M10).  Registered with
// Q_COREAPP_STARTUP_FUNCTION, so nothing calls into this file.

#include "ztorymodel.h"
#include "ztorycharacter.h"
#include "ztoryshotops.h"
#include "ztorysceneclass.h"
#include "ztorymouthapply.h"
#include "ztorymouthlibrary.h"
#include "ztorymouthmap.h"
#include "toonzqt/paletteviewergui.h"
#include "toonz/palettecontroller.h"
#include "toonz/txshleveltypes.h"
#include "tpalette.h"
#include "iocommand.h"
#include "tapp.h"
#include "toonz/toonzscene.h"
#include "toonz/txsheet.h"
#include "toonz/txshcolumn.h"
#include "toonz/txshcell.h"
#include "toonz/txshchildlevel.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/childstack.h"
#include "toonz/tscenehandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/tstageobject.h"
#include "toonz/tproject.h"
#include "toonz/levelset.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDialog>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QMessageBox>
#include <QPointer>
#include <QTextStream>
#include <QTimer>
#include <QWidget>

#include <cstdio>  // fprintf
#ifdef _WIN32
#include <process.h>  // _exit
#else
#include <unistd.h>  // _exit
#endif

// Test mode (ZTORYC_ROUNDTRIP, see StoryboardPanel::refreshFromScene): armed as
// soon as the application exists — not by a Board or the model, which a
// workflow with no Board (Cutout, Character) creates late or never, after the
// first modal box has already stopped the load.
static void ztoryArmRoundtripWatchdog() {
  // Copies of real storyboards miss their panel drawings, and Tahoma stops the load with a
  // modal «file missing» box.  Nobody is there to press OK, so close it: only
  // the .ztoryc matters to the round trip.
  if (qEnvironmentVariableIsSet("ZTORYC_ROUNDTRIP")) {
    QTimer *dismiss = new QTimer(qApp);
    dismiss->setInterval(50);
    // ZTORYC_DIALOG_HOLD_MS: leave each box up that long before closing it,
    // as a person reading it would — the windows behind it repaint meanwhile
    // (the palette crash of 2026-10-11 needed exactly that).
    static const int holdMs = qEnvironmentVariableIntValue("ZTORYC_DIALOG_HOLD_MS");
    QObject::connect(dismiss, &QTimer::timeout, qApp, []() {
      static QPointer<QWidget> seen;
      static QElapsedTimer since;
      QWidget *active = QApplication::activeModalWidget();
      if (active && active != seen) { seen = active; since.start(); }
      if (active && holdMs > 0 && since.elapsed() < holdMs) return;
      if (QWidget *w = active) {
        QString text;
        for (QLabel *l : w->findChildren<QLabel *>())
          if (!l->text().isEmpty()) text += l->text().left(160) + " | ";
        fprintf(stderr, "[ZTORY] roundtrip: closing dialog '%s': %s\n",
                qPrintable(w->windowTitle()), qPrintable(text));
        if (QDialog *d = qobject_cast<QDialog *>(w)) d->reject();
        else w->close();
      }
    });
    dismiss->start();
  }
}
Q_COREAPP_STARTUP_FUNCTION(ztoryArmRoundtripWatchdog)

// Test mode (ZTORYC_SCENETEST, docs/SHOT_DOCUMENT_PLAN.md): the shot and
// character scenes the round trip never covers — the path that emptied a real
// tracker on 2026-10-07 (a shot opened in Cutout, no Board). Once the scene
// from the command line is loaded, in whatever workflow the preferences say:
// write the project DB as an edit would, then ⌘S ("save") or Save As
// ("saveas:<abs .tnz>"), and exit. Watching the tracker is the script's job.
static void ztoryArmSceneTest() {
  QString action = qEnvironmentVariable("ZTORYC_SCENETEST");
  if (action.isEmpty()) return;
  QTimer *poll = new QTimer(qApp);
  poll->setInterval(500);
  QObject::connect(poll, &QTimer::timeout, qApp, [poll, action]() {
    TApp *app = TApp::instance();
    if (!app || !app->getCurrentScene()) return;
    ToonzScene *scene = app->getCurrentScene()->getScene();
    if (!scene || scene->isUntitled()) return;
    poll->stop();
    QTimer::singleShot(6000, qApp, [action]() {
      ZtoryModel *m = ZtoryModel::instance();
      fprintf(stderr, "[ZTORY] scenetest: role=%s shots=%d assets=%d\n",
              m->shotDocumentState().isShotScene ? "shot"
              : m->shotDocumentState().isCharacterScene ? "character"
              : m->shotDocumentState().isPlainScene     ? "plain"
              : m->shotDocumentState().unclassified     ? "unclassified"
                                                        : "storyboard",
              (int)m->projectShots().size(), (int)m->assets().size());
      // ZTORYC_SCENETEST_SUBSCENE=<.tnz>: load it as a sub-scene first, as
      // File > Load As Sub-Scene does (a character brought into a shot).
      const QString sub = qEnvironmentVariable("ZTORYC_SCENETEST_SUBSCENE");
      if (!sub.isEmpty()) {
        const bool ok = IoCmd::loadSubScene(TFilePath(sub.toStdWString()));
        fprintf(stderr, "[ZTORY] scenetest: loaded sub-scene %s: %s\n",
                qPrintable(sub), ok ? "ok" : "FAILED");
      }
      {  // the sub-scenes in the cast, by name, and the current xsheet
        ToonzScene *sc = TApp::instance()->getCurrentScene()->getScene();
        QStringList subs;
        TLevelSet *ls = sc->getLevelSet();
        for (int i = 0; i < ls->getLevelCount(); i++)
          if (ls->getLevel(i) && ls->getLevel(i)->getChildLevel())
            subs << QString::fromStdWString(ls->getLevel(i)->getName());
        TXsheet *cur = TApp::instance()->getCurrentXsheet()->getXsheet();
        std::function<void(TXsheet *, int)> tree = [&](TXsheet *x, int d) {
          if (!x || d > 6) return;
          for (int c = 0; c < x->getColumnCount(); c++) {
            int r0 = 0, r1 = -1;
            x->getCellRange(c, r0, r1);
            QSet<TXshLevel *> done;
            for (int r = r0; r <= r1; r++) {
              const TXshCell cell = x->getCell(r, c);
              if (!cell.m_level || done.contains(cell.m_level.getPointer())) continue;
              done.insert(cell.m_level.getPointer());
              if (TXshChildLevel *cl = cell.m_level->getChildLevel()) {
                fprintf(stderr, "[ZTORY] scenetest:   tree %s[col %d] %s\n",
                        qPrintable(QString(d * 2, ' ')), c,
                        qPrintable(QString::fromStdWString(cell.m_level->getName())));
                tree(cl->getXsheet(), d + 1);
              }
            }
          }
        };
        tree(cur, 0);
        if (cur)
          for (int c = 0; c < cur->getColumnCount(); c++) {
            int r0 = 0, r1 = -1;
            cur->getCellRange(c, r0, r1);
            const TXshCell cell = cur->getCell(r0, c);
            fprintf(stderr, "[ZTORY] scenetest:   column %d rows %d-%d: %s\n", c, r0, r1,
                    cell.m_level ? qPrintable(QString::fromStdWString(cell.m_level->getName()))
                                 : "(empty)");
          }
        for (const QString &n : subs) {
          MouthMap lm;
          QString who;
          const bool ok = ZtoryMouthLibrary::libraryMapForSubScene(sc, n, &lm, &who);
          fprintf(stderr, "[ZTORY] scenetest:   library of '%s': %s %s %d sets\n",
                  qPrintable(n), ok ? "yes" : "no", qPrintable(who),
                  (int)lm.sets.size());
        }
        fprintf(stderr, "[ZTORY] scenetest: sub-scenes in cast: %s | current xsheet is top: %s, %d columns\n",
                qPrintable(subs.join(",")),
                cur == sc->getChildStack()->getTopXsheet() ? "yes" : "no",
                cur ? cur->getColumnCount() : -1);
      }
      {  // the characters the mouth windows would offer
        const MouthCharacters mc = ZtoryMouthApply::characters(
            TApp::instance()->getCurrentScene()->getScene());
        for (const QString &k : mc.keys) {
          int n = 0;
          for (auto it = mc.groupOf.constBegin(); it != mc.groupOf.constEnd(); ++it)
            if (it.value() == k) n++;
          fprintf(stderr, "[ZTORY] scenetest: character '%s' (key '%s'%s): %d levels\n",
                  qPrintable(mc.labels.value(k)), qPrintable(k),
                  mc.fromLibrary(k) ? ", library" : "", n);
        }
      }
      // The mouth targets the lip sync popup would offer here.
      for (const MouthApplyTarget &t : ZtoryMouthApply::findTargets(
               TApp::instance()->getCurrentScene()->getScene())) {
        QStringList names;
        for (const MouthSet &ms : t.map.sets) names << ms.name;
        fprintf(stderr, "[ZTORY] scenetest: mouths '%s' sub='%s' from='%s' %d sets: %s\n",
                qPrintable(t.label), qPrintable(t.subScene),
                qPrintable(t.fromCharacter), (int)t.map.sets.size(),
                qPrintable(names.join(",")));
      }
      // «switchsave:<other project .xml>»: the review's I-1 (2026-10-13).
      // The project is switched as «Set as current project» does, with no
      // scene of it opened, then an edit is saved as the Production panel
      // would.  Does the memory follow the switch, or is the save refused?
      if (action.startsWith("switchsave:")) {
        fprintf(stderr, "[ZTORY] scenetest: before switch: db=%s title='%s'\n",
                qPrintable(m->projectDbPath()), qPrintable(m->title()));
        TProjectManager::instance()->setCurrentProjectPath(
            TFilePath(action.mid(11).toStdWString()));
        QTimer::singleShot(1500, qApp, [m]() {
          fprintf(stderr, "[ZTORY] scenetest: after switch: db=%s title='%s'\n",
                  qPrintable(m->projectDbPath()), qPrintable(m->title()));
          m->setTitle(m->title() + " [switchsave]");
          m->saveProjectDb();
          fprintf(stderr, "[ZTORY] scenetest: switchsave done\n");
          fflush(stderr);
          QTimer::singleShot(1000, qApp, []() { _exit(0); });
        });
        return;
      }
      // «markmouth:<sub-scene>:<character>»: what «Mark as Mouth Column…»
      // writes, on a copy — then the file is read back: the marks AND every
      // set must be there.
      if (action.startsWith("markmouth:")) {
        // markmouth:<sub>:<character>:<character scene .tnz> — the scene is
        // read from its file, not opened (opening a character scene from the
        // command line stops on the startup window).
        const QStringList parts = action.mid(10).split(':');
        ToonzScene libScene;
        libScene.loadTnzFile(TFilePath(parts.value(2).toStdWString()));
        ToonzScene *sc = &libScene;
        const TFilePath owner = TFilePath(parts.value(2).toStdWString());
        TXsheet *subXsh = nullptr;
        TLevelSet *ls = sc->getLevelSet();
        for (int i = 0; i < ls->getLevelCount() && !subXsh; i++)
          if (ls->getLevel(i) && ls->getLevel(i)->getChildLevel() &&
              QString::fromStdWString(ls->getLevel(i)->getName()) == parts.value(0))
            subXsh = ls->getLevel(i)->getChildLevel()->getXsheet();
        QSet<QString> before;
        ZtoryMouthMap::mappedSubScenes(owner, &before);
        MouthMap setsBefore;
        ZtoryMouthMap::load(owner, parts.value(0), setsBefore);
        MouthColumnTag tag;
        tag.subScene  = parts.value(0);
        tag.id        = "test";
        tag.character = parts.value(1);
        tag.files     = ZtoryMouthLibrary::drawingFingerprint(subXsh);
        QVector<MouthColumnTag> tags{tag};
        QString why;
        const bool ok = subXsh && ZtoryMouthMap::saveColumnTags(owner, tags, &why);
        QSet<QString> after;
        ZtoryMouthMap::mappedSubScenes(owner, &after);
        MouthMap setsAfter;
        ZtoryMouthMap::load(owner, parts.value(0), setsAfter);
        QVector<MouthColumnTag> back;
        ZtoryMouthMap::loadColumnTags(owner, &back);
        fprintf(stderr, "[ZTORY] scenetest: markmouth %s: %s %s | %d files | sub-scenes with sets %d -> %d | sets of it %d -> %d | marks read back %d\n",
                qPrintable(parts.value(0)), ok ? "saved" : "FAILED", qPrintable(why),
                (int)tag.files.size(), (int)before.size(), (int)after.size(),
                (int)setsBefore.sets.size(), (int)setsAfter.sets.size(), (int)back.size());
        fflush(stderr);
        QTimer::singleShot(500, qApp, []() { _exit(0); });
        return;
      }
      // «palettecycle»: the Windows crash of 2026-10-15 (0.17.1). The level
      // palette viewer shows the DEFAULT vector palette (empty column,
      // autocreate), then another one, then the default again — what entering
      // shots from the Animatic does. The default palettes are never
      // ref-counted: a viewer that holds and drops one deletes it.
      if (action == "palettecycle") {
        PaletteController *pc = TApp::instance()->getPaletteController();
        TPalette *def   = pc->getDefaultPalette(PLI_XSHLEVEL);
        TPalette *other = pc->getDefaultPalette(TZP_XSHLEVEL);
        fprintf(stderr, "[ZTORY] scenetest: palettecycle default refcount=%d\n",
                def ? def->getRefCount() : -1);
        QWidget host;  // the viewer reads its parent's width
        host.resize(400, 300);
        auto *pv = new PaletteViewerGUI::PageViewer(&host);
        pv->setPage(def ? def->getPage(0) : nullptr);
        pv->setPage(other ? other->getPage(0) : nullptr);
        delete pv;
        // Read it back: freed, MallocScribble makes this garbage or a crash.
        fprintf(stderr, "[ZTORY] scenetest: palettecycle default after: name='%s'\n",
                def ? qPrintable(QString::fromStdWString(def->getPaletteName())) : "-");
        fflush(stderr);
        QTimer::singleShot(500, qApp, []() { _exit(0); });
        return;
      }
      if (action == "probe") {  // look only: nothing is written
        fprintf(stderr, "[ZTORY] scenetest: probe done\n");
        fflush(stderr);
        QTimer::singleShot(500, qApp, []() { _exit(0); });
        return;
      }
      m->saveProjectDb();
      TApp::instance()->getCurrentScene()->setDirtyFlag(true);
      bool ok = action.startsWith("saveas:")
                    ? IoCmd::saveScene(TFilePath(action.mid(7).toStdWString()), 0)
                    : IoCmd::saveAll();
      fprintf(stderr, "[ZTORY] scenetest: %s %s\n", qPrintable(action),
              ok ? "saved" : "FAILED");
      fflush(stderr);
      QTimer::singleShot(3000, qApp, []() { _exit(0); });
    });
  });
  poll->start();
}
Q_COREAPP_STARTUP_FUNCTION(ztoryArmSceneTest)
