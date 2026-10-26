#pragma once
#include "HomeCatalog.h"
#include <QList>
#include <QString>
#include <functional>

class AppSettings;
class QWidget;

// Workshop items the launcher installs itself instead of through a mod's own
// release: texture and sound packs, the Plutonium add-ons (ReShade, DLSS 5)
// and the Quality of Life mods. The Workshop and the Mods tab both go through
// here, so they always agree on what is installed.
namespace WorkshopItems
{
    struct Installed {
        QString id;       // catalog id
        QString name;
        QString game;     // t4 t5 t6, or "plutonium"
        QString detail;   // one line for the Mods tab
    };

    bool handles(const HomeCatalog::Mod &m);
    HomeCatalog::Presence presence(const AppSettings &s, const HomeCatalog::Mod &m);
    // type is "texture", "sound" or "plutonium"; game is ignored for "plutonium".
    QList<Installed> installed(const AppSettings &s, const QString &type, const QString &game);

    // Both run their work in the download panel and call `done` on the UI
    // thread once it is over, whether it worked or not.
    using Done = std::function<void()>;
    void install(QWidget *parent, AppSettings &s, const HomeCatalog::Mod &m, const Done &done);
    void remove(QWidget *parent, AppSettings &s, const Installed &item, const Done &done);
}
