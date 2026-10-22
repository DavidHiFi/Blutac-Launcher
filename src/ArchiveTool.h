#pragma once
#include <QString>
#include <QStringList>
#include <QList>
#include <functional>

namespace ArchiveTool
{
    enum class ExtractResult
    {
        Success,
        UnsupportedExtension,
        NotStandardModFormat,
        Failed,
        SevenZipMissing,
    };

    // Looks for bundled/system 7-Zip or a NanaZip command-line alias.
    QString findSevenZip();
    bool hasSevenZip();
    // Writes the 7z.exe/7z.dll carried in the exe's resources to <appdir>/7z/
    // on first use, so a player never needs to install 7-Zip. No-op when a
    // copy is already found. False when the folder could not be written
    // (read-only install location); the download-7-Zip flows stay as the fallback.
    bool ensureBundledSevenZip();

    ExtractResult extractArchive(const QString &modFolder, const QString &archivePath);
    bool extractToDirectory(const QString &archivePath, const QString &destDir, QString *errorOut = nullptr,
                           const std::function<void(int percent)> &onProgress = nullptr,
                           const std::function<bool()> &cancelled = nullptr);
    QStringList listEntries(const QString &archivePath);

    struct ListedFile {
        QString path;
        qint64 size = 0;
        bool isDir = false;
    };
    QList<ListedFile> listDetailed(const QString &archivePath);
    bool extractPaths(const QString &archivePath, const QString &destDir,
                      const QStringList &innerPaths, QString *errorOut = nullptr);

    // pu.dat = LLQTPKG1 header + XOR'd 7z. This is not real encryption.
    bool extractObfuscatedBundle(const QString &datPath, const QString &destDir, QString *errorOut = nullptr);

    bool extractBootstrapZip(const QString &zipPath, const QString &destDir);
}
