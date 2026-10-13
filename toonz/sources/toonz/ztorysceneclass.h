#pragma once

// What a scene IS for Ztoryc: storyboard, shot, character or plain.
//
// The role lives in the scene's sidecar (.ztoryc, root attribute "role").  A
// scene with no sidecar used to be taken for a storyboard: its sub-scenes
// became shots, their columns got shot names and the Board published them into
// the Production Tracker.  Now a saved scene with no sidecar is UNCLASSIFIED —
// treated as plain (nothing interpreted, nothing written) — and the user is
// asked what it is the first time it is opened (Franco, 2026-10-11).
//
// The role is a declaration, so it is written at once, as a character scene's
// always was (startuppopup.cpp) and a shot's at export: a scene classified and
// not yet saved must not reopen as something else.  The storyboard's CONTENT
// (shots, texts) still reaches the file only with ⌘S (step 3b).

#include <QString>
#include <QMap>

class QWidget;

namespace ZtorySceneClass {

// The sidecar next to a scene (.tnz → .ztoryc).
QString sidecarFor(const QString &tnzPath);
// Whether a scene (or its sidecar) lives in the folder of the current
// project, whose Production Tracker is the one a save writes to.
bool isInCurrentProject(const QString &path);

// role="plain": a scene of the project that is not a storyboard.  \p workflow
// is the room set it opens in ("Tradigital", "Cutout", "StopMotion"; the
// names Preferences::getCurrentRoomChoice() uses), empty for none.
bool declarePlain(const QString &tnzPath, const QString &workflow,
                  QString *error = nullptr);
// The workflow a plain scene keeps in its sidecar; empty if none.
QString plainWorkflowOf(const QString &tnzPath);
// The workflow command for a room-set name (MI_Workflow2D…); empty if unknown.
QString workflowCommandForRoom(const QString &room);
// In Auto, after a scene is opened: a plain scene goes to its workflow (a
// Board does it for storyboards and shots, the start-up popup for the rest).
void followPlainWorkflow();
// role="storyboard" and the Production Tracker answer; the shots come with ⌘S.
bool declareStoryboard(const QString &tnzPath, bool trackerOff,
                       QString *error = nullptr);
// role="shot", linked to the project shot \p shotUuid of \p projectDb — the
// same file the storyboard export writes.
bool declareShot(const QString &tnzPath, const QString &shotUuid,
                 const QString &projectDb, QString *error = nullptr);
// Shared with the export (StoryboardPanel): the shot back-link sidecar.
bool writeShotLink(const QString &sidecarPath, const QString &shotUuid,
                   const QString &projectDb, const QString &technique);

// Project shots that already have a scene: shot uuid → that scene's .tnz.
// Read from the sidecars under the project folder, the only place the link
// is written.
QMap<QString, QString> linkedShotScenes(const QString &projectDb);

// After a scene is opened: ask what it is, if nobody has said.
void askIfUnclassified();
// The Board was edited in a scene nobody classified: what is typed there is
// not saved until it is a storyboard.  Asked again (even if declined), once
// per scene and session.
void askBecauseEdited();
// The dialog for any scene (the open one, or one in the start-up list).
// True if its type changed; the open scene is then read again.
bool chooseFor(const QString &tnzPath, QWidget *parent);
// Scene > Scene Type…: classify (or reclassify) the current scene.
void chooseForCurrentScene();

}  // namespace ZtorySceneClass
