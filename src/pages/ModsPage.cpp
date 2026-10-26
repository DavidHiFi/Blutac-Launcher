#include "../Checkables.h"
#include "ModsPage.h"
#include "AppSettings.h"
#include "ArchiveTool.h"
#include "CllInstaller.h"
#include "ModPreview.h"
#include "GameCatalog.h"
#include "Dialogs.h"
#include "Downloader.h"
#include "GithubModInstaller.h"
#include "HomeCatalog.h"
#include "QolService.h"
#include "ScriptStore.h"
#include "SmartModInstaller.h"
#include "Storage.h"
#include "../WorkshopItems.h"

#include <QComboBox>
#include <QSet>
#include <algorithm>
#include <QClipboard>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QStackedWidget>
#include <QStyle>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>
#include <QMetaObject>
#include <QMimeData>
#include <QUrl>

namespace {
bool safeRemoveTempDir(const QString &path)
{
    if (path.isEmpty())
        return false;
    const QFileInfo fi(path);
    const QString abs = QDir::cleanPath(fi.absoluteFilePath());
    if (abs.isEmpty() || abs == QLatin1String("/") || abs == QLatin1String(".") )
        return false;
    if (abs.length() < 8)
        return false;
    const QString name = fi.fileName();
    if (!name.startsWith(QLatin1String("LanLauncher")))
        return false;
    const QString tempRoot = QDir::cleanPath(QDir::tempPath());
    if (!abs.startsWith(tempRoot, Qt::CaseInsensitive))
        return false;
    return QDir(abs).removeRecursively();
}
}

