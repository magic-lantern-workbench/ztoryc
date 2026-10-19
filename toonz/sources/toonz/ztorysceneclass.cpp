#include "ztorysceneclass.h"
#include "ztorymodel.h"
#include "ztorycharacter.h"
#include "menubarcommandids.h"
#include "tapp.h"
#include "toonz/toonzscene.h"
#include "toonz/tscenehandle.h"
#include "toonzqt/menubarcommand.h"
#include "toonzqt/dvdialog.h"
#include "toonz/preferences.h"

#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDirIterator>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

namespace {

enum class Role { Storyboard, Shot, Character, Plain, None };

QString currentScenePath() {
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (!scene || scene->isUntitled()) return QString();
  return QString::fromStdWString(scene->getScenePath().getWideString());
}

// The role written in a sidecar; None when there is no file.  A file with no
// attribute is an old storyboard (the attribute came later).
Role readRole(const QString &sidecar) {
  QFile f(sidecar);
  if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return Role::None;
  QXmlStreamReader xml(&f);
  while (!xml.atEnd())
    if (xml.readNext() == QXmlStreamReader::StartElement) {
      const QString r = xml.attributes().value("role").toString();
      if (r == "shot") return Role::Shot;
      if (r == "character") return Role::Character;
      if (r == "plain") return Role::Plain;
      return Role::Storyboard;
    }
  return Role::Storyboard;
}

// The newest copy of \p sidecar kept while the scene was a storyboard WITH
// shots (backupSidecar names them <sidecar>.bak-yyyyMMdd-HHmmss, so the name
// order is the time order); empty if none.
QString storyboardBackupOf(const QString &sidecar) {
  const QFileInfo fi(sidecar);
  const QDir dir = fi.dir();
  const QStringList baks = dir.entryList(
      QStringList() << fi.fileName() + ".bak-*", QDir::Files, QDir::Name);
  for (int i = baks.size() - 1; i >= 0; i--) {
    const QString path = dir.filePath(baks[i]);
    if (readRole(path) != Role::Storyboard) continue;
    QFile f(path);
    if (f.open(QIODevice::ReadOnly | QIODevice::Text) &&
        f.readAll().contains("<shot "))
      return path;
  }
  return QString();
}

// Changing what a scene is drops what its sidecar held for the old role (the
// storyboard's texts, the shot's link, the mouth sets): a copy stays next to it.
bool backupSidecar(const QString &sidecar) {
  const QString stamp =
      QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss");
  const QString bak = sidecar + ".bak-" + stamp;
  // The open storyboard's texts typed since the last ⌘S are only in memory
  // (the file is written with the scene): the copy is made from memory, or
  // they would be lost with the type change (review 0.17, M5).
  ZtoryModel *m = ZtoryModel::instance();
  if (m->shotDataLoadedFor(sidecar) && m->shotCount() > 0 &&
      !m->shotDocumentState().keepsOwnSidecar()) {
    std::vector<const ShotData *> shots;
    for (int i = 0; i < m->shotCount(); i++) shots.push_back(&m->shot(i));
    if (m->writeShotDocument(bak, shots)) return true;
  }
  return QFile::copy(sidecar, bak);
}

// Whether \p tnz lives in the project whose Production Tracker is in memory.
// A shot or character link is written against THAT tracker, so classifying a
// scene of another project (from the start-up list) linked it to the wrong
// one — and opening it then wrote one project's tracker into the other's, the
// damage of 2026-10-07 by another road (review 0.17, B1).
bool inCurrentProject(const QString &tnz) {
  const QString dbPath  = ZtoryModel::instance()->projectDbPath();
  const QString projDir = QFileInfo(dbPath).absolutePath();
  return !dbPath.isEmpty() && !projDir.isEmpty() &&
         QFileInfo(tnz).absoluteFilePath().startsWith(projDir + "/",
#ifdef _WIN32
                                                      // c:\ and C:\ are one disk
                                                      Qt::CaseInsensitive
#else
                                                      Qt::CaseSensitive
#endif
         );  // review 0.17, M12
}

bool isOpenScene(const QString &tnz) {
  return QFileInfo(tnz) == QFileInfo(currentScenePath());
}

// The Tracker answer a storyboard's sidecar holds (the two names the Board
// writes, ZtoryModel::writeShotDocument).
bool sidecarTrackerOff(const QString &sidecar) {
  QFile f(sidecar);
  if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
  QXmlStreamReader xml(&f);
  while (!xml.atEnd())
    if (xml.readNext() == QXmlStreamReader::StartElement) {
      const auto a = xml.attributes();
      return a.value("productionTracker") == QLatin1String("off") ||
             a.value("projectPublication") == QLatin1String("local");
    }
  return false;
}

// Changes only the Tracker answer in a storyboard's sidecar, keeping the shots
// and texts it holds: for a scene that is not the open one, whose data is not
// in memory to be written with ⌘S.
bool setSidecarTrackerOff(const QString &sidecar, bool off, QString *error) {
  QFile in(sidecar);
  if (!in.open(QIODevice::ReadOnly | QIODevice::Text)) {
    if (error) *error = QObject::tr("cannot read %1").arg(QFileInfo(sidecar).fileName());
    return false;
  }
  QString text = QString::fromUtf8(in.readAll());
  in.close();
  static const QRegularExpression kRoot("<ztoryc\\b[^>]*>");
  const QRegularExpressionMatch m = kRoot.match(text);
  if (!m.hasMatch()) {
    if (error) *error = QObject::tr("%1 is not a .ztoryc").arg(QFileInfo(sidecar).fileName());
    return false;
  }
  static const QRegularExpression kAttrs(
      "\\s+(productionTracker|projectPublication)=\"[^\"]*\"");
  QString root = m.captured(0);
  root.remove(kAttrs);
  if (off) {
    const int close = root.endsWith("/>") ? root.size() - 2 : root.size() - 1;
    root.insert(close, " productionTracker=\"off\" projectPublication=\"local\"");
  }
  text.replace(m.capturedStart(0), m.capturedLength(0), root);
  QSaveFile out(sidecar);
  if (!out.open(QIODevice::WriteOnly | QIODevice::Text) ||
      out.write(text.toUtf8()) < 0 || !out.commit()) {
    if (error) *error = QObject::tr("cannot write %1").arg(QFileInfo(sidecar).fileName());
    return false;
  }
  return true;
}

// ── The dialog ───────────────────────────────────────────────────────────────

class SceneClassDialog final : public QDialog {
public:
  SceneClassDialog(const QString &tnzPath, Role current, QWidget *parent,
                   bool becauseEdited = false);
  Role role() const;
  bool trackerOn() const { return m_tracker->isChecked(); }
  QString shotUuid() const;
  QString characterUuid() const { return m_character->currentData().toString(); }
  QString plainWorkflow() const { return m_plainWorkflow->currentData().toString(); }
  QString characterName() const {
    return characterUuid().isEmpty() ? QString() : m_character->currentText();
  }

private:
  void fillShots(const QString &tnzPath);
  void fillCharacters();
  void updateState();

