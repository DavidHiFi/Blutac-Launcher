#include "HomePage.h"
#include "../ScriptStore.h"
#include "../Theme.h"
#include "../Version.h"
#include "../WorkshopItems.h"
#include "AppSettings.h"

#include <QApplication>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QStyle>
#include <QVBoxLayout>
#include <QVariantAnimation>

#include <algorithm>
#include <functional>

namespace {

constexpr int kCardMinWidth = 210;
constexpr int kSectionRows = 2; // rows a capped section shows before "See all"
inline int kRadius() { return Theme::radius(); }

QNetworkRequest remoteImageRequest(const QString &url)
{
    QNetworkRequest req{QUrl(url)};
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setMaximumRedirectsAllowed(8);
    req.setRawHeader("User-Agent",
                     "Mozilla/5.0 (Windows NT 10.0; Win64; x64) Chrome/128.0.0.0 Safari/537.36");
    req.setRawHeader("Accept", "image/avif,image/webp,image/apng,image/*,*/*;q=0.8");
    return req;
}

QPixmap coverPixmap(const QPixmap &src, const QSize &target, int radius)
{
    if (src.isNull() || target.isEmpty())
        return QPixmap();

    const qreal dpr = qApp ? qApp->devicePixelRatio() : 1.0;
    const QSize dev = target * dpr;
    QPixmap out(dev);
    out.setDevicePixelRatio(dpr);
    out.fill(Qt::transparent);

    QPainter p(&out);
    p.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform, true);
    QPainterPath path;
    path.addRoundedRect(QRectF(QPointF(0, 0), QSizeF(target)), radius, radius);
    p.setClipPath(path);

    const QPixmap scaled = src.scaled(target, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    p.drawPixmap(QPointF((target.width() - scaled.width() / dpr) / 2.0,
                         (target.height() - scaled.height() / dpr) / 2.0),
                 scaled);
    return out;
}

// ---------------------------------------------------------------- thumbnail
class Thumb : public QWidget
{
public:
    explicit Thumb(QWidget *parent = nullptr) : QWidget(parent)
    {
        setMinimumHeight(120);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        // Troca de tema muda o raio: descarta o recorte em cache.
        QObject::connect(&ThemeHub::instance(), &ThemeHub::themeChanged, this, [this]() {
            m_cache = QPixmap();
            update();
        });
    }
    void setSource(const QPixmap &pm) { m_src = pm; m_cache = QPixmap(); update(); }
    void setBadge(const QString &text) { m_badge = text; update(); }
    // Scripts and packs have no art: the cover shows a name (a script's file
    // name in mono) and the game code.
    void setTextCover(const QString &text, const QString &gameCode, bool mono)
    {
        m_file = text;
        m_code = gameCode;
        m_mono = mono;
        update();
    }
    int heightForWidth(int w) const override { return int(w * 10.0 / 16.0); }
    bool hasHeightForWidth() const override { return true; }
    QSize sizeHint() const override { return QSize(kCardMinWidth, kCardMinWidth * 10 / 16); }

protected:
    void resizeEvent(QResizeEvent *e) override
    {
        QWidget::resizeEvent(e);
        const int h = heightForWidth(width());
        if (height() != h)
            setFixedHeight(h);
        m_cache = QPixmap();
    }
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform, true);
        QPainterPath path;
        path.addRoundedRect(QRectF(rect()), kRadius(), kRadius());
        p.fillPath(path, Theme::color("SURFACE"));

        if (m_cache.isNull() && !m_src.isNull())
            m_cache = coverPixmap(m_src, size(), kRadius());
        if (!m_cache.isNull())
            p.drawPixmap(0, 0, m_cache);

        if (m_src.isNull() && !m_file.isEmpty()) {
            QColor tint = Theme::color("ACCENT");
            tint.setAlpha(46);
            QLinearGradient g(0, 0, width(), height());
            g.setColorAt(0.0, tint);
            g.setColorAt(1.0, Qt::transparent);
            p.fillPath(path, g);

            QFont cover = font();
            if (m_mono) {
                cover = QFont(QStringLiteral("Cascadia Mono"));
                cover.setStyleHint(QFont::Monospace);
                cover.setPointSizeF(12.5);
            } else {
                cover.setPointSizeF(cover.pointSizeF() + 3.0);
                cover.setBold(true);
            }
            p.setFont(cover);
            p.setPen(Theme::color("ACCENT_SOFT"));
            const QRect inner = rect().adjusted(14, 0, -14, 0);
            p.drawText(inner, Qt::AlignCenter,
                       p.fontMetrics().elidedText(m_file, Qt::ElideMiddle, inner.width()));

            QFont small = font();
            small.setPointSizeF(small.pointSizeF() - 1.0);
            small.setBold(true);
            p.setFont(small);
            p.setPen(Theme::color("TEXT_DIM"));
            p.drawText(rect().adjusted(12, 0, -12, -10), Qt::AlignLeft | Qt::AlignBottom, m_code);
        }

        if (!m_badge.isEmpty()) {
            QFont f = font();
            f.setPointSizeF(f.pointSizeF() - 1.5);
            f.setLetterSpacing(QFont::AbsoluteSpacing, 1.0);
            p.setFont(f);
            const QRect text = p.fontMetrics().boundingRect(m_badge.toUpper());
            const QRect box(8, 8, text.width() + 14, text.height() + 6);
            QPainterPath bp;
            bp.addRoundedRect(QRectF(box), kRadius() ? 4 : 0, kRadius() ? 4 : 0);
            p.fillPath(bp, QColor(12, 13, 22, 190));
            p.setPen(QColor("#cfd1dc"));
            p.drawText(box, Qt::AlignCenter, m_badge.toUpper());
        }
    }

private:
    QPixmap m_src, m_cache;
    QString m_badge, m_file, m_code;
    bool m_mono = true;
};

QString modeName(const QString &mode)
{
    return mode == QLatin1String("mp") ? QObject::tr("Multiplayer") : QObject::tr("Zombies");
}

// The Workshop's categories. Plutonium add-ons count as mods.
QString categoryOf(const HomeCatalog::Mod &m)
{
    if (m.isScript() || m.isPack())
        return m.type;
    return QStringLiteral("mod");
}

QString categoryName(const QString &category)
{
    if (category == QLatin1String("script"))
        return QObject::tr("Scripts");
    if (category == QLatin1String("texture"))
        return QObject::tr("Texture packs");
    if (category == QLatin1String("sound"))
        return QObject::tr("Sound packs");
    return QObject::tr("Mods");
}

QString badgeFor(const HomeCatalog::Mod &m)
{
    if (m.isScript())
        return QObject::tr("Script · %1").arg(m.mode.toUpper());
    if (m.type == QLatin1String("texture"))
        return QObject::tr("Texture pack");
    if (m.type == QLatin1String("sound"))
        return QObject::tr("Sound pack");
    if (m.isPlutonium())
        return QObject::tr("Add-on");
    return QObject::tr("Mod");
}

// Where a pack's files go, as the user sees it in the Plutonium folder.
QString packFolder(const HomeCatalog::Mod &m)
{
    return QStringLiteral("storage\\%1\\%2")
        .arg(m.game, m.type == QLatin1String("sound") ? QStringLiteral("zone") : QStringLiteral("images"));
}

QString itemCount(int n)
{
    return n == 1 ? QObject::tr("1 item") : QObject::tr("%1 items").arg(n);
}

// Scripts, packs and add-ons can be removed right here; mods are removed in the Mods tab.
bool removableHere(const HomeCatalog::Mod &m)
{
    return m.isScript() || m.isPack() || m.isPlutonium();
}