ModsPage::ModsPage(AppSettings &settings, QWidget *parent)
    : QWidget(parent)
    , m_settings(settings)
{
    // Game picker lives on the library card header.
    // Install sits on a separate card below.
    setAcceptDrops(true);
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(28, 22, 28, 22);
    root->setSpacing(14);

    auto *listCard = new QFrame(this);
    listCard->setObjectName("Card");
    auto *listLayout = new QVBoxLayout(listCard);
    listLayout->setContentsMargins(18, 15, 18, 16);
    listLayout->setSpacing(12);

    auto *head = new QHBoxLayout();
    head->setSpacing(8);
    auto *headCol = new QVBoxLayout();
    headCol->setSpacing(2);
    auto *listKicker = new QLabel(tr("BIBLIOTECA"), listCard);
    listKicker->setProperty("role", "modsListKicker");
    listKicker->setObjectName("CardKicker");
    m_listTitle = new QLabel(listCard);
    m_listTitle->setObjectName("CardTitle");
    headCol->addWidget(listKicker);
    headCol->addWidget(m_listTitle);
    head->addLayout(headCol, 1);

    // Mods | Scripts | Texture packs | Sound packs | Plutonium: which kind of
    // install the list below manages.
    auto *views = new QHBoxLayout();
    views->setSpacing(0);
    for (int i = 0; i < ViewCount; ++i) {
        auto *b = new QPushButton(listCard);
        b->setObjectName("TypeSegment");
        b->setCheckable(true);
        b->setChecked(i == 0);
        b->setCursor(Qt::PointingHandCursor);
        b->setMinimumHeight(32);
        connect(b, &QPushButton::clicked, this, [this, i]() { showView(i); });
        views->addWidget(b);
        m_viewButtons << b;
    }
    views->addStretch(1);

    m_gameCombo = new Ui::ComboBox(listCard);
    // The item data is the settings id ("Black ops II"); the text is the title.
    for (const QString &id : {"World at War", "Black ops", "Black ops II", "Modern Warfare 3"})
        m_gameCombo->addItem(GameCatalog::byId(id).title, id);
    m_gameCombo->setCurrentIndex(m_gameCombo->findData(QStringLiteral("Black ops II")));
    m_gameCombo->setMinimumWidth(200);
    auto *refreshBtn = new QPushButton(tr("Atualizar"), listCard);
    refreshBtn->setObjectName("modsRefresh");
    refreshBtn->setProperty("cssClass", "ghost");
    auto *deselectBtn = new QPushButton(tr("Limpar selecao"), listCard);
    deselectBtn->setObjectName("modsDeselect");
    deselectBtn->setProperty("cssClass", "ghost");
    head->addWidget(m_gameCombo);
    head->addWidget(refreshBtn);
    head->addWidget(deselectBtn);
    listLayout->addLayout(head);
    listLayout->addLayout(views);

    m_titleLabel = new QLabel(tr("Selecione um jogo para ver os mods"), listCard);
    m_titleLabel->setObjectName("MutedHint");
    listLayout->addWidget(m_titleLabel);

    // Scripts and packs get their own lists so selectedMod() (the mod a
    // launch loads) can never pick one of them up.
    m_listStack = new QStackedWidget(listCard);
    m_modList = new QListWidget(m_listStack);
    m_scriptList = new QListWidget(m_listStack);
    m_packList = new QListWidget(m_listStack);
    m_packList->setWordWrap(true);
    m_listStack->addWidget(m_modList);
    m_listStack->addWidget(m_scriptList);
    m_listStack->addWidget(m_packList);
    listLayout->addWidget(m_listStack, 1);

    auto *listFoot = new QHBoxLayout();
    m_listHint = new QLabel(listCard);
    m_listHint->setObjectName("MutedHint");
    m_listHint->setWordWrap(true);
    m_deleteBtn = new QPushButton(tr("Excluir mod"), listCard);
    auto *deleteBtn = m_deleteBtn;
    deleteBtn->setObjectName("modsDelete");
    deleteBtn->setProperty("cssClass", "danger");
    listFoot->addWidget(m_listHint, 1);
    listFoot->addStretch();
    listFoot->addWidget(deleteBtn);
    listLayout->addLayout(listFoot);
    root->addWidget(listCard, 1);

    auto *installCard = new QFrame(this);
    installCard->setObjectName("Card");
    auto *installLayout = new QVBoxLayout(installCard);
    installLayout->setContentsMargins(18, 15, 18, 16);
    installLayout->setSpacing(10);
    auto *installKicker = new QLabel(tr("INSTALAR"), installCard);
    installKicker->setProperty("role", "modsInstallKicker");
    installKicker->setObjectName("CardKicker");
    auto *installTitle = new QLabel(tr("Add a mod, map or script"), installCard);
    installTitle->setProperty("role", "modsInstallTitle");
    installTitle->setObjectName("CardTitle");
    auto *installDesc = new QLabel(
        tr("Zip, Rar, 7z, exe, .cll, .gsc or a GitHub link. You can also drop a file here."), installCard);
    installDesc->setObjectName("modsInstallDesc");
    installLayout->addWidget(installKicker);
    installLayout->addWidget(installTitle);
    installLayout->addWidget(installDesc);
    auto *row = new QHBoxLayout();
    row->setSpacing(8);
    m_modPathEdit = new QLineEdit(installCard);
    m_modPathEdit->setPlaceholderText(tr("Arquivo do mod ou link do GitHub"));
    auto *browseBtn = new QPushButton(tr("Procurar"), installCard);
    browseBtn->setObjectName("modsBrowse");
    m_installBtn = new QPushButton(tr("Instalar mod"), installCard);
    m_installBtn->setProperty("cssClass", "primary");
    m_installBtn->setMinimumWidth(140);
    m_installBtn->setCursor(Qt::PointingHandCursor);
    row->addWidget(m_modPathEdit, 1);
    row->addWidget(browseBtn);
    row->addWidget(m_installBtn);
    installLayout->addLayout(row);
    m_progressLabel = new QLabel(installCard);
    m_progressLabel->setObjectName("MutedHint");
    m_progress = new QProgressBar(installCard);
    m_progress->setRange(0, 100);
    m_progress->setValue(0);
    m_progress->setVisible(false);
    m_progressLabel->setVisible(false);
    m_cancelBtn = new QPushButton(tr("Cancelar"), installCard);
    m_cancelBtn->setVisible(false);
    auto *progressRow = new QHBoxLayout();
    progressRow->setSpacing(8);
    progressRow->addWidget(m_progress, 1);
    progressRow->addWidget(m_cancelBtn);
    installLayout->addWidget(m_progressLabel);
    installLayout->addLayout(progressRow);
    connect(m_cancelBtn, &QPushButton::clicked, this, [this]() {
        if (m_cancel)
            m_cancel->cancel();
        m_cancelBtn->setEnabled(false); // the stop has landed; the row shows it stop
    });
    root->addWidget(installCard);

    connect(m_gameCombo, &QComboBox::currentIndexChanged, this,
            [this]() { onGameSelected(m_gameCombo->currentData().toString()); });
    connect(refreshBtn, &QPushButton::clicked, this, [this]() { onRefreshOrDeselect(false); });
    m_modList->setCursor(Qt::PointingHandCursor);
    connect(m_modList, &QListWidget::currentItemChanged, this, [this]() { emit selectionChanged(); });
    connect(deselectBtn, &QPushButton::clicked, this, [this]() { onRefreshOrDeselect(true); });
    connect(browseBtn, &QPushButton::clicked, this, &ModsPage::onBrowseModFile);
    connect(m_installBtn, &QPushButton::clicked, this, &ModsPage::onInstallMod);
    connect(deleteBtn, &QPushButton::clicked, this, [this]() {
        if (m_view == ViewMods)
            onDeleteMod();
        else if (m_view == ViewScripts)
            onDeleteScript();
        else
            onDeletePack();
    });

    connect(&m_installWatcher, &QFutureWatcher<QString>::finished, this, [this]() {
        const Job job = m_job;
        // Cleared before setBusy, which only resets the Delete label once no
        // Remove job is running.
        m_job = Job::None;
        setBusy(false);
        m_progress->setVisible(false);
        m_progressLabel->setVisible(false);
        m_cancelBtn->setVisible(false);
        const QString err = m_installWatcher.result();
        if (job == Job::Peek && err == QLatin1String("__PEEK_OK__")) {
            auto preview = m_pendingPreview;
            if (!preview.error.isEmpty() && preview.mappings.isEmpty()) {
                safeRemoveTempDir(m_pendingExtract);
                Dialogs::error(this, preview.error);
                return;
            }
            if (m_pendingGithub && m_pendingGhMan.hasExecutables) {
                QMessageBox warn(this);
                warn.setIcon(QMessageBox::Warning);
                warn.setWindowTitle(tr("Unsafe files"));
                warn.setText(tr("This release contains .exe or .dll files that can harm your computer.\n"
                                "The program does not verify the origin of these files."));
                if (!m_pendingGhMan.executableNames.isEmpty())
                    warn.setInformativeText(m_pendingGhMan.executableNames.mid(0, 12).join(QStringLiteral("\n")));
                warn.addButton(tr("Cancel"), QMessageBox::RejectRole);
                auto *go = warn.addButton(tr("Install anyway"), QMessageBox::AcceptRole);
                warn.exec();
                if (warn.clickedButton() != go) {
                    m_pendingGithub = false;
                    return;
                }
            }
            if (!ModPreview::confirm(this, preview)) {
                safeRemoveTempDir(m_pendingExtract);
                m_pendingGithub = false;
                return;
            }
            const QString pu = m_settings.plutoniumInstance;
            const QString gameFolder = m_settings.gameFolder(m_settings.modId);
            const QString archivePath = m_pendingArchive;
            const QString tempDir = m_pendingExtract;
            const bool github = m_pendingGithub;
            const auto ghMan = m_pendingGhMan;
            m_pendingGithub = false;
            m_job = Job::Install;
            setBusy(true);
            // A fresh install gets a fresh stop flag; a cancelled one from
            // the last job must never trip this run.
            m_cancel = std::make_shared<CancelState>();
            m_cancelBtn->setEnabled(true);
            m_cancelBtn->setVisible(true);
            m_progress->setVisible(true);
            m_progressLabel->setVisible(true);
            m_progress->setRange(0, 100);
            m_progress->setValue(0);
            m_progressLabel->setText(github ? tr("Downloading pack...") : tr("Extraindo pacote..."));
            const CancelHandle cancel = m_cancel;
            auto future = QtConcurrent::run([=]() -> QString {
                if (github) {
                    QList<GithubModInstaller::FailedDownload> failed;
                    const QString e = GithubModInstaller::apply(
                        ghMan, preview, pu, gameFolder,
                        [this](int pct, const QString &label) {
                            QMetaObject::invokeMethod(this, [this, pct, label]() {
                                m_progress->setRange(0, 100);
                                m_progress->setValue(pct);
                                m_progressLabel->setText(label);
                            }, Qt::QueuedConnection);
                        },
                        &failed, cancel);
                    QMetaObject::invokeMethod(this, [this, failed]() {
                        m_lastGhFailed = failed;
                    }, Qt::BlockingQueuedConnection);
                    return e;
                }
                safeRemoveTempDir(tempDir);
                QDir().mkpath(tempDir);
                QString extractError;
                const bool extracted = ArchiveTool::extractToDirectory(
                    archivePath, tempDir, &extractError,
                    [this](int pct) {
                        QMetaObject::invokeMethod(this, [this, pct]() {
                            m_progress->setRange(0, 100);
                            m_progress->setValue(int(pct * 0.7));
                            m_progressLabel->setText(tr("Extraindo pacote... %1%").arg(pct));
                        }, Qt::QueuedConnection);
                    },
                    [cancel]() { return cancel && *cancel; });
                if (!extracted) {
                    safeRemoveTempDir(tempDir);
                    return extractError.isEmpty() ? QObject::tr("Falha ao extrair o arquivo.") : extractError;
                }
                // The copy phase swaps live files in the game folders, one
                // rename from a stop to finished; do not sit on the flag here.
                if (cancel && *cancel) {
                    safeRemoveTempDir(tempDir);
                    return QObject::tr("Cancelled.");
                }
                QMetaObject::invokeMethod(this, [this]() {
                    m_progress->setValue(72);
                    m_progressLabel->setText(tr("Copiando arquivos..."));
                }, Qt::QueuedConnection);
                auto ready = preview;
                ready.extractedRoot = tempDir;
                const QString e = ModPreview::apply(ready, pu, gameFolder, archivePath);
                safeRemoveTempDir(tempDir);
                return e;
            });
            m_installWatcher.setFuture(future);
            return;
        }
        if (job == Job::Install || job == Job::Remove) {
            refreshList();
            emit catalogChanged();
        }
        if (err.isEmpty()) {
            if (job == Job::Remove)
                QMessageBox::information(this, tr("Mods"), tr("Mod removido."));
            else
                QMessageBox::information(this, tr("Mods"), tr("Mod instalado com sucesso."));
        } else if (err == QLatin1String("Cancelled.")) {
            // A stopped install is the user's choice; whatever already landed
            // landed as a prefix and the list refresh above shows it.
        } else if (err == QLatin1String("__PARTIAL__")) {
            QStringList lines;
            QString clip;
            const int show = qMin(12, m_lastGhFailed.size());
            for (int i = 0; i < m_lastGhFailed.size(); ++i) {
                const auto &f = m_lastGhFailed[i];
                clip += f.url + QLatin1Char('\n') + f.destPath + QLatin1String("\n\n");
                if (i < show)
                    lines << f.name;
            }
            if (m_lastGhFailed.size() > show)
                lines << tr("… e mais %1").arg(m_lastGhFailed.size() - show);
            QMessageBox box(this);
            box.setIcon(QMessageBox::Warning);
            box.setWindowTitle(tr("Missing files"));
            box.setText(tr("Pack installed, but %1 file(s) failed and were skipped.\n"
                           "Download them manually and put each file in the destination folder.")
                            .arg(m_lastGhFailed.size()));
            box.setInformativeText(lines.join(QLatin1Char('\n')));
            auto *copyBtn = box.addButton(tr("Copy links"), QMessageBox::ActionRole);
            box.addButton(tr("OK"), QMessageBox::AcceptRole);
            box.exec();
            if (box.clickedButton() == copyBtn) {
                QGuiApplication::clipboard()->setText(clip.trimmed());
                QMessageBox::information(this, tr("Missing files"),
                                         tr("Links and destination folders were copied."));
            }
        } else {
            m_pendingGithub = false;
            Dialogs::error(this, err);
        }
    });

    connect(&m_sevenZipWatcher, &QFutureWatcher<int>::finished, this, [this]() {
        setBusy(false);
        m_progress->setVisible(false);
        m_progressLabel->setVisible(false);
        m_cancelBtn->setVisible(false);
        switch (m_sevenZipWatcher.result()) {
        case 0: Dialogs::info(this, Dialogs::Msg::SevenZipSuccess, m_settings); break;
        case 1: Dialogs::info(this, Dialogs::Msg::SevenZipFailDownload, m_settings); break;
        default: Dialogs::info(this, Dialogs::Msg::SevenZipFailInstall, m_settings); break;
        }
    });

    onGameSelected(m_gameCombo->currentData().toString());
    showView(0);
}

