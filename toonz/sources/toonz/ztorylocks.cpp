#include "ztorylocks.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace ZtoryLocks {

QString canonicalPath(const QString &path) {
  if (path.isEmpty()) return path;
  const QFileInfo fi(path);
  const QString c = fi.canonicalFilePath();
  return c.isEmpty() ? fi.absoluteFilePath() : c;
}

QString lockFilePath(const QString &kind, const QString &forPath) {
  QString dir =
      QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
  if (dir.isEmpty()) dir = QDir::tempPath() + "/ztoryc";
  dir += "/locks";
  QDir().mkpath(dir);
  const QByteArray hash =
      QCryptographicHash::hash(canonicalPath(forPath).toUtf8(),
                               QCryptographicHash::Sha1)
          .toHex()
          .left(16);
  return dir + "/" + kind + "_" + QString::fromLatin1(hash) + ".lock";
}

}  // namespace ZtoryLocks
