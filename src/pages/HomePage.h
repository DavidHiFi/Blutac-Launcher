#pragma once
#include <QHash>
#include <QTimer>
#include <QWidget>

#include "../HomeCatalog.h"

class QComboBox;
class QLineEdit;
class QPushButton;
class QVBoxLayout;
class QHBoxLayout;
class QScrollArea;
class QLabel;
class QNetworkAccessManager;
class AppSettings;
class HeroSlider;

// Workshop: featured slider, then mods, GSC scripts, texture and sound packs
// and Plutonium add-ons, browsed per game and per category, fed by
// qol_home.json. Scripts install straight into Plutonium's script folders;
// packs and add-ons go through WorkshopItems.
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
    void installedChanged();                      // a pack, add-on or script came or went

protected:
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void buildUi();
    void rebuildFilters();
    void rebuildContent();
    void relayoutGrids();
    int columnCount() const;
    void openDetails(const QString &modId);
    void requestRemote(const QString &url, const QString &modId);
    QPixmap resolvedCover(const QString &ref) const;
    void applyRemoteCover(const QString &modId, const QPixmap &pm);
    void applyRemoteUrl(const QString &url, const QPixmap &pm);
    QList<HomeCatalog::Mod> filteredMods(const QString &gameId) const;
    bool matchesFilters(const HomeCatalog::Mod &m) const;
    void syncChips();
    void installItem(const QString &modId);
    void removeItem(const QString &modId);
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
    QWidget *m_typeBar = nullptr;
    QList<QPushButton *> m_filterButtons;
    QList<QPushButton *> m_typeButtons;
    QComboBox *m_sort = nullptr;
    QLabel *m_sortLabel = nullptr;
    QPushButton *m_addScriptBtn = nullptr;
    QPushButton *m_shareBtn = nullptr;
    HeroSlider *m_hero = nullptr;
    QScrollArea *m_scroll = nullptr;
    QWidget *m_content = nullptr;
    QVBoxLayout *m_contentLayout = nullptr;
    QLabel *m_status = nullptr;
    QPushButton *m_importBtn = nullptr;

    QString m_game;                            // empty = every game
    QString m_type = QStringLiteral("all");    // "all" | "mod" | "script" | "texture" | "sound"
    QString m_query;
    int m_columns = 0;                         // card columns the rows were built for
    QTimer m_searchDebounce;
    QTimer m_relayout;                         // rebuilds rows after the column count changes
    QNetworkAccessManager *m_net = nullptr;
    QHash<QString, QPixmap> m_remote; // url -> pixmap ja baixado
};
