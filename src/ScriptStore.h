#pragma once
#include <QList>
#include <QString>

class QWidget;

// Loose GSC scripts: where Plutonium loads them from, and installing,
// listing and removing them. Games are storage ids: t4, t5, t6, iw5.
namespace ScriptStore
{

// The folder Plutonium loads <game>'s loose scripts from for one mode
// ("zm" or "mp"). With modFolder set (T6 only), the scripts folder inside
// that mod, so the script only loads with the mod. Empty when unknown.
QString folder(const QString &plutoniumRoot, const QString &game,
               const QString &mode, const QString &modFolder = QString());

struct Installed
{
    QString path;  // absolute
    QString label; // relative to storage/<game>, e.g. "scripts/zm/health_bar.gsc"
};

// Every loose script for <game>, mod folders excluded.
QList<Installed> installed(const QString &plutoniumRoot, const QString &game);

// Writes <name> into <dir>, creating it. Returns an error, empty on success.
QString write(const QString &dir, const QString &name, const QByteArray &bytes);

// Moves the script to the Recycle Bin. Returns an error, empty on success.
QString remove(const QString &path);

bool isScriptFile(const QString &path);

// Asks which game and mode a local .gsc is for, then copies it there.
// Returns the installed path, or empty if cancelled or failed (after
// telling the user why).
QString importWithDialog(QWidget *parent, const QString &plutoniumRoot,
                         const QString &file, const QString &defaultGame);

} // namespace ScriptStore