  QButtonGroup *m_group;
  QCheckBox *m_tracker;
  QTreeWidget *m_shots;
  QComboBox *m_character;
  QComboBox *m_plainWorkflow;
  QPushButton *m_ok;
  QString m_scenePath;
  bool m_ownProject = true;  // the scene is in the project of the Tracker
};

SceneClassDialog::SceneClassDialog(const QString &tnzPath, Role current,
                                   QWidget *parent, bool becauseEdited)
    : QDialog(parent), m_scenePath(tnzPath) {
  setWindowTitle(QObject::tr("Scene Type"));
  setMinimumWidth(560);
  auto *lay = new QVBoxLayout(this);
  const QString name = QFileInfo(tnzPath).completeBaseName();
  lay->addWidget(new QLabel(
      current == Role::None
          ? QObject::tr("<b>%1</b> has not been classified yet. What is it?\n")
                .arg(name)
          : QObject::tr("What is <b>%1</b>?").arg(name)));
  if (current == Role::None) {
    auto *note = new QLabel(
        becauseEdited
            ? QObject::tr("You are writing in the Board of a scene that is not "
                          "a storyboard yet: what you type there is NOT saved "
                          "until you classify it as a storyboard.")
            : QObject::tr("Until it is classified, Ztoryc treats it as a plain "
                          "scene: its sub-scenes are not shots and nothing "
                          "reaches the Production Tracker."),
        this);
    note->setWordWrap(true);
    lay->addWidget(note);
  }

  m_group = new QButtonGroup(this);
  auto addChoice = [&](Role r, const QString &title, const QString &desc) {
    auto *rb = new QRadioButton(title, this);
    m_group->addButton(rb, int(r));
    lay->addWidget(rb);
    auto *d = new QLabel(desc, this);
    d->setWordWrap(true);
    d->setIndent(22);
    d->setStyleSheet("color: gray;");
    lay->addWidget(d);
  };

  const bool hasTracker = QFile::exists(ZtoryModel::instance()->projectDbPath());

  addChoice(Role::Storyboard, QObject::tr("Storyboard"),
            QObject::tr("Each sub-scene is a shot, shown in the Board and the "
                        "Animatic."));
  m_tracker = new QCheckBox(
      QObject::tr("Show its shots in the project's Production Tracker"), this);
  // The box starts from what the scene says, not from «the project has a
  // Tracker»: Apply on a storyboard kept local, touching nothing, published
  // it with the next ⌘S (review 0.17, I1).  The open scene's answer is in
  // memory (it may not be saved yet), another scene's in its sidecar.
  bool keptLocal = false;
  if (current == Role::Storyboard)
    keptLocal = isOpenScene(tnzPath)
                    ? ZtoryModel::instance()->shotDocumentState().trackerOff
                    : sidecarTrackerOff(ZtorySceneClass::sidecarFor(tnzPath));
  m_tracker->setChecked(hasTracker && !keptLocal);
  m_tracker->setEnabled(hasTracker);
  m_tracker->setStyleSheet("margin-left: 22px;");
  lay->addWidget(m_tracker);

  addChoice(Role::Shot, QObject::tr("Shot"),
            QObject::tr("The scene where one shot of the Production Tracker is "
                        "made. Pick the shot:"));
  m_shots = new QTreeWidget(this);
  m_shots->setHeaderLabels({QObject::tr("Shot"), QObject::tr("Sequence"),
                            QObject::tr("Storyboard"), QObject::tr("Technique"),
                            QObject::tr("Scene already linked")});
  m_shots->setRootIsDecorated(false);
  m_shots->setIconSize(QSize(64, 36));
  m_shots->setMinimumHeight(160);
  lay->addWidget(m_shots);
  fillShots(tnzPath);

  addChoice(Role::Character, QObject::tr("Character"),
            QObject::tr("A character's scene: its rig and mouth sets."));
  m_character = new QComboBox(this);
  lay->addWidget(m_character);
  fillCharacters();

  addChoice(Role::Plain, QObject::tr("Plain scene"),
            QObject::tr("A scene of the project that is none of the above: "
                        "its sub-scenes stay sub-scenes. It opens in:"));
  m_plainWorkflow = new QComboBox(this);
  m_plainWorkflow->addItem(QObject::tr("2D Tradigital Mode"), "Tradigital");
  m_plainWorkflow->addItem(QObject::tr("Cutout Digital Mode"), "Cutout");
  m_plainWorkflow->addItem(QObject::tr("Stop-Motion Mode"), "StopMotion");
  {
    // Its own workflow if it has one, else the drawing workflow in use.
    QString keep = ZtorySceneClass::plainWorkflowOf(tnzPath);
    if (keep.isEmpty()) keep = Preferences::instance()->getCurrentRoomChoice();
    const int i = m_plainWorkflow->findData(keep);
    if (i >= 0) m_plainWorkflow->setCurrentIndex(i);
  }
  lay->addWidget(m_plainWorkflow);

  auto *buttons = new QDialogButtonBox(this);
  m_ok = buttons->addButton(QObject::tr("Apply"), QDialogButtonBox::AcceptRole);
  buttons->addButton(current == Role::None ? QObject::tr("Decide later")
                                           : QObject::tr("Cancel"),
                     QDialogButtonBox::RejectRole);
  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  lay->addWidget(buttons);

  if (!hasTracker) m_group->button(int(Role::Shot))->setEnabled(false);
  m_ownProject = inCurrentProject(tnzPath);
  if (!m_ownProject) {
    const QString why = QObject::tr(
        "This scene is not in the project open now: it can be linked to a shot "
        "or a character only with its own project open.");
    for (Role r : {Role::Shot, Role::Character}) {
      m_group->button(int(r))->setEnabled(false);
      m_group->button(int(r))->setToolTip(why);
    }
    m_shots->setEnabled(false);
    m_character->setEnabled(false);
    auto *note = new QLabel(why, this);
    note->setWordWrap(true);
    note->setStyleSheet("color: gray;");
    lay->insertWidget(lay->count() - 1, note);
  }

  // The current role, or for a scene nobody classified the workflow it is
  // opened in: the likeliest answer, still to be confirmed.
  Role pre = current;
  if (pre == Role::None) {
    const QString room = Preferences::instance()->getCurrentRoomChoice();
    pre = room == "Storyboard" ? Role::Storyboard
        : room == "Character"  ? Role::Character
                               : Role::Plain;
  }
  m_group->button(int(pre))->setChecked(true);

  connect(m_group, &QButtonGroup::idClicked, this, [this](int) { updateState(); });
  connect(m_shots, &QTreeWidget::itemSelectionChanged, this,
          [this] { updateState(); });
  connect(m_shots, &QTreeWidget::itemClicked, this, [this] {
    m_group->button(int(Role::Shot))->setChecked(true);
    updateState();
  });
  connect(m_plainWorkflow, QOverload<int>::of(&QComboBox::activated), this, [this] {
    m_group->button(int(Role::Plain))->setChecked(true);
    updateState();
  });
  connect(m_character, QOverload<int>::of(&QComboBox::activated), this, [this] {
    m_group->button(int(Role::Character))->setChecked(true);
    updateState();
  });
  updateState();
}

void SceneClassDialog::fillShots(const QString &tnzPath) {
  ZtoryModel *m = ZtoryModel::instance();
  // The Tracker's thumbnails, read from the project if nothing loaded them
  // yet (a scene opened with no Board): what tells two shots apart at a glance.
  if (m->thumbCache().isEmpty()) m->loadThumbsFromDisk();
  const QMap<QString, QString> linked = ZtorySceneClass::linkedShotScenes(m->projectDbPath());
  // This scene's own shots, published while it was a storyboard: it cannot
  // be the scene of one of its own shots.
  const QString ownSource = QFileInfo(ZtorySceneClass::sidecarFor(tnzPath)).fileName();
  for (const ProjectShot &ps : m->projectShots()) {
    if (ps.source == ownSource) continue;
    const QString scene = linked.value(ps.uuid);
    const bool mine = !scene.isEmpty() && QFileInfo(scene) == QFileInfo(tnzPath);
    auto *it = new QTreeWidgetItem(
        {ps.label, ps.seq, QFileInfo(ps.source).completeBaseName(),
         ps.technique.isEmpty() ? m->defaultTechnique() : ps.technique,
         mine ? QObject::tr("this scene") : QFileInfo(scene).fileName()});
    it->setData(0, Qt::UserRole, ps.uuid);
    if (m->thumbCache().contains(ps.uuid))
      it->setIcon(0, QIcon(m->thumbCache().value(ps.uuid)));
    // A shot that has its scene elsewhere is not offered: two scenes for one
    // shot would split its work.  The path says where it is.
    if (!scene.isEmpty() && !mine) it->setFlags(it->flags() & ~Qt::ItemIsEnabled);
    m_shots->addTopLevelItem(it);
    if (mine) m_shots->setCurrentItem(it);
  }
  m_shots->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
}

void SceneClassDialog::fillCharacters() {
  m_character->addItem(QObject::tr("(no character of the project)"), QString());
  for (const Asset &a : ZtoryModel::instance()->assets()) {
    if (!ZtoryModel::isCharacterType(a.type)) continue;
    // A character that has its scene elsewhere is not offered: a second scene
    // would take the link from the first (as in the start-up popup).  The real
    // file counts: if that scene was deleted the character is free again.
    if (!a.filePath.isEmpty() && QFile::exists(a.filePath) &&
        QFileInfo(a.filePath) != QFileInfo(m_scenePath))
      continue;
    m_character->addItem(a.name, a.uuid);
    if (QFileInfo(a.filePath) == QFileInfo(m_scenePath))
      m_character->setCurrentIndex(m_character->count() - 1);
  }
}

Role SceneClassDialog::role() const { return Role(m_group->checkedId()); }

QString SceneClassDialog::shotUuid() const {
  QTreeWidgetItem *it = m_shots->currentItem();
  return it ? it->data(0, Qt::UserRole).toString() : QString();
}

void SceneClassDialog::updateState() {
  const Role r = role();
  // The lists stay usable whatever is selected: picking from one IS choosing
  // its role (a disabled list could not be clicked to say "this shot").
  m_tracker->setVisible(r == Role::Storyboard);
  const bool linked = r == Role::Shot || r == Role::Character;
  m_ok->setEnabled((r != Role::Shot || !shotUuid().isEmpty()) &&
                   (m_ownProject || !linked));
}

// ── Applying the answer ──────────────────────────────────────────────────────

// A scene that stops being a storyboard leaves its published shots in the
// Tracker for good: nobody republishes them and nobody removes them — that is
// how a project ended up with four SH010 from four files (2026-08-17).  Offer
// to remove them.  Only for the project the Tracker holds: removing shots of
// another project would be silent damage.
void offerToUnpublish(const QString &tnz, QWidget *parent) {
  ZtoryModel *m          = ZtoryModel::instance();
  const bool sameProject = inCurrentProject(tnz);
  const QString source = QFileInfo(ZtorySceneClass::sidecarFor(tnz)).fileName();
  const int n = sameProject ? m->projectShotCountFromSource(source) : 0;
  if (n <= 0) return;
  QMessageBox ask(parent);
  ask.setWindowTitle(QObject::tr("Shots in the Production Tracker"));
  ask.setIcon(QMessageBox::Question);
  ask.setText(QObject::tr("This scene has %1 shot(s) in the Production Tracker, "
                          "published while it was a storyboard.")
                  .arg(n));
  ask.setInformativeText(QObject::tr(
      "Now that it is not a storyboard any more, nothing will keep them up to "
      "date — and a shot called like one of another storyboard makes the two "
      "impossible to tell apart.\n\nTheir task statuses, assignees and "
      "breakdown are removed with them."));
  QPushButton *rem =
      ask.addButton(QObject::tr("Remove them"), QMessageBox::AcceptRole);
  ask.addButton(QObject::tr("Keep them"), QMessageBox::RejectRole);
  ask.exec();
  if (ask.clickedButton() != rem) return;
  const int removed = m->removeProjectShotsFromSource(source);
  m->saveProjectDb();
  emit m->taskStatusChanged();
  DVGui::info(QObject::tr("%1 shot(s) removed from the project.").arg(removed));
}

bool apply(const SceneClassDialog &dlg, const QString &tnz, Role current) {
  const QString sidecar = ZtorySceneClass::sidecarFor(tnz);
  const Role next       = dlg.role();
  QString why;
  bool ok = true;
  // The dialog already greys these out; refused here too, the damage being
  // another project's tracker (B1).
  if ((next == Role::Shot || next == Role::Character) && !inCurrentProject(tnz)) {
    DVGui::warning(QObject::tr("This scene is not in the project open now: open "
                               "its project to link it to a shot or a character."));
    return false;
  }
  // With no Tracker in the project nothing was asked: «off» written now would
  // keep the storyboard out of the Tracker the project may have later (I1).
  const bool hasTracker =
      QFile::exists(ZtoryModel::instance()->projectDbPath());
  const bool keepLocal = hasTracker && !dlg.trackerOn();

  // What the old role kept in the sidecar is gone with the new one: say so,
  // and keep a copy.  A plain scene's sidecar holds nothing.
  if (current != Role::None && current != Role::Plain && next != current) {
    const QString lost =
        current == Role::Storyboard
            ? QObject::tr("As a storyboard, its .ztoryc holds the shots' "
                          "dialogue, notes and numbering.")
        : current == Role::Shot
            ? QObject::tr("As a shot, its .ztoryc holds the link to its shot "
                          "in the Production Tracker.")
            : QObject::tr("As a character, its .ztoryc holds its mouth sets "
                          "and the link to its character.");
    if (DVGui::MsgBox(lost + "\n" +
                          QObject::tr("Changing its type replaces it; a copy "
                                      "is kept next to it.\n\nChange it?"),
                      QObject::tr("Change"), QObject::tr("Cancel"), 1) != 1)
      return false;
    if (!backupSidecar(sidecar)) {
      DVGui::warning(QObject::tr("Could not keep a copy of %1: nothing changed.")
                         .arg(QFileInfo(sidecar).fileName()));
      return false;
    }
  }

  switch (next) {
  case Role::Storyboard:
    if (current == Role::Storyboard) {
      // Same type: only the Tracker answer.  The open scene's reaches the file
      // with ⌘S; another scene's is changed in its own sidecar — it used to
      // land on the OPEN scene's state, and the chosen one kept its answer (I1).
      if (!hasTracker) return true;
      if (isOpenScene(tnz)) {
        ZtoryModel::ShotDocumentState &st =
            ZtoryModel::instance()->shotDocumentState();
        if (st.trackerOff != keepLocal) {
          st.trackerOff = keepLocal;
          ZtoryModel::instance()->markShotDocumentDirty();
        }
        return true;
      }
      ok = setSidecarTrackerOff(sidecar, keepLocal, &why);
      break;
    }
    {
      // A scene that WAS a storyboard: its shots kept their uuids in the copy
      // made when it stopped being one.  Declared afresh, they came back with
      // new uuids and the next publish dropped the Tracker's old rows —
      // statuses, assignees, Kitsu ids — if «Keep them» had kept them
      // (review 0.17, M6).  Brought back, the shots are the same shots.
      const QString bak = storyboardBackupOf(sidecar);
      if (!bak.isEmpty() &&
          DVGui::MsgBox(
              QObject::tr("This scene was a storyboard before. Its shots and "
                          "texts were kept in %1.\n\nBring them back? Its "
                          "shots stay the same shots in the Production "
                          "Tracker, with their statuses.")
                  .arg(QFileInfo(bak).fileName()),
              QObject::tr("Bring them back"), QObject::tr("Start empty"),
              0) == 1) {
        ok = (!QFile::exists(sidecar) || QFile::remove(sidecar)) &&
             QFile::copy(bak, sidecar) &&
             (!hasTracker || setSidecarTrackerOff(sidecar, keepLocal, &why));
        if (!ok && why.isEmpty())
          why = QObject::tr("cannot restore %1").arg(QFileInfo(bak).fileName());
        break;
      }
    }
    ok = ZtorySceneClass::declareStoryboard(tnz, keepLocal, &why);
    break;
  case Role::Shot:
    ok = ZtorySceneClass::declareShot(tnz, dlg.shotUuid(),
                                      ZtoryModel::instance()->projectDbPath(), &why);
    break;
  case Role::Character:
    ok = ZtoryCharacter::declareCharacterScene(tnz, dlg.characterUuid(),
                                               dlg.characterName(), &why);
    if (ok && !dlg.characterUuid().isEmpty()) {
      // The asset points at its scene, as when the scene is created from it.
      ZtoryModel *m = ZtoryModel::instance();
      for (int i = 0; i < m->assetCount(); i++)
        if (m->assets()[i].uuid == dlg.characterUuid()) {
          m->setAssetFilePath(i, tnz);
          m->saveProjectDb();
          break;
        }
    }
    break;
  case Role::Plain:
    ok = ZtorySceneClass::declarePlain(tnz, dlg.plainWorkflow(), &why);
    break;
  case Role::None:
    return false;
  }
  if (!ok) {
    DVGui::warning(QObject::tr("Could not classify the scene: %1").arg(why));
    return false;
  }
  if (current == Role::Storyboard && next != Role::Storyboard)
    offerToUnpublish(tnz, const_cast<SceneClassDialog *>(&dlg)->parentWidget());
  // The open scene is read again as what it now is: the Board, the model and
  // the workflow follow the sidecar exactly as when a scene is opened.
  if (isOpenScene(tnz)) {
    // The answer goes into memory too, BEFORE the scene is read again: the
    // Board keeps what memory holds about the scene over what the file says
    // (loadZtoryc, restoreKeptScene), and gave back the old «tracker on» over
    // the «off» just written — the next ⌘S published the shots (I2).
    if (next == Role::Storyboard)
      ZtoryModel::instance()->shotDocumentState().trackerOff = keepLocal;
    TApp::instance()->getCurrentScene()->notifySceneSwitched();
    // A new storyboard's shots (and anything typed in the Board meanwhile)
    // are only in memory: the asterisk, so closing asks to save them.
    if (next == Role::Storyboard)
      QTimer::singleShot(0, qApp,
                         [] { ZtoryModel::instance()->markShotDocumentDirty(); });
    // In Auto, the rooms follow the new type as when a scene is opened (a
    // Board does it for storyboards and shots, but there may be none, and
    // nothing did it for a character or a plain scene).
    if (ZtoryModel::autoWorkflowDetection()) {
      QString cmd;
      if (next == Role::Plain) {
        cmd = ZtorySceneClass::workflowCommandForRoom(dlg.plainWorkflow());
      } else {
        QString role = next == Role::Storyboard ? "storyboard"
                     : next == Role::Shot       ? "shot"
                                                : "character";
        QString tech;
        for (const ProjectShot &ps : ZtoryModel::instance()->projectShots())
          if (ps.uuid == dlg.shotUuid()) { tech = ps.technique; break; }
        if (tech.isEmpty()) tech = ZtoryModel::instance()->defaultTechnique();
        cmd = ZtoryModel::workflowCommand(role, tech);
      }
      if (!cmd.isEmpty())
        QTimer::singleShot(0, qApp, [cmd] {
          CommandManager::instance()->execute(cmd.toStdString().c_str());
        });
    }
  }
  return true;
}

class SceneTypeCommand final : public MenuItemHandler {
public:
  SceneTypeCommand() : MenuItemHandler(MI_ZtorySceneType) {}
  void execute() override { ZtorySceneClass::chooseForCurrentScene(); }
} sceneTypeCommand;

}  // namespace

