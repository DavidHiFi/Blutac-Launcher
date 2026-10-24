#pragma once
#include <QWidget>

#include "Theme.h"

class QPushButton;

// The main window's frame on Windows: dark mode and caption colours that
// follow the theme, the window material (Mica, Mica Alt, Acrylic, Blur,
// Frosted glass) and the optional Blutac title bar, which removes the
// Windows caption and lets the app draw its own buttons in the client area.
// The native frame stays (resize borders, shadow, snapping); only the
// caption goes. On other platforms every call does nothing.
//
// Materials need a window created with WA_TranslucentBackground, and the
// title bar choice is made once when the window is created, so changing
// either from Solid / Windows takes a restart (see MainWindow).
namespace WindowChrome
{
    bool supports(Theme::Material m);       // this Windows build can draw it
    bool supportsCustomTitleBar();

    // Once, right after the native window exists.
    void install(QWidget *window, bool customTitleBar);
    // Dark mode, caption colours and material. Call again on theme changes.
    void apply(QWidget *window, Theme::Material material, bool customTitleBar);
    // From QWidget::nativeEvent. True when the message was handled here.
    bool nativeEvent(QWidget *window, void *message, qintptr *result,
                     QWidget *captionButtons);
}

// Minimise, maximise and close for the Blutac title bar. The maximise
// button reports itself to Windows as the real one, so hovering it opens
// the Windows 11 snap layouts.
class CaptionButtons : public QWidget
{
    Q_OBJECT
public:
    explicit CaptionButtons(QWidget *window, QWidget *parent = nullptr);
    QPushButton *maximizeButton() const { return m_max; }
    void syncState();                       // maximise vs restore glyph
    void setMaxHot(bool hot);               // hover drawn for Windows' sake
    void setMaxPressed(bool pressed);

private:
    QWidget *m_window;
    QPushButton *m_min;
    QPushButton *m_max;
    QPushButton *m_close;
};
