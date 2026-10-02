#pragma once

#ifndef ZTRACKMERGE_H
#define ZTRACKMERGE_H

// Three-way merge of production.ztrack (2026-10-02).
//
// More than one Ztoryc can now run at once. Each keeps the tracker in memory
// and rewrites the whole file on every save, so without this the last one to
// save wipes what the others did. Before writing, the model merges:
//   base   = the file as this instance last read or wrote it,
//   ours   = what this instance has in memory now,
//   theirs = what is on disk now (another instance, or Drive).
//
// Items are matched by identity, not position: assets and shots by uuid, tasks
// by type, people and techniques by name. Per item and per attribute, the side
// that changed it wins; when both changed the same attribute, ours wins and the
// clash is listed in `conflicts`. A deletion loses against a change made on the
// other side (the item stays). Items added on either side are all kept.
//
// Works on the XML, not on the model, so a field added to the format later is
// merged without touching this file.

#include <QByteArray>
#include <QStringList>

namespace ZtrackMerge {

// Empty result and *ok = false when any of the three does not parse.
QByteArray merge(const QByteArray &base, const QByteArray &ours,
                 const QByteArray &theirs, QStringList *conflicts = nullptr,
                 bool *ok = nullptr);

// Same content, ignoring formatting and attribute order. False if either side
// does not parse.
bool sameContent(const QByteArray &a, const QByteArray &b);

bool isWellFormed(const QByteArray &xml);

}  // namespace ZtrackMerge

#endif