// ─────────────────────────────────────────────────────────────────────────────

bool ZtorySceneClass::isInCurrentProject(const QString &path) {
  return inCurrentProject(path);
}

QString ZtorySceneClass::sidecarFor(const QString &tnzPath) {
  QString p = tnzPath;
  p.replace(QRegularExpression("\\.tnz$"), ".ztoryc");
  return p;
}

static bool writeRoleOnly(const QString &sidecar, const QString &role,
                          QString *error, bool trackerOff = false,
                          const QString &workflow = QString()) {
  QSaveFile f(sidecar);  // never a half-written sidecar (review 0.17, M1)
  if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
    if (error) *error = QObject::tr("cannot write %1").arg(QFileInfo(sidecar).fileName());
    return false;
  }
  QXmlStreamWriter xml(&f);
  xml.setAutoFormatting(true);
  xml.writeStartDocument();
  xml.writeStartElement("ztoryc");
  xml.writeAttribute("version", "2");
  xml.writeAttribute("role", role);
  if (trackerOff) {  // the same two attributes the Board writes
    xml.writeAttribute("productionTracker", "off");
    xml.writeAttribute("projectPublication", "local");
  }
  if (!workflow.isEmpty()) xml.writeAttribute("workflow", workflow);
  xml.writeEndElement();
  xml.writeEndDocument();
  if (xml.hasError() || !f.commit()) {
    if (error) *error = QObject::tr("cannot write %1").arg(QFileInfo(sidecar).fileName());
    return false;
  }
  return true;
}

