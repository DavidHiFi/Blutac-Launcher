#include "BackupsPage.h"
#include "AppSettings.h"
#include "DownloadCenter.h"
#include "QolBackups.h"
#include "QolService.h"
#include "../Checkables.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QStyle>
#include <QUrl>
#include <QVBoxLayout>

namespace {

QLabel *section(QWidget *parent, const QString &text)
{
    auto *l = new QLabel(text, parent);
    l->setObjectName("SectionTitle");
    return l;
}

QString fmtFiles(int n)
{
    return n == 1 ? QObject::tr("1 file") : QObject::tr("%1 files").arg(n);
}

QString fmtSize(qint64 b)
{
    if (b >= 1000LL * 1000 * 1000) return QObject::tr("%1 GB").arg(b / 1e9, 0, 'f', 1);
    if (b >= 1000 * 1000)          return QObject::tr("%1 MB").arg(b / 1e6, 0, 'f', 0);
    if (b >= 1000)                 return QObject::tr("%1 KB").arg(b / 1e3, 0, 'f', 0);
    return QObject::tr("%1 bytes").arg(b);
}

QString fmtWhen(const QDateTime &t)
{
    return t.toString(QStringLiteral("d MMM yyyy h:mm AP"));
}

QString gameName(const QString &game)
{
    if (game == QLatin1String("t6")) return QObject::tr("Black Ops II");
    if (game == QLatin1String("t5")) return QObject::tr("Black Ops");
    if (game == QLatin1String("t4")) return QObject::tr("World at War");
    if (game == QLatin1String("iw5")) return QObject::tr("Modern Warfare 3");
    if (game == QLatin1String("plutonium")) return QStringLiteral("Plutonium");
    return game;
}

// Where a game's backups sit on disk. Plutonium's own share Black Ops II's
// folder, as they always have.
QString backupsDir(const AppSettings &s, const QString &game)
{
    if (game.isEmpty())
        return QDir(s.plutoniumInstance).filePath(QStringLiteral("storage"));
    const QStringList kinds = QolBackups::kinds(game);
    return kinds.isEmpty() ? QolBackups::root(s) : QFileInfo(QolBackups::folder(s, kinds.first())).path();
}

} // namespace