// --------------------------------------------------------------- mod card
class ModCard : public QFrame
{
public:
    // madeFor: the name of the mod a script runs inside, empty for anything else.
    ModCard(const HomeCatalog::Mod &mod, const QString &gameName, const QString &gameCode,
            HomeCatalog::Presence presence, const QString &madeFor, QWidget *parent = nullptr)
        : QFrame(parent), m_id(mod.id)
    {
        setObjectName("ModCard");
        setProperty("modId", mod.id);
        setAttribute(Qt::WA_Hover, true);
        setCursor(Qt::PointingHandCursor);
        setMinimumWidth(kCardMinWidth);

        auto *v = new QVBoxLayout(this);
        v->setContentsMargins(0, 0, 0, 0);
        v->setSpacing(0);

        const bool script = mod.isScript();
        m_thumb = new Thumb(this);
        m_thumb->setSource(HomeCatalog::pixmap(mod.image));
        m_thumb->setBadge(badgeFor(mod));
        if (script)
            m_thumb->setTextCover(QFileInfo(mod.file).fileName(), gameCode, true);
        else
            m_thumb->setTextCover(mod.name, gameCode, false);
        v->addWidget(m_thumb);

        auto *body = new QVBoxLayout();
        body->setContentsMargins(12, 10, 12, 12);
        body->setSpacing(3);

        auto *name = new QLabel(mod.name, this);
        name->setObjectName("ModCardTitle");
        name->setWordWrap(true);
        auto *author = new QLabel(QObject::tr("por %1").arg(mod.author), this);
        author->setObjectName("ModCardMeta");
        QString metaText = script
            ? QStringLiteral("%1  ·  %2").arg(modeName(mod.mode), gameName)
            : QStringLiteral("%1  ·  %2").arg(mod.size.isEmpty() ? QStringLiteral("—") : mod.size,
                                              gameName);
        if (!madeFor.isEmpty())
            metaText += QLatin1Char('\n') + QObject::tr("Runs inside %1").arg(madeFor);
        auto *meta = new QLabel(metaText, this);
        meta->setObjectName("ModCardMeta");
        meta->setWordWrap(true);

        body->addWidget(name);
        body->addWidget(author);
        if (presence != HomeCatalog::Presence::Missing) {
            auto *st = new QLabel(presence == HomeCatalog::Presence::Update
                                      ? QObject::tr("Atualizar")
                                      : QObject::tr("Instalado"),
                                  this);
            st->setObjectName(presence == HomeCatalog::Presence::Update
                                  ? QStringLiteral("ModCardUpdate")
                                  : QStringLiteral("ModCardInstalled"));
            body->addWidget(st);
        }
        body->addSpacing(2);
        body->addWidget(meta);

        // One click from the grid: install, update, or remove.
        const bool remove = removableHere(mod) && presence == HomeCatalog::Presence::Installed;
        QString action;
        if (presence == HomeCatalog::Presence::Missing)
            action = QObject::tr("Instalar");
        else if (presence == HomeCatalog::Presence::Update)
            action = QObject::tr("Atualizar");
        else if (remove)
            action = QObject::tr("Remove");
        if (!action.isEmpty()) {
            auto *btn = new QPushButton(action, this);
            btn->setProperty("cssClass", remove ? "ghost" : "primary");
            btn->setCursor(Qt::PointingHandCursor);
            btn->setFixedHeight(30);
            QObject::connect(btn, &QPushButton::clicked, this, [this, remove]() {
                if (onAction)
                    onAction(m_id, remove);
            });
            body->addSpacing(8);
            body->addWidget(btn, 0, Qt::AlignLeft);
        }
        v->addLayout(body);
    }

    void setThumb(const QPixmap &pm) { m_thumb->setSource(pm); }
    QString modId() const { return m_id; }
    std::function<void(QString)> onClick;
    std::function<void(QString, bool remove)> onAction;

protected:
    void mouseReleaseEvent(QMouseEvent *e) override
    {
        if (e->button() == Qt::LeftButton && rect().contains(e->pos()) && onClick)
            onClick(m_id);
        QFrame::mouseReleaseEvent(e);
    }

private:
    QString m_id;
    Thumb *m_thumb = nullptr;
};

} // namespace

// ------------------------------------------------------------- hero slider
class HeroSlider : public QWidget
{
public:
    struct Slide
    {
        QString modId;
        QString title;
        QString summary;
        QString author;
        QStringList tags;
        qint64 downloads = 0;
        QString imageUrl;
        QString logoUrl;
        QPixmap bg;
        QPixmap logo;
        HomeCatalog::Presence presence = HomeCatalog::Presence::Missing;
    };

    explicit HeroSlider(QWidget *parent = nullptr) : QWidget(parent)
    {
        setObjectName("HeroSlider");
        setMinimumHeight(300);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(32, 28, 32, 22);
        root->addStretch(1);

        m_logo = new QLabel(this);
        m_logo->setObjectName("HeroSlideLogo");
        m_title = new QLabel(this);
        m_title->setObjectName("HeroSlideTitle");
        m_title->setWordWrap(true);
        m_summary = new QLabel(this);
        m_summary->setObjectName("HeroSlideSummary");
        m_summary->setWordWrap(true);
        m_meta = new QLabel(this);
        m_meta->setObjectName("HeroSlideMeta");

        root->addWidget(m_logo);
        root->addSpacing(6);
        root->addWidget(m_title);
        root->addWidget(m_summary);
        root->addWidget(m_meta);
        root->addSpacing(14);

        auto *actions = new QHBoxLayout();
        actions->setSpacing(8);
        m_install = new QPushButton(QObject::tr("Instalar"), this);
        m_install->setProperty("cssClass", "primary");
        m_install->setCursor(Qt::PointingHandCursor);
        m_install->setMinimumHeight(38);
        m_details = new QPushButton(QObject::tr("Detalhes"), this);
        m_details->setCursor(Qt::PointingHandCursor);
        m_details->setMinimumHeight(38);
        actions->addWidget(m_install);
        actions->addWidget(m_details);
        actions->addStretch(1);

        m_dots = new QHBoxLayout();
        m_dots->setSpacing(7);
        actions->addLayout(m_dots);
        root->addLayout(actions);

        m_prev = new QPushButton(QStringLiteral("‹"), this);
        m_next = new QPushButton(QStringLiteral("›"), this);
        for (QPushButton *b : {m_prev, m_next}) {
            b->setObjectName("HeroArrow");
            b->setCursor(Qt::PointingHandCursor);
            b->setFixedSize(38, 38);
        }
        connect(m_prev, &QPushButton::clicked, this, [this]() { step(-1); });
        connect(m_next, &QPushButton::clicked, this, [this]() { step(1); });
        connect(m_install, &QPushButton::clicked, this, [this]() {
            if (onInstall && m_index < m_slides.size()) onInstall(m_slides[m_index].modId);
        });
        connect(m_details, &QPushButton::clicked, this, [this]() {
            if (onDetails && m_index < m_slides.size()) onDetails(m_slides[m_index].modId);
        });

        m_fade.setDuration(340);
        m_fade.setStartValue(0.0);
        m_fade.setEndValue(1.0);
        connect(&m_fade, &QVariantAnimation::valueChanged, this, [this](const QVariant &) { update(); });

        m_auto.setInterval(6000);
        connect(&m_auto, &QTimer::timeout, this, [this]() { step(1); });
    }

    void setSlides(const QList<Slide> &slides)
    {
        m_slides = slides;
        m_index = 0;
        rebuildDots();
        apply(false);
        if (slides.size() > 1) m_auto.start(); else m_auto.stop();
        setVisible(!slides.isEmpty());
    }

