#pragma once
#include <QColor>
#include <QIcon>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

// Emite themeChanged() sempre que Theme::apply() troca o tema.
class ThemeHub : public QObject
{
    Q_OBJECT
public:
    static ThemeHub &instance();
signals:
    void themeChanged();
};

// Themes. Three come from Cod LAN Launcher (nocturne, classic_dark,
// classic_light); twelve are ported from the 1.x WinForms mod manager
// (qol_classic, oled, the four Catppuccin flavours, the six T3 palettes).
// The table kThemes in Theme.cpp is the one list; names(), displayName()
// and paletteFor() all read it. :/style/base.qss uses @TOKEN@ placeholders
// that apply() substitutes from the chosen palette.
namespace Theme
{
    QString accent();      // cor de destaque do tema atual
    QString accentDark();  // estado pressionado / hover cheio
    QString accentText();  // texto sobre a cor de destaque
    QString muted();       // texto secundario
    QString line();        // linhas e bordas
    QString token(const QString &name); // qualquer token da paleta atual
    QColor color(const QString &name);   // token como QColor
    // Substitui os @TOKEN@ de uma folha de estilo avulsa (diálogos com QSS
    // próprio) pela paleta do tema atual. Tokens mais longos primeiro.
    QString expandTokens(const QString &qss);
    int radius();                        // 8 (Nocturne) ou 0 (Classic Things)
    bool isSquare();                     // true nos temas Classic Things
    // Icone monocromatico recolorido com a cor de icone do tema.
    QIcon icon(const QString &resourcePath);

    QStringList names();                       // chaves dos temas disponiveis
    QString displayName(const QString &themeKey);
    QString normalizeKey(const QString &themeKey); // aceita chaves antigas do INI
    QString current();
    void apply(const QString &themeKey = QString());

    // Accent override, applied on top of the theme by apply(): empty keeps
    // the theme's own accent, "system" follows the Windows accent colour,
    // "#rrggbb" is used as given. setAccent() does not re-apply by itself.
    void setAccent(const QString &spec);
    QString accentSpec();
    QString normalizeAccent(const QString &spec); // "" | "system" | "#rrggbb"
    QColor systemAccent();                        // invalid off Windows
    struct Swatch { const char *name; const char *hex; };
    QList<Swatch> swatches();                     // Catppuccin Macchiato accents

    // Window material behind the app. Anything but Solid needs a window
    // created with WA_TranslucentBackground (see WindowChrome); apply()
    // only makes the stylesheet surfaces see-through to match.
    enum class Material { Solid, Mica, MicaAlt, Acrylic, Blur, Frosted };
    void setMaterial(Material m, int tintPercent);
    Material material();
    int materialTint();
    Material materialFromKey(const QString &key);
    QString materialKey(Material m);
    QList<Material> materials();
    bool isDark(); // the current theme has a dark background
}
