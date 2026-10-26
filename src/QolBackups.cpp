#include "QolBackups.h"
#include "AppSettings.h"
#include "QolService.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QMap>
#include <QRegularExpression>
#include <QSet>
#include <QStorageInfo>
#include <QTextStream>

namespace {

// One part of a backup: a whole folder, or a named handful of loose files
// (plus anything matching `glob`) from one folder. Each part lands in its own
// named subfolder so a restore knows exactly where it goes, and a person
// reading the tree can tell too.
struct Part {
    QString sub;             // subfolder inside backups\<kind>
    QString path;            // where the files live
    bool folder = false;     // true: everything under path, recursively
    QStringList items;       // folder == false: these names
    QString glob;            // folder == false: and these; folder == true: only these
    QStringList skip;        // folder == true: top-level entries left out
    QStringList notOurs;     // install manifests whose files are ours, not the player's
    QStringList forget;      // more manifests a restore takes its files out of
};

struct Set {
    QString label, title, desc;
    QList<Part> parts;
};

// The glyphs any controller pack can replace. The icon backup takes only
// these, not the whole images folder: the textures backup already covers that
// folder whole, and two copies of the same gigabyte would help nobody.
const QStringList kIconNames = {
    "xenon_controller_top.iwi",
    "xenonbutton_a.iwi", "xenonbutton_b.iwi", "xenonbutton_x.iwi", "xenonbutton_y.iwi",
    "xenonbutton_back.iwi", "xenonbutton_start.iwi", "xenonbutton_lb.iwi", "xenonbutton_rb.iwi",
    "xenonbutton_lt.iwi", "xenonbutton_rt.iwi", "xenonbutton_ls.iwi", "xenonbutton_rs.iwi",
    "xenonbutton_dpad_all.iwi", "xenonbutton_dpad_up.iwi", "xenonbutton_dpad_down.iwi",
    "xenonbutton_dpad_left.iwi", "xenonbutton_dpad_right.iwi", "xenonbutton_dpad_ud.iwi",
    "xenonbutton_dpad_rl.iwi", "hud_dpad_blood.iwi"};

const QStringList kReShadeNames = {"ReShade.ini", "Cinematic Colour Grading.ini", "BO2.ini",
                                   "BO1.ini", "MW3.ini", "WAW.ini", "dxgi.dll"};

// Settings, never stats. The game writes its stats into the same folders, and
// this app does not own anyone's progress, so a settings backup names what it
// saves rather than taking the folder. That is also what lets repairAaSamples
// reach into the backup copy of plutonium_zm.cfg.
const QStringList kModSettings = {"plutonium_zm.cfg", "bindings_zm.bdg", "hardware_zm.chp",
                                  "user_zm.cgp", "user_common.cgp"};
const QStringList kGameSettings = {"plutonium_zm.cfg", "plutonium_mp.cfg", "bindings_zm.bdg",
                                   "bindings_mp.bdg", "hardware_zm.chp", "hardware_mp.chp",
                                   "user_zm.cgp", "user_mp.cgp", "user_common.cgp"};

QString t6(const AppSettings &s) { return QolService::t6Storage(s); }
QString under(const QString &root, const QString &rel) { return QDir::cleanPath(QDir(root).filePath(rel)); }
// "storage\t5\images", the way the page names a folder.
QString shown(const QString &game, const QString &sub)
{
    return QStringLiteral("storage\\%1\\%2").arg(game, sub);
}

// Every game's backups, keyed by kind id (see QolBackups::kinds).
QMap<QString, Set> sets(const AppSettings &s)
{
    QMap<QString, Set> m;
    for (const QString &g : {QStringLiteral("t6"), QStringLiteral("t5"), QStringLiteral("t4"), QStringLiteral("iw5")}) {
        const QString storage = QolService::gameStorage(s, g);
        const auto kind = [&g](const QString &base) { return QolBackups::kindFor(g, base); };
        const bool t6 = g == QLatin1String("t6");
        // Texture and sound packs are World at War, Black Ops and Black Ops II
        // only. Both backups leave out the files a pack put there: those can
        // be downloaded again any time, and a restore should hand back exactly
        // what was the player's.
        if (g != QLatin1String("iw5")) {
            m[kind("images")] = {QObject::tr("My textures"), QObject::tr("your textures"),
                                 QObject::tr("Everything in %1 that this app did not put there. "
                                             "Texture packs install here.").arg(shown(g, "images")),
                                 {{"images", under(storage, "images"), true, {}, {}, {},
                                   t6 ? QStringList{"images", "controller"} : QStringList{"images"}}}};
            m[kind("zone")] = {QObject::tr("My sounds"), QObject::tr("your sounds"),
                               QObject::tr("Everything in %1 that this app did not put there. "
                                           "Sound packs install here.").arg(shown(g, "zone")),
                               {{"zone", under(storage, "zone"), true, {}, {}, {}, {"zone"}}}};
        }
        // Both places Plutonium loads loose scripts from: scripts\ (GSC mods
        // dropped in by hand) and raw\ (scripts, menus and maps, where Zombies
        // Declassified and other script packs install). The game runs them on
        // every map whatever mod is picked, which is exactly why they need to
        // be easy to park and put back.
        m[kind("scripts")] = {QObject::tr("My scripts"), QObject::tr("your scripts"),
                              QObject::tr("Everything in %1 and %2: loose GSC scripts, menus and maps.")
                                  .arg(shown(g, "scripts"), shown(g, "raw")),
                              {{"scripts", under(storage, "scripts"), true},
                               {"raw", under(storage, "raw"), true}}};
        // Every other mod. Can run to gigabytes, so it is only ever backed up
        // when asked, never before an install.
        if (t6)
            m[kind("mods")] = {QObject::tr("My other mods and maps"), QObject::tr("your other mods and maps"),
                               QObject::tr("Every mod in storage\\t6\\mods except Quality of Life, and every custom "
                                           "map in storage\\t6\\usermaps. Can be large, so it is never automatic."),
                               {{"mods", under(storage, "mods"), true, {}, {}, {"zm_qol"}},
                                {"usermaps", under(storage, "usermaps"), true}}};
        else
            m[kind("mods")] = {QObject::tr("My mods"), QObject::tr("your mods"),
                               QObject::tr("Every mod in %1. Can be large, so it is never automatic.")
                                   .arg(shown(g, "mods")),
                               {{"mods", under(storage, "mods"), true}}};
    }

    const QString storage = t6(s);
    m["controller"] = {QObject::tr("My controller icons"), QObject::tr("your controller icons"),
                       QObject::tr("The button-prompt images a controller icon pack replaces."),
                       {{"controller", under(storage, "images"), false, kIconNames, {}, {}, {"controller"},
                         {"images"}}}};
    m["mod"] = {QObject::tr("Quality of Life + my settings"), QObject::tr("Quality of Life and its settings"),
                QObject::tr("The Quality of Life mod folder and your saved menu settings for it."),
                {{"files", under(storage, "mods/zm_qol"), true},
                 {"settings", under(storage, "players/mods/zm_qol"), false, kModSettings}}};
    m["settings"] = {QObject::tr("My game settings"), QObject::tr("your game settings"),
                     QObject::tr("Plutonium's configs, key bindings and graphics settings for Zombies "
                                 "and Multiplayer, and your Quality of Life menu settings. Never your stats."),
                     {{"players", under(storage, "players"), false, kGameSettings},
                      {"mod-settings", under(storage, "players/mods/zm_qol"), false, kModSettings}}};
    // Black Ops keeps its configs loose in players\; World at War keeps one
    // set per profile under players\profiles\<name>\. Both write stats next
    // to them in files that are not .cfg.
    m["t5/settings"] = {QObject::tr("My game settings"), QObject::tr("your game settings"),
                        QObject::tr("The config files in storage\\t5\\players. Never your stats."),
                        {{"players", under(QolService::gameStorage(s, "t5"), "players"), false, {}, "*.cfg"}}};
    m["t4/settings"] = {QObject::tr("My game settings"), QObject::tr("your game settings"),
                        QObject::tr("Every profile's config files under storage\\t4\\players. Never your stats."),
                        {{"players", under(QolService::gameStorage(s, "t4"), "players"), true, {}, "*.cfg"}}};
    // Glob *.ini catches a preset saved under the player's own name, which the
    // fixed list cannot know in advance. ReShade is backed up before it is
    // installed, so nothing here is left out as ours, but a restore still
    // takes the files it puts back out of the install manifest.
    const QString bin = under(s.plutoniumInstance, "bin");
    m["reshade"] = {QObject::tr("My ReShade setup"), QObject::tr("your ReShade setup"),
                    QObject::tr("ReShade's dxgi.dll, every preset .ini and the reshade-shaders folder "
                                "in Plutonium's bin."),
                    {{"bin", bin, false, kReShadeNames, "*.ini", {}, {}, {"reshade"}},
                     {"reshade-shaders", under(bin, "reshade-shaders"), true}}};
    return m;
}

// The install manifests named `base` for one game: the app's own pack
// (installed-images.txt) and each Workshop pack (installed-images-<id>.txt).
QStringList manifestFiles(const AppSettings &s, const QString &game, const QString &base)
{
    const QDir state(QolService::stateDir(s, game));
    QStringList out;
    const QString own = state.filePath(QStringLiteral("installed-%1.txt").arg(base));
    if (QFileInfo::exists(own))
        out << own;
    for (const QString &n : state.entryList({QStringLiteral("installed-%1-*.txt").arg(base)}, QDir::Files))
        out << state.filePath(n);
    return out;
}

QStringList readLines(const QString &path)
{
    QStringList out;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return out;
    QTextStream in(&f);
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (!line.isEmpty())
            out << QDir::fromNativeSeparators(line);
    }
    return out;
}