BackupsPage::BackupsPage(AppSettings &settings, QWidget *parent)
    : QWidget(parent)
    , m_settings(settings)
{
    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *inner = new QWidget;
    auto *root = new QVBoxLayout(inner);
    root->setContentsMargins(28, 22, 28, 24);
    root->setSpacing(12);

    m_intro = new QLabel(inner);
    m_intro->setObjectName("MutedHint");
    m_intro->setWordWrap(true);
    root->addWidget(m_intro);

    m_plutoHint = new QLabel(inner);
    m_plutoHint->setObjectName("MutedHint");
    m_plutoHint->setWordWrap(true);
    root->addWidget(m_plutoHint);

    // All games | Black Ops II | Black Ops | ... : which game's backups show.
    const QStringList games = QolBackups::games();
    auto *filter = new QHBoxLayout();
    filter->setSpacing(0);
    for (int i = 0; i <= games.size(); ++i) {
        const QString game = i == 0 ? QString() : games[i - 1];
        auto *b = new QPushButton(inner);
        b->setObjectName("TypeSegment");
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setMinimumHeight(32);
        connect(b, &QPushButton::clicked, this, [this, game]() { showGame(game); });
        filter->addWidget(b);
        m_filterButtons << b;
    }
    filter->addStretch(1);
    m_backupOpen = new QPushButton(inner);
    m_backupOpen->setCursor(Qt::PointingHandCursor);
    m_backupOpen->setMinimumHeight(32);
    filter->addWidget(m_backupOpen);
    root->addLayout(filter);

    m_backupAuto = new Ui::CheckBox(QString(), inner);
    m_backupAuto->setChecked(m_settings.backupBeforeInstall);
    root->addWidget(m_backupAuto);

    for (const QString &game : games) {
        QLabel *head = section(inner, gameName(game).toUpper());
        root->addWidget(head);
        m_sections << head;
        if (QolBackups::automaticKinds(game).size() > 1) {
            Row all = addRow(root, tr("EVERYTHING"), tr("All my %1 files").arg(gameName(game)),
                             tr("Every kind below except mods and maps, which can be very large. With the "
                                "box above ticked, this copy is taken before each install."));
            all.game = game;
            m_allRows << all;
        }
        for (const QString &kind : QolBackups::kinds(game)) {
            const QolBackups::Info i = QolBackups::info(m_settings, kind, false);
            Row r = addRow(root, tr("BACKUP"), i.label, i.desc);
            r.game = game;
            r.kind = kind;
            r.tertiary = new QPushButton(tr("More"), r.card);
            r.tertiary->setCursor(Qt::PointingHandCursor);
            r.tertiary->setMinimumHeight(36);
            static_cast<QHBoxLayout *>(r.card->layout())->insertWidget(1, r.tertiary, 0, Qt::AlignVCenter);
            m_rows << r;
        }
    }
    root->addStretch();

    scroll->setWidget(inner);
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(scroll);

    connect(m_backupAuto, &QCheckBox::toggled, this, [this](bool on) {
        m_settings.backupBeforeInstall = on;
        m_settings.saveToIni();
    });
    connect(m_backupOpen, &QPushButton::clicked, this, [this]() {
        const QString dir = backupsDir(m_settings, m_game);
        QDir().mkpath(dir);
        QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
    });
    for (const Row &r : m_allRows) {
        const QString game = r.game;
        connect(r.primary, &QPushButton::clicked, this, [this, game]() { backUpAll(game); });
        connect(r.secondary, &QPushButton::clicked, this, [this, game]() { putAllBack(game); });
    }
    for (const Row &r : m_rows) {
        const QString kind = r.kind;
        connect(r.primary, &QPushButton::clicked, this, [this, kind]() {
            const QolBackups::Info info = QolBackups::info(m_settings, kind);
            if (info.exists) {
                if (!confirm(info.label, tr("Put %1 back?\n\nThe backup from %2 is copied over what is there "
                                            "now. Files added since are left alone.")
                                             .arg(info.title, fmtWhen(info.when))))
                    return;
                runJob(tr("Putting back %1").arg(info.title),
                       [this, kind](QString &err, const std::function<void(const QString &, int)> &p,
                                    const CancelHandle &) {
                           return QolBackups::restore(m_settings, kind, p, err);
                       });
                return;
            }
            auto message = std::make_shared<QString>();
            runJob(tr("Backing up %1").arg(info.title),
                   [this, kind, message](QString &err, const std::function<void(const QString &, int)> &p,
                                         const CancelHandle &) {
                       if (QolBackups::backup(m_settings, kind, false, p, *message) == QolBackups::Result::Failed) {
                           err = *message;
                           return false;
                       }
                       return true;
                   },
                   [this, info, message]() { QMessageBox::information(this, info.label, *message); });
        });
        connect(r.secondary, &QPushButton::clicked, this, [this, kind]() {
            const QolBackups::Info info = QolBackups::info(m_settings, kind, false);
            if (info.exists)
                QDesktopServices::openUrl(QUrl::fromLocalFile(info.folder));
        });
        connect(r.tertiary, &QPushButton::clicked, this, [this, kind]() { backupMenu(kind); });
    }

    retranslate();
    syncFilter();
}

void BackupsPage::showGame(const QString &game)
{
    m_game = game;
    syncFilter();
}

void BackupsPage::syncFilter()
{
    const QStringList games = QolBackups::games();
    for (int i = 0; i < m_filterButtons.size(); ++i) {
        QPushButton *b = m_filterButtons[i];
        b->setChecked(i == 0 ? m_game.isEmpty() : games.value(i - 1) == m_game);
        const char *segment = i == 0 ? "left" : (i == m_filterButtons.size() - 1 ? "right" : "");
        if (b->property("segment").toString() != QLatin1String(segment)) {
            b->setProperty("segment", segment);
            b->style()->unpolish(b);
            b->style()->polish(b);
        }
    }
    const auto shown = [this](const QString &game) { return m_game.isEmpty() || m_game == game; };
    for (int i = 0; i < m_sections.size(); ++i)
        m_sections[i]->setVisible(shown(games.value(i)));
    for (const Row &r : m_allRows)
        r.card->setVisible(shown(r.game));
    for (const Row &r : m_rows)
        r.card->setVisible(shown(r.game));
    m_backupOpen->setToolTip(QDir::toNativeSeparators(backupsDir(m_settings, m_game)));
}

