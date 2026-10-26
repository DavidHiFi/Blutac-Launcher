#pragma once
#include <QWidget>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFutureWatcher>
#include "CancelState.h"
#include "ModPreview.h"
#include "GithubModInstaller.h"

class QComboBox;
class QListWidget;
class QLineEdit;
class QPushButton;
class QLabel;
class QProgressBar;
class QStackedWidget;
class AppSettings;
class HomePage;

class ModsPage : public QWidget
{
    Q_OBJECT
public:
    explicit ModsPage(AppSettings &settings, QWidget *parent = nullptr);
    QString selectedMod() const;
    void refreshList();
    void selectGame(const QString &gameId);
    void installFromCatalog(const QString &modId);

signals:
    void catalogChanged();
    void selectionChanged();

private slots:
    void onGameSelected(const QString &gameName);
    void onRefreshOrDeselect(bool clearSelection);
    void onBrowseModFile();
    void onInstallMod();
    void onDeleteMod();

private:
    void setBusy(bool busy);
    void runSevenZipBootstrap();
    void installStandardOrSmart(const QString &archivePath);
    void handleModFile(const QString &path);
    bool tryCllInstall(const QString &path);
    void tryGithubInstall(const QString &url);
    enum View { ViewMods, ViewScripts, ViewTextures, ViewSounds, ViewPlutonium, ViewCount };
    void showView(int index);
    void syncViews();
    void applyViewText();
    void refreshScripts();
    void onDeleteScript();
    void refreshPacks();
    void onDeletePack();
    QString packType() const;   // "texture", "sound" or "plutonium" for the pack views
    void importScript(const QString &path);
protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
public:
    void retranslate();
private:

    AppSettings &m_settings;
    QComboBox *m_gameCombo = nullptr;
    QLabel *m_titleLabel = nullptr;
    QListWidget *m_modList = nullptr;
    QListWidget *m_scriptList = nullptr;
    QListWidget *m_packList = nullptr;   // texture packs, sound packs or Plutonium add-ons
    QStackedWidget *m_listStack = nullptr;
    int m_view = ViewMods;
    QList<QPushButton *> m_viewButtons;
    QLabel *m_listTitle = nullptr;
    QLabel *m_listHint = nullptr;
    QLineEdit *m_modPathEdit = nullptr;
    QPushButton *m_installBtn = nullptr;
    QPushButton *m_deleteBtn = nullptr;
    QProgressBar *m_progress = nullptr;
    QLabel *m_progressLabel = nullptr;
    QPushButton *m_cancelBtn = nullptr;  // stops the running install from the install card
    CancelHandle m_cancel;               // fresh install: new flag, new button
    enum class Job { None, Install, Remove, Peek };
    QString m_pendingExtract;
    QString m_pendingArchive;
    ModPreview::Preview m_pendingPreview;
    bool m_pendingGithub = false;
    GithubModInstaller::Manifest m_pendingGhMan;
    QList<GithubModInstaller::FailedDownload> m_lastGhFailed;
    Job m_job = Job::None;

    QFutureWatcher<QString> m_installWatcher; // empty = ok; otherwise an error string
    QFutureWatcher<int> m_sevenZipWatcher;
};
