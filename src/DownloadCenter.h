#pragma once
#include "CancelState.h"
#include <QList>
#include <QPointer>
#include <QString>
#include <QWidget>
#include <functional>

// One panel for every download the app runs: QoL installs, game clients,
// updates. Sits at the bottom of the main window, so nothing blocks the
// rest of the app while a job runs - every page stays usable, the user can
// come back, and each row carries its own Cancel.
//
// A job may flip its CancelHandle to "uncancellable" at the point where
// stopping would leave live files half-written (backups already taken,
// replacement going in). From there the button reads Finishing... until the
// row settles, and the game's own rollback covers the rest.

class DownloadCenter : public QWidget
{
    Q_OBJECT
public:
    struct Row;   // one panel row; defined in the .cpp
    using Progress = std::function<void(const QString &text, int percent)>;
    // The old job shape plus a cancel handle, so existing work moves over
    // without rewriting what the job actually does.
    using Job = std::function<bool(QString &, const Progress &, const CancelHandle &)>;

    static DownloadCenter *instance();

    // Registers a row and runs the job on QtConcurrent. `after` runs on the
    // GUI thread with (ok, error) when the job settles. Call from the GUI
    // thread (every caller here is one).
    static void run(QWidget *parentWindow, const QString &title, const Job &job,
                    const std::function<void(bool ok, const QString &error)> &after = {});

    int activeJobs() const;
    void retranslate();

signals:
    void anyBusyChanged(bool busy);

private:
    explicit DownloadCenter(QWidget *parent = nullptr);
    void start(const QString &title, const Job &job,
               const std::function<void(bool ok, const QString &error)> &after);
    void finishRow(Row *row, bool ok, const QString &error);
    void activeChanged();

    class QVBoxLayout *m_rows = nullptr;
    class QLabel *m_headline = nullptr;
    QList<Row *> m_all;
    bool m_any = false;
};