bool ZtorySceneClass::declarePlain(const QString &tnzPath,
                                   const QString &workflow, QString *error) {
  return writeRoleOnly(sidecarFor(tnzPath), "plain", error, false, workflow);
}

QString ZtorySceneClass::plainWorkflowOf(const QString &tnzPath) {
  QFile f(sidecarFor(tnzPath));
  if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return QString();
  QXmlStreamReader xml(&f);
  while (!xml.atEnd())
    if (xml.readNext() == QXmlStreamReader::StartElement) {
      const auto a = xml.attributes();
      return a.value("role") == QLatin1String("plain")
                 ? a.value("workflow").toString()
                 : QString();
    }
  return QString();
}

QString ZtorySceneClass::workflowCommandForRoom(const QString &room) {
  if (room == "Tradigital") return MI_Workflow2D;
  if (room == "Cutout") return MI_WorkflowCutout;
  if (room == "StopMotion") return MI_WorkflowStopMotion;
  return QString();
}

QString ZtorySceneClass::autoWorkflowCommandFor(const QString &tnzPath) {
  QString ztoryPath = tnzPath;
  ztoryPath.replace(QRegularExpression("\\.tnz$"), ".ztoryc");
  QFile zf(ztoryPath);
  if (!zf.exists() || !zf.open(QIODevice::ReadOnly | QIODevice::Text))
    return QString();
  const QString head = QString::fromUtf8(zf.read(1024));
  // The role is READ, not guessed from «shot» (a character went to the
  // storyboard rooms). Absent = storyboard, as sidecars written before roles.
  QString role = "storyboard", technique;
  QRegularExpressionMatch rm =
      QRegularExpression("<ztoryc[^>]*\\brole=\"([^\"]*)\"").match(head);
  if (rm.hasMatch() && !rm.captured(1).isEmpty()) role = rm.captured(1);
  QRegularExpressionMatch mt =
      QRegularExpression("technique=\"([^\"]*)\"").match(head);
  if (mt.hasMatch()) technique = mt.captured(1);
  if (role == QLatin1String("plain"))
    return workflowCommandForRoom(plainWorkflowOf(tnzPath));
  return ZtoryModel::workflowCommand(role, technique);
}