QSet<QString> oursIn(const AppSettings &s, const QString &game, const Part &part)
{
    QSet<QString> out;
    for (const QString &k : part.notOurs)
        for (const QString &mf : manifestFiles(s, game, k))
            for (const QString &rel : readLines(mf))
                out.insert(rel.toLower());
    return out;
}

struct FileRef { QString path; QString rel; qint64 size = 0; };

QList<FileRef> partFiles(const AppSettings &s, const QString &game, const Part &part)
{
    QList<FileRef> out;
    const QSet<QString> ours = oursIn(s, game, part);
    if (part.folder) {
        const QDir dir(part.path);
        if (!dir.exists())
            return out;
        QDirIterator it(part.path, QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            const QString rel = dir.relativeFilePath(it.filePath());
            if (!part.skip.isEmpty()
                && part.skip.contains(rel.section(QLatin1Char('/'), 0, 0), Qt::CaseInsensitive))
                continue;
            if (!part.glob.isEmpty() && !QDir::match(part.glob, it.fileName()))
                continue;
            if (ours.contains(rel.toLower()))
                continue;
            out << FileRef{it.filePath(), rel, it.fileInfo().size()};
        }
        return out;
    }
    QSet<QString> seen;
    auto take = [&](const QFileInfo &fi) {
        const QString key = fi.fileName().toLower();
        if (!fi.isFile() || seen.contains(key) || ours.contains(key))
            return;
        seen.insert(key);
        out << FileRef{fi.filePath(), fi.fileName(), fi.size()};
    };
    for (const QString &n : part.items)
        take(QFileInfo(QDir(part.path).filePath(n)));
    if (!part.glob.isEmpty())
        for (const QFileInfo &fi : QDir(part.path).entryInfoList({part.glob}, QDir::Files))
            take(fi);
    return out;
}

