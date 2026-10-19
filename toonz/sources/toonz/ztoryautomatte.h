#pragma once

// Auto-matte: a solid silhouette of a drawing column, on a level of its own in
// the column just below, so the character hides the background behind it
// (Storyboard Pro's «Generate Auto-Matte»; ANIMATIC_TASKS, 13/10 and 17/10).
//
// The matte is a Toonz Raster level with one style: its colour can be changed
// later from the level palette.  The matte column is parented to the drawing
// column, so it follows every move of it.  «Update Auto-Matte» recomputes the
// frames after the drawing has been changed.  Each command is one undo.
//
// Sources: Vector (the main case), Toonz Raster and Raster levels.

class QMenu;
class QString;
class TXsheet;

namespace ZtoryAutoMatte {

// Adds «Generate Auto-Matte…» or «Update Auto-Matte» to the column menu
// when the column holds a drawing level.
void addColumnActions(QMenu *menu, TXsheet *xsh, int col);

// Test mode (ZTORY_SCENETEST «automatte:<col>:<gap>»): generates with no
// dialog, white, and returns the matte level name ("" on failure).
QString generateForTest(TXsheet *xsh, int col, int gap);

}  // namespace ZtoryAutoMatte
