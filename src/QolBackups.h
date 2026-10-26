#pragma once
#include <QDateTime>
#include <QString>
#include <QStringList>
#include <functional>

class AppSettings;

// Backups of the player's own files, per game: textures, sounds, scripts,
// settings and mods for each Plutonium game, plus Plutonium's own ReShade
// setup.
//
// They live in plain sight at storage\<game>\backups\<kind>\, one folder
// per kind. Black Ops II's layout is exactly the one qol-installer.ps1 and the
// 1.x app used, so a backup taken by either of those is found and restored
// here. They are the player's files, so they must stay findable and copyable
// by hand.
//
// A backup keeps the OLDER copy unless it is replaced on purpose. The older
// one is the one taken before this app first wrote anything, and that is the
// one worth having; an automatic backup before a later install would otherwise
// capture our own files and throw the player's away.
namespace QolBackups
{
    struct Info {
        QString kind;       // see kinds()
        QString game;       // t6 t5 t4 iw5 plutonium
        QString label;      // "My textures"
        QString title;      // "your textures"
        QString desc;       // what it covers, for the page
        QString folder;     // storage\<game>\backups\<kind>
        bool exists = false;
        int files = 0;
        qint64 bytes = 0;
        QDateTime when;
        int liveFiles = 0;  // what a backup taken now would copy
        qint64 liveBytes = 0;
    };
    using Progress = std::function<void(const QString &text, int percent)>;

    // Games that have backups, in page order. "plutonium" holds what every
    // game shares (ReShade); its backup sits with Black Ops II's on disk.
    QStringList games();
    // Kind ids. Black Ops II keeps the bare names every earlier version used
    // ("images"), so its old backups are still found; the other games use
    // "<game>/<kind>" ("t5/images"). An empty game lists every kind.
    QStringList kinds(const QString &game = QString());
    // What "Back up all" and every install for that game take. Leaves out
    // "mod" (our own mod, which can be downloaded again) and "mods" (every
    // other mod and custom map, which can run to many gigabytes); both are
    // there to take by hand.
    QStringList automaticKinds(const QString &game = QStringLiteral("t6"));
    QString kindFor(const QString &game, const QString &base); // ("t5", "images") -> "t5/images"
    QString gameOf(const QString &kind);
    QString root(const AppSettings &s);          // <pluto>/storage/t6/backups
    QString folder(const AppSettings &s, const QString &kind);
    bool exists(const AppSettings &s, const QString &kind);
    // `measure` walks the live folder and the backup to count files. Off, it
    // returns the names and whether a backup exists, for building the page.
    Info info(const AppSettings &s, const QString &kind, bool measure = true);

    enum class Result { Saved, KeptOlder, NothingToSave, Failed };
    // `replace` overwrites an existing backup; without it the older one is
    // kept. Written beside the old one and swapped in only once complete, so a
    // failed copy never costs the backup that was already there.
    Result backup(const AppSettings &s, const QString &kind, bool replace,
                  const Progress &p, QString &message);

    // What every install calls before it writes. `required` are the kinds the
    // install is about to overwrite: if one of those cannot be backed up, the
    // install must not go ahead. `extra` are backed up while we are at it and
    // skipped quietly when they cannot be (no room, a locked file).
    bool backupBeforeInstall(const AppSettings &s, const QStringList &required,
                             const QStringList &extra, const Progress &p, QString &error);

    // Puts a backup back over what is there. Nothing added since is deleted.
    // Files that come back are the player's again, so they are dropped from
    // the install manifests - otherwise the next Remove would delete them.
    // `only`, when given, limits the restore to those paths (relative, as
    // an install manifest lists them), so removing one pack puts back the
    // files it replaced and leaves another pack's files alone.
    bool restore(const AppSettings &s, const QString &kind, const Progress &p, QString &error,
                 const QStringList &only = {});
    // Deletes one backup. The files on the PC are not touched.
    bool remove(const AppSettings &s, const QString &kind, QString &error);

    // r_aaSamples above 8 stops the game starting (E_INVALIDARG at the renderer
    // restart as the mod loads). Old builds of the mod saved 16. Takes it back
    // to 4 in the live mod config and in the settings backup, so neither an
    // install nor a restore hands the player back a game that will not start.
    // Returns how many files were changed.
    int repairAaSamples(const AppSettings &s);
}