    void setBackgroundFor(const QString &modId, const QPixmap &pm)
    {
        for (int i = 0; i < m_slides.size(); ++i) {
            if (m_slides[i].modId == modId) {
                m_slides[i].bg = pm;
                if (i == m_index) apply(false);
            }
        }
    }

    void setAssetByUrl(const QString &url, const QPixmap &pm)
    {
        if (url.isEmpty() || pm.isNull())
            return;
        bool touched = false;
        for (int i = 0; i < m_slides.size(); ++i) {
            if (m_slides[i].imageUrl == url) {
                m_slides[i].bg = pm;
                touched = true;
            }
            if (m_slides[i].logoUrl == url) {
                m_slides[i].logo = pm;
                touched = true;
            }
            if (touched && i == m_index)
                apply(false);
        }
    }

    void setAutoplay(bool on)
    {
        if (on && m_slides.size() > 1) m_auto.start();
        else m_auto.stop();
    }

    void retranslate()
    {
        if (m_details)
            m_details->setText(QObject::tr("Detalhes"));
        if (m_install && m_index >= 0 && m_index < m_slides.size())
            applyInstallLabel(m_slides[m_index].presence);
        else if (m_install)
            m_install->setText(QObject::tr("Instalar"));
    }

    std::function<void(QString)> onInstall;
    std::function<void(QString)> onDetails;

protected:
    void resizeEvent(QResizeEvent *e) override
    {
        QWidget::resizeEvent(e);
        m_prev->move(12, (height() - m_prev->height()) / 2);
        m_next->move(width() - m_next->width() - 12, (height() - m_next->height()) / 2);
        m_cache = QPixmap();
        m_prevCache = QPixmap();
    }

    void showEvent(QShowEvent *e) override
    {
        QWidget::showEvent(e);
        if (!property("themeHooked").toBool()) {
            setProperty("themeHooked", true);
            QObject::connect(&ThemeHub::instance(), &ThemeHub::themeChanged, this, [this]() {
                m_cache = QPixmap();
                m_prevCache = QPixmap();
                update();
            });
        }
    }

    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform, true);

        QPainterPath clip;
        clip.addRoundedRect(QRectF(rect()), kRadius() ? 14 : 0, kRadius() ? 14 : 0);
        p.setClipPath(clip);
        p.fillPath(clip, Theme::color("BG_DEEP"));

        const Slide *s = (m_index < m_slides.size()) ? &m_slides[m_index] : nullptr;
        if (s && !s->bg.isNull()) {
            if (m_cache.isNull())
                m_cache = coverPixmap(s->bg, size(), kRadius() ? 14 : 0);
            const qreal t = m_fade.state() == QAbstractAnimation::Running
                                ? m_fade.currentValue().toReal() : 1.0;
            if (!m_prevCache.isNull() && t < 1.0)
                p.drawPixmap(0, 0, m_prevCache);
            p.setOpacity(t);
            p.drawPixmap(0, 0, m_cache);
            p.setOpacity(1.0);
        }

        QLinearGradient g(0, 0, width() * 0.75, height());
        g.setColorAt(0.0, QColor(10, 11, 14, 40));
        g.setColorAt(0.45, QColor(10, 11, 14, 150));
        g.setColorAt(1.0, QColor(10, 11, 14, 225));
        p.fillRect(rect(), g);

        QLinearGradient bottom(0, height() - 220, 0, height());
        bottom.setColorAt(0, QColor(10, 11, 14, 0));
        bottom.setColorAt(1, QColor(10, 11, 14, 235));
        p.fillRect(QRect(0, height() - 220, width(), 220), bottom);

        QPen edge(QColor("#2b2e42"));
        edge.setWidth(1);
        p.setPen(edge);
        p.drawPath(clip);
    }

private:
    void step(int d)
    {
        if (m_slides.size() < 2)
            return;
        m_prevCache = m_cache;
        m_index = (m_index + d + m_slides.size()) % m_slides.size();
        apply(true);
    }

    void rebuildDots()
    {
        while (QLayoutItem *it = m_dots->takeAt(0)) {
            delete it->widget();
            delete it;
        }
        m_dotButtons.clear();
        for (int i = 0; i < m_slides.size(); ++i) {
            auto *dot = new QPushButton(this);
            dot->setObjectName("HeroDot");
            dot->setCheckable(true);
            dot->setCursor(Qt::PointingHandCursor);
            dot->setFixedSize(8, 8);
            connect(dot, &QPushButton::clicked, this, [this, i]() {
                m_prevCache = m_cache;
                m_index = i;
                apply(true);
                m_auto.start();
            });
            m_dots->addWidget(dot);
            m_dotButtons << dot;
        }
    }

    void applyInstallLabel(HomeCatalog::Presence presence)
    {
        if (!m_install)
            return;
        if (presence == HomeCatalog::Presence::Update)
            m_install->setText(QObject::tr("Atualizar"));
        else if (presence == HomeCatalog::Presence::Installed)
            m_install->setText(QObject::tr("Reinstalar"));
        else
            m_install->setText(QObject::tr("Instalar"));
    }

    void apply(bool animate)
    {
        if (m_index >= m_slides.size())
            return;
        const Slide &s = m_slides[m_index];
        applyInstallLabel(s.presence);

        if (!s.logo.isNull()) {
            m_logo->setPixmap(s.logo.scaled(QSize(230, 30), Qt::KeepAspectRatio, Qt::SmoothTransformation));
            m_logo->show();
        } else {
            m_logo->hide();
        }
        m_title->setText(s.title);
        m_summary->setText(s.summary);
        QString meta = QObject::tr("por %1").arg(s.author);
        if (!s.tags.isEmpty())
            meta = s.tags.join(QStringLiteral("  ·  ")) + QStringLiteral("      ") + meta;
        m_meta->setText(meta);

        for (int i = 0; i < m_dotButtons.size(); ++i) {
            m_dotButtons[i]->setChecked(i == m_index);
            m_dotButtons[i]->setFixedWidth(i == m_index ? 22 : 8);
        }

        m_cache = QPixmap();
        if (animate) {
            m_fade.stop();
            m_fade.start();
        }
        update();
    }

    QList<Slide> m_slides;
    int m_index = 0;
    QLabel *m_logo = nullptr;
    QLabel *m_title = nullptr;
    QLabel *m_summary = nullptr;
    QLabel *m_meta = nullptr;
    QPushButton *m_install = nullptr;
    QPushButton *m_details = nullptr;
    QPushButton *m_prev = nullptr;
    QPushButton *m_next = nullptr;
    QHBoxLayout *m_dots = nullptr;
    QList<QPushButton *> m_dotButtons;
    QPixmap m_cache, m_prevCache;
    QVariantAnimation m_fade;
    QTimer m_auto;
};

// ------------------------------------------------------------- details dlg
namespace {

class ModDetailsDialog : public QDialog
{
public:
    static constexpr int kRemove = 2; // exec() result next to Accepted / Rejected

