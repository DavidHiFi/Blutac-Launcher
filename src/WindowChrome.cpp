#if defined(_WIN32) && (!defined(_WIN32_WINNT) || _WIN32_WINNT < 0x0A00)
#  undef _WIN32_WINNT
#  define _WIN32_WINNT 0x0A00 // GetDpiForWindow, GetSystemMetricsForDpi
#endif

#include "WindowChrome.h"

#include <QAbstractButton>
#include <QAbstractSlider>
#include <QAbstractSpinBox>
#include <QComboBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QOperatingSystemVersion>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStyle>
#include <QTabBar>
#include <QTextEdit>

#ifdef Q_OS_WIN
#  include <windows.h>
#  include <windowsx.h>
#  include <dwmapi.h>
#endif

namespace {

constexpr int kCaptionHeight = 32; // logical px; matches the caption buttons

#ifdef Q_OS_WIN
// DWM attributes by number: MinGW's headers lag behind the SDK.
constexpr DWORD kDarkMode = 20;       // DWMWA_USE_IMMERSIVE_DARK_MODE
constexpr DWORD kCaptionColor = 35;   // DWMWA_CAPTION_COLOR
constexpr DWORD kTextColor = 36;      // DWMWA_TEXT_COLOR
constexpr DWORD kBackdropType = 38;   // DWMWA_SYSTEMBACKDROP_TYPE, 22H2+
constexpr DWORD kMicaEffect = 1029;   // undocumented, Windows 11 21H2 only
constexpr DWORD kColorDefault = 0xFFFFFFFF;
enum Backdrop : DWORD { BackdropAuto = 0, BackdropMica = 2, BackdropAcrylic = 3, BackdropMicaAlt = 4 };

// SetWindowCompositionAttribute: undocumented, but it is how every blur-behind
// tool on Windows 10 and 11 works (Windhawk mods, FreeDictate, TranslucentTB).
struct AccentPolicy { int state; int flags; DWORD gradient; int animation; };
struct CompositionData { int attribute; void *data; SIZE_T size; };
enum Accent { AccentOff = 0, AccentBlur = 3, AccentAcrylic = 4 };
constexpr int kAccentPolicy = 19; // WCA_ACCENT_POLICY

int windowsBuild()
{
    return QOperatingSystemVersion::current().microVersion();
}

HWND hwndOf(QWidget *w) { return reinterpret_cast<HWND>(w->winId()); }

// Off the windows platform plugin (offscreen screenshots, tests) winId()
// is not an HWND.
bool onWindowsPlatform() { return QGuiApplication::platformName() == QLatin1String("windows"); }

void setDword(HWND hwnd, DWORD attribute, DWORD value)
{
    DwmSetWindowAttribute(hwnd, attribute, &value, sizeof(value));
}

COLORREF colorRef(const QColor &c) { return RGB(c.red(), c.green(), c.blue()); }

void setAccent(HWND hwnd, Accent state, const QColor &tint = QColor())
{
    using Fn = BOOL(WINAPI *)(HWND, CompositionData *);
    static const auto fn = reinterpret_cast<Fn>(
        reinterpret_cast<void *>(GetProcAddress(GetModuleHandleW(L"user32.dll"), "SetWindowCompositionAttribute")));
    if (!fn)
        return;
    AccentPolicy policy{state, 0, 0, 0};
    if (state == AccentAcrylic) {
        // Flag 2 tints the acrylic with the gradient colour (#AABBGGRR). A
        // zero alpha turns the effect off on some builds, so keep a little.
        policy.flags = 2;
        policy.gradient = (DWORD(qMax(tint.alpha(), 1)) << 24) | colorRef(tint);
    }
    CompositionData data{kAccentPolicy, &policy, sizeof(policy)};
    fn(hwnd, &data);
}

int resizeBorder(HWND hwnd)
{
    const UINT dpi = GetDpiForWindow(hwnd);
    return GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi) + GetSystemMetricsForDpi(SM_CYSIZEFRAME, dpi);
}

