#include "ztorymaingate.h"

// Ztoryc
#include "ztorymodel.h"
#include "ztorysceneclass.h"

// Tahoma2D
#include "celldata.h"
#include "tapp.h"
#include "tundo.h"
#include "toonzqt/rasterimagedata.h"
#include "toonzqt/strokesdata.h"
#include "tools/tool.h"
#include "toonz/childstack.h"
#include "toonz/toonzscene.h"
#include "toonz/tscenehandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/tframehandle.h"
#include "toonz/txsheet.h"
#include "toonz/txshcolumn.h"
#include "toonz/txshlevelcolumn.h"
#include "toonz/txshcell.h"
#include "toonz/txshlevel.h"
#include "toonz/txshleveltypes.h"
#include "toonz/tstageobject.h"
#include "toonz/tstageobjectid.h"
#include "toonzqt/dvdialog.h"

// Qt
#include <QApplication>
#include <QMap>
#include <QMessageBox>
#include <QObject>
#include <QPushButton>
#include <QTimer>

namespace {

ToonzScene *currentScene() {
  TApp *app = TApp::instance();
  return app && app->getCurrentScene() ? app->getCurrentScene()->getScene()
                                       : nullptr;
}


// The reason shown by every editing tool in the storyboard's main xsheet.
// Level Strip editing (the filmstrip) is left alone: it edits a drawing that
// already lives in a shot, and it puts nothing in the main xsheet.
QString toolVeto() {
  if (!ZtoryMainGate::isStoryboardMain()) return QString();
  TApp *app = TApp::instance();
  if (app->getCurrentFrame() && app->getCurrentFrame()->isEditingLevel())
    return QString();
  return QObject::tr(
      "Main timeline: in a storyboard it holds only the shots and the "
      "audio.\nDouble-click a shot to draw or animate inside it.");
}

bool isDrawingLevelType(int type) {
  return type == PLI_XSHLEVEL || type == TZP_XSHLEVEL ||
         type == OVL_XSHLEVEL || type == MESH_XSHLEVEL;
}

// How many drawing columns were last seen in each scene's main xsheet: the
// warning comes once, and again only if more appear (a paste, a drop).
QMap<QString, int> &seenDrawingColumns() {
  static QMap<QString, int> seen;
  return seen;
}

}  // namespace

//-----------------------------------------------------------------------------

// A storyboard scene: Storyboard workflow and a scene that is not a shot, a
// character, a plain or an unclassified one (those keep their own sidecar and
// their sub-scenes are not shots, ztorymodel.h ShotDocumentState).
bool ZtoryMainGate::isStoryboardScene() {
  ZtoryModel *m = ZtoryModel::instance();
  return m->isStoryboardWorkflow() &&
         !m->shotDocumentState().keepsOwnSidecar();
}

//-----------------------------------------------------------------------------

QString ZtoryMainGate::currentShotName() {
  ToonzScene *scene = currentScene();
  if (!scene || !isStoryboardScene()) return QString();
  ChildStack *cs = scene->getChildStack();
  if (cs->getAncestorCount() == 0) return QString();
  // Depth 0 is the main xsheet: its column holding the shot gives the name.
  AncestorNode *node = cs->getAncestorInfo(0);
  if (!node || !node->m_xsheet) return QString();
  return QString::fromStdString(
      node->m_xsheet->getStageObject(TStageObjectId::ColumnId(node->m_col))
          ->getName());
}

//-----------------------------------------------------------------------------

bool ZtoryMainGate::isStoryboardMain() {
  ToonzScene *scene = currentScene();
  if (!scene || scene->getChildStack()->getAncestorCount() != 0) return false;
  return isStoryboardScene();
}

//-----------------------------------------------------------------------------

void ZtoryMainGate::install() {
  TTool::setEditVeto(&toolVeto);
}

//-----------------------------------------------------------------------------

bool ZtoryMainGate::refuseInMain(const QString &what) {
  if (!isStoryboardMain()) return false;
  DVGui::warning(
      QObject::tr("Cannot %1 in the main timeline.\n\nIn a storyboard the main "
                  "timeline holds only the shots and the audio. Double-click "
                  "a shot to work inside it.")
          .arg(what));
  return true;
}

//-----------------------------------------------------------------------------