    ModDetailsDialog(const HomeCatalog::Mod &mod,
                     const QString &gameName,
                     HomeCatalog::Presence presence,
                     const QPixmap &cachedCover,
                     QNetworkAccessManager *net,
                     std::function<void(QString, QPixmap)> onCoverFetched,
                     const QString &scriptTarget,
                     const QString &madeFor,
                     QWidget *parent)
        : QDialog(parent)
    {
        const bool script = mod.isScript();
        setWindowTitle(mod.name);
        setMinimumWidth(560);

        auto *v = new QVBoxLayout(this);
        v->setContentsMargins(0, 0, 0, 0);
        v->setSpacing(0);

        auto *art = new Thumb(this);
        QPixmap cover = cachedCover.isNull() ? HomeCatalog::pixmap(mod.image) : cachedCover;
        art->setSource(cover);
        art->setBadge(gameName);
        if (script)
            art->setTextCover(QFileInfo(mod.file).fileName(), modeName(mod.mode), true);
        else
            art->setTextCover(mod.name, badgeFor(mod), false);
        v->addWidget(art);

        if (cover.isNull() && HomeCatalog::isRemote(mod.image) && net) {
            const QString imageUrl = mod.image;
            QNetworkReply *reply = net->get(remoteImageRequest(imageUrl));
            QObject::connect(reply, &QNetworkReply::finished, art,
                             [art, reply, imageUrl, onCoverFetched]() {
                reply->deleteLater();
                if (reply->error() != QNetworkReply::NoError)
                    return;
                const QByteArray bytes = reply->readAll();
                QPixmap pm;
                if (!pm.loadFromData(bytes))
                    return;
                HomeCatalog::storeRemotePixmap(imageUrl, bytes);
                art->setSource(pm);
                if (onCoverFetched)
                    onCoverFetched(imageUrl, pm);
            });
        }

        auto *body = new QVBoxLayout();
        body->setContentsMargins(20, 16, 20, 18);
        body->setSpacing(10);

        auto *title = new QLabel(mod.name, this);
        title->setObjectName("CardTitle");
        auto *kicker = new QLabel(gameName.toUpper(), this);
        kicker->setObjectName("CardKicker");
        body->addWidget(kicker);
        body->addWidget(title);

        if (!mod.tags.isEmpty()) {
            auto *tags = new QLabel(mod.tags.join(QStringLiteral("   ·   ")), this);
            tags->setObjectName("ModTags");
            body->addWidget(tags);
        }

        auto *desc = new QLabel(mod.description, this);
        desc->setObjectName("CardDesc");
        desc->setWordWrap(true);
        body->addWidget(desc);

        auto *grid = new QGridLayout();
        grid->setHorizontalSpacing(24);
        grid->setVerticalSpacing(2);
        const QString version = mod.version.isEmpty() ? QStringLiteral("—") : mod.version;
        const QString size = mod.size.isEmpty() ? QStringLiteral("—") : mod.size;
        QList<QPair<QString, QString>> facts = {{QObject::tr("AUTOR"), mod.author},
                                                {QObject::tr("VERSAO"), version}};
        if (script)
            facts << QPair<QString, QString>{QObject::tr("MODE"), modeName(mod.mode)}
                  << QPair<QString, QString>{QObject::tr("RUNS WITH"),
                                             madeFor.isEmpty() ? QObject::tr("The base game") : madeFor};
        else if (mod.isPack())
            facts << QPair<QString, QString>{QObject::tr("TAMANHO"), size}
                  << QPair<QString, QString>{QObject::tr("GOES INTO"), packFolder(mod)};
        else if (mod.isPlutonium())
            facts << QPair<QString, QString>{QObject::tr("TAMANHO"), size}
                  << QPair<QString, QString>{QObject::tr("FOR"), QObject::tr("Plutonium")};
        else
            facts << QPair<QString, QString>{QObject::tr("TAMANHO"), size}
                  << QPair<QString, QString>{QObject::tr("ORIGEM"),
                                             mod.kind.compare("github", Qt::CaseInsensitive) == 0
                                                 ? QStringLiteral("GitHub") : QObject::tr("Pacote .cll")};
        for (int i = 0; i < facts.size(); ++i) {
            auto *k = new QLabel(facts[i].first, this);
            k->setObjectName("CardKicker");
            auto *val = new QLabel(facts[i].second, this);
            grid->addWidget(k, 0, i);
            grid->addWidget(val, 1, i);
        }
        body->addLayout(grid);

        auto *url = new QLabel(script
                                   ? (scriptTarget.isEmpty()
                                          ? QObject::tr("Set the Plutonium folder in Settings to install scripts.")
                                          : QObject::tr("Installs to %1").arg(QDir::toNativeSeparators(scriptTarget)))
                                   : mod.url,
                               this);
        url->setObjectName("MutedHint");
        url->setWordWrap(true);
        url->setVisible(!url->text().isEmpty());
        body->addWidget(url);

        auto *row = new QHBoxLayout();
        row->addStretch(1);
        if (removableHere(mod) && presence == HomeCatalog::Presence::Installed) {
            auto *remove = new QPushButton(QObject::tr("Remove"), this);
            remove->setMinimumHeight(38);
            connect(remove, &QPushButton::clicked, this, [this]() { done(kRemove); });
            row->addWidget(remove);
        }
        auto *close = new QPushButton(QObject::tr("Fechar"), this);
        auto *install = new QPushButton(presence == HomeCatalog::Presence::Update
                                           ? QObject::tr("Atualizar")
                                           : (presence == HomeCatalog::Presence::Installed
                                                  ? QObject::tr("Reinstalar")
                                                  : QObject::tr("Instalar")),
                                       this);
        install->setProperty("cssClass", "primary");
        install->setMinimumHeight(38);
        close->setMinimumHeight(38);
        row->addWidget(close);
        row->addWidget(install);
        body->addLayout(row);

        v->addLayout(body);

        connect(close, &QPushButton::clicked, this, &QDialog::reject);
        connect(install, &QPushButton::clicked, this, &QDialog::accept);
    }
};

} // namespace

// ------------------------------------------------------------------ page
HomePage::HomePage(AppSettings &settings, QWidget *parent)
    : QWidget(parent)
    , m_settings(settings)
{
    setObjectName("HomePage");
    m_net = new QNetworkAccessManager(this);

    m_searchDebounce.setSingleShot(true);
    m_searchDebounce.setInterval(180);
    connect(&m_searchDebounce, &QTimer::timeout, this, [this]() {
        m_query = m_search->text().trimmed();
        rebuildContent();
    });

    buildUi();
    reload();
}

