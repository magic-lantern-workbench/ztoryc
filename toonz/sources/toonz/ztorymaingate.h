#pragma once

// The storyboard's main xsheet holds only the SHOTS (one sub-scene column per
// shot) and the audio.  Drawings, levels, imported assets and loaded scenes
// belong inside a shot.  Nothing used to enforce it: a user who added a vector
// level to the main xsheet and drew there got a scrambled frame sequence,
// strokes that did not show and no onion skin (Discord report, 2026-10-17).
//
// Rules (Franco, 2026-10-18):
//  - in a storyboard's main xsheet every editing tool is off (drawing tools
//    and the Animate tool); generic tools (zoom, hand, rotate view) still work;
//  - creating or loading a level there is refused — no "create it in the shot
//    instead": it is simply blocked, with the reason;
//  - loading resources there (levels, images, assets, scenes as sub-scenes)
//    is refused too; audio is the one thing the main xsheet takes;
//  - a storyboard whose main xsheet already holds drawing levels gets a
//    warning: in storyboard mode they cannot be there, so if the scene is not
//    a storyboard, change workflow and reclassify it (shot, character, plain).
//    With the blocks in place, a drawing level in the main xsheet means the
//    scene really is a storyboard that predates them.
//
// Only storyboard scenes are concerned: plain, shot and character scenes, and
// Ztoryc used as a plain Tahoma2D, keep the usual behaviour.

#include <QString>
#include <QStringList>
#include <set>

class QMimeData;

namespace ZtoryMainGate {

// True when the current scene is a storyboard (Storyboard workflow, not a
// shot, character, plain or unclassified scene).
bool isStoryboardScene();

// True when the current scene is a storyboard and its main xsheet is current
// (not inside a shot).
bool isStoryboardMain();

// Inside a shot of a storyboard: the shot's name (its column in the main
// xsheet).  Empty in the main xsheet and in any other kind of scene.
QString currentShotName();

// Where you are, shown on every surface you work in (Franco, 2026-10-18):
// the accent colour inside a shot, neutral in the main timeline.
static const char *const kInsideShotColor = "#F5B800";  // Ztoryc yellow

// Installs the tool veto (TTool::setEditVeto) and the checks on scene load.
// Call once, at start-up.
void install();

// To call before creating or loading something into the current xsheet: when
// it would land in a storyboard's main xsheet, shows why it is refused and
// returns true.  \p what names the action for the message ("create a level").
bool refuseInMain(const QString &what);

// To call before pasting cells: when the clipboard holds drawings (cells of
// drawing levels, strokes, raster images) and the target is a storyboard's
// main xsheet, shows why it is refused and returns true.
bool refusePasteInMain(const QMimeData *mime);

// To call after pasting columns at \p indices: if drawing-level columns
// landed in a storyboard's main xsheet, the paste is undone (it must be the
// last undo block) with the reason; returns true in that case.
bool undoIfDrawingColumnsPasted(const std::set<int> &indices);

// The drawing-level columns of the current scene's main xsheet, by name.
QStringList drawingColumnsInMain();

// Warns if the storyboard's main xsheet holds drawing levels (once per scene
// and session, again only if more appear).
void checkMainForDrawingLevels();

}  // namespace ZtoryMainGate
