#pragma once
#include "AppSettings.h"
#include <QWidget>

class QLineEdit;
class QPushButton;
class QLabel;
class QComboBox;
class QCheckBox;
class QFrame;
class QSlider;
class QRadioButton;
class QVBoxLayout;

class SettingsPage : public QWidget
{
    Q_OBJECT
public:
    explicit SettingsPage(AppSettings &settings, QWidget *parent = nullptr);
    void pullFromSettings();
    void beginEdit();
    void saveEdit();
    void cancelEdit();
    void retranslate();
    // MainWindow says whether the chosen material or title bar waits for a restart.
    void setRestartNeeded(bool needed);

signals:
    void lookChanged();       // theme, accent, material, tint or title bar edited
    void restartRequested();
    void plutoniumFolderChanged();
    void homeEnabledChanged(bool enabled);
    void cancelled();
    void checkUpdatesRequested();

private:
    QLineEdit *folderRow(class QVBoxLayout *layout, const QString &label,
                          QString AppSettings::*field);
    void updateKitButton();
    QFrame *buildAppearanceCard(QWidget *parent);
    void syncAppearance();    // controls <- m_settings
    void setAccentSpec(const QString &spec);

    AppSettings &m_settings;
    QLineEdit *m_username = nullptr;
    QLineEdit *m_plutonium = nullptr;
    QPushButton *m_kitBtn = nullptr;
    QLabel *m_kitHint = nullptr;
    QComboBox *m_langCombo = nullptr;
    QComboBox *m_themeCombo = nullptr;
    QList<QPushButton *> m_accentButtons; // property "accent": "", "system" or "#rrggbb"
    QLineEdit *m_accentHex = nullptr;
    QComboBox *m_materialCombo = nullptr;
    QLabel *m_materialDesc = nullptr;
    QSlider *m_tint = nullptr;
    QLabel *m_tintValue = nullptr;
    QRadioButton *m_titleWindows = nullptr;
    QRadioButton *m_titleBlutac = nullptr;
    QLabel *m_lookHint = nullptr;
    QWidget *m_restartRow = nullptr;
    bool m_restartNeeded = false;
    QCheckBox *m_homeEnabled = nullptr;
    QCheckBox *m_checkUpdates = nullptr;
    QPushButton *m_checkUpdatesBtn = nullptr;
    QPushButton *m_saveBtn = nullptr;
    QPushButton *m_cancelBtn = nullptr;
    AppSettings m_snapshot;
};