void HomePage::buildUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Row 1: search, and the add/share/import buttons.
    // Row 2: one game at a time, or all of them.
    // Row 3: the category (mods, scripts, packs) and the sort order.
    auto *bar = new QWidget(this);
    bar->setObjectName("HomeBar");
    auto *barRows = new QVBoxLayout(bar);
    barRows->setContentsMargins(28, 14, 28, 12);
    barRows->setSpacing(10);
    auto *barLayout = new QHBoxLayout();
    barLayout->setSpacing(10);
    barRows->addLayout(barLayout);

    m_search = new QLineEdit(bar);
    m_search->setClearButtonEnabled(true);
    m_search->setMinimumHeight(34);
    m_search->setMinimumWidth(220);
    m_search->setMaximumWidth(420);
    connect(m_search, &QLineEdit::textChanged, this, [this](const QString &) { m_searchDebounce.start(); });
    barLayout->addWidget(m_search);
    barLayout->addStretch(1);

    m_addScriptBtn = new QPushButton(bar);
    m_addScriptBtn->setCursor(Qt::PointingHandCursor);
    m_addScriptBtn->setMinimumHeight(34);
    connect(m_addScriptBtn, &QPushButton::clicked, this, &HomePage::addLocalScript);
    barLayout->addWidget(m_addScriptBtn);

    m_shareBtn = new QPushButton(bar);
    m_shareBtn->setCursor(Qt::PointingHandCursor);
    m_shareBtn->setMinimumHeight(34);
    connect(m_shareBtn, &QPushButton::clicked, this, []() {
        QDesktopServices::openUrl(QUrl(QStringLiteral(QOL_REPO_URL "/issues/new?template=share-a-script.yml")));
    });
    barLayout->addWidget(m_shareBtn);

    m_importBtn = new QPushButton(bar);
    m_importBtn->setProperty("cssClass", "primary");
    m_importBtn->setCursor(Qt::PointingHandCursor);
    m_importBtn->setMinimumHeight(34);
    connect(m_importBtn, &QPushButton::clicked, this, [this]() { emit installRequested(QString()); });
    barLayout->addWidget(m_importBtn);

    m_filterBar = new QWidget(bar);
    m_filterBar->setObjectName("HomeFilters");
    m_filterLayout = new QHBoxLayout(m_filterBar);
    m_filterLayout->setContentsMargins(0, 0, 0, 0);
    m_filterLayout->setSpacing(6);
    barRows->addWidget(m_filterBar);

    m_typeBar = new QWidget(bar);
    auto *typeRow = new QHBoxLayout(m_typeBar);
    typeRow->setContentsMargins(0, 0, 0, 0);
    typeRow->setSpacing(0);
    const QStringList typeIds = {QStringLiteral("all"), QStringLiteral("mod"), QStringLiteral("script"),
                                 QStringLiteral("texture"), QStringLiteral("sound")};
    for (int i = 0; i < typeIds.size(); ++i) {
        auto *b = new QPushButton(m_typeBar);
        b->setObjectName("TypeSegment");
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setMinimumHeight(32);
        b->setProperty("typeId", typeIds[i]);
        connect(b, &QPushButton::clicked, this, [this, id = typeIds[i]]() { setType(id); });
        typeRow->addWidget(b);
        m_typeButtons << b;
    }
    typeRow->addStretch(1);
    m_sortLabel = new QLabel(m_typeBar);
    m_sortLabel->setObjectName("ChipRowLabel");
    typeRow->addWidget(m_sortLabel);
    typeRow->addSpacing(8);
    m_sort = new QComboBox(m_typeBar);
    m_sort->setMinimumHeight(32);
    m_sort->setCursor(Qt::PointingHandCursor);
    for (const char *id : {"featured", "name", "installed"})
        m_sort->addItem(QString(), QString::fromLatin1(id));
    connect(m_sort, &QComboBox::currentIndexChanged, this, [this](int) { rebuildContent(); });
    typeRow->addWidget(m_sort);
    barRows->addWidget(m_typeBar);

    root->addWidget(bar);
    retranslateBar();

    // conteudo rolavel
    m_scroll = new QScrollArea(this);
    m_scroll->setWidgetResizable(true);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_content = new QWidget(m_scroll);
    m_content->setObjectName("HomeContent");
    m_contentLayout = new QVBoxLayout(m_content);
    m_contentLayout->setContentsMargins(28, 20, 28, 32);
    m_contentLayout->setSpacing(18);

    m_status = new QLabel(m_content);
    m_status->setObjectName("MutedHint");
    m_status->setWordWrap(true);
    m_status->hide();
    m_contentLayout->addWidget(m_status);

    m_hero = new HeroSlider(m_content);
    m_hero->onInstall = [this](const QString &id) { installItem(id); };
    m_hero->onDetails = [this](const QString &id) { openDetails(id); };
    m_contentLayout->addWidget(m_hero);

    m_scroll->setWidget(m_content);
    root->addWidget(m_scroll, 1);

    m_scroll->viewport()->installEventFilter(this);
    m_relayout.setSingleShot(true);
    m_relayout.setInterval(60);
    connect(&m_relayout, &QTimer::timeout, this, [this]() {
        const int scroll = m_scroll->verticalScrollBar()->value();
        rebuildContent();
        QTimer::singleShot(0, this, [this, scroll]() { m_scroll->verticalScrollBar()->setValue(scroll); });
    });
}

void HomePage::reload()
{
    m_catalog = HomeCatalog::load();
    WorkshopItems::applyLatestQol(m_catalog);
    WorkshopItems::fetchLatestQol(this, [this]() {
        WorkshopItems::applyLatestQol(m_catalog);
        refreshInstallState();
    });
    m_installs = HomeCatalog::scanInstalled(m_settings.plutoniumInstance);
    if (!m_catalog.ok) {
        m_status->setText(tr("Catalogo indisponivel — %1").arg(m_catalog.error));
        m_status->show();
    } else if (m_catalog.fromCache) {
        m_status->setText(m_catalog.error.isEmpty()
                              ? tr("Showing the last downloaded catalog.")
                              : m_catalog.error);
        m_status->show();
    } else {
        m_status->hide();
    }
    rebuildFilters();
    rebuildContent();
}

void HomePage::refreshInstallState()
{
    m_installs = HomeCatalog::scanInstalled(m_settings.plutoniumInstance);
    const int scroll = m_scroll->verticalScrollBar()->value();
    rebuildContent();
    QTimer::singleShot(0, this, [this, scroll]() { m_scroll->verticalScrollBar()->setValue(scroll); });
}