void BackupsPage::backUpAll(const QString &game)
{
    // Kinds that already have a backup keep it: this fills the gaps, it does
    // not overwrite the copies taken before anything was installed.
    auto summary = std::make_shared<QStringList>();
    runJob(tr("Backing up your %1 files").arg(gameName(game)),
           [this, game, summary](QString &err, const std::function<void(const QString &, int)> &p,
                                 const CancelHandle &) {
               for (const QString &k : QolBackups::automaticKinds(game)) {
                   QString msg;
                   if (QolBackups::backup(m_settings, k, false, p, msg) == QolBackups::Result::Failed) {
                       err = msg;
                       return false;
                   }
                   *summary << msg;
               }
               return true;
           },
           [this, summary]() {
               QMessageBox::information(this, tr("Backups"), summary->join(QStringLiteral("\n\n")));
           });
}

void BackupsPage::putAllBack(const QString &game)
{
    QStringList have;
    for (const QString &k : QolBackups::automaticKinds(game))
        if (QolBackups::exists(m_settings, k))
            have << k;
    if (have.isEmpty())
        return;
    QStringList titles;
    for (const QString &k : have)
        titles << QolBackups::info(m_settings, k, false).title;
    if (!confirm(tr("Backups"), tr("Put back %1?\n\nYour backed-up files are copied over what is there now. "
                                   "Files added since are left alone.").arg(titles.join(QStringLiteral(", ")))))
        return;
    runJob(tr("Putting your %1 files back").arg(gameName(game)),
           [this, have](QString &err, const std::function<void(const QString &, int)> &p, const CancelHandle &) {
               for (const QString &k : have)
                   if (!QolBackups::restore(m_settings, k, p, err))
                       return false;
               return true;
           });
}

void BackupsPage::backupMenu(const QString &kind)
{
    const QolBackups::Info info = QolBackups::info(m_settings, kind);
    QMessageBox box(this);
    box.setWindowTitle(info.label);
    box.setText(info.exists
                    ? tr("Backup of %1 from %2: %3, %4.\n\nWhat would you like to do?")
                          .arg(info.title, fmtWhen(info.when), fmtFiles(info.files), fmtSize(info.bytes))
                    : tr("There is no backup of %1 yet.").arg(info.title));
    QAbstractButton *replace = box.addButton(info.exists ? tr("Back up again, replacing it") : tr("Back up now"),
                                             QMessageBox::AcceptRole);
    QAbstractButton *del = info.exists ? box.addButton(tr("Delete this backup"), QMessageBox::DestructiveRole) : nullptr;
    box.addButton(QMessageBox::Cancel);
    box.exec();
    if (box.clickedButton() == replace) {
        if (info.exists && !confirm(info.label, tr("Replace the backup from %1 with what is there now?\n\n"
                                                   "The older copy is the one from before anything was "
                                                   "installed. Once replaced it is gone.")
                                                    .arg(fmtWhen(info.when))))
            return;
        auto message = std::make_shared<QString>();
        runJob(tr("Backing up %1").arg(info.title),
               [this, kind, message](QString &err, const std::function<void(const QString &, int)> &p,
                                     const CancelHandle &) {
                   if (QolBackups::backup(m_settings, kind, true, p, *message) == QolBackups::Result::Failed) {
                       err = *message;
                       return false;
                   }
                   return true;
               },
               [this, info, message]() { QMessageBox::information(this, info.label, *message); });
    } else if (del && box.clickedButton() == del) {
        if (!confirm(info.label, tr("Delete the backup of %1? The files on your PC are not touched.").arg(info.title)))
            return;
        QString err;
        if (!QolBackups::remove(m_settings, kind, err))
            QMessageBox::warning(this, info.label, err);
        refresh();
    }
}

BackupsPage::Row BackupsPage::addRow(QVBoxLayout *into, const QString &kicker, const QString &title,
                                     const QString &desc)
{
    Row r;
    r.card = new QFrame(this);
    r.card->setObjectName("Card");
    auto *h = new QHBoxLayout(r.card);
    h->setContentsMargins(18, 14, 18, 14);
    h->setSpacing(14);
    auto *col = new QVBoxLayout();
    col->setSpacing(3);
    auto *k = new QLabel(kicker, r.card);
    k->setObjectName("CardKicker");
    auto *t = new QLabel(title, r.card);
    t->setObjectName("CardTitle");
    col->addWidget(k);
    col->addWidget(t);
    if (!desc.isEmpty()) {
        auto *d = new QLabel(desc, r.card);
        d->setObjectName("CardDesc");
        d->setWordWrap(true);
        col->addWidget(d);
    }
    r.status = new QLabel(r.card);
    r.status->setObjectName("ModCardMeta");
    col->addWidget(r.status);
    h->addLayout(col, 1);
    r.secondary = new QPushButton(r.card);
    r.secondary->setCursor(Qt::PointingHandCursor);
    r.secondary->setMinimumHeight(36);
    r.primary = new QPushButton(r.card);
    r.primary->setProperty("cssClass", "primary");
    r.primary->setCursor(Qt::PointingHandCursor);
    r.primary->setMinimumHeight(36);
    r.primary->setMinimumWidth(110);
    h->addWidget(r.secondary, 0, Qt::AlignVCenter);
    h->addWidget(r.primary, 0, Qt::AlignVCenter);
    into->addWidget(r.card);
    return r;
}