void ModsPage::showView(int index)
{
    m_view = index;
    syncViews();
    m_listStack->setCurrentIndex(qMin(m_view, int(ViewTextures)));
    refreshPacks();
    applyViewText();
}

// Texture and sound packs exist for World at War, Black Ops and Black Ops II
// only. Plutonium add-ons work in every game, so that view hides the game box.
void ModsPage::syncViews()
{
    const bool packs = Storage::gameStorageId(m_settings.modId) != QLatin1String("iw5");
    if (!packs && (m_view == ViewTextures || m_view == ViewSounds))
        m_view = ViewMods;
    QList<QPushButton *> shown;
    for (int i = 0; i < m_viewButtons.size(); ++i) {
        QPushButton *b = m_viewButtons[i];
        const bool show = packs || (i != ViewTextures && i != ViewSounds);
        b->setVisible(show);
        if (show)
            shown << b;
        b->setChecked(i == m_view);
    }
    for (QPushButton *b : m_viewButtons) {
        const char *segment = b == shown.value(0) ? "left" : (b == shown.value(shown.size() - 1) ? "right" : "");
        if (b->property("segment").toString() != QLatin1String(segment)) {
            b->setProperty("segment", segment);
            b->style()->unpolish(b);
            b->style()->polish(b);
        }
    }
    m_gameCombo->setVisible(m_view != ViewPlutonium);
}

