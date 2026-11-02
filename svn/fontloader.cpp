#include "fontloader.h"
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFontDatabase>

namespace {
QStringList loaded;
}

QStringList FontLoader::load(const QString &directory) {
    loaded.clear();
    if (!QDir(directory).exists()) return loaded;
    QStringList files;
    QDirIterator it(directory, {"*.ttf", "*.otf"}, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) files.append(it.next());
    files.sort();
    for (const QString &path : files) {
        const int id = QFontDatabase::addApplicationFont(path);
        if (id < 0) {
            qWarning() << "No se pudo cargar la fuente" << path;
            continue;
        }
        for (const QString &family : QFontDatabase::applicationFontFamilies(id))
            if (!loaded.contains(family)) loaded.append(family);
    }
    return loaded;
}

QStringList FontLoader::loadedFamilies() {
    return loaded;
}