void BackupsPage::refresh()
{
    // Sizing each backup walks the disk, so a hidden page waits to be shown.
    m_stale = !isVisible();
    if (m_stale)
        return;
    const bool pluto = AppSettings::isPlutoniumRoot(m_settings.plutoniumInstance);
    m_plutoHint->setVisible(!pluto);
    m_plutoHint->setText(tr("Plutonium was not found at %1. Set its folder in Settings to use backups.")
                             .arg(QDir::toNativeSeparators(m_settings.plutoniumInstance)));

    for (Row &r : m_rows) {
        const QolBackups::Info info = QolBackups::info(m_settings, r.kind);
        const QString now = info.liveFiles == 0
            ? tr("nothing of yours there now")
            : tr("%1, %2 there now").arg(fmtFiles(info.liveFiles), fmtSize(info.liveBytes));
        r.status->setText(info.exists ? tr("Backed up %1: %2, %3. (%4)")
                                            .arg(fmtWhen(info.when), fmtFiles(info.files), fmtSize(info.bytes), now)
                                      : tr("No backup yet. (%1)").arg(now));
        r.primary->setText(info.exists ? tr("Put back") : tr("Back up"));
        r.primary->setEnabled(pluto && (info.exists || info.liveFiles > 0));
        r.secondary->setText(tr("Open"));
        r.secondary->setVisible(info.exists);
        r.tertiary->setEnabled(pluto);
    }
    for (Row &r : m_allRows) {
        const QStringList kinds = QolBackups::automaticKinds(r.game);
        int have = 0;
        for (const QString &k : kinds)
            if (QolBackups::exists(m_settings, k))
                ++have;
        r.status->setText(have == 0 ? tr("Nothing backed up yet.")
                                    : tr("%1 of %2 kinds backed up.").arg(have).arg(kinds.size()));
        r.primary->setText(tr("Back up all"));
        r.primary->setEnabled(pluto && have < kinds.size());
        r.secondary->setText(tr("Put all back"));
        r.secondary->setVisible(have > 0);
        r.secondary->setEnabled(pluto);
    }
    m_backupAuto->setEnabled(pluto);
    m_backupOpen->setEnabled(pluto);
}

void BackupsPage::runJob(const QString &title,
                         const std::function<bool(QString &,
                                                  const std::function<void(const QString &, int)> &,
                                                  const CancelHandle &)> &job,
                         const std::function<void()> &after)
{
    // The download panel at the bottom of the window owns the row. The page
    // stays usable while the job runs.
    DownloadCenter::run(this, title, job, [this, title, after](bool ok, const QString &error) {
        if (!ok && error != tr("Cancelled."))
            QMessageBox::warning(this, title, error.isEmpty() ? tr("That did not work.") : error);
        if (ok && after)
            after();
        refresh();
        emit installedChanged();
    });
}

void BackupsPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (m_stale)
        refresh();
}

bool BackupsPage::confirm(const QString &title, const QString &text)
{
    return QMessageBox::question(this, title, text, QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
           == QMessageBox::Yes;
}

void BackupsPage::retranslate()
{
    m_intro->setText(tr("Each kind keeps its oldest copy, the one from before anything was installed, until you "
                        "replace it. Backups are plain folders, so you can also copy them by hand."));
    m_filterButtons.value(0)->setText(tr("All games"));
    const QStringList games = QolBackups::games();
    for (int i = 1; i < m_filterButtons.size(); ++i)
        m_filterButtons[i]->setText(gameName(games.value(i - 1)));
    m_backupOpen->setText(tr("Open backups folder"));
    m_backupAuto->setText(tr("Back up my files before every install"));
    m_backupAuto->setToolTip(tr("Before a mod or pack goes in, the files it would replace are copied to the "
                                "backups folder. Only the first time for each kind; after that the older "
                                "copy is kept."));
    refresh();
}