void ModsPage::applyViewText()
{
    const QString game = m_gameCombo->currentText();
    const QStringList names = {tr("Mods"), tr("Scripts"), tr("Texture packs"), tr("Sound packs"),
                               QStringLiteral("Plutonium")};
    for (int i = 0; i < m_viewButtons.size() && i < names.size(); ++i)
        m_viewButtons[i]->setText(names[i]);

    const QString folder = QStringLiteral("storage\\%1\\%2")
                               .arg(Storage::gameStorageId(m_settings.modId),
                                    m_view == ViewTextures ? QStringLiteral("images") : QStringLiteral("zone"));
    QString title, hint, line, remove;
    switch (m_view) {
    case ViewScripts:
        title = tr("Installed scripts");
        hint = tr("Plutonium loads these every time you start the game, with or without a mod.");
        line = tr("Scripts for %1").arg(game);
        remove = tr("Remove script");
        break;
    case ViewTextures:
        title = tr("Installed texture packs");
        hint = tr("Texture packs sit in %1 and load with every mod.").arg(folder);
        line = tr("Texture packs for %1").arg(game);
        remove = tr("Remove pack");
        break;
    case ViewSounds:
        title = tr("Installed sound packs");
        hint = tr("Sound packs sit in %1 and load with every mod.").arg(folder);
        line = tr("Sound packs for %1").arg(game);
        remove = tr("Remove pack");
        break;
    case ViewPlutonium:
        title = tr("Plutonium add-ons");
        hint = tr("Turn them on or off for each launch on the Play page.");
        line = tr("These work in every Plutonium game.");
        remove = tr("Remove add-on");
        break;
    default:
        title = tr("Mods instalados");
        hint = tr("O mod selecionado e usado ao iniciar o jogo.");
        line = tr("Mods disponiveis para %1").arg(game);
        remove = tr("Excluir mod");
        break;
    }
    m_listTitle->setText(title);
    m_listHint->setText(hint);
    m_titleLabel->setText(game.isEmpty() && m_view != ViewPlutonium ? tr("Selecione um jogo para ver os mods") : line);
    if (m_job != Job::Remove)
        m_deleteBtn->setText(remove);
}

QString ModsPage::packType() const
{
    switch (m_view) {
    case ViewTextures: return QStringLiteral("texture");
    case ViewSounds: return QStringLiteral("sound");
    case ViewPlutonium: return QStringLiteral("plutonium");
    default: return QString();
    }
}