bool ZtoryMainGate::refusePasteInMain(const QMimeData *mime) {
  if (!mime || !isStoryboardMain()) return false;
  bool drawings = dynamic_cast<const StrokesData *>(mime) ||
                  dynamic_cast<const RasterImageData *>(mime);
  if (const TCellData *cells = dynamic_cast<const TCellData *>(mime)) {
    for (int i = 0; i < cells->getCellCount() && !drawings; i++) {
      const TXshCell cell = cells->getCell(i);
      drawings = !cell.isEmpty() && cell.m_level &&
                 isDrawingLevelType(cell.m_level->getType());
    }
  }
  return drawings && refuseInMain(QObject::tr("paste drawings"));
}

//-----------------------------------------------------------------------------

bool ZtoryMainGate::undoIfDrawingColumnsPasted(const std::set<int> &indices) {
  if (indices.empty() || !isStoryboardMain()) return false;
  ToonzScene *scene = currentScene();
  TXsheet *xsh      = scene->getChildStack()->getTopXsheet();
  for (int c : indices) {
    TXshColumn *column = xsh->getColumn(c);
    TXshLevelColumn *lc = column ? column->getLevelColumn() : nullptr;
    if (!lc) continue;
    int r0, r1;
    lc->getRange(r0, r1);
    for (int r = r0; r <= r1; r++) {
      const TXshCell cell = lc->getCell(r);
      if (cell.isEmpty() || !cell.m_level ||
          !isDrawingLevelType(cell.m_level->getType()))
        continue;
      // The columns cannot be inspected before pasting (StageObjectsData has
      // no accessor): the paste is undone as a whole, then explained.
      TUndoManager::manager()->undo();
      refuseInMain(QObject::tr("paste drawing columns"));
      return true;
    }
  }
  return false;
}

//-----------------------------------------------------------------------------

QStringList ZtoryMainGate::drawingColumnsInMain() {
  QStringList names;
  ToonzScene *scene = currentScene();
  if (!scene) return names;
  TXsheet *xsh = scene->getChildStack()->getTopXsheet();
  if (!xsh) return names;
  for (int c = 0; c < xsh->getColumnCount(); c++) {
    TXshColumn *column = xsh->getColumn(c);
    if (!column || column->isEmpty()) continue;
    TXshLevelColumn *lc = column->getLevelColumn();
    if (!lc) continue;  // shots are child columns, audio sound columns
    int r0, r1;
    lc->getRange(r0, r1);
    for (int r = r0; r <= r1; r++) {
      TXshCell cell = lc->getCell(r);
      if (cell.isEmpty() || !cell.m_level) continue;
      if (isDrawingLevelType(cell.m_level->getType())) {
        names << QString::fromStdString(
            xsh->getStageObject(TStageObjectId::ColumnId(c))->getName());
        break;
      }
    }
  }
  return names;
}

//-----------------------------------------------------------------------------

void ZtoryMainGate::checkMainForDrawingLevels() {
  ToonzScene *scene = currentScene();
  if (!scene || !ZtoryMainGate::isStoryboardScene()) return;
  const QString key   = scene->getScenePath().getQString();
  const QStringList c = drawingColumnsInMain();
  int &seen           = seenDrawingColumns()[key];
  if (c.size() <= seen) {
    seen = c.size();  // some were removed: fine, remember the new count
    return;
  }
  // Another box is up (a load warning, the scene type question): after it.
  if (QApplication::activeModalWidget()) {
    QTimer::singleShot(1000, qApp, [] { checkMainForDrawingLevels(); });
    return;
  }
  seen = c.size();

  QMessageBox box(QApplication::activeWindow());
  box.setIcon(QMessageBox::Warning);
  box.setWindowTitle(QObject::tr("Drawing levels in the main timeline"));
  box.setText(
      QObject::tr("This storyboard has drawing levels in the main timeline "
                  "(column: %1).")
          .arg(c.join(", ")));
  box.setInformativeText(QObject::tr(
      "In storyboard mode the main timeline can hold only the shots and the "
      "audio: drawing levels there are not handled, and drawing on them is "
      "disabled.\n\nIf this scene is not a storyboard, change the workflow "
      "and reclassify the scene as a shot, a character or a plain scene "
      "(Scene > Scene Type…).\nIf it is a storyboard, move those drawings "
      "inside a shot."));
  QPushButton *typeBtn =
      box.addButton(QObject::tr("Scene Type…"), QMessageBox::ActionRole);
  box.addButton(QMessageBox::Ok);
  box.exec();
  if (box.clickedButton() == typeBtn) ZtorySceneClass::chooseForCurrentScene();
}