void ZtorySceneClass::followSceneWorkflow() {
  if (!ZtoryModel::autoWorkflowDetection()) return;
  const QString tnz = currentScenePath();
  if (tnz.isEmpty()) return;
  // Once per opening: a later manual room switch is the user's.
  static QString lastFollowed;
  if (tnz == lastFollowed) return;
  lastFollowed = tnz;
  const QString cmd = autoWorkflowCommandFor(tnz);
  if (cmd.isEmpty()) return;
  // Already there (the start-up popup or a Board did it): nothing to do —
  // and it matters: the Storyboard command also jumps to the first room.
  const QString room = Preferences::instance()->getCurrentRoomChoice();
  const QString here = room == "Storyboard"  ? QString(MI_WorkflowStoryboard)
                       : room == "Character" ? QString(MI_WorkflowCharacter)
                                             : workflowCommandForRoom(room);
  if (here == cmd) return;
  CommandManager::instance()->execute(cmd.toStdString().c_str());
}

bool ZtorySceneClass::declareStoryboard(const QString &tnzPath, bool trackerOff,
                                        QString *error) {
  // The role and the Tracker answer only, never shots: the scene may not be
  // the open one, and the model holds the open one's.  The shots reach the
  // file with the first ⌘S, as for any storyboard.
  return writeRoleOnly(sidecarFor(tnzPath), "storyboard", error, trackerOff);
}

