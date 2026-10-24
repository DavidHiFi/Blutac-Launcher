#include "T7Patch.h"
#include "ArchiveTool.h"
#include "Downloader.h"
#include "GameLauncher.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QUrl>

namespace T7Patch
{

namespace {

const QString kRepo = QStringLiteral("Scroptss/T7Patch");
const QString kMarker = QStringLiteral("blutac-t7patch.json");
const QString kConf = QStringLiteral("t7patch.conf");
const QString kDsound = QStringLiteral("dsound.dll");
const QString kLoaderDll = QStringLiteral("t7patchloader.dll");
const QString kLoaderAsi = QStringLiteral("t7patchloader.asi");
// T7 Patch's own dsound.dll, set aside while the plugin's loader starts the patch.
const QString kParkedDsound = QStringLiteral("dsound.dll.t7patch");
const QString kBackup = QStringLiteral(".blutac-backup");

// The marker: {"version", "files": [ours], "backups": [names kept as
// <name>.blutac-backup], "plugin": {"files", "backups"}}.
QJsonObject readMarker(const QString &dir)
{
    QFile f(QDir(dir).filePath(kMarker));
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(f.readAll()).object();
}

bool writeMarker(const QString &dir, const QJsonObject &o, QString &error)
{
    QSaveFile f(QDir(dir).filePath(kMarker));
    if (!f.open(QIODevice::WriteOnly) || f.write(QJsonDocument(o).toJson()) < 0 || !f.commit()) {
        error = QObject::tr("Could not write %1: %2").arg(kMarker, f.errorString());
        return false;
    }
    return true;
}

QStringList toList(const QJsonValue &v)
{
    QStringList out;
    for (const QJsonValue &x : v.toArray())
        out << x.toString();
    return out;
}

// Copies src to dir/rel. A file already there that Blutac did not put there is
// renamed to <rel>.blutac-backup first and listed in backups.
bool place(const QString &src, const QString &dir, const QString &rel, const QStringList &ours,
           QStringList &backups, QString &error)
{
    const QString dest = QDir(dir).filePath(rel);
    QDir().mkpath(QFileInfo(dest).absolutePath());
    if (QFileInfo::exists(dest) && !ours.contains(rel, Qt::CaseInsensitive)) {
        if (QFileInfo::exists(dest + kBackup) || !QFile::rename(dest, dest + kBackup)) {
            error = QObject::tr("Could not set aside the existing %1. Close Black Ops III and try again.").arg(rel);
            return false;
        }
        backups << rel;
    }
    if (QFileInfo::exists(dest) && !QFile::remove(dest)) {
        error = QObject::tr("Could not replace %1. Close Black Ops III and try again.").arg(rel);
        return false;
    }
    if (!QFile::copy(src, dest)) {
        error = QObject::tr("Could not copy %1 into the game folder.").arg(rel);
        return false;
    }
    return true;
}

// Deletes files and puts the backups back. Carries on past a failure so one
// locked file does not strand the rest; error names the first one.
bool restore(const QString &dir, const QStringList &files, const QStringList &backups, QString &error)
{
    bool ok = true;
    for (const QString &rel : files) {
        const QString path = QDir(dir).filePath(rel);
        if (QFileInfo::exists(path) && !QFile::remove(path) && ok) {
            error = QObject::tr("Could not remove %1. Close Black Ops III and try again.").arg(rel);
            ok = false;
        }
    }
    for (const QString &rel : backups) {
        const QString path = QDir(dir).filePath(rel);
        if (!QFileInfo::exists(path))
            QFile::rename(path + kBackup, path);
    }
    return ok;
}

// Renames one of our files and keeps the list in step.
bool renameOurs(const QString &dir, QStringList &ours, const QString &from, const QString &to,
                QString &error)
{
    const int at = ours.indexOf(QRegularExpression(QRegularExpression::escape(from),
                                                   QRegularExpression::CaseInsensitiveOption));
    if (at < 0)
        return true;
    const QString a = QDir(dir).filePath(from);
    const QString b = QDir(dir).filePath(to);
    if (QFileInfo::exists(a) && (QFileInfo::exists(b) || !QFile::rename(a, b))) {
        error = QObject::tr("Could not rename %1 to %2. Close Black Ops III and try again.").arg(from, to);
        return false;
    }
    ours[at] = to;
    return true;
}

// Every file under root, as paths relative to it. A zip whose content sits in
// one top folder is read from inside that folder.
QMap<QString, QString> filesUnder(QString root)
{
    const QStringList top = QDir(root).entryList(QDir::AllEntries | QDir::NoDotAndDotDot);
    if (top.size() == 1 && QFileInfo(QDir(root).filePath(top.first())).isDir())
        root = QDir(root).filePath(top.first());
    QMap<QString, QString> out;
    QDirIterator it(root, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        out.insert(QDir(root).relativeFilePath(path), path);
    }
    return out;
}

} // namespace

QString runningGame()
{
    for (const QString &exe : {QStringLiteral("BlackOps3.exe"), QStringLiteral("boiii.exe"),
                               QStringLiteral("t7.exe"), QStringLiteral("bo3.exe")})
        if (GameLauncher::findProcessByName(exe) > 0)
            return exe;
    return QString();
}

QString installedVersion(const QString &gameDir)
{
    if (gameDir.isEmpty())
        return QString();
    const QString v = readMarker(gameDir).value(QStringLiteral("version")).toString();
    if (!v.isEmpty())
        return v;
    return QFileInfo::exists(QDir(gameDir).filePath(QStringLiteral("t7patch.dll")))
               ? QStringLiteral("manual") : QString();
}

bool pluginInstalled(const QString &gameDir)
{
    return !gameDir.isEmpty() && readMarker(gameDir).contains(QStringLiteral("plugin"));
}

QString latestVersion(QString &error)
{
    // /releases/latest redirects to /releases/tag/<tag>; plain github.com, no API.
    const QString target = Downloader::redirectTargetOf(
        QStringLiteral("https://github.com/%1/releases/latest").arg(kRepo), error, 15000);
    static const QRegularExpression re(QStringLiteral("/releases/tag/([^/?#]+)"));
    const QRegularExpressionMatch hit = re.match(target);
    if (!hit.hasMatch())
        return QString();
    error.clear();
    return QUrl::fromPercentEncoding(hit.captured(1).toUtf8());
}

bool install(const QString &gameDir, const Progress &p, QString &error,
             const std::function<bool()> &cancelled)
{
    if (!QFileInfo::exists(QDir(gameDir).filePath(QStringLiteral("BlackOps3.exe")))) {
        error = QObject::tr("BlackOps3.exe is not in %1. Set the Black Ops III folder in Settings.")
                    .arg(QDir::toNativeSeparators(gameDir));
        return false;
    }
    if (const QString g = runningGame(); !g.isEmpty()) {
        error = QObject::tr("Close Black Ops III first (%1 is running).").arg(g);
        return false;
    }
    if (p) p(QObject::tr("Looking up the latest T7 Patch..."), 5);
    const QString tag = latestVersion(error);
    if (tag.isEmpty()) {
        if (error.isEmpty())
            error = QObject::tr("Could not find the latest T7 Patch release.");
        return false;
    }

    // The "manual install" zip holds the four files the game loads.
    const QString asset = QStringLiteral("Linux.Steamdeck.and.Manual.Windows.Install.%1.zip").arg(tag);
    QTemporaryDir tmp;
    if (!tmp.isValid()) {
        error = QObject::tr("Could not create a temporary folder.");
        return false;
    }
    const QString zip = tmp.filePath(asset);
    if (!Downloader::downloadToFile(
            QStringLiteral("https://github.com/%1/releases/download/%2/%3").arg(kRepo, tag, asset), zip, error,
            [&](qint64 got, qint64 total) {
                if (p) p(QObject::tr("Downloading T7 Patch %1...").arg(tag), total > 0 ? int(10 + got * 60 / total) : 20);
            },
            5 * 60 * 1000, cancelled))
        return false;
    if (p) p(QObject::tr("Unpacking..."), 75);
    ArchiveTool::ensureBundledSevenZip();
    const QString unpacked = tmp.filePath(QStringLiteral("x"));
    if (!ArchiveTool::extractToDirectory(zip, unpacked, &error, {}, cancelled))
        return false;

    QMap<QString, QString> found; // lower-case file name -> path
    QDirIterator it(unpacked, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString f = it.next();
        found.insert(QFileInfo(f).fileName().toLower(), f);
    }
    for (const QString &need : {kDsound, QStringLiteral("t7patch.dll"), kLoaderDll, kConf}) {
        if (!found.contains(need)) {
            error = QObject::tr("The T7 Patch %1 download has no %2. Install it by hand from github.com/%3.")
                        .arg(tag, need, kRepo);
            return false;
        }
    }

    QJsonObject marker = readMarker(gameDir);
    QStringList files = toList(marker.value(QStringLiteral("files")));
    QStringList backups = toList(marker.value(QStringLiteral("backups")));
    const bool plugin = marker.contains(QStringLiteral("plugin"));
    // With the plugin in, its loader starts the patch from the .asi and our
    // dsound.dll stays set aside.
    QList<QPair<QString, QString>> items = {
        {found.value(QStringLiteral("t7patch.dll")), QStringLiteral("t7patch.dll")},
        {found.value(kLoaderDll), plugin ? kLoaderAsi : kLoaderDll},
        {found.value(kDsound), plugin ? kParkedDsound : kDsound},
    };
    // An existing t7patch.conf holds the player's name and password.
    if (!QFileInfo::exists(QDir(gameDir).filePath(kConf)))
        items.append({found.value(kConf), kConf});

    if (p) p(QObject::tr("Copying into the game folder..."), 85);
    bool ok = true;
    for (const auto &item : items) {
        if (!place(item.first, gameDir, item.second, files, backups, error)) {
            ok = false;
            break;
        }
        if (!files.contains(item.second, Qt::CaseInsensitive))
            files << item.second;
    }
    // Saved even after a failure, so Remove can still undo what went in.
    marker.insert(QStringLiteral("files"), QJsonArray::fromStringList(files));
    marker.insert(QStringLiteral("backups"), QJsonArray::fromStringList(backups));
    if (ok)
        marker.insert(QStringLiteral("version"), tag);
    QString markerError;
    if (!writeMarker(gameDir, marker, markerError) && ok) {
        error = markerError;
        ok = false;
    }
    return ok;
}

bool remove(const QString &gameDir, QString &error)
{
    if (const QString g = runningGame(); !g.isEmpty()) {
        error = QObject::tr("Close Black Ops III first (%1 is running).").arg(g);
        return false;
    }
    if (readMarker(gameDir).isEmpty()) {
        error = QObject::tr("Blutac did not install T7 Patch here, so it leaves those files alone.");
        return false;
    }
    if (pluginInstalled(gameDir) && !removePlugin(gameDir, error))
        return false;
    const QJsonObject marker = readMarker(gameDir);
    if (!restore(gameDir, toList(marker.value(QStringLiteral("files"))),
                 toList(marker.value(QStringLiteral("backups"))), error))
        return false;
    QFile::remove(QDir(gameDir).filePath(kMarker));
    return true;
}

bool installPlugin(const QString &gameDir, const QString &zipPath, QString &error)
{
    if (const QString g = runningGame(); !g.isEmpty()) {
        error = QObject::tr("Close Black Ops III first (%1 is running).").arg(g);
        return false;
    }
    if (readMarker(gameDir).value(QStringLiteral("version")).toString().isEmpty()) {
        error = QObject::tr("Install T7 Patch here first. The plugin needs it.");
        return false;
    }
    if (pluginInstalled(gameDir) && !removePlugin(gameDir, error))
        return false;

    QTemporaryDir tmp;
    if (!tmp.isValid()) {
        error = QObject::tr("Could not create a temporary folder.");
        return false;
    }
    ArchiveTool::ensureBundledSevenZip();
    if (!ArchiveTool::extractToDirectory(zipPath, tmp.path(), &error))
        return false;
    const QMap<QString, QString> content = filesUnder(tmp.path());
    if (content.isEmpty()) {
        error = QObject::tr("That zip is empty.");
        return false;
    }
    for (auto it = content.cbegin(); it != content.cend(); ++it) {
        if (QFileInfo(it.key()).fileName().compare(QLatin1String("BlackOps3.exe"), Qt::CaseInsensitive) == 0) {
            error = QObject::tr("That zip would replace BlackOps3.exe, so it is not the plugin.");
            return false;
        }
    }

    QJsonObject marker = readMarker(gameDir);
    QStringList ours = toList(marker.value(QStringLiteral("files")));
    // The plugin's own loader starts T7 Patch as an .asi (its README's steps),
    // so ours steps aside instead of loading the patch a second time.
    if (!renameOurs(gameDir, ours, kDsound, kParkedDsound, error)
        || !renameOurs(gameDir, ours, kLoaderDll, kLoaderAsi, error)) {
        marker.insert(QStringLiteral("files"), QJsonArray::fromStringList(ours));
        QString ignored;
        writeMarker(gameDir, marker, ignored);
        return false;
    }

    QStringList files, backups;
    bool ok = true;
    for (auto it = content.cbegin(); it != content.cend(); ++it) {
        if (!place(it.value(), gameDir, it.key(), ours + files, backups, error)) {
            ok = false;
            break;
        }
        files << it.key();
    }
    marker.insert(QStringLiteral("files"), QJsonArray::fromStringList(ours));
    QJsonObject plug;
    plug.insert(QStringLiteral("files"), QJsonArray::fromStringList(files));
    plug.insert(QStringLiteral("backups"), QJsonArray::fromStringList(backups));
    marker.insert(QStringLiteral("plugin"), plug);
    QString markerError;
    if (!writeMarker(gameDir, marker, markerError) && ok) {
        error = markerError;
        ok = false;
    }
    return ok;
}

bool removePlugin(const QString &gameDir, QString &error)
{
    if (const QString g = runningGame(); !g.isEmpty()) {
        error = QObject::tr("Close Black Ops III first (%1 is running).").arg(g);
        return false;
    }
    QJsonObject marker = readMarker(gameDir);
    if (!marker.contains(QStringLiteral("plugin")))
        return true;
    const QJsonObject plug = marker.value(QStringLiteral("plugin")).toObject();
    if (!restore(gameDir, toList(plug.value(QStringLiteral("files"))),
                 toList(plug.value(QStringLiteral("backups"))), error))
        return false;
    QStringList ours = toList(marker.value(QStringLiteral("files")));
    const bool ok = renameOurs(gameDir, ours, kLoaderAsi, kLoaderDll, error)
                    && renameOurs(gameDir, ours, kParkedDsound, kDsound, error);
    marker.insert(QStringLiteral("files"), QJsonArray::fromStringList(ours));
    if (ok)
        marker.remove(QStringLiteral("plugin"));
    QString markerError;
    if (!writeMarker(gameDir, marker, markerError) && ok) {
        error = markerError;
        return false;
    }
    return ok;
}

Config readConfig(const QString &gameDir)
{
    Config c;
    QFile f(QDir(gameDir).filePath(kConf));
    if (!f.open(QIODevice::ReadOnly))
        return c;
    for (const QString &line : QString::fromUtf8(f.readAll()).split(QLatin1Char('\n'))) {
        const int eq = line.indexOf(QLatin1Char('='));
        if (eq <= 0)
            continue;
        const QString key = line.left(eq).trimmed().toLower();
        const QString value = line.mid(eq + 1).trimmed();
        if (key == QLatin1String("playername"))
            c.playerName = value;
        else if (key == QLatin1String("networkpassword"))
            c.networkPassword = value;
        else if (key == QLatin1String("isfriendsonly"))
            c.friendsOnly = value != QLatin1String("0");
    }
    return c;
}

bool writeConfig(const QString &gameDir, const Config &c, QString &error)
{
    const QString path = QDir(gameDir).filePath(kConf);
    QStringList lines;
    {
        QFile f(path);
        if (f.open(QIODevice::ReadOnly))
            lines = QString::fromUtf8(f.readAll()).remove(QLatin1Char('\r')).split(QLatin1Char('\n'));
    }
    if (!lines.isEmpty() && lines.last().isEmpty())
        lines.removeLast();
    const auto oneLine = [](QString v) { return v.remove(QLatin1Char('\r')).remove(QLatin1Char('\n')).trimmed(); };
    const QList<QPair<QString, QString>> keys = {
        {QStringLiteral("playername"), oneLine(c.playerName)},
        {QStringLiteral("isfriendsonly"), c.friendsOnly ? QStringLiteral("1") : QStringLiteral("0")},
        {QStringLiteral("networkpassword"), oneLine(c.networkPassword)},
    };
    for (const auto &k : keys) {
        bool done = false;
        for (QString &line : lines) {
            const int eq = line.indexOf(QLatin1Char('='));
            if (eq > 0 && line.left(eq).trimmed().compare(k.first, Qt::CaseInsensitive) == 0) {
                line = k.first + QLatin1Char('=') + k.second;
                done = true;
                break;
            }
        }
        if (!done)
            lines << k.first + QLatin1Char('=') + k.second;
    }
    // A plain rewrite, not a rename: the patch may hold the file open while
    // the game runs.
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)
        || f.write((lines.join(QLatin1Char('\n')) + QLatin1Char('\n')).toUtf8()) < 0) {
        error = QObject::tr("Could not write %1: %2").arg(kConf, f.errorString());
        return false;
    }
    return true;
}

} // namespace T7Patch