bool isInteractive(QWidget *w, QWidget *window)
{
    for (; w && w != window; w = w->parentWidget()) {
        if (qobject_cast<QAbstractButton *>(w) || qobject_cast<QLineEdit *>(w)
            || qobject_cast<QComboBox *>(w) || qobject_cast<QAbstractSpinBox *>(w)
            || qobject_cast<QAbstractSlider *>(w) || qobject_cast<QTabBar *>(w)
            || qobject_cast<QTextEdit *>(w) || qobject_cast<QPlainTextEdit *>(w))
            return true;
        // Clickable cards and links set a hand cursor.
        if (w->testAttribute(Qt::WA_SetCursor) && w->cursor().shape() == Qt::PointingHandCursor)
            return true;
    }
    return false;
}

bool inDragArea(QWidget *w, QWidget *window)
{
    for (; w && w != window; w = w->parentWidget()) {
        if (w->property("dragArea").toBool())
            return w->isVisible();
    }
    return false;
}

LRESULT hitTest(QWidget *window, const QPoint &pos, CaptionButtons *buttons)
{
    // Windows only offers snap layouts over what it believes is the real
    // maximise button.
    QPushButton *max = buttons->maximizeButton();
    if (max->isVisible() && max->rect().contains(max->mapFrom(window, pos)))
        return HTMAXBUTTON;
    QWidget *child = window->childAt(pos);
    if (isInteractive(child, window))
        return HTCLIENT;
    if (pos.y() < kCaptionHeight || inDragArea(child, window))
        return HTCAPTION;
    return HTCLIENT;
}
#endif

} // namespace