void HomePage::rebuildFilters()
{
    while (QLayoutItem *it = m_filterLayout->takeAt(0)) {
        delete it->widget();
        delete it;
    }
    m_filterButtons.clear();

    auto addChip = [this](const QString &id, const QString &label) {
        auto *btn = new QPushButton(label, m_filterBar);
        btn->setObjectName("GameChip");
        btn->setCheckable(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setProperty("gameId", id);
        connect(btn, &QPushButton::clicked, this, [this, id]() { selectGame(id); });
        m_filterLayout->addWidget(btn);
        m_filterButtons << btn;
    };
    addChip(QString(), tr("All games"));
    for (const HomeCatalog::Game &g : m_catalog.games)
        addChip(g.id, g.name);
    m_filterLayout->addStretch(1);

    if (!m_game.isEmpty() && !HomeCatalog::findGame(m_catalog, m_game))
        m_game.clear();
    syncChips();
}

// Texture and sound packs exist for World at War, Black Ops and Black Ops II
// only, and Plutonium add-ons have no categories, so the category row follows
// the game.
void HomePage::syncChips()
{
    for (QPushButton *b : m_filterButtons)
        b->setChecked(b->property("gameId").toString() == m_game);

    const bool plutonium = m_game == QLatin1String("plutonium");
    const bool packs = m_game.isEmpty() || m_game == QLatin1String("t4") || m_game == QLatin1String("t5")
        || m_game == QLatin1String("t6");
    QList<QPushButton *> shown;
    for (QPushButton *b : m_typeButtons) {
        const QString id = b->property("typeId").toString();
        const bool show = !plutonium
            && (packs || (id != QLatin1String("texture") && id != QLatin1String("sound")));
        b->setVisible(show);
        if (show)
            shown << b;
    }
    bool typeShown = false;
    for (QPushButton *b : shown)
        typeShown |= b->property("typeId").toString() == m_type;
    if (!typeShown)
        m_type = QStringLiteral("all");
    for (QPushButton *b : m_typeButtons) {
        const char *segment = b == shown.value(0) ? "left" : (b == shown.value(shown.size() - 1) ? "right" : "");
        if (b->property("segment").toString() != QLatin1String(segment)) {
            b->setProperty("segment", segment);
            b->style()->unpolish(b);
            b->style()->polish(b);
        }
        b->setChecked(b->property("typeId").toString() == m_type);
    }
    // Scripts are made for games, not for Plutonium.
    m_addScriptBtn->setVisible(!plutonium);
    m_shareBtn->setVisible(!plutonium);
}

void HomePage::selectGame(const QString &gameId)
{
    m_game = gameId == QLatin1String("all") ? QString() : gameId;
    syncChips();
    rebuildContent();
    m_scroll->verticalScrollBar()->setValue(0);
}

void HomePage::setType(const QString &type)
{
    m_type = type;
    syncChips();
    rebuildContent();
    m_scroll->verticalScrollBar()->setValue(0);
}

bool HomePage::matchesFilters(const HomeCatalog::Mod &m) const
{
    if (m_type != QLatin1String("all") && categoryOf(m) != m_type)
        return false;
    if (!m_game.isEmpty() && m.game != m_game)
        return false;
    if (!m_query.isEmpty()) {
        const HomeCatalog::Game *g = HomeCatalog::findGame(m_catalog, m.game);
        const HomeCatalog::Mod *hub = m.forMod.isEmpty() ? nullptr : HomeCatalog::findMod(m_catalog, m.forMod);
        const QString hay = QStringList{m.name, m.author, m.summary, m.tags.join(' '), badgeFor(m),
                                        g ? g->name : QString(), hub ? hub->name : QString()}
                                .join(' ');
        if (!hay.contains(m_query, Qt::CaseInsensitive))
            return false;
    }
    return true;
}

// Matching items for one game ("" = all), in the order the sort box asks for.
QList<HomeCatalog::Mod> HomePage::filteredMods(const QString &gameId) const
{
    QList<HomeCatalog::Mod> out;
    for (const HomeCatalog::Mod &m : m_catalog.mods)
        if ((gameId.isEmpty() || m.game == gameId) && matchesFilters(m))
            out << m;

    const QString sort = m_sort ? m_sort->currentData().toString() : QString();
    if (sort == QLatin1String("name")) {
        std::stable_sort(out.begin(), out.end(), [](const HomeCatalog::Mod &a, const HomeCatalog::Mod &b) {
            return a.name.localeAwareCompare(b.name) < 0;
        });
    } else if (sort == QLatin1String("installed")) {
        QHash<QString, bool> have;
        for (const HomeCatalog::Mod &m : out)
            have.insert(m.id, presenceOf(m) != HomeCatalog::Presence::Missing);
        std::stable_sort(out.begin(), out.end(), [&have](const HomeCatalog::Mod &a, const HomeCatalog::Mod &b) {
            return have.value(a.id) && !have.value(b.id);
        });
    } else {
        // Featured items first, then the catalog's own order.
        const QStringList &featured = m_catalog.featured;
        auto rank = [&featured](const HomeCatalog::Mod &m) {
            const int i = int(featured.indexOf(m.id));
            return i < 0 ? int(featured.size()) : i;
        };
        std::stable_sort(out.begin(), out.end(), [&rank](const HomeCatalog::Mod &a, const HomeCatalog::Mod &b) {
            return rank(a) < rank(b);
        });
    }
    return out;
}

HomeCatalog::Presence HomePage::presenceOf(const HomeCatalog::Mod &m) const
{
    if (WorkshopItems::handles(m))
        return WorkshopItems::presence(m_settings, m);
    if (!m.isScript())
        return HomeCatalog::presenceOf(m, m_installs);
    const QString dir = scriptDir(m);
    return !dir.isEmpty() && QFileInfo::exists(dir + QLatin1Char('/') + QFileInfo(m.file).fileName())
        ? HomeCatalog::Presence::Installed
        : HomeCatalog::Presence::Missing;
}

// Scripts install here, packs and add-ons through WorkshopItems, and other
// mods through the Mods tab.
void HomePage::installItem(const QString &modId)
{
    const HomeCatalog::Mod *m = HomeCatalog::findMod(m_catalog, modId);
    if (!m) {
        emit installRequested(modId);
        return;
    }
    if (m->isScript()) {
        installScript(modId);
        return;
    }
    if (WorkshopItems::handles(*m)) {
        WorkshopItems::install(this, m_settings, *m, [this]() {
            refreshInstallState();
            emit installedChanged();
        });
        return;
    }
    emit installRequested(modId);
}

void HomePage::removeItem(const QString &modId)
{
    const HomeCatalog::Mod *m = HomeCatalog::findMod(m_catalog, modId);
    if (!m || !removableHere(*m))
        return;
    if (m->isScript()) {
        removeScript(modId);
        return;
    }
    WorkshopItems::remove(this, m_settings, {m->id, m->name, m->game, QString()}, [this]() {
        refreshInstallState();
        emit installedChanged();
    });
}

// Where a catalog script goes. One made for a T6 mod goes inside that mod's
// folder so it only loads with it; *missingMod gets the mod's name when that
// mod is not installed yet.
QString HomePage::scriptDir(const HomeCatalog::Mod &m, QString *missingMod) const
{
    const QString root = m_settings.plutoniumInstance;
    QString modFolder;
    if (m.game == QLatin1String("t6") && !m.forMod.isEmpty()) {
        if (const HomeCatalog::Mod *hub = HomeCatalog::findMod(m_catalog, m.forMod)) {
            modFolder = hub->folder;
            if (!modFolder.isEmpty() && missingMod && !root.isEmpty()
                && !QFileInfo::exists(QDir::cleanPath(root) + QStringLiteral("/storage/t6/mods/") + modFolder))
                *missingMod = hub->name;
        }
    }
    return ScriptStore::folder(root, m.game, m.mode, modFolder);
}

void HomePage::showStatus(const QString &text)
{
    m_status->setText(text);
    m_status->setVisible(!text.isEmpty());
}

void HomePage::installScript(const QString &modId)
{
    const HomeCatalog::Mod *m = HomeCatalog::findMod(m_catalog, modId);
    if (!m || !m->isScript())
        return;
    QString missingMod;
    const QString dir = scriptDir(*m, &missingMod);
    if (dir.isEmpty()) {
        showStatus(tr("Set the Plutonium folder in Settings first."));
        return;
    }
    if (!missingMod.isEmpty()) {
        showStatus(tr("Install %1 first. %2 runs inside it.").arg(missingMod, m->name));
        return;
    }
    const QString name = QFileInfo(m->file).fileName();
    const QString title = m->name;
    auto finish = [this, dir, name, title](const QByteArray &bytes) {
        const QString err = ScriptStore::write(dir, name, bytes);
        showStatus(err.isEmpty()
                       ? tr("%1 installed. Plutonium loads it next time you start the game.").arg(title)
                       : err);
        refreshInstallState();
        emit installedChanged();
    };

    // Scripts that ship with the app install offline; community ones download.
    QFile bundled(QStringLiteral(":/") + m->file);
    if (bundled.open(QIODevice::ReadOnly)) {
        finish(bundled.readAll());
        return;
    }
    const QString url = m->url.isEmpty() ? QStringLiteral(QOL_RAW_BASE) + m->file : m->url;
    QNetworkRequest req{QUrl(url)};
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    showStatus(tr("Downloading %1…").arg(title));
    QNetworkReply *reply = m_net->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, title, finish]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            showStatus(tr("Could not download %1: %2").arg(title, reply->errorString()));
            return;
        }
        finish(reply->readAll());
    });
}

void HomePage::removeScript(const QString &modId)
{
    const HomeCatalog::Mod *m = HomeCatalog::findMod(m_catalog, modId);
    if (!m || !m->isScript())
        return;
    const QString dir = scriptDir(*m);
    if (dir.isEmpty())
        return;
    const QString err = ScriptStore::remove(dir + QLatin1Char('/') + QFileInfo(m->file).fileName());
    showStatus(err.isEmpty() ? tr("%1 moved to the Recycle Bin.").arg(m->name) : err);
    refreshInstallState();
    emit installedChanged();
}

