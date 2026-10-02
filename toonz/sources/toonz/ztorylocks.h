#pragma once

#ifndef ZTORYLOCKS_H
#define ZTORYLOCKS_H

// Lock files shared by the Ztoryc instances running on this computer
// (2026-10-02, when more than one instance became allowed).
//
// They live in the local cache, NEVER next to the file they guard: projects and
// scenes sit on Google Drive, and a lock there would sync to the other machines
// and block them.

#include <QString>

namespace ZtoryLocks {

// `kind` keeps unrelated locks on the same file apart ("ztrack", "kitsusync",
// "scene"). The file name comes from a hash of the canonical path.
QString lockFilePath(const QString &kind, const QString &forPath);

// Canonical when the file exists (symlinks resolved), absolute otherwise.
QString canonicalPath(const QString &path);

// This instance holds its own lock for as long as it runs (call once, early).
void registerInstance();

// True when another Ztoryc instance on this computer is running. A lock left
// by a crashed instance is recognised (its process is gone) and removed.
bool otherInstancesRunning();

}  // namespace ZtoryLocks

#endif