bool ZtorySceneClass::writeShotLink(const QString &sidecarPath,
                                    const QString &shotUuid,
                                    const QString &projectDb,
                                    const QString &technique) {
  ZtoryModel *model = ZtoryModel::instance();
  QSaveFile f(sidecarPath);  // never a half-written sidecar (review 0.17, M1)
  if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
  QXmlStreamWriter xml(&f);
  xml.setAutoFormatting(true);
  xml.writeStartDocument();
  xml.writeStartElement("ztoryc");
  xml.writeAttribute("version",     "2");
  xml.writeAttribute("role",        "shot");
  xml.writeAttribute("projectShot", shotUuid);
  xml.writeAttribute("project",     projectDb);
  // Inherit project-level metadata so shot scene knows its context.
  xml.writeStartElement("project");
  xml.writeAttribute("production", model->production());
  xml.writeAttribute("season",     model->season());
  xml.writeAttribute("episode",    model->episode());
  xml.writeAttribute("title",      model->title());
  xml.writeAttribute("technique",  technique);
  xml.writeEndElement();
  xml.writeEndElement();
  xml.writeEndDocument();
  return !xml.hasError() && f.commit();
}

bool ZtorySceneClass::declareShot(const QString &tnzPath, const QString &shotUuid,
                                  const QString &projectDb, QString *error) {
  ZtoryModel *m = ZtoryModel::instance();
  QString tech;
  for (const ProjectShot &ps : m->projectShots())
    if (ps.uuid == shotUuid) { tech = ps.technique; break; }
  if (tech.isEmpty()) tech = m->defaultTechnique();
  if (shotUuid.isEmpty() || projectDb.isEmpty() ||
      !writeShotLink(sidecarFor(tnzPath), shotUuid, projectDb, tech)) {
    if (error) *error = QObject::tr("cannot write the shot link");
    return false;
  }
  return true;
}

