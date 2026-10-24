#pragma once
#include <QString>
#include <functional>

// T7 Patch for Black Ops III (github.com/Scroptss/T7Patch, the maintained fork
// of shiversoftdev/t7patch): security fixes for the Steam game's online play.
// The manual install is four files in the game folder; Steam loads them with
// the game through dsound.dll. Everything Blutac adds is listed in
// blutac-t7patch.json in that folder, so Remove takes out exactly that and
// puts back any file it replaced.
//
// The optional Mod Check Bypass plugin (github.com/Callum-Bell/BO3-Mod-Check-Patch)
// lets modded lobbies earn XP. It is only offered behind an ad page, so the
// user downloads its zip and Blutac installs it from there.
namespace T7Patch
{
    using Progress = std::function<void(const QString &text, int percent)>;

    // "v3.09" when Blutac installed it, "manual" when the patch is there but
    // Blutac did not put it there, empty when it is not installed.
    QString installedVersion(const QString &gameDir);
    bool pluginInstalled(const QString &gameDir);
    // True when Blutac keeps a record of files it added here, so Remove can act.
    bool managed(const QString &gameDir);

    // Latest release tag (network). Empty on failure.
    QString latestVersion(QString &error);

    // Installs or updates the patch. Keeps an existing t7patch.conf.
    bool install(const QString &gameDir, const Progress &p, QString &error,
                 const std::function<bool()> &cancelled = {});
    // Removes the plugin too, if installed.
    bool remove(const QString &gameDir, QString &error);

    bool installPlugin(const QString &gameDir, const QString &zipPath, QString &error);
    bool removePlugin(const QString &gameDir, QString &error);

    struct Config
    {
        QString playerName;
        QString networkPassword; // empty = normal matchmaking
        bool friendsOnly = true;
    };
    Config readConfig(const QString &gameDir);
    // Rewrites the three keys and keeps any other lines. The patch rereads the
    // file while the game runs.
    bool writeConfig(const QString &gameDir, const Config &c, QString &error);

    // Name of a running Black Ops III process, or empty. Changes refuse while one runs.
    QString runningGame();
    // -selftest only: its checks use a scratch folder no running game can hold.
    void ignoreRunningGameForSelftest();
}
