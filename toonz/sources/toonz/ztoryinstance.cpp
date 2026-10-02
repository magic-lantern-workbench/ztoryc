#include "ztoryinstance.h"

#include <QCoreApplication>
#include <QDir>
#include <QProcess>
#include <QStringList>

void ztoryOpenInNewInstance(const QString &scenePath) {
#ifdef Q_OS_MACOS
  // applicationDirPath() is <bundle>/Contents/MacOS.
  QDir bundle(QCoreApplication::applicationDirPath());
  bundle.cdUp();
  bundle.cdUp();
  QStringList args{"-n", "-a", bundle.absolutePath()};
  if (!scenePath.isEmpty()) args << "--args" << scenePath;
  QProcess::startDetached("open", args);
#else
  QStringList args;
  if (!scenePath.isEmpty()) args << scenePath;
  QProcess::startDetached(QCoreApplication::applicationFilePath(), args);
#endif
}
