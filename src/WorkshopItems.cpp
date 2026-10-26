#include "WorkshopItems.h"
#include "AppSettings.h"
#include "DownloadCenter.h"
#include "QolBackups.h"
#include "QolService.h"
#include "VersionCompare.h"

#include <QAbstractButton>
#include <QApplication>
#include <QHash>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QRegularExpression>
#include <QWidget>
#include <QtConcurrent/QtConcurrent>

#include <optional>

namespace WorkshopItems
{
namespace {

enum class Builtin { None, HdTextures, Sounds, Controller, ReShade, Dlss, Qol };

const QString kControllerPrefix = QStringLiteral("t6-controller-");

Builtin builtinOf(const QString &id)
{
    if (id == QLatin1String("t6-hd-textures")) return Builtin::HdTextures;
    if (id == QLatin1String("t6-custom-sounds")) return Builtin::Sounds;
    if (id.startsWith(kControllerPrefix)) return Builtin::Controller;
    if (id == QLatin1String("plutonium-reshade")) return Builtin::ReShade;
    if (id == QLatin1String("plutonium-dlss5")) return Builtin::Dlss;
    if (id.endsWith(QLatin1String("-quality-of-life"))
        && QolService::seriesFor(id.section(QLatin1Char('-'), 0, 0)).released)
        return Builtin::Qol;
    return Builtin::None;
}

QString controllerName(const QString &pack)
{
    if (pack == QLatin1String("ps5")) return QObject::tr("PlayStation 5 button icons");
    if (pack == QLatin1String("switch")) return QObject::tr("Nintendo Switch button icons");
    return QObject::tr("Xbox One button icons");
}

// The catalog gives a GitHub link; the pack installer takes "owner/repo" for
// "newest release" and anything else as a direct download.
QString packUrl(const HomeCatalog::Mod &m)
{
    const QString gh = QStringLiteral("https://github.com/");
    if (m.url.startsWith(gh) && m.url.count(QLatin1Char('/')) == 4)
        return m.url.mid(gh.size()).remove(QRegularExpression(QStringLiteral("\\.git$|/$")));
    return m.url;
}

bool cancelled(const QString &error)
{
    return error == QObject::tr("Cancelled.");
}

void runJob(QWidget *parent, const QString &title, const DownloadCenter::Job &job,
            const std::function<void()> &after, const Done &done)
{
    QPointer<QWidget> guard(parent);
    DownloadCenter::run(parent, title, job, [guard, title, after, done](bool ok, const QString &error) {
        if (!ok && !cancelled(error) && guard)
            QMessageBox::warning(guard, title, error.isEmpty() ? QObject::tr("The install failed.") : error);
        if (ok && after)
            after();
        if (done)
            done();
    });
}

bool confirm(QWidget *parent, const QString &title, const QString &text)
{
    return QMessageBox::question(parent, title, text, QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
           == QMessageBox::Yes;
}

// Remove, and when a backup of what the item replaced exists, offer to put
// it back in the same step. A remove that fills `files` (a pack) gets back
// only the originals of those files; one that leaves it unset gets the whole
// backup.
using Remover = std::function<bool(QString &error, std::optional<QStringList> &files)>;

void removeWithRestore(QWidget *parent, AppSettings &s, const QString &title, const QString &question,
                       const QString &backupKind, const Remover &remove,
                       const std::function<void()> &after, const Done &done)
{
    QString err;
    std::optional<QStringList> files;
    if (backupKind.isEmpty() || !QolBackups::exists(s, backupKind)) {
        if (!confirm(parent, title, question))
            return;
        if (!remove(err, files))
            QMessageBox::warning(parent, title, err);
        else if (after)
            after();
        if (done)
            done();
        return;
    }
    const QolBackups::Info info = QolBackups::info(s, backupKind);
    QMessageBox box(parent);
    box.setWindowTitle(title);
    box.setText(question);
    box.setInformativeText(QObject::tr("You have a backup of %1 from %2.")
                               .arg(info.title, info.when.toString(QStringLiteral("d MMM yyyy h:mm AP"))));
    QAbstractButton *withRestore = box.addButton(QObject::tr("Remove and put my originals back"),
                                                 QMessageBox::AcceptRole);
    QAbstractButton *plain = box.addButton(QObject::tr("Just remove"), QMessageBox::DestructiveRole);
    box.addButton(QMessageBox::Cancel);
    box.setDefaultButton(static_cast<QPushButton *>(withRestore));
    box.exec();
    if (box.clickedButton() != withRestore && box.clickedButton() != plain)
        return;
    if (!remove(err, files)) {
        QMessageBox::warning(parent, title, err);
        if (done)
            done();
        return;
    }
    if (after)
        after();
    if (box.clickedButton() != withRestore || (files && files->isEmpty())) {
        if (done)
            done();
        return;
    }
    runJob(parent, QObject::tr("Putting back %1").arg(info.title),
           [&s, backupKind, only = files.value_or(QStringList())](QString &e, const DownloadCenter::Progress &p,
                                                                  const CancelHandle &) {
               return QolBackups::restore(s, backupKind, p, e, only);
           },
           {}, done);
}

// Game code to the newest release tag GitHub reported, without the "v".
QHash<QString, QString> &latestQol()
{
    static QHash<QString, QString> tags;
    return tags;
}

} // namespace

void fetchLatestQol(QObject *context, const Done &done)
{
    static bool asked = false;
    if (asked)
        return;
    asked = true;
    const QPointer<QObject> guard(context);
    auto future = QtConcurrent::run([guard, done]() {
        QHash<QString, QString> tags;
        for (const QolService::SeriesMod &m : QolService::series()) {
            QString err;
            const QString tag = m.released ? QolService::latestModVersion(m, err) : QString();
            if (!tag.isEmpty())
                tags.insert(m.gameCode, VersionCompare::normalise(tag));
        }
        QMetaObject::invokeMethod(qApp, [guard, done, tags]() {
            latestQol().insert(tags);
            if (guard)
                done();
        }, Qt::QueuedConnection);
    });
    Q_UNUSED(future);
}

void applyLatestQol(HomeCatalog::Catalog &cat)
{
    for (HomeCatalog::Mod &m : cat.mods) {
        const QString tag = latestQol().value(m.game);
        if (builtinOf(m.id) == Builtin::Qol && !tag.isEmpty()
            && (m.version.isEmpty() || VersionCompare::isUpdate(m.version, tag)))
            m.version = tag;
    }
}

bool handles(const HomeCatalog::Mod &m)
{
    return m.isPack() || m.isPlutonium() || builtinOf(m.id) == Builtin::Qol;
}

HomeCatalog::Presence presence(const AppSettings &s, const HomeCatalog::Mod &m)
{
    using P = HomeCatalog::Presence;
    switch (builtinOf(m.id)) {
    case Builtin::HdTextures:
        return QolService::packInstalled(s, QolService::Pack::Textures) ? P::Installed : P::Missing;
    case Builtin::Sounds:
        return QolService::packInstalled(s, QolService::Pack::Sounds) ? P::Installed : P::Missing;
    case Builtin::Controller:
        return QolService::installedController(s) == m.id.mid(kControllerPrefix.size()) ? P::Installed : P::Missing;
    case Builtin::ReShade:
        return s.launchReShade || QolService::reShadeInstalled(s) ? P::Installed : P::Missing;
    case Builtin::Dlss:
        return QolService::dlssPayloadReady(s) ? P::Installed : P::Missing;
    case Builtin::Qol: {
        const QString have = QolService::installedModVersion(s, QolService::seriesFor(m.game));
        if (have.isEmpty())
            return P::Missing;
        return !m.version.isEmpty() && VersionCompare::isUpdate(have, m.version) ? P::Update : P::Installed;
    }
    case Builtin::None:
        break;
    }
    for (const QolService::CommunityPack &p : QolService::communityPacks(s, m.game))
        if (p.id == m.id)
            return p.partial || VersionCompare::isUpdate(p.version, m.version) ? P::Update : P::Installed;
    return P::Missing;
}

QList<Installed> installed(const AppSettings &s, const QString &type, const QString &game)
{
    QList<Installed> out;
    if (type == QLatin1String("plutonium")) {
        const QString pluto = QStringLiteral("plutonium");
        if (s.launchReShade || QolService::reShadeInstalled(s))
            out << Installed{QStringLiteral("plutonium-reshade"), QObject::tr("ReShade"), pluto,
                             QObject::tr("Starts with every Plutonium game. Press End in game for its menu.")};
        if (QolService::dlssPayloadReady(s)) {
            const QString v = QolService::installedDlssVersion(s);
            out << Installed{QStringLiteral("plutonium-dlss5"), QObject::tr("DLSS 5"), pluto,
                             (v.isEmpty() ? QString() : QObject::tr("Version %1. ").arg(v))
                                 + (s.launchReShade ? QObject::tr("Black Ops II LAN games only.")
                                                    : QObject::tr("Needs ReShade, which is not installed."))};
        }
        return out;
    }
    const bool textures = type == QLatin1String("texture");
    const QString folder = QStringLiteral("storage\\%1\\%2").arg(game, textures ? QStringLiteral("images")
                                                                                 : QStringLiteral("zone"));
    if (game == QLatin1String("t6")) {
        const QString t6 = QStringLiteral("t6");
        if (textures && QolService::packInstalled(s, QolService::Pack::Textures))
            out << Installed{QStringLiteral("t6-hd-textures"), QObject::tr("HD textures"), t6, folder};
        if (textures) {
            const QString pad = QolService::installedController(s);
            if (!pad.isEmpty())
                out << Installed{kControllerPrefix + pad, controllerName(pad), t6, folder};
        }
        if (!textures && QolService::packInstalled(s, QolService::Pack::Sounds))
            out << Installed{QStringLiteral("t6-custom-sounds"), QObject::tr("Custom sounds"), t6, folder};
    }
    for (const QolService::CommunityPack &p : QolService::communityPacks(s, game)) {
        if (p.type != type)
            continue;
        out << Installed{p.id, p.name, game,
                         p.version.isEmpty() ? folder : QObject::tr("Version %1, in %2").arg(p.version, folder)};
    }
    return out;
}

void install(QWidget *parent, AppSettings &s, const HomeCatalog::Mod &m, const Done &done)
{
    const QString title = QObject::tr("Installing %1").arg(m.name);
    using Progress = DownloadCenter::Progress;
    switch (builtinOf(m.id)) {
    case Builtin::HdTextures:
    case Builtin::Sounds: {
        const auto pack = builtinOf(m.id) == Builtin::HdTextures ? QolService::Pack::Textures
                                                                  : QolService::Pack::Sounds;
        runJob(parent, title, [&s, pack](QString &e, const Progress &p, const CancelHandle &c) {
            return QolService::installPack(s, pack, p, e, c);
        }, {}, done);
        return;
    }
    case Builtin::Controller: {
        const QString pad = m.id.mid(kControllerPrefix.size());
        runJob(parent, title, [&s, pad](QString &e, const Progress &p, const CancelHandle &c) {
            return QolService::installController(s, pad, p, e, c);
        }, {}, done);
        return;
    }
    case Builtin::ReShade: {
        // The files ship inside the launcher, so this is quick. The launch
        // switch is what keeps it there: Plutonium empties bin on every start.
        QString err;
        if (!QolService::installReShade(s, err)) {
            QMessageBox::warning(parent, m.name, err);
        } else {
            s.launchReShade = true;
            s.saveToIni();
        }
        if (done)
            done();
        return;
    }
    case Builtin::Dlss:
        runJob(parent, title, [&s](QString &e, const Progress &p, const CancelHandle &) {
            if (!QolService::reShadeInstalled(s) && !QolService::installReShade(s, e))
                return false;
            p(QObject::tr("Checking the published DLSS 5 release"), 2);
            QolService::DlssRelease release;
            if (!QolService::latestDlssRelease(release, e))
                return false;
            return QolService::installDlssPayload(s, release, p, e);
        }, [&s]() {
            s.launchReShade = true;
            s.launchDlss5 = true;
            s.saveToIni();
        }, done);
        return;
    case Builtin::Qol: {
        const QolService::SeriesMod series = QolService::seriesFor(m.game);
        runJob(parent, title, [&s, series](QString &e, const Progress &p, const CancelHandle &c) {
            return QolService::installMod(s, series, p, e, c);
        }, {}, done);
        return;
    }
    case Builtin::None:
        break;
    }
    QolService::CommunityPack pack;
    pack.id = m.id;
    pack.name = m.name;
    pack.game = m.game;
    pack.type = m.type;
    pack.version = m.version;
    pack.url = packUrl(m);
    runJob(parent, title, [&s, pack](QString &e, const Progress &p, const CancelHandle &c) {
        return QolService::installCommunityPack(s, pack, p, e, c);
    }, {}, done);
}

void remove(QWidget *parent, AppSettings &s, const Installed &item, const Done &done)
{
    // Every remove below can end in a restore, and a restore cannot write
    // over files the game holds open. Asking now means nothing is half done.
    if (const QString g = QolService::runningGame(); !g.isEmpty()) {
        QMessageBox::warning(parent, item.name,
                             QObject::tr("Close Plutonium first (%1 is running). Nothing was removed.").arg(g));
        return;
    }
    const QString question = QObject::tr("Remove %1?").arg(item.name);
    switch (builtinOf(item.id)) {
    case Builtin::HdTextures:
        removeWithRestore(parent, s, item.name, question, QStringLiteral("images"), [&s](QString &e, std::optional<QStringList> &files) {
            return QolService::removePack(s, QolService::Pack::Textures, e, &files.emplace());
        }, {}, done);
        return;
    case Builtin::Sounds:
        removeWithRestore(parent, s, item.name, question, QStringLiteral("zone"), [&s](QString &e, std::optional<QStringList> &files) {
            return QolService::removePack(s, QolService::Pack::Sounds, e, &files.emplace());
        }, {}, done);
        return;
    case Builtin::Controller:
        removeWithRestore(parent, s, item.name, QObject::tr("Remove %1 and go back to the game's own button "
                                                            "icons?").arg(item.name),
                          QStringLiteral("controller"), [&s](QString &e, std::optional<QStringList> &files) {
            return QolService::removeController(s, e, &files.emplace());
        }, {}, done);
        return;
    case Builtin::ReShade:
        removeWithRestore(parent, s, item.name,
                          QolService::dlssPayloadReady(s)
                              ? QObject::tr("Remove ReShade? DLSS 5 stays on this PC, but it cannot run "
                                            "without ReShade.")
                              : question,
                          QStringLiteral("reshade"), [&s](QString &e, std::optional<QStringList> &) {
            // Gone means gone: the add-ons, the 64-bit host and the preset
            // entries that name them go too, not only dxgi.dll.
            return QolService::ensureReShadeAbsent(s, e) && QolService::removeReShade(s, e);
        }, [&s]() {
            s.launchReShade = false;
            s.launchDlss5 = false;
            s.saveToIni();
        }, done);
        return;
    case Builtin::Dlss:
        removeWithRestore(parent, s, item.name, QObject::tr("Remove DLSS 5 from this PC? ReShade stays installed."),
                          QString(), [&s](QString &e, std::optional<QStringList> &) {
            return QolService::removeDlssPayload(s, e);
        }, [&s]() {
            s.launchDlss5 = false;
            s.saveToIni();
        }, done);
        return;
    case Builtin::Qol:
        return; // a mod: the Mods tab removes it with the others
    case Builtin::None:
        break;
    }
    QString base = QStringLiteral("images");
    for (const QolService::CommunityPack &p : QolService::communityPacks(s, item.game))
        if (p.id == item.id && p.type == QLatin1String("sound"))
            base = QStringLiteral("zone");
    const QString game = item.game;
    const QString id = item.id;
    removeWithRestore(parent, s, item.name, question, QolBackups::kindFor(game, base),
                      [&s, game, id](QString &e, std::optional<QStringList> &files) {
        return QolService::removeCommunityPack(s, game, id, e, &files.emplace());
    }, {}, done);
}

} // namespace WorkshopItems
