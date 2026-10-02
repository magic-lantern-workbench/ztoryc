#include "ztorylocks.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace ZtoryLocks {

namespace {
// Where every lock lives: the per-user cache, or a folder in /tmp when the
// system gives none. Created on first use.
QString locksFolder() {
  const QString cache =
      QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
  const QString base = !cache.isEmpty() ? cache : QDir::tempPath() + "/ztoryc";
  const QString folder = base + "/locks";
  QDir().mkpath(folder);
  return folder;
}
}  // namespace

// The same file reached through a symlink, a relative path or "..", gives the
// same string — so two instances agree on one lock. A file that does not
// exist yet has no canonical form: its absolute path stands in.
QString canonicalPath(const QString &path) {
  if (path.isEmpty()) return QString();
  QFileInfo info(path);
  if (info.exists()) return info.canonicalFilePath();
  return info.absoluteFilePath();
}

// One lock per (kind, file): the name carries a short digest of the file's
// canonical path, so any path length or character set fits in a file name.
QString lockFilePath(const QString &kind, const QString &forPath) {
  const QByteArray digest =
      QCryptographicHash::hash(canonicalPath(forPath).toUtf8(),
                               QCryptographicHash::Sha1)
          .toHex();
  return QString("%1/%2_%3.lock")
      .arg(locksFolder(), kind, QString::fromLatin1(digest.left(16)));
}

}  // namespace ZtoryLocks
