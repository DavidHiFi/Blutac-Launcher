#pragma once
#include "DownloadCenter.h"
#include <QWidget>
#include <QList>
#include <functional>

class QCheckBox;
class QLabel;
class QPushButton;
class QVBoxLayout;
class QFrame;
class AppSettings;

// Backups of the player's own files for every Plutonium game, one section per
// game with a filter on top. Installing and removing mods and packs happens
// in the Workshop and on the Mods page; this page keeps the copies and puts
// them back.
class BackupsPage : public QWidget
{
    Q_OBJECT
public:
    explicit BackupsPage(AppSettings &settings, QWidget *parent = nullptr);
    void refresh();
    void retranslate();

signals:
    // A put-back changed files the other pages list.
    void installedChanged();

protected:
    void showEvent(QShowEvent *event) override;

private:
    struct Row {
        QFrame *card = nullptr;
        QLabel *status = nullptr;
        QPushButton *primary = nullptr;
        QPushButton *secondary = nullptr;
        QPushButton *tertiary = nullptr;   // "More" on the per-kind rows
        QString game;
        QString kind;                      // empty on a game's "everything" row
    };
    Row addRow(QVBoxLayout *into, const QString &kicker, const QString &title, const QString &desc);
    void runJob(const QString &title,
                const std::function<bool(QString &, const std::function<void(const QString &, int)> &,
                                         const CancelHandle &)> &job,
                const std::function<void()> &after = {});
    bool confirm(const QString &title, const QString &text);
    void backupMenu(const QString &kind);
    void backUpAll(const QString &game);
    void putAllBack(const QString &game);
    void showGame(const QString &game);   // empty shows every game
    void syncFilter();

    AppSettings &m_settings;
    QLabel *m_intro = nullptr;
    QLabel *m_plutoHint = nullptr;
    QCheckBox *m_backupAuto = nullptr;
    QPushButton *m_backupOpen = nullptr;
    QList<QPushButton *> m_filterButtons;  // "All games", then QolBackups::games()
    QList<QLabel *> m_sections;            // parallel to QolBackups::games()
    QList<Row> m_allRows;                  // one per game with more than one kind
    QList<Row> m_rows;                     // parallel to QolBackups::kinds()
    QString m_game;
    bool m_stale = true;                   // a refresh came while hidden
};