void appendLog(const AppSettings &s, const QString &text)
{
    const QString dir = QolService::stateDir(s);
    QDir().mkpath(dir);
    QFile f(QDir(dir).filePath(QStringLiteral("installer.log")));
    if (f.open(QIODevice::Append | QIODevice::Text))
        QTextStream(&f) << QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"))
                        << "  info   " << text << '\n';
}

QString fmtSize(qint64 b)
{
    if (b >= 1000LL * 1000 * 1000) return QStringLiteral("%1 GB").arg(b / 1e9, 0, 'f', 1);
    if (b >= 1000 * 1000)          return QStringLiteral("%1 MB").arg(b / 1e6, 0, 'f', 0);
    if (b >= 1000)                 return QStringLiteral("%1 KB").arg(b / 1e3, 0, 'f', 0);
    return QStringLiteral("%1 bytes").arg(b);
}

QDateTime takenAt(const QString &folder)
{
    // Birth time of the backup folder, which is when the backup was made.
    // Copied files keep their own dates, so they cannot say when.
    const QFileInfo fi(folder);
    return fi.birthTime().isValid() ? fi.birthTime() : fi.lastModified();
}

bool copyFile(const QString &from, const QString &to, QString &error)
{
    QDir().mkpath(QFileInfo(to).absolutePath());
    if (QFileInfo::exists(to)) {
        QFile::setPermissions(to, QFile::permissions(to) | QFileDevice::WriteOwner);
        if (!QFile::remove(to)) {
            error = QObject::tr("%1 is in use. Close Plutonium and try again.").arg(QDir::toNativeSeparators(to));
            return false;
        }
    }
    if (!QFile::copy(from, to)) {
        error = QObject::tr("Could not copy %1 to %2").arg(QDir::toNativeSeparators(from), QDir::toNativeSeparators(to));
        return false;
    }
    return true;
}