void HomePage::addLocalScript()
{
    const QString file = QFileDialog::getOpenFileName(this, tr("Add a script"), QString(),
                                                      tr("GSC scripts (*.gsc *.csc)"));
    if (file.isEmpty())
        return;
    const QString game = m_game.isEmpty() || m_game == QLatin1String("plutonium") ? QStringLiteral("t6") : m_game;
    const QString path = ScriptStore::importWithDialog(this, m_settings.plutoniumInstance, file, game);
    if (!path.isEmpty())
        showStatus(tr("Added %1.").arg(QDir::toNativeSeparators(path)));
}

void HomePage::rebuildContent()
{
    // limpa tudo abaixo do hero
    while (m_contentLayout->count() > 2) {
        QLayoutItem *item = m_contentLayout->takeAt(2);
        if (QWidget *w = item->widget())
            w->deleteLater();
        delete item;
    }

    // slider so aparece sem busca ativa
    QList<HeroSlider::Slide> slides;
    if (m_query.isEmpty()) {
        QStringList ids = m_catalog.featured;
        if (ids.isEmpty())
            for (const HomeCatalog::Mod &m : m_catalog.mods)
                if (ids.size() < 4) ids << m.id;
        for (const QString &id : ids) {
            const HomeCatalog::Mod *m = HomeCatalog::findMod(m_catalog, id);
            if (!m)
                continue;
            if (!matchesFilters(*m))
                continue;
            const HomeCatalog::Game *g = HomeCatalog::findGame(m_catalog, m->game);
            HeroSlider::Slide s;
            s.modId = m->id;
            s.title = m->name;
            s.summary = m->summary;
            s.author = m->author;
            s.tags = m->tags;
            s.downloads = m->downloads;
            s.imageUrl = m->image;
            s.logoUrl = g ? g->logo : QString();
            s.bg = resolvedCover(m->image);
            s.logo = g ? resolvedCover(g->logo) : QPixmap();
            if (s.bg.isNull() && HomeCatalog::isRemote(m->image))
                requestRemote(m->image, m->id);
            if (s.logo.isNull() && g && HomeCatalog::isRemote(g->logo))
                requestRemote(g->logo, QString());
            s.presence = presenceOf(*m);
            slides << s;
        }
    }
    m_hero->setSlides(slides);

    if (!m_catalog.ok) {
        auto *card = new QFrame(m_content);
        card->setObjectName("Card");
        auto *cl = new QVBoxLayout(card);
        cl->setContentsMargins(18, 16, 18, 16);
        cl->setSpacing(8);
        auto *k = new QLabel(HomeCatalog::catalogUrl(), card);
        k->setObjectName("MutedHint");
        auto *t = new QLabel(tr("Could not load the page"), card);
        t->setObjectName("SectionTitle");
        t->setWordWrap(true);
        auto *b = new QLabel(m_catalog.error.isEmpty()
                                 ? tr("Could not load the page.")
                                 : m_catalog.error, card);
        b->setObjectName("MutedHint");
        b->setWordWrap(true);
        cl->addWidget(k);
        cl->addWidget(t);
        cl->addWidget(b);
        m_contentLayout->addWidget(card);
        m_contentLayout->addStretch();
        return;
    }

    // A search shows one list of results. Otherwise "All games" shows a row
    // per game, one game shows a row per category, and a game plus a category
    // shows all of it. Rows stop after kSectionRows lines and offer "See all".
    struct SectionDef {
        QString title;
        QString logo;
        QList<HomeCatalog::Mod> mods;
        std::function<void()> seeAll; // empty: the section shows everything
    };
    const HomeCatalog::Game *game = HomeCatalog::findGame(m_catalog, m_game);
    const QString gameName = game ? game->name : QString();
    QList<SectionDef> sections;
    if (!m_query.isEmpty()) {
        const QList<HomeCatalog::Mod> found = filteredMods(QString());
        if (!found.isEmpty())
            sections << SectionDef{found.size() == 1 ? tr("1 result for \"%1\"").arg(m_query)
                                                     : tr("%1 results for \"%2\"").arg(found.size()).arg(m_query),
                                   QString(), found, {}};
    } else if (m_game.isEmpty()) {
        for (const HomeCatalog::Game &g : m_catalog.games) {
            const QList<HomeCatalog::Mod> mods = filteredMods(g.id);
            if (!mods.isEmpty())
                sections << SectionDef{g.name, g.logo, mods, [this, id = g.id]() { selectGame(id); }};
        }
    } else if (m_type == QLatin1String("all") && m_game != QLatin1String("plutonium")) {
        const QList<HomeCatalog::Mod> mods = filteredMods(m_game);
        for (const QString &cat : {QStringLiteral("mod"), QStringLiteral("script"), QStringLiteral("texture"),
                                   QStringLiteral("sound")}) {
            QList<HomeCatalog::Mod> in;
            for (const HomeCatalog::Mod &m : mods)
                if (categoryOf(m) == cat)
                    in << m;
            if (!in.isEmpty())
                sections << SectionDef{categoryName(cat), QString(), in, [this, cat]() { setType(cat); }};
        }
    } else {
        const QList<HomeCatalog::Mod> mods = filteredMods(m_game);
        if (!mods.isEmpty())
            sections << SectionDef{m_type == QLatin1String("all") ? gameName : categoryName(m_type),
                                   game ? game->logo : QString(), mods, {}};
    }

    m_columns = columnCount();
    const int cap = m_columns * kSectionRows;
    for (const SectionDef &def : sections) {
        auto *section = new QWidget(m_content);
        auto *sv = new QVBoxLayout(section);
        sv->setContentsMargins(0, 8, 0, 0);
        sv->setSpacing(12);

        auto *head = new QHBoxLayout();
        head->setSpacing(10);
        const QPixmap logo = resolvedCover(def.logo);
        if (!logo.isNull() || HomeCatalog::isRemote(def.logo)) {
            auto *logoLabel = new QLabel(section);
            logoLabel->setObjectName("HomeGameLogo");
            logoLabel->setProperty("assetUrl", def.logo);
            if (!logo.isNull())
                logoLabel->setPixmap(logo.scaled(QSize(150, 22), Qt::KeepAspectRatio, Qt::SmoothTransformation));
            head->addWidget(logoLabel);
            if (logo.isNull() && HomeCatalog::isRemote(def.logo))
                requestRemote(def.logo, QString());
        }
        auto *name = new QLabel(def.title, section);
        name->setObjectName("SectionTitle");
        head->addWidget(name);
        if (!m_query.isEmpty() || def.seeAll) {
            auto *count = new QLabel(itemCount(int(def.mods.size())), section);
            count->setObjectName("MutedHint");
            head->addWidget(count);
        }
        head->addStretch(1);
        const bool capped = def.seeAll && def.mods.size() > cap;
        if (capped) {
            auto *all = new QPushButton(tr("See all"), section);
            all->setProperty("cssClass", "ghost");
            all->setCursor(Qt::PointingHandCursor);
            connect(all, &QPushButton::clicked, this, def.seeAll);
            head->addWidget(all);
        }
        sv->addLayout(head);

        auto *rule = new QFrame(section);
        rule->setObjectName("NavRule");
        rule->setFrameShape(QFrame::HLine);
        sv->addWidget(rule);

        auto *grid = new QGridLayout();
        grid->setHorizontalSpacing(14);
        grid->setVerticalSpacing(14);
        grid->setProperty("isCardGrid", true);
        const int shown = capped ? cap : int(def.mods.size());
        for (int i = 0; i < shown; ++i) {
            const HomeCatalog::Mod &m = def.mods[i];
            const HomeCatalog::Game *g = HomeCatalog::findGame(m_catalog, m.game);
            const HomeCatalog::Mod *hub = m.forMod.isEmpty() ? nullptr : HomeCatalog::findMod(m_catalog, m.forMod);
            auto *card = new ModCard(m, g ? g->name : m.game, g ? g->code : m.game.toUpper(),
                                    presenceOf(m), hub ? hub->name : QString(), section);
            card->onClick = [this](const QString &id) { openDetails(id); };
            card->onAction = [this](const QString &id, bool remove) {
                remove ? removeItem(id) : installItem(id);
            };
            const QPixmap cover = resolvedCover(m.image);
            if (!cover.isNull())
                card->setThumb(cover);
            else if (HomeCatalog::isRemote(m.image))
                requestRemote(m.image, m.id);
            grid->addWidget(card, 0, grid->count());
        }
        sv->addLayout(grid);
        m_contentLayout->addWidget(section);
    }

    if (sections.isEmpty()) {
        QString text;
        if (!m_query.isEmpty())
            text = tr("Nothing matches \"%1\".").arg(m_query);
        else if (m_type != QLatin1String("all") && !gameName.isEmpty())
            text = tr("No %1 for %2 yet.").arg(categoryName(m_type).toLower(), gameName);
        else if (m_type != QLatin1String("all"))
            text = tr("No %1 yet.").arg(categoryName(m_type).toLower());
        else if (!gameName.isEmpty())
            text = tr("Nothing for %1 yet.").arg(gameName);
        else
            text = tr("The Workshop is empty.");
        auto *empty = new QLabel(text, m_content);
        empty->setObjectName("MutedHint");
        empty->setAlignment(Qt::AlignCenter);
        empty->setMinimumHeight(160);
        m_contentLayout->addWidget(empty);
    }

    m_contentLayout->addStretch(1);
    relayoutGrids();
}