// The Workshop installs packs and add-ons; this list shows what is in place
// and takes them out again.
void ModsPage::refreshPacks()
{
    m_packList->clear();
    const QString type = packType();
    if (type.isEmpty())
        return;
    const QList<WorkshopItems::Installed> items =
        WorkshopItems::installed(m_settings, type, Storage::gameStorageId(m_settings.modId));
    for (const WorkshopItems::Installed &it : items) {
        auto *item = new QListWidgetItem(it.detail.isEmpty() ? it.name : it.name + QLatin1Char('\n') + it.detail);
        item->setData(Qt::UserRole, it.id);
        item->setData(Qt::UserRole + 1, it.name);
        item->setData(Qt::UserRole + 2, it.game);
        m_packList->addItem(item);
    }
    if (items.isEmpty()) {
        const QString text = type == QLatin1String("plutonium")
            ? tr("Nothing installed yet. ReShade and DLSS 5 are in the Workshop under Plutonium.")
            : (type == QLatin1String("texture") ? tr("No texture packs yet. The Workshop has some for %1.")
                                                : tr("No sound packs yet. The Workshop has some for %1."))
                  .arg(m_gameCombo->currentText());
        auto *empty = new QListWidgetItem(text);
        empty->setFlags(Qt::NoItemFlags);
        m_packList->addItem(empty);
    }
}

void ModsPage::onDeletePack()
{
    auto *item = m_packList->currentItem();
    const QString id = item ? item->data(Qt::UserRole).toString() : QString();
    if (id.isEmpty()) {
        QMessageBox::information(this, m_deleteBtn->text(), tr("Pick one in the list first."));
        return;
    }
    WorkshopItems::remove(this, m_settings,
                          {id, item->data(Qt::UserRole + 1).toString(), item->data(Qt::UserRole + 2).toString(),
                           QString()},
                          [this]() {
                              refreshPacks();
                              emit catalogChanged();
                          });
}

void ModsPage::refreshScripts()
{
    m_scriptList->clear();
    const QString sid = Storage::gameStorageId(m_settings.modId);
    for (const ScriptStore::Installed &s : ScriptStore::installed(m_settings.plutoniumInstance, sid)) {
        auto *item = new QListWidgetItem(QDir::toNativeSeparators(s.label));
        item->setData(Qt::UserRole, s.path);
        item->setToolTip(QDir::toNativeSeparators(s.path));
        m_scriptList->addItem(item);
    }
}

void ModsPage::onDeleteScript()
{
    auto *item = m_scriptList->currentItem();
    if (!item) {
        QMessageBox::information(this, tr("Remove script"), tr("Pick a script in the list first."));
        return;
    }
    const QString path = item->data(Qt::UserRole).toString();
    if (QMessageBox::question(this, tr("Remove script"),
                              tr("Move %1 to the Recycle Bin?").arg(item->text()))
        != QMessageBox::Yes)
        return;
    const QString err = ScriptStore::remove(path);
    if (!err.isEmpty())
        QMessageBox::warning(this, tr("Remove script"), err);
    refreshScripts();
    emit catalogChanged();
}

void ModsPage::importScript(const QString &path)
{
    const QString done = ScriptStore::importWithDialog(this, m_settings.plutoniumInstance, path,
                                                       Storage::gameStorageId(m_settings.modId));
    if (done.isEmpty())
        return;
    m_modPathEdit->clear();
    refreshScripts();
    showView(1);
    emit catalogChanged();
}

QString ModsPage::selectedMod() const
{
    auto *item = m_modList->currentItem();
    if (!item)
        return QString();
    const QString id = item->data(Qt::UserRole).toString();
    return id.isEmpty() ? item->text() : id;
}

void ModsPage::refreshList()
{
    if (m_settings.modId.isEmpty())
        return;
    const QString path = Storage::subdir(m_settings.plutoniumInstance, m_settings.modId, "mods");
    QStringList folders = Storage::listModFolders(path);
    const QString sid = Storage::gameStorageId(m_settings.modId);
    const QString pu = m_settings.plutoniumInstance;
    const QStringList ckpts = SmartModInstaller::checkpointIds(pu, sid);
    for (const QString &id : ckpts)
        SmartModInstaller::cleanupEmptyAliasFolder(pu, sid, id);

    QStringList hidden;
    for (const QString &id : ckpts) {
        for (const QString &owned : SmartModInstaller::checkpointOwnedModFolders(pu, sid, id)) {
            if (owned.compare(id, Qt::CaseInsensitive) != 0)
                hidden << owned;
        }
    }

    QSet<QString> hiddenSet;
    for (const QString &h : hidden)
        hiddenSet.insert(h.toLower());

    struct Row { QString id; QString label; };
    QList<Row> rows;
    QSet<QString> seen;
    for (const QString &id : ckpts) {
        if (hiddenSet.contains(id.toLower()))
            continue;
        const QString label = SmartModInstaller::checkpointDisplayName(pu, sid, id);
        rows.append({id, label.isEmpty() ? id : label});
        seen.insert(id.toLower());
    }
    for (const QString &folder : folders) {
        bool hide = seen.contains(folder.toLower());
        for (const QString &h : hidden) {
            if (folder.compare(h, Qt::CaseInsensitive) == 0)
                hide = true;
        }
        if (hide)
            continue;
        // The Workshop installs Quality of Life as zm_qol; show its name.
        const QString qol = QolService::seriesFor(sid).folder;
        const bool isQol = !qol.isEmpty() && folder.compare(qol, Qt::CaseInsensitive) == 0;
        rows.append({folder, isQol ? QStringLiteral("Quality of Life (%1)").arg(folder) : folder});
        seen.insert(folder.toLower());
    }
    std::sort(rows.begin(), rows.end(), [](const Row &a, const Row &b) {
        return a.label.compare(b.label, Qt::CaseInsensitive) < 0;
    });
    // Keep the selected mod across refreshes; losing it here made a launch
    // silently start without the mod.
    const QString keep = selectedMod();
    m_modList->clear();
    for (const Row &r : rows) {
        auto *item = new QListWidgetItem(r.label);
        item->setData(Qt::UserRole, r.id);
        m_modList->addItem(item);
        if (!keep.isEmpty() && r.id.compare(keep, Qt::CaseInsensitive) == 0)
            m_modList->setCurrentItem(item);
    }
    refreshScripts();
    refreshPacks();
    emit selectionChanged();
}

