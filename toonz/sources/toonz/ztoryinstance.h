#pragma once

#include <QString>

// Opens another instance of Ztoryc, optionally on a scene.
//
// On macOS the system keeps ONE instance per application: clicking the Dock
// icon, relaunching from the Finder or double-clicking a .tnz all bring the
// running copy forward. A second one needs `open -n` — which no animator types
// in a terminal — so the app does it. On Windows and Linux the executable is
// simply launched again. The scene path is passed as the positional argument
// main() already reads.
void ztoryOpenInNewInstance(const QString &scenePath = QString());