namespace WindowChrome {

bool supports(Theme::Material m)
{
#ifdef Q_OS_WIN
    switch (m) {
    case Theme::Material::Solid:
    case Theme::Material::Blur:
    case Theme::Material::Frosted:
        return true;
    case Theme::Material::Mica:
        return windowsBuild() >= 22000;
    case Theme::Material::MicaAlt:
    case Theme::Material::Acrylic:
        return windowsBuild() >= 22621;
    }
    return false;
#else
    return m == Theme::Material::Solid;
#endif
}

bool supportsCustomTitleBar()
{
#ifdef Q_OS_WIN
    return true;
#else
    return false;
#endif
}

void install(QWidget *window, bool customTitleBar)
{
#ifdef Q_OS_WIN
    if (!customTitleBar || !onWindowsPlatform())
        return;
    // Makes Windows ask WM_NCCALCSIZE again, where nativeEvent() hands the
    // caption area to the client.
    SetWindowPos(hwndOf(window), nullptr, 0, 0, 0, 0,
                 SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
#else
    Q_UNUSED(window);
    Q_UNUSED(customTitleBar);
#endif
}

void apply(QWidget *window, Theme::Material material, bool customTitleBar)
{
#ifdef Q_OS_WIN
    if (!onWindowsPlatform())
        return;
    const HWND hwnd = hwndOf(window);
    if (!window->testAttribute(Qt::WA_TranslucentBackground) || !supports(material))
        material = Theme::Material::Solid;
    const bool solid = material == Theme::Material::Solid;

    setDword(hwnd, kDarkMode, Theme::isDark() ? TRUE : FALSE);
    // Solid paints the Windows title bar in the theme's own colours; a
    // material leaves it to DWM so the backdrop runs under it too.
    setDword(hwnd, kCaptionColor, solid ? colorRef(Theme::color("SIDEBAR_BG")) : kColorDefault);
    setDword(hwnd, kTextColor, solid ? colorRef(Theme::color("TEXT")) : kColorDefault);

    // With the caption gone and the frame extended, DWM would draw its own
    // caption buttons under the see-through top of the window.
    if (customTitleBar) {
        const LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
        const LONG_PTR wanted = solid ? (style | WS_SYSMENU) : (style & ~LONG_PTR(WS_SYSMENU));
        if (wanted != style) {
            SetWindowLongPtrW(hwnd, GWL_STYLE, wanted);
            SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                         SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        }
    }

    // Start from nothing so a switch between materials leaves no trace.
    const bool build22H2 = windowsBuild() >= 22621;
    setAccent(hwnd, AccentOff);
    if (build22H2)
        setDword(hwnd, kBackdropType, BackdropAuto);
    else if (windowsBuild() >= 22000)
        setDword(hwnd, kMicaEffect, FALSE);

    // Mica and Acrylic fill the extended frame. A frame extended from the top
    // brings back the accent-coloured caption Windows draws when "show accent
    // colour on title bars" is on, so the Blutac title bar extends from the
    // left only, far enough to cover the window (the qwindowkit approach).
    const MARGINS none{0, 0, 0, 0};
    const MARGINS sheet = customTitleBar ? MARGINS{65536, 0, 0, 0} : MARGINS{-1, -1, -1, -1};
    switch (material) {
    case Theme::Material::Solid:
    case Theme::Material::Blur:
    case Theme::Material::Frosted:
        // Blur behind stops working on an extended frame.
        DwmExtendFrameIntoClientArea(hwnd, &none);
        break;
    case Theme::Material::Mica:
        DwmExtendFrameIntoClientArea(hwnd, &sheet);
        if (build22H2)
            setDword(hwnd, kBackdropType, BackdropMica);
        else
            setDword(hwnd, kMicaEffect, TRUE);
        break;
    case Theme::Material::MicaAlt:
        DwmExtendFrameIntoClientArea(hwnd, &sheet);
        setDword(hwnd, kBackdropType, BackdropMicaAlt);
        break;
    case Theme::Material::Acrylic:
        DwmExtendFrameIntoClientArea(hwnd, &sheet);
        setDword(hwnd, kBackdropType, BackdropAcrylic);
        break;
    }
    if (material == Theme::Material::Blur) {
        setAccent(hwnd, AccentBlur);
    } else if (material == Theme::Material::Frosted) {
        QColor tint = Theme::color("BG");
        tint.setAlpha(0x33); // the stylesheet tint does the rest
        setAccent(hwnd, AccentAcrylic, tint);
    }
    window->update();
#else
    Q_UNUSED(window);
    Q_UNUSED(material);
    Q_UNUSED(customTitleBar);
#endif
}

bool nativeEvent(QWidget *window, void *message, qintptr *result, QWidget *captionButtons)
{
#ifdef Q_OS_WIN
    auto *buttons = qobject_cast<CaptionButtons *>(captionButtons);
    if (!buttons)
        return false;
    const MSG *msg = static_cast<MSG *>(message);
    const HWND hwnd = msg->hwnd;
    switch (msg->message) {
    case WM_NCCALCSIZE: {
        if (!msg->wParam)
            return false;
        // Let Windows lay out the frame, then take back the caption: the
        // client keeps the side and bottom borders but starts at the top.
        auto *params = reinterpret_cast<NCCALCSIZE_PARAMS *>(msg->lParam);
        const LONG top = params->rgrc[0].top;
        const LRESULT r = DefWindowProcW(hwnd, WM_NCCALCSIZE, msg->wParam, msg->lParam);
        if (r != 0) {
            *result = r;
            return true;
        }
        params->rgrc[0].top = top;
        // Maximised windows hang their borders off the screen edge.
        if (IsZoomed(hwnd))
            params->rgrc[0].top += resizeBorder(hwnd);
        *result = 0;
        return true;
    }
    case WM_NCHITTEST: {
        const LRESULT hit = DefWindowProcW(hwnd, WM_NCHITTEST, msg->wParam, msg->lParam);
        if (hit != HTCLIENT) {
            *result = hit;
            return true;
        }
        POINT pt{GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam)};
        ScreenToClient(hwnd, &pt);
        if (!IsZoomed(hwnd) && pt.y < resizeBorder(hwnd)) {
            *result = HTTOP; // the top border went with the caption
            return true;
        }
        const qreal dpr = window->devicePixelRatioF();
        *result = hitTest(window, QPoint(qRound(pt.x / dpr), qRound(pt.y / dpr)), buttons);
        return true;
    }
    // Over HTMAXBUTTON Windows sends non-client mouse messages, so the
    // button's hover and click are drawn and handled here.
    case WM_NCMOUSEMOVE:
        buttons->setMaxHot(msg->wParam == HTMAXBUTTON);
        if (msg->wParam == HTMAXBUTTON) {
            TRACKMOUSEEVENT tme{sizeof(tme), TME_LEAVE | TME_NONCLIENT, hwnd, 0};
            TrackMouseEvent(&tme);
        }
        return false;
    case WM_NCMOUSELEAVE:
        buttons->setMaxHot(false);
        buttons->setMaxPressed(false);
        return false;
    case WM_NCLBUTTONDOWN:
    case WM_NCLBUTTONDBLCLK:
        if (msg->wParam != HTMAXBUTTON)
            return false;
        buttons->setMaxPressed(true);
        *result = 0;
        return true;
    case WM_NCLBUTTONUP:
        if (msg->wParam != HTMAXBUTTON)
            return false;
        if (buttons->maximizeButton()->property("down").toBool()) {
            buttons->setMaxPressed(false);
            buttons->maximizeButton()->click();
        }
        *result = 0;
        return true;
    default:
        return false;
    }
#else
    Q_UNUSED(window);
    Q_UNUSED(message);
    Q_UNUSED(result);
    Q_UNUSED(captionButtons);
    return false;
#endif
}

} // namespace WindowChrome