void ModsPage::onGameSelected(const QString &gameName)
{
    if (gameName.isEmpty())
        return;
    m_settings.modId = gameName;
    if (m_listStack) {
        syncViews();
        m_listStack->setCurrentIndex(qMin(m_view, int(ViewTextures)));
        applyViewText();
    }
    refreshList();
}

void ModsPage::selectGame(const QString &gameId)
{
    if (gameId.isEmpty())
        return;
    const int idx = m_gameCombo->findData(gameId);
    if (idx >= 0)
        m_gameCombo->setCurrentIndex(idx);
    onGameSelected(m_gameCombo->currentData().toString());
}

void ModsPage::onRefreshOrDeselect(bool clearSelection)
{
    refreshList();
    if (clearSelection) {
        // In the Scripts view this clears the script pick, never the mod the
        // next launch loads.
        if (m_view == ViewScripts)
            m_scriptList->setCurrentRow(-1);
        else if (m_view == ViewMods)
            m_modList->setCurrentRow(-1);
        else
            m_packList->setCurrentRow(-1);
    }
}

void ModsPage::onBrowseModFile()
{
    const QString file = QFileDialog::getOpenFileName(
        this, tr("Procurar"), QString(),
        tr("Mods and scripts (*.zip *.rar *.7z *.exe *.cll *.gsc *.csc)"));
    if (!file.isEmpty())
        handleModFile(file);
}

void ModsPage::setBusy(bool busy)
{
    m_installBtn->setDisabled(busy);
    if (m_deleteBtn)
        m_deleteBtn->setDisabled(busy);
    if (m_modList)
        m_modList->setDisabled(busy);
    if (busy && m_job == Job::Remove)
        m_installBtn->setText(tr("Instalar mod"));
    else
        m_installBtn->setText(busy ? tr("Instalando...") : tr("Instalar mod"));
    if (m_deleteBtn && busy && m_job == Job::Remove)
        m_deleteBtn->setText(tr("Removendo..."));
    else if (m_deleteBtn)
        applyViewText();
}

void ModsPage::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void ModsPage::dropEvent(QDropEvent *event)
{
    const auto urls = event->mimeData()->urls();
    if (urls.isEmpty())
        return;
    const QString path = urls.first().toLocalFile();
    if (!path.isEmpty())
        handleModFile(path);
}

void ModsPage::handleModFile(const QString &path)
{
    if (ScriptStore::isScriptFile(path)) {
        importScript(path);
        return;
    }
    m_modPathEdit->setText(path);
    if (!CllInstaller::isSupportedArchive(path))
        return;
    if (CllInstaller::isCllPack(path) || CllInstaller::archiveMentionsInstaller(path))
        tryCllInstall(path);
}

bool ModsPage::tryCllInstall(const QString &path)
{
    if (!ArchiveTool::hasSevenZip()) {
        if (Dialogs::confirmDownload7z(this))
            runSevenZipBootstrap();
        return true;
    }
    const auto man = CllInstaller::peekArchive(path);
    if (!man.valid) {
        if (CllInstaller::isCllPack(path)) {
            Dialogs::error(this, man.error.isEmpty()
                                     ? tr("A .cll file must contain cll_installer.json.")
                                     : man.error);
            return true;
        }
        return false;
    }
    if (!man.gameId.isEmpty())
        selectGame(man.gameId);
    const QString pu = m_settings.plutoniumInstance;
    const QString gameFolder = m_settings.gameFolder(man.gameId);
    bool makeBackup = true;
    if (!CllInstaller::confirmAndShow(this, man, pu, gameFolder, &makeBackup))
        return true;
    m_job = Job::Install;
    setBusy(true);
    m_progress->setVisible(true);
    m_progressLabel->setVisible(true);
    m_progress->setRange(0, 100);
    m_progress->setValue(0);
    m_progressLabel->setText(tr("Extracting pack..."));
    auto future = QtConcurrent::run([=]() -> QString {
        const QString tempDir = QDir::temp().filePath("LanLauncher_cll_extract");
        safeRemoveTempDir(tempDir);
        QDir().mkpath(tempDir);
        QString extractError;
        const bool extracted = ArchiveTool::extractToDirectory(
            path, tempDir, &extractError,
            [this](int pct) {
                QMetaObject::invokeMethod(this, [this, pct]() {
                    m_progress->setRange(0, 100);
                    m_progress->setValue(int(pct * 0.45));
                    m_progressLabel->setText(tr("Extracting pack... %1%").arg(pct));
                }, Qt::QueuedConnection);
            });
        if (!extracted) {
            safeRemoveTempDir(tempDir);
            return extractError.isEmpty() ? QObject::tr("Failed to extract the pack.") : extractError;
        }
        QMetaObject::invokeMethod(this, [this]() {
            m_progress->setValue(45);
            m_progressLabel->setText(tr("Copying files..."));
        }, Qt::QueuedConnection);
        const QString err = CllInstaller::apply(
            man, tempDir, pu, gameFolder, path,
            [this](int pct, const QString &label) {
                QMetaObject::invokeMethod(this, [this, pct, label]() {
                    m_progress->setRange(0, 100);
                    m_progress->setValue(45 + int(pct * 0.55));
                    m_progressLabel->setText(label);
                }, Qt::QueuedConnection);
            },
            makeBackup);
        safeRemoveTempDir(tempDir);
        return err;
    });
    m_installWatcher.setFuture(future);
    return true;
}

