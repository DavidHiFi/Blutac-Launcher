#include "ScriptStore.h"
#include "Checkables.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSaveFile>
#include <QVBoxLayout>

#include <algorithm>

namespace ScriptStore
{

namespace {

QString storage(const QString &root, const QString &game)
{
    if (root.isEmpty() || game.isEmpty())
        return QString();
    return QDir::cleanPath(root) + QStringLiteral("/storage/") + game;
}

} // namespace

// T6 and IW5 follow Plutonium's docs (plutonium.pw/docs/modding/loading-mods).
// The docs skip T4 and T5; their folders are the ones existing Plutonium
// installs load from: zombies runs in the sp scripts on both.
QString folder(const QString &root, const QString &game, const QString &mode,
               const QString &modFolder)
{
    const QString base = storage(root, game);
    if (base.isEmpty())
        return QString();
    // The mod folder comes from the catalog: one plain folder name, nothing
    // that could climb out of storage.
    if (modFolder.contains(QLatin1String("..")) || modFolder.contains(QLatin1Char('/'))
        || modFolder.contains(QLatin1Char('\\')) || modFolder.contains(QLatin1Char(':')))
        return QString();
    const bool mp = mode == QLatin1String("mp");
    if (game == QLatin1String("t6")) {
        const QString sub = mp ? QStringLiteral("/scripts/mp") : QStringLiteral("/scripts/zm");
        if (!modFolder.isEmpty())
            return base + QStringLiteral("/mods/") + modFolder + sub;
        return base + sub;
    }
    if (game == QLatin1String("t5"))
        return base + (mp ? QStringLiteral("/raw/scripts/mp") : QStringLiteral("/raw/scripts/sp/zom"));
    if (game == QLatin1String("t4"))
        return base + (mp ? QStringLiteral("/raw/scripts/mp") : QStringLiteral("/raw/scripts/sp"));
    if (game == QLatin1String("iw5"))
        return base + QStringLiteral("/scripts");
    return QString();
}

bool isScriptFile(const QString &path)
{
    return path.endsWith(QLatin1String(".gsc"), Qt::CaseInsensitive)
        || path.endsWith(QLatin1String(".csc"), Qt::CaseInsensitive);
}

QList<Installed> installed(const QString &root, const QString &game)
{
    QList<Installed> out;
    const QString base = storage(root, game);
    if (base.isEmpty())
        return out;
    for (const QString &sub : {QStringLiteral("/scripts"), QStringLiteral("/raw/scripts")}) {
        QDirIterator it(base + sub, {QStringLiteral("*.gsc"), QStringLiteral("*.csc")},
                        QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString path = QDir::cleanPath(it.next());
            out << Installed{path, QDir(base).relativeFilePath(path)};
        }
    }
    std::sort(out.begin(), out.end(), [](const Installed &a, const Installed &b) {
        return a.label.compare(b.label, Qt::CaseInsensitive) < 0;
    });
    return out;
}

QString write(const QString &dir, const QString &name, const QByteArray &bytes)
{
    if (dir.isEmpty())
        return QObject::tr("Set the Plutonium folder in Settings first.");
    if (name.isEmpty() || name.contains(QLatin1Char('/')) || name.contains(QLatin1Char('\\'))
        || !isScriptFile(name))
        return QObject::tr("The script has no usable file name.");
    if (!QDir().mkpath(dir))
        return QObject::tr("Could not create %1").arg(QDir::toNativeSeparators(dir));
    QSaveFile f(dir + QLatin1Char('/') + name);
    if (!f.open(QIODevice::WriteOnly) || f.write(bytes) != bytes.size() || !f.commit())
        return QObject::tr("Could not write %1: %2").arg(name, f.errorString());
    return QString();
}

QString remove(const QString &path)
{
    if (!isScriptFile(path))
        return QObject::tr("Only .gsc and .csc files can be removed here.");
    // Recycle Bin only: the messages promise the file can come back.
    if (QFile::moveToTrash(path))
        return QString();
    return QObject::tr("Could not remove %1").arg(QDir::toNativeSeparators(path));
}

QString importWithDialog(QWidget *parent, const QString &root, const QString &file,
                         const QString &defaultGame)
{
    if (root.isEmpty()) {
        QMessageBox::warning(parent, QObject::tr("Add a script"),
                             QObject::tr("Set the Plutonium folder in Settings first."));
        return QString();
    }

    QDialog dlg(parent);
    dlg.setWindowTitle(QObject::tr("Add a script"));
    dlg.setMinimumWidth(420);
    auto *v = new QVBoxLayout(&dlg);
    v->setContentsMargins(20, 18, 20, 16);
    v->setSpacing(12);
    auto *name = new QLabel(QFileInfo(file).fileName(), &dlg);
    name->setObjectName("CardTitle");
    v->addWidget(name);

    auto *form = new QFormLayout();
    auto *game = new Ui::ComboBox(&dlg);
    const QList<QPair<QString, QString>> games = {
        {QStringLiteral("t6"), QStringLiteral("Black Ops II")},
        {QStringLiteral("t5"), QStringLiteral("Black Ops")},
        {QStringLiteral("t4"), QStringLiteral("World at War")},
        {QStringLiteral("iw5"), QStringLiteral("Modern Warfare 3")},
    };
    for (const auto &g : games)
        game->addItem(g.second, g.first);
    const int at = game->findData(defaultGame);
    game->setCurrentIndex(at < 0 ? 0 : at);
    auto *mode = new Ui::ComboBox(&dlg);
    mode->addItem(QObject::tr("Zombies"), QStringLiteral("zm"));
    mode->addItem(QObject::tr("Multiplayer"), QStringLiteral("mp"));
    form->addRow(QObject::tr("Game"), game);
    form->addRow(QObject::tr("Mode"), mode);
    v->addLayout(form);

    auto *where = new QLabel(&dlg);
    where->setObjectName("MutedHint");
    where->setWordWrap(true);
    v->addWidget(where);
    auto sync = [&]() {
        const bool iw5 = game->currentData().toString() == QLatin1String("iw5");
        if (iw5)
            mode->setCurrentIndex(1);
        mode->setEnabled(!iw5);
        where->setText(QObject::tr("Goes to %1").arg(QDir::toNativeSeparators(
            folder(root, game->currentData().toString(), mode->currentData().toString()))));
    };
    QObject::connect(game, &QComboBox::currentIndexChanged, &dlg, sync);
    QObject::connect(mode, &QComboBox::currentIndexChanged, &dlg, sync);
    sync();

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    buttons->button(QDialogButtonBox::Ok)->setText(QObject::tr("Add script"));
    buttons->button(QDialogButtonBox::Ok)->setProperty("cssClass", "primary");
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    v->addWidget(buttons);
    if (dlg.exec() != QDialog::Accepted)
        return QString();

    QFile src(file);
    if (!src.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(parent, QObject::tr("Add a script"), src.errorString());
        return QString();
    }
    const QString dir = folder(root, game->currentData().toString(), mode->currentData().toString());
    const QString target = dir + QLatin1Char('/') + QFileInfo(file).fileName();
    if (QFileInfo::exists(target)
        && QMessageBox::question(parent, QObject::tr("Add a script"),
                                 QObject::tr("%1 is already there. Replace it?")
                                     .arg(QDir::toNativeSeparators(target)))
               != QMessageBox::Yes)
        return QString();
    const QString err = write(dir, QFileInfo(file).fileName(), src.readAll());
    if (!err.isEmpty()) {
        QMessageBox::warning(parent, QObject::tr("Add a script"), err);
        return QString();
    }
    return dir + QLatin1Char('/') + QFileInfo(file).fileName();
}

} // namespace ScriptStore