namespace {

// Segoe Fluent Icons on Windows 11, Segoe MDL2 Assets on Windows 10.
const QChar kGlyphMin(0xE921), kGlyphMax(0xE922), kGlyphRestore(0xE923), kGlyphClose(0xE8BB);

void repolish(QWidget *w)
{
    w->style()->unpolish(w);
    w->style()->polish(w);
    w->update();
}

} // namespace

CaptionButtons::CaptionButtons(QWidget *window, QWidget *parent)
    : QWidget(parent)
    , m_window(window)
{
    auto *lay = new QHBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    QFont glyphs;
    glyphs.setFamilies({QStringLiteral("Segoe Fluent Icons"), QStringLiteral("Segoe MDL2 Assets")});
    glyphs.setPixelSize(10);

    auto make = [&](QChar glyph, const char *name, const QString &tip) {
        auto *b = new QPushButton(QString(glyph), this);
        b->setObjectName(QLatin1String(name));
        b->setFont(glyphs);
        b->setFixedSize(46, kCaptionHeight);
        b->setFocusPolicy(Qt::NoFocus);
        b->setToolTip(tip);
        lay->addWidget(b);
        return b;
    };
    m_min = make(kGlyphMin, "CaptionButton", tr("Minimise"));
    m_max = make(kGlyphMax, "CaptionButton", tr("Maximise"));
    m_close = make(kGlyphClose, "CaptionClose", tr("Close"));
    // Windows draws its own snap-layout tip over the maximise button.
    m_max->setToolTip(QString());

    connect(m_min, &QPushButton::clicked, m_window, &QWidget::showMinimized);
    connect(m_max, &QPushButton::clicked, this, [this]() {
        m_window->isMaximized() ? m_window->showNormal() : m_window->showMaximized();
    });
    connect(m_close, &QPushButton::clicked, m_window, &QWidget::close);
    adjustSize();
}

void CaptionButtons::syncState()
{
    m_max->setText(QString(m_window->isMaximized() ? kGlyphRestore : kGlyphMax));
    setMaxHot(false);
    setMaxPressed(false);
}

void CaptionButtons::setMaxHot(bool hot)
{
    if (m_max->property("hot").toBool() == hot)
        return;
    m_max->setProperty("hot", hot);
    repolish(m_max);
}

void CaptionButtons::setMaxPressed(bool pressed)
{
    if (m_max->property("down").toBool() == pressed)
        return;
    m_max->setProperty("down", pressed);
    repolish(m_max);
}