void ModsPage::installFromCatalog(const QString &modId)
{
    const auto cat = HomeCatalog::load();
    const auto *mod = HomeCatalog::findMod(cat, modId);
    if (!mod || mod->url.isEmpty())
        return;
    if (!mod->game.isEmpty())
        selectGame(mod->game);
    if (GithubModInstaller::looksLikeUrl(mod->url)
        || mod->kind.compare(QLatin1String("github"), Qt::CaseInsensitive) == 0
        || mod->kind.compare(QLatin1String("host"), Qt::CaseInsensitive) == 0) {
        tryGithubInstall(mod->url);
        return;
    }
    handleModFile(HomeCatalog::resolveAsset(mod->url));
}

void ModsPage::tryGithubInstall(const QString &url)
{
    const QString pu = m_settings.plutoniumInstance;
    if (pu.isEmpty()) {
        Dialogs::error(this, tr("Configure a pasta do Plutonium em Configuracoes antes de instalar mods."));
        return;
    }
    const QString gameId = m_settings.modId;
    const QString gameCode = Storage::gameStorageId(m_settings.modId);
    const QString gameFolder = m_settings.gameFolder(gameId);
    m_pendingArchive = url;
    m_pendingExtract.clear();
    m_pendingGithub = true;
    m_job = Job::Peek;
    setBusy(true);
    m_progress->setVisible(true);
    m_progressLabel->setVisible(true);
    m_progress->setRange(0, 0);
    m_progressLabel->setText(tr("Reading GitHub manifest..."));
    auto future = QtConcurrent::run([this, url, gameId, gameCode, pu, gameFolder]() -> QString {
        QString error;
        ModPreview::Preview preview;
        const auto man = GithubModInstaller::peek(url, gameId, gameCode, pu, gameFolder, &preview, error);
        QMetaObject::invokeMethod(this, [this, preview, man]() {
            m_pendingPreview = preview;
            m_pendingGhMan = man;
            if (!preview.gameId.isEmpty())
                selectGame(preview.gameId);
        }, Qt::BlockingQueuedConnection);
        if (!man.valid || !preview.valid())
            return error.isEmpty()
                       ? (preview.error.isEmpty() ? QObject::tr("Could not read the GitHub pack.") : preview.error)
                       : error;
        return QStringLiteral("__PEEK_OK__");
    });
    m_installWatcher.setFuture(future);
}

void ModsPage::onInstallMod()
{
    // The visible combo is the source of truth — do not rely on a signal
    // that may not have fired on first open.
    const QString game = m_gameCombo->currentData().toString();
    if (game.isEmpty()) {
        Dialogs::error(this, tr("Selecione um jogo na lista acima antes de instalar o mod."));
        return;
    }
    m_settings.modId = game;

    const QString archivePath = m_modPathEdit->text().trimmed();
    if (GithubModInstaller::looksLikeUrl(archivePath)) {
        tryGithubInstall(archivePath);
        return;
    }
    if (ScriptStore::isScriptFile(archivePath) && QFileInfo::exists(archivePath)) {
        importScript(archivePath);
        return;
    }
    if (archivePath.isEmpty() || !QFileInfo::exists(archivePath)) {
        Dialogs::error(this, tr("Escolha o arquivo do mod (zip, rar, 7z, exe ou cll) em Procurar."));
        return;
    }
    if (!ArchiveTool::hasSevenZip()) {
        if (Dialogs::confirmDownload7z(this))
            runSevenZipBootstrap();
        return;
    }
    if (CllInstaller::isCllPack(archivePath) || CllInstaller::archiveMentionsInstaller(archivePath)) {
        tryCllInstall(archivePath);
        return;
    }
    installStandardOrSmart(archivePath);
}

void ModsPage::installStandardOrSmart(const QString &archivePath)
{
    const QString pu = m_settings.plutoniumInstance;
    const QString gameFolder = m_settings.gameFolder(m_settings.modId);

    if (pu.isEmpty()) {
        Dialogs::error(this, tr("Configure a pasta do Plutonium em Configuracoes antes de instalar mods."));
        return;
    }

    setBusy(true);
    m_progress->setVisible(true);
    m_progressLabel->setVisible(true);
    m_progress->setRange(0, 0);

    m_pendingArchive = archivePath;
    m_pendingExtract = QDir::temp().filePath("LanLauncher_mod_extract");
    const QString gameId = m_settings.modId;
    const QString gameCode = Storage::gameStorageId(m_settings.modId);
    m_job = Job::Peek;
    m_progressLabel->setText(tr("Lendo pacote..."));
    auto future = QtConcurrent::run([this, archivePath, gameId, gameCode, pu, gameFolder]() -> QString {
        const auto preview = ModPreview::analyzeArchive(archivePath, gameId, gameCode, pu, gameFolder);
        QMetaObject::invokeMethod(this, [this, preview]() {
            m_pendingPreview = preview;
        }, Qt::BlockingQueuedConnection);
        if (!preview.valid())
            return preview.error.isEmpty() ? QObject::tr("Nao foi possivel projetar a instalacao deste pacote.") : preview.error;
        return QStringLiteral("__PEEK_OK__");
    });
    m_installWatcher.setFuture(future);
}