QMap<QString, QString> ZtorySceneClass::linkedShotScenes(const QString &projectDb) {
  QMap<QString, QString> out;
  if (projectDb.isEmpty()) return out;
  const QString root = QFileInfo(projectDb).absolutePath();
  QDirIterator it(root, {"*.ztoryc"}, QDir::Files, QDirIterator::Subdirectories);
  while (it.hasNext()) {
    const QString path = it.next();
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
    QXmlStreamReader xml(&f);
    while (!xml.atEnd())
      if (xml.readNext() == QXmlStreamReader::StartElement) {
        const auto a = xml.attributes();
        // Only links to THIS project: a shot scene copied from elsewhere
        // still names its own tracker.
        if (a.value("role") == QLatin1String("shot") &&
            QFileInfo(a.value("project").toString()) == QFileInfo(projectDb)) {
          QString tnz = path;
          tnz.replace(QRegularExpression("\\.ztoryc$"), ".tnz");
          out.insert(a.value("projectShot").toString(), tnz);
        }
        break;  // the root element is enough
      }
  }
  return out;
}

void ZtorySceneClass::askIfUnclassified() {
  ZtoryModel *m   = ZtoryModel::instance();
  const QString tnz = currentScenePath();
  if (tnz.isEmpty() || !m->currentSceneIsUnclassified() ||
      m->sceneClassDeclined(tnz))
    return;
  // Another box is up (the start-up popup, a load warning): ask after it.
  if (QApplication::activeModalWidget()) {
    QTimer::singleShot(1000, qApp, [] { askIfUnclassified(); });
    return;
  }
  if (!chooseFor(tnz, QApplication::activeWindow()))
    m->noteSceneClassDeclined(tnz);  // asked again next time it is opened
}

void ZtorySceneClass::askBecauseEdited() {
  static QSet<QString> askedForEdit;
  const QString tnz = currentScenePath();
  if (tnz.isEmpty() || !ZtoryModel::instance()->currentSceneIsUnclassified() ||
      askedForEdit.contains(tnz) || QApplication::activeModalWidget())
    return;
  askedForEdit.insert(tnz);
  SceneClassDialog dlg(tnz, Role::None, QApplication::activeWindow(),
                       /*becauseEdited=*/true);
  if (dlg.exec() == QDialog::Accepted) apply(dlg, tnz, Role::None);
}

bool ZtorySceneClass::chooseFor(const QString &tnzPath, QWidget *parent) {
  const Role current = readRole(sidecarFor(tnzPath));
  SceneClassDialog dlg(tnzPath, current, parent);
  if (dlg.exec() != QDialog::Accepted) return false;
  return apply(dlg, tnzPath, current);
}

void ZtorySceneClass::chooseForCurrentScene() {
  const QString tnz = currentScenePath();
  if (tnz.isEmpty()) {
    DVGui::info(QObject::tr("Save the scene first: its type is written next to it."));
    return;
  }
  chooseFor(tnz, QApplication::activeWindow());
}