int HomePage::columnCount() const
{
    const int available = (m_scroll ? m_scroll->viewport()->width() : width()) - 56;
    return qMax(2, (available + 14) / (kCardMinWidth + 14));
}

void HomePage::relayoutGrids()
{
    const int columns = columnCount();

    for (QGridLayout *grid : m_content->findChildren<QGridLayout *>()) {
        if (!grid->property("isCardGrid").toBool())
            continue;
        QList<QWidget *> cards;
        while (QLayoutItem *item = grid->takeAt(0)) {
            if (QWidget *w = item->widget())
                cards << w;
            delete item;
        }
        for (int i = 0; i < cards.size(); ++i)
            grid->addWidget(cards[i], i / columns, i % columns);
        for (int c = 0; c < columns; ++c)
            grid->setColumnStretch(c, 1);
    }
}

void HomePage::openDetails(const QString &modId)
{
    const HomeCatalog::Mod *m = HomeCatalog::findMod(m_catalog, modId);
    if (!m)
        return;
    emit detailsRequested(modId);
    const HomeCatalog::Game *g = HomeCatalog::findGame(m_catalog, m->game);
    const QPixmap cover = resolvedCover(m->image);
    const HomeCatalog::Mod *hub = m->forMod.isEmpty() ? nullptr : HomeCatalog::findMod(m_catalog, m->forMod);
    const bool script = m->isScript();
    ModDetailsDialog dlg(*m, g ? g->name : m->game,
                         presenceOf(*m), cover, m_net,
                         [this, modId](const QString &url, const QPixmap &pm) {
        m_remote.insert(url, pm);
        applyRemoteCover(modId, pm);
    }, script ? scriptDir(*m) : QString(), hub ? hub->name : QString(), this);
    m_hero->setAutoplay(false);
    const int result = dlg.exec();
    m_hero->setAutoplay(true);
    if (result == ModDetailsDialog::kRemove)
        removeItem(modId);
    else if (result == QDialog::Accepted)
        installItem(modId);
}

QPixmap HomePage::resolvedCover(const QString &ref) const
{
    QPixmap pm = HomeCatalog::pixmap(ref);
    if (pm.isNull() && HomeCatalog::isRemote(ref))
        pm = m_remote.value(ref);
    if (pm.isNull() && HomeCatalog::isRemote(ref))
        pm = HomeCatalog::cachedRemotePixmap(ref);
    return pm;
}

void HomePage::applyRemoteCover(const QString &modId, const QPixmap &pm)
{
    if (pm.isNull())
        return;
    if (m_hero && !modId.isEmpty())
        m_hero->setBackgroundFor(modId, pm);
    if (!m_content)
        return;
    for (QFrame *w : m_content->findChildren<QFrame *>()) {
        if (w->objectName() != QLatin1String("ModCard"))
            continue;
        if (w->property("modId").toString() != modId)
            continue;
        static_cast<ModCard *>(w)->setThumb(pm);
    }
}

void HomePage::applyRemoteUrl(const QString &url, const QPixmap &pm)
{
    if (url.isEmpty() || pm.isNull())
        return;
    if (m_hero)
        m_hero->setAssetByUrl(url, pm);
    if (!m_content)
        return;
    for (QLabel *lab : m_content->findChildren<QLabel *>()) {
        if (lab->objectName() != QLatin1String("HomeGameLogo"))
            continue;
        if (lab->property("assetUrl").toString() != url)
            continue;
        lab->setPixmap(pm.scaled(QSize(150, 22), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
}

void HomePage::requestRemote(const QString &url, const QString &modId)
{
    if (url.isEmpty())
        return;
    const QPixmap cached = m_remote.value(url);
    if (!cached.isNull()) {
        applyRemoteCover(modId, cached);
        applyRemoteUrl(url, cached);
        return;
    }
    if (m_remote.contains(url))
        return;
    m_remote.insert(url, QPixmap());

    QNetworkReply *reply = m_net->get(remoteImageRequest(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply, url, modId]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            m_remote.remove(url);
            return;
        }
        const QByteArray bytes = reply->readAll();
        QPixmap pm;
        if (!pm.loadFromData(bytes)) {
            m_remote.remove(url);
            return;
        }
        HomeCatalog::storeRemotePixmap(url, bytes);
        m_remote.insert(url, pm);
        applyRemoteCover(modId, pm);
        applyRemoteUrl(url, pm);
    });
}

void HomePage::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_hero)
        m_hero->setFixedHeight(qBound(280, int(width() * 0.34), 420));
}

// The card columns follow the scroll viewport, which gets its real width only
// after the page is shown. A capped section holds two lines of cards, so a
// new column count needs the sections built again, not just re-flowed.
bool HomePage::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::Resize && m_scroll && watched == m_scroll->viewport()) {
        if (m_columns && columnCount() != m_columns)
            m_relayout.start();
        else
            relayoutGrids();
    }
    return QWidget::eventFilter(watched, event);
}

void HomePage::retranslate()
{
    retranslateBar();
    if (m_hero)
        m_hero->retranslate();
    rebuildFilters();
    rebuildContent();
}

void HomePage::retranslateBar()
{
    m_search->setPlaceholderText(tr("Search mods, scripts, packs, authors…"));
    for (QPushButton *b : m_typeButtons) {
        const QString id = b->property("typeId").toString();
        b->setText(id == QLatin1String("all") ? tr("Everything") : categoryName(id));
    }
    m_sortLabel->setText(tr("Sort by"));
    const QStringList sorts = {tr("Recommended"), tr("Name"), tr("Installed first")};
    for (int i = 0; i < m_sort->count() && i < sorts.size(); ++i)
        m_sort->setItemText(i, sorts[i]);
    m_addScriptBtn->setText(tr("Add a script"));
    m_addScriptBtn->setToolTip(tr("Copy a .gsc file from your PC into the right Plutonium folder."));
    m_shareBtn->setText(tr("Share a script"));
    m_shareBtn->setToolTip(tr("Send a script to the Workshop. It opens a form on GitHub."));
    m_importBtn->setText(tr("Importar manualmente"));
}