void ModsPage::onDeleteMod()
{
    const QString path = Storage::subdir(m_settings.plutoniumInstance, m_settings.modId, "mods");
    const QString modToDelete = selectedMod();
    if (modToDelete.isEmpty()) {
        Dialogs::info(this, Dialogs::Msg::NoMod, m_settings);
        return;
    }

    const QString sid = Storage::gameStorageId(m_settings.modId);
    const bool hasCkpt = SmartModInstaller::hasCheckpoint(m_settings.plutoniumInstance, sid, modToDelete);
    QMessageBox box(this);
    box.setWindowTitle(tr("Excluir mod"));
    box.setText(hasCkpt
                    ? tr("Remover \"%1\" e restaurar os arquivos originais do checkpoint?\n"
                         "Os arquivos que este mod adicionou serao apagados.\n"
                         "Os que foram substituidos voltam ao estado anterior.\n"
                         "O backup do checkpoint sera excluido em seguida.")
                          .arg(modToDelete)
                    : tr("Remover a pasta do mod \"%1\"? Nao ha checkpoint para restaurar originais.")
                          .arg(modToDelete));
    auto *yes = box.addButton(tr("Excluir e restaurar"), QMessageBox::AcceptRole);
    if (!hasCkpt)
        yes->setText(tr("Excluir"));
    box.addButton(tr("Cancelar"), QMessageBox::RejectRole);
    box.exec();
    if (box.clickedButton() != yes)
        return;

    const QString pu = m_settings.plutoniumInstance;
    const QString gameFolder = m_settings.gameFolder(m_settings.modId);
    m_job = Job::Remove;
    setBusy(true);
    m_progress->setVisible(true);
    m_progressLabel->setVisible(true);
    m_progress->setRange(0, 0);
    m_progressLabel->setText(tr("Removing mod..."));
    auto future = QtConcurrent::run([=]() -> QString {
        const QString err = SmartModInstaller::rollback(pu, gameFolder, sid, modToDelete);
        if (!err.isEmpty())
            return err;
        if (!path.isEmpty())
            QDir(path + "/" + modToDelete).removeRecursively();
        return QString();
    });
    m_installWatcher.setFuture(future);
}

void ModsPage::runSevenZipBootstrap()
{
    setBusy(true);
    m_progress->setVisible(true);
    m_progressLabel->setVisible(true);
    m_progress->setRange(0, 100);
    m_progress->setValue(0);
    m_progressLabel->setText(tr("Baixando 7-Zip..."));

    auto future = QtConcurrent::run([this]() -> int {
        const QString zipPath = QDir::temp().filePath("LanLauncherQt_7z.zip");
        QString error;
        if (!Downloader::downloadToFile(
                "https://raw.githubusercontent.com/JugAndDoubleTap/LanLauncher/main/7z.zip",
                zipPath, error,
                [this](qint64 got, qint64 total) {
                    QMetaObject::invokeMethod(this, [this, got, total]() {
                        if (total > 0) {
                            m_progress->setRange(0, 100);
                            m_progress->setValue(int(got * 100 / total));
                        }
                        m_progressLabel->setText(tr("Baixando 7-Zip... %1 / %2 KB")
                                                     .arg(got / 1024).arg(total > 0 ? total / 1024 : 0));
                    }, Qt::QueuedConnection);
                })) {
            return 1;
        }
        QMetaObject::invokeMethod(this, [this]() {
            m_progress->setRange(0, 0);
            m_progressLabel->setText(tr("Extraindo 7-Zip..."));
        }, Qt::QueuedConnection);
        const bool ok = ArchiveTool::extractBootstrapZip(zipPath, QCoreApplication::applicationDirPath());
        QFile::remove(zipPath);
        return ok ? 0 : 2;
    });
    m_sevenZipWatcher.setFuture(future);
}

void ModsPage::retranslate()
{
    // The card labels carry their style in objectName, so they are found by role.
    auto label = [this](const char *role) -> QLabel * {
        for (QLabel *l : findChildren<QLabel *>())
            if (l->property("role").toByteArray() == role)
                return l;
        return nullptr;
    };
    if (auto *w = label("modsListKicker")) w->setText(tr("BIBLIOTECA"));
    if (auto *w = findChild<QPushButton*>("modsRefresh")) w->setText(tr("Atualizar"));
    if (auto *w = findChild<QPushButton*>("modsDeselect")) w->setText(tr("Limpar selecao"));
    if (auto *w = label("modsInstallKicker")) w->setText(tr("INSTALAR"));
    if (auto *w = label("modsInstallTitle")) w->setText(tr("Add a mod, map or script"));
    if (auto *w = findChild<QLabel*>("modsInstallDesc"))
        w->setText(tr("Zip, Rar, 7z, exe, .cll, .gsc or a GitHub link. You can also drop a file here."));
    if (auto *w = findChild<QPushButton*>("modsBrowse")) w->setText(tr("Procurar"));
    if (m_installBtn) m_installBtn->setText(tr("Instalar mod"));
    if (m_modPathEdit) m_modPathEdit->setPlaceholderText(tr("Arquivo do mod ou link do GitHub"));
    applyViewText();
}
