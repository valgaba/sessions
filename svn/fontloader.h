#pragma once
#include <QString>
#include <QStringList>

// Registers the font files stored next to the executable so designs keep their
// typography on computers where those fonts are not installed. The fonts stay
// private to Sessions: nothing is written to the Windows font directory.
namespace FontLoader {
QStringList load(const QString &directory);
QStringList loadedFamilies();
}
