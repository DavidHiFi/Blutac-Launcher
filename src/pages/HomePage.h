#pragma once
#include <QHash>
#include <QSet>
#include <QTimer>
#include <QWidget>

#include "../HomeCatalog.h"

class QLineEdit;
class QPushButton;
class QVBoxLayout;
class QHBoxLayout;
class QScrollArea;
class QLabel;
class QNetworkAccessManager;
class AppSettings;
class HeroSlider;

// Workshop: featured slider, then mods and GSC scripts grouped by game, fed by
// qol_home.json. Scripts install straight into Plutonium's script folders.
class HomePage : public QWidget
{
    Q_OBJECT
public:
    explicit HomePage(AppSettings &settings, QWidget *parent = nullptr);

    void reload();
    void refreshInstallState();
    void selectGame(const QString &gameId);       // "" / "all" = todos
    void retranslate();

signals:
    void installRequested(const QString &modId);  // ligar no CllInstaller / GithubModInstaller
    void detailsRequested(const QString &modId);
    void gameRequested(const QString &gameId);    // clique em "Ver todos"

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void buildUi();
    void rebuildFilters();
    void rebuildContent();
    void relayoutGrids();
    void openDetails(const QString &modId);
    void requestRemote(const QString &url, const QString &modId);
    QPixmap resolvedCover(const QString &ref) const;
    void applyRemoteCover(const QString &modId, const QPixmap &pm);
    void applyRemoteUrl(const QString &url, const QPixmap &pm);
    QList<HomeCatalog::Mod> filteredMods(const QString &gameId) const;
    bool matchesFilters(const HomeCatalog::Mod &m) const;
    void togglePick(const QString &id);
    void syncChips();
    void retranslateBar();
    void setType(const QString &type);
    HomeCatalog::Presence presenceOf(const HomeCatalog::Mod &m) const;
    QString scriptDir(const HomeCatalog::Mod &m, QString *missingMod = nullptr) const;
    void installScript(const QString &modId);
    void removeScript(const QString &modId);
    void addLocalScript();
    void showStatus(const QString &text);

    AppSettings &m_settings;
    HomeCatalog::Catalog m_catalog;
    HomeCatalog::InstallIndex m_installs;

    QLineEdit *m_search = nullptr;
    QWidget *m_filterBar = nullptr;
    QHBoxLayout *m_filterLayout = nullptr; // games
    QHBoxLayout *m_hubLayout = nullptr;    // mods that scripts can be made for
    QList<QPushButton *> m_filterButtons;
    QList<QPushButton *> m_typeButtons;
    QPushButton *m_addScriptBtn = nullptr;
    QPushButton *m_shareBtn = nullptr;
    HeroSlider *m_hero = nullptr;
    QScrollArea *m_scroll = nullptr;
    QWidget *m_content = nullptr;
    QVBoxLayout *m_contentLayout = nullptr;
    QLabel *m_status = nullptr;
    QPushButton *m_importBtn = nullptr;

    QSet<QString> m_picked;                    // game ids and hub mod ids; empty = everything
    QString m_type = QStringLiteral("all");    // "all" | "mod" | "script"
    QString m_query;
    QTimer m_searchDebounce;
    QNetworkAccessManager *m_net = nullptr;
    QHash<QString, QPixmap> m_remote; // url -> pixmap ja baixado
};