// Drops restored paths from an install manifest. A file that came back from a
// backup is the player's again, and the manifest is the list a Remove deletes,
// so leaving it there would delete their file on the next Remove. A manifest
// left with nothing in it means nothing of that pack remains.
void forgetFromManifest(const AppSettings &s, const QString &game, const QString &base,
                        const QStringList &rels)
{
    if (rels.isEmpty())
        return;
    QSet<QString> back;
    for (const QString &r : rels)
        back.insert(r.toLower());
    for (const QString &path : manifestFiles(s, game, base)) {
        const QStringList before = readLines(path);
        QStringList keep;
        for (const QString &line : before)
            if (!back.contains(line.toLower()))
                keep << line;
        if (keep.size() == before.size())
            continue;
        if (keep.isEmpty()) {
            QFile::remove(path);
            if (base == QLatin1String("controller"))
                QFile::remove(QDir(QolService::stateDir(s)).filePath(QStringLiteral("controller-pack.txt")));
            continue;
        }
        QFile f(path);
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            QTextStream out(&f);
            for (const QString &k : keep)
                out << k << '\n';
        }
    }
}

} // namespace

namespace QolBackups {

QStringList games()
{
    return {"t6", "t5", "t4", "iw5", "plutonium"};
}

QStringList kinds(const QString &game)
{
    if (game.isEmpty()) {
        QStringList all;
        for (const QString &g : games())
            all << kinds(g);
        return all;
    }
    // Page order: the ones an install writes over first.
    if (game == QLatin1String("t6"))
        return {"images", "zone", "controller", "scripts", "settings", "mod", "mods"};
    if (game == QLatin1String("plutonium"))
        return {"reshade"};
    QStringList bases;
    if (game == QLatin1String("t5") || game == QLatin1String("t4"))
        bases = {"images", "zone", "scripts", "settings", "mods"};
    else if (game == QLatin1String("iw5"))
        bases = {"scripts", "mods"};
    QStringList out;
    for (const QString &b : bases)
        out << kindFor(game, b);
    return out;
}

QStringList automaticKinds(const QString &game)
{
    // Not "mod": that is our own mod, which can be downloaded again, and a
    // first-install copy of it kept forever would be 400 MB of nothing. The
    // player's menu settings for it are in "settings".
    QStringList out;
    for (const QString &k : kinds(game)) {
        const QString base = k.section(QLatin1Char('/'), -1);
        if (base != QLatin1String("mod") && base != QLatin1String("mods"))
            out << k;
    }
    return out;
}

QString kindFor(const QString &game, const QString &base)
{
    if (game == QLatin1String("t6") || game == QLatin1String("plutonium"))
        return base;
    return game + QLatin1Char('/') + base;
}

QString gameOf(const QString &kind)
{
    if (kind.contains(QLatin1Char('/')))
        return kind.section(QLatin1Char('/'), 0, 0);
    return kind == QLatin1String("reshade") ? QStringLiteral("plutonium") : QStringLiteral("t6");
}

QString root(const AppSettings &s) { return QDir(t6(s)).filePath(QStringLiteral("backups")); }

// Black Ops II's backups, and ReShade's, sit where every earlier version put
// them; the other games keep theirs in their own storage folder.
QString folder(const AppSettings &s, const QString &kind)
{
    const QString g = gameOf(kind);
    if (g == QLatin1String("t6") || g == QLatin1String("plutonium"))
        return QDir(root(s)).filePath(kind);
    return QDir(QolService::gameStorage(s, g)).filePath(QStringLiteral("backups/") + kind.section(QLatin1Char('/'), 1));
}

bool exists(const AppSettings &s, const QString &kind)
{
    return QDir(folder(s, kind)).exists();
}

Info info(const AppSettings &s, const QString &kind, bool measure)
{
    const auto all = sets(s);
    Info out;
    out.kind = kind;
    out.game = gameOf(kind);
    if (!all.contains(kind))
        return out;
    const Set &set = all[kind];
    out.label = set.label;
    out.title = set.title;
    out.desc = set.desc;
    out.folder = folder(s, kind);
    out.exists = QDir(out.folder).exists();
    if (out.exists)
        out.when = takenAt(out.folder);
    if (!measure)
        return out;
    for (const Part &part : set.parts)
        for (const FileRef &f : partFiles(s, out.game, part)) {
            ++out.liveFiles;
            out.liveBytes += f.size;
        }
    if (out.exists) {
        QDirIterator it(out.folder, QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            ++out.files;
            out.bytes += it.fileInfo().size();
        }
    }
    return out;
}

Result backup(const AppSettings &s, const QString &kind, bool replace, const Progress &p, QString &message)
{
    const auto all = sets(s);
    if (!all.contains(kind)) {
        message = QObject::tr("Unknown backup kind \"%1\".").arg(kind);
        return Result::Failed;
    }
    const Set &set = all[kind];
    const QString dest = folder(s, kind);
    // Checked before anything is measured: this runs before every install,
    // and walking a gigabyte of textures only to keep the old copy is waste.
    if (QDir(dest).exists() && !replace) {
        message = QObject::tr("A backup of %1 already exists from %2 - kept it. That is the older copy, "
                              "so it is the one worth having.")
                      .arg(set.title, takenAt(dest).toString(QStringLiteral("d MMM yyyy h:mm AP")));
        return Result::KeptOlder;
    }

    QList<QPair<Part, QList<FileRef>>> plan;
    int total = 0;
    qint64 bytes = 0;
    for (const Part &part : set.parts) {
        const QList<FileRef> files = partFiles(s, gameOf(kind), part);
        total += files.size();
        for (const FileRef &f : files) bytes += f.size;
        plan << qMakePair(part, files);
    }
    if (total == 0) {
        message = QObject::tr("Nothing to back up for %1 - there are no files of yours there yet.").arg(set.title);
        return Result::NothingToSave;
    }
    // Room check up front. A backup that runs the disk dry half-way through
    // helps nobody and can take the install down with it.
    const QString parent = QFileInfo(dest).absolutePath();
    QDir().mkpath(parent);
    const QStorageInfo disk(parent);
    if (disk.isValid() && disk.bytesAvailable() > 0 && disk.bytesAvailable() < bytes + 64LL * 1024 * 1024) {
        message = QObject::tr("Not enough free space to back up %1: it needs %2 and %3 is free.")
                      .arg(set.title, fmtSize(bytes), fmtSize(disk.bytesAvailable()));
        return Result::Failed;
    }

    // Built beside the old copy and swapped in only when whole, so a copy
    // that fails half-way never costs the backup that was already there.
    const QString staging = dest + QStringLiteral(".new");
    QDir(staging).removeRecursively();
    int done = 0;
    for (const auto &step : plan) {
        const QString to = QDir(staging).filePath(step.first.sub);
        for (const FileRef &f : step.second) {
            QString err;
            if (!copyFile(f.path, QDir(to).filePath(f.rel), err)) {
                QDir(staging).removeRecursively();
                message = QObject::tr("Backup of %1 failed - nothing was changed. %2").arg(set.title, err);
                return Result::Failed;
            }
            ++done;
            if (p && (done % 25 == 0 || done == total))
                p(QObject::tr("Backing up %1 (%2 of %3)").arg(set.title).arg(done).arg(total),
                  int(done * 100LL / total));
        }
    }
    const QString old = dest + QStringLiteral(".old");
    QDir(old).removeRecursively();
    if (QDir(dest).exists() && !QDir().rename(dest, old)) {
        QDir(staging).removeRecursively();
        message = QObject::tr("Could not replace the old backup in %1. Close anything that has it open.")
                      .arg(QDir::toNativeSeparators(dest));
        return Result::Failed;
    }
    if (!QDir().rename(staging, dest)) {
        if (QDir(old).exists())
            QDir().rename(old, dest);
        QDir(staging).removeRecursively();
        message = QObject::tr("Could not finish the backup in %1.").arg(QDir::toNativeSeparators(dest));
        return Result::Failed;
    }
    QDir(old).removeRecursively();
    appendLog(s, QStringLiteral("backup: %1 -> %2 (%3 files)").arg(kind, dest).arg(total));
    message = QObject::tr("Backed up %1 - %2, %3, to %4.")
                  .arg(set.title, total == 1 ? QObject::tr("1 file") : QObject::tr("%1 files").arg(total),
                       fmtSize(bytes), QDir::toNativeSeparators(dest));
    if (kind == QLatin1String("mod") || kind == QLatin1String("settings"))
        repairAaSamples(s);
    return Result::Saved;
}

bool backupBeforeInstall(const AppSettings &s, const QStringList &required, const QStringList &extra,
                         const Progress &p, QString &error)
{
    for (const QString &k : required) {
        QString msg;
        if (backup(s, k, false, p, msg) == Result::Failed) {
            error = QObject::tr("%1\n\nThe install was stopped so none of your files are overwritten "
                                "without a copy. Free some space, or untick \"Back up my files before "
                                "every install\" under Backups to install anyway.").arg(msg);
            return false;
        }
    }
    for (const QString &k : extra) {
        if (required.contains(k))
            continue;
        QString msg;
        if (backup(s, k, false, p, msg) == Result::Failed)
            appendLog(s, QStringLiteral("backup before install skipped: %1 - %2").arg(k, msg));
    }
    return true;
}

bool restore(const AppSettings &s, const QString &kind, const Progress &p, QString &error,
             const QStringList &only)
{
    if (const QString g = QolService::runningGame(); !g.isEmpty()) {
        error = QObject::tr("Close Plutonium first (%1 is running). The game keeps these files open.").arg(g);
        return false;
    }
    const auto all = sets(s);
    if (!all.contains(kind)) {
        error = QObject::tr("Unknown backup kind \"%1\".").arg(kind);
        return false;
    }
    const Set &set = all[kind];
    const QString dir = folder(s, kind);
    if (!QDir(dir).exists()) {
        error = QObject::tr("There is no backup of %1 to put back.").arg(set.title);
        return false;
    }
    // Take the sting out of an old settings backup before it goes live.
    if (kind == QLatin1String("mod") || kind == QLatin1String("settings"))
        repairAaSamples(s);

    // Where each file in the backup goes. Folder parts keep their tree; loose
    // parts go back flat, exactly where they were taken from.
    struct Move { QString from, to, rel; const Part *part; };
    QList<Move> moves;
    for (const Part &part : set.parts) {
        const QString from = QDir(dir).filePath(part.sub);
        QDirIterator it(from, QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            const QString rel = part.folder ? QDir(from).relativeFilePath(it.filePath()) : it.fileName();
            moves << Move{it.filePath(), QDir(part.path).filePath(rel), rel, &part};
        }
    }
    // Versions 2.0 to 2.2 of this app kept the ReShade backup flat, straight
    // in backups\reshade rather than in its bin subfolder. Those go to bin.
    if (kind == QLatin1String("reshade"))
        for (const QFileInfo &fi : QDir(dir).entryInfoList(QDir::Files))
            moves << Move{fi.filePath(), QDir(set.parts.first().path).filePath(fi.fileName()),
                          fi.fileName(), &set.parts.first()};
    if (!only.isEmpty()) {
        QSet<QString> wanted;
        for (const QString &r : only)
            wanted.insert(QDir::fromNativeSeparators(r).toLower());
        moves.removeIf([&wanted](const Move &m) { return !wanted.contains(m.rel.toLower()); });
        if (moves.isEmpty())
            return true; // none of the removed files had an original of yours
    }
    if (moves.isEmpty()) {
        error = QObject::tr("That backup is empty - there is nothing to put back.");
        return false;
    }

    int done = 0;
    QHash<const Part *, QStringList> back;
    for (const Move &m : moves) {
        if (!copyFile(m.from, m.to, error)) {
            error = QObject::tr("Putting back %1 stopped part-way: %2").arg(set.title, error);
            return false;
        }
        back[m.part] << m.rel;
        ++done;
        if (p && (done % 25 == 0 || done == moves.size()))
            p(QObject::tr("Putting back %1 (%2 of %3)").arg(set.title).arg(done).arg(moves.size()),
              int(done * 100LL / moves.size()));
    }
    for (auto it = back.cbegin(); it != back.cend(); ++it)
        for (const QString &base : it.key()->notOurs + it.key()->forget)
            forgetFromManifest(s, gameOf(kind), base, it.value());
    appendLog(s, QStringLiteral("restore: %1 (%2 files)").arg(kind).arg(done));
    return true;
}

bool remove(const AppSettings &s, const QString &kind, QString &error)
{
    const QString dir = folder(s, kind);
    if (!QDir(dir).exists())
        return true;
    if (!QDir(dir).removeRecursively()) {
        error = QObject::tr("Could not delete %1. Close anything that has it open.").arg(QDir::toNativeSeparators(dir));
        return false;
    }
    appendLog(s, QStringLiteral("backup deleted: %1").arg(kind));
    return true;
}

int repairAaSamples(const AppSettings &s)
{
    static const QRegularExpression re(QStringLiteral("(?m)^([ \\t]*seta[ \\t]+r_aaSamples[ \\t]+)\"?(-?\\d+)\"?"));
    const QString storage = t6(s);
    const QString bk = root(s);
    int fixed = 0;
    for (const QString &path : {under(storage, "players/mods/zm_qol/plutonium_zm.cfg"),
                                under(bk, "mod/settings/plutonium_zm.cfg"),
                                under(storage, "players/plutonium_zm.cfg"),
                                under(bk, "settings/players/plutonium_zm.cfg"),
                                under(bk, "settings/mod-settings/plutonium_zm.cfg")}) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly))
            continue;
        const QByteArray raw = f.readAll();
        f.close();
        const QString text = QString::fromUtf8(raw);
        const QRegularExpressionMatch m = re.match(text);
        if (!m.hasMatch())
            continue;
        const int v = m.captured(2).toInt();
        if (v == 1 || v == 2 || v == 4 || v == 8)
            continue;
        // Only that one line changes; every other byte, line endings included,
        // is written back as it was read.
        QString out = text;
        out.replace(m.capturedStart(0), m.capturedLength(0), m.captured(1) + QStringLiteral("\"4\""));
        const QByteArray bytes = out.toUtf8();
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate) && f.write(bytes) == bytes.size()) {
            ++fixed;
            appendLog(s, QStringLiteral("repaired r_aaSamples %1 -> 4 in %2").arg(v).arg(path));
        }
    }
    return fixed;
}

} // namespace QolBackups
