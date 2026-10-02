#include "ztorylocks.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QLockFile>
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

namespace {
QString instanceLockName(qint64 pid) {
  return QString("instance_%1.lock").arg(pid);
}
QLockFile *s_instanceLock = nullptr;  // held until the process ends
}  // namespace

void registerInstance() {
  if (s_instanceLock) return;
  s_instanceLock = new QLockFile(
      locksFolder() + "/" + instanceLockName(QCoreApplication::applicationPid()));
  s_instanceLock->setStaleLockTime(0);  // stale only when its process is gone
  s_instanceLock->tryLock(0);
}

bool otherInstancesRunning() {
  const qint64 me = QCoreApplication::applicationPid();
  const QDir dir(locksFolder());
  for (const QString &name :
       dir.entryList({"instance_*.lock"}, QDir::Files)) {
    if (name == instanceLockName(me)) continue;
    QLockFile other(dir.filePath(name));
    other.setStaleLockTime(0);
    // Taking it means its owner is gone: a crashed instance's leftover,
    // removed by unlock(). Failing to take it means that instance is alive.
    if (other.tryLock(0))
      other.unlock();
    else
      return true;
  }
  return false;
}

}  // namespace ZtoryLocks
