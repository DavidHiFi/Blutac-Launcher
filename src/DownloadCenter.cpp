#include "DownloadCenter.h"
#include "Version.h"

#include <QApplication>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMetaObject>
#include <QProgressBar>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

// A row owns no life of its own: it is plain data around widgets parented to
// its card, and DownloadCenter deletes it eight seconds after the job ends
// (or when the next job starts, whichever comes first). Status from the
// worker always lands before the finishing call - the queue is FIFO - so no
// report ever touches a row the panel has already taken down.
struct DownloadCenter::Row
{
    QFrame card;
    QLabel *title = nullptr;
    QLabel *status = nullptr;
    QProgressBar *bar = nullptr;
    QPushButton *cancel = nullptr;
    CancelHandle handle = std::make_shared<CancelState>();
    bool done = false;
};

namespace {

void paintCancelButton(DownloadCenter::Row *row)
{
    if (row->handle->locked()) {
        row->cancel->setText(QObject::tr("Finishing..."));
        row->cancel->setEnabled(false);
        return;
    }
    if (row->handle->requested.load()) {
        row->cancel->setText(QObject::tr("Cancelling..."));
        row->cancel->setEnabled(false);
    }
}

} // namespace

DownloadCenter *DownloadCenter::instance()
{
    static DownloadCenter *one = nullptr;
    if (!one)
        one = new DownloadCenter();
    return one;
}

DownloadCenter::DownloadCenter(QWidget *parent) : QWidget(parent)
{
    setObjectName("DownloadCenter");
    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(12, 8, 12, 10);
    lay->setSpacing(6);
    m_headline = new QLabel(this);
    m_headline->setObjectName("DownloadsHead");
    lay->addWidget(m_headline);
    m_rows = new QVBoxLayout();
    m_rows->setContentsMargins(0, 0, 0, 0);
    m_rows->setSpacing(4);
    lay->addLayout(m_rows);
    hide();
}

void DownloadCenter::run(QWidget *parentWindow, const QString &title, const Job &job,
                         const std::function<void(bool, const QString &)> &after)
{
    Q_UNUSED(parentWindow);
    instance()->start(title, job, after);
}

void DownloadCenter::start(const QString &title, const Job &job,
                           const std::function<void(bool ok, const QString &error)> &after)
{
    // Rows from jobs that already ended go first; a fresh run starts clean.
    for (int i = m_all.size() - 1; i >= 0; --i) {
        if (m_all[i]->done) {
            m_all[i]->card.deleteLater();
            delete m_all[i];
            m_all.removeAt(i);
        }
    }

    auto *row = new Row();
    row->card.setObjectName("DownloadCard");
    auto *lay = new QVBoxLayout(&row->card);
    lay->setContentsMargins(10, 6, 10, 8);
    lay->setSpacing(3);

    auto *top = new QHBoxLayout();
    top->setSpacing(8);
    row->title = new QLabel(title, &row->card);
    row->title->setObjectName("DownloadTitle");
    top->addWidget(row->title, 1);
    row->cancel = new QPushButton(tr("Cancel"), &row->card);
    row->cancel->setCursor(Qt::PointingHandCursor);
    row->cancel->setObjectName("DownloadCancel");
    top->addWidget(row->cancel, 0, Qt::AlignVCenter);
    lay->addLayout(top);

    row->status = new QLabel(tr("Starting..."), &row->card);
    row->status->setObjectName("MutedHint");
    row->status->setWordWrap(true);
    lay->addWidget(row->status);

    row->bar = new QProgressBar(&row->card);
    row->bar->setRange(0, 0); // indeterminate until the job reports a percent
    row->bar->setMaximumHeight(10);
    lay->addWidget(row->bar);

    connect(row->cancel, &QPushButton::clicked, this, [row]() {
        if (!row->handle->locked()) {
            row->handle->requested.store(true);
            paintCancelButton(row);
        }
    });

    m_rows->addWidget(&row->card);
    m_all.append(row);
    activeChanged();

    const QPointer<DownloadCenter> self(this);
    auto report = [self, row](const QString &text, int pct) {
        QMetaObject::invokeMethod(qApp, [self, row, text, pct]() {
            if (!self)
                return;
            row->status->setText(text);
            if (pct < 0)
                row->bar->setRange(0, 0);
            else {
                row->bar->setRange(0, 100);
                row->bar->setValue(qBound(0, pct, 100));
            }
        }, Qt::QueuedConnection);
    };

    const CancelHandle handle = row->handle;

    const auto future = QtConcurrent::run([self, job, handle, report, row, after]() {
        QString error;
        const bool ok = job(error, report, handle);
        QMetaObject::invokeMethod(qApp, [self, row, ok, error, after]() {
            if (!self)
                return;
            self->finishRow(row, ok, error);
            if (after)
                after(ok, ok ? QString() : error);
        }, Qt::QueuedConnection);
    });
    Q_UNUSED(future);
}

void DownloadCenter::finishRow(Row *row, bool ok, const QString &error)
{
    if (row->done)
        return;
    row->done = true;
    row->cancel->setEnabled(false);
    row->cancel->setText(ok ? tr("Done") : tr("Cancelled"));
    row->status->setText(ok ? tr("Done.") : (error.isEmpty() ? tr("Failed.") : error));
    row->bar->setRange(0, 100);
    row->bar->setValue(100);

    // The row lingers eight seconds so the user sees how it ended, then the
    // panel goes dark if nothing else runs.
    QTimer::singleShot(8000, this, [this, row]() {
        m_all.removeAll(row);
        row->card.deleteLater();
        delete row;
        activeChanged();
    });
    activeChanged();
}

void DownloadCenter::activeChanged()
{
    const bool any = !m_all.isEmpty();
    const bool wasAny = m_any;
    m_any = any;
    setVisible(any);
    if (any != wasAny)
        emit anyBusyChanged(any);
}

int DownloadCenter::activeJobs() const
{
    int live = 0;
    for (const Row *r : m_all)
        if (!r->done)
            ++live;
    return live;
}

void DownloadCenter::retranslate()
{
    if (m_headline)
        m_headline->setText(tr("Downloads"));
}
