#include "SettingsPage.h"
#include "AppSettings.h"
#include "Version.h"
#include "I18n.h"
#include "ImportKitDialog.h"
#include "../Checkables.h"
#include "../Theme.h"
#include "../WindowChrome.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QStandardItemModel>
#include <QVBoxLayout>

namespace {

// Standard card: kicker + title + optional description, vertical body.
QFrame *makeCard(QWidget *parent, const QString &kicker, const QString &title,
                 const QString &desc, QVBoxLayout **bodyOut)
{
    auto *card = new QFrame(parent);
    card->setObjectName("Card");
    auto *lay = new QVBoxLayout(card);
    lay->setContentsMargins(18, 15, 18, 16);
    lay->setSpacing(8);
    auto *k = new QLabel(kicker, card);
    k->setObjectName("CardKicker");
    lay->addWidget(k);
    auto *t = new QLabel(title, card);
    t->setObjectName("CardTitle");
    lay->addWidget(t);
    if (!desc.isEmpty()) {
        auto *d = new QLabel(desc, card);
        d->setObjectName("CardDesc");
        d->setWordWrap(true);
        lay->addWidget(d);
    }
    lay->addSpacing(2);
    *bodyOut = lay;
    return card;
}

QString materialName(Theme::Material m)
{
    switch (m) {
    case Theme::Material::Solid: return SettingsPage::tr("Solid");
    case Theme::Material::Mica: return SettingsPage::tr("Mica");
    case Theme::Material::MicaAlt: return SettingsPage::tr("Mica Alt");
    case Theme::Material::Acrylic: return SettingsPage::tr("Acrylic");
    case Theme::Material::Blur: return SettingsPage::tr("Blur");
    case Theme::Material::Frosted: return SettingsPage::tr("Frosted glass");
    }
    return QString();
}

QString materialBlurb(Theme::Material m)
{
    switch (m) {
    case Theme::Material::Solid:
        return SettingsPage::tr("Plain theme colours. Nothing shows through.");
    case Theme::Material::Mica:
        return SettingsPage::tr("A soft tint taken from your wallpaper. Needs Windows 11.");
    case Theme::Material::MicaAlt:
        return SettingsPage::tr("Mica with more of the wallpaper's colour. Needs Windows 11 22H2 or later.");
    case Theme::Material::Acrylic:
        return SettingsPage::tr("See-through and blurred, like the Start menu. Needs Windows 11 22H2 or later.");
    case Theme::Material::Blur:
        return SettingsPage::tr("A clear blur of whatever sits behind the window.");
    case Theme::Material::Frosted:
        return SettingsPage::tr("Blur with a fine grain over it, like frosted glass.");
    }
    return QString();
}

void fillMaterials(QComboBox *combo)
{
    combo->clear();
    auto *model = qobject_cast<QStandardItemModel *>(combo->model());
    for (Theme::Material m : Theme::materials()) {
        combo->addItem(materialName(m), Theme::materialKey(m));
        if (model && !WindowChrome::supports(m))
            model->item(combo->count() - 1)->setEnabled(false);
    }
}

} // namespace

SettingsPage::SettingsPage(AppSettings &settings, QWidget *parent)
    : QWidget(parent)
    , m_settings(settings)
{
    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *inner = new QWidget;
    auto *root = new QVBoxLayout(inner);
    root->setContentsMargins(28, 22, 28, 24);
    root->setSpacing(14);

    // ---- 1. Profile ----
    QVBoxLayout *profileBody = nullptr;
    auto *profile = makeCard(inner, tr("PERFIL"), tr("Seu nickname"),
                             QString(), &profileBody);
    m_username = new QLineEdit(settings.username, profile);
    m_username->setPlaceholderText(tr("e.g. Player"));
    m_username->setMinimumHeight(38);
    profileBody->addWidget(m_username);
    connect(m_username, &QLineEdit::textChanged, this, [this](const QString &t) { m_settings.username = t; });
    profile->setObjectName("cardProfile");
    root->addWidget(profile);

    QVBoxLayout *langBody = nullptr;
    auto *langCard = makeCard(inner, tr("IDIOMA"), tr("Idioma do programa"),
                              tr("Vale para o setup e para o launcher. Alguns textos ja abertos atualizam ao reabrir."),
                              &langBody);
    m_langCombo = new Ui::ComboBox(langCard);
    m_langCombo->setMinimumHeight(36);
    for (const QString &code : I18n::codes())
        m_langCombo->addItem(I18n::displayName(code), code);
    int li = m_langCombo->findData(m_settings.language);
    m_langCombo->setCurrentIndex(li >= 0 ? li : 0);
    langBody->addWidget(m_langCombo);
    connect(m_langCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        const QString code = m_langCombo->currentData().toString();
        if (code.isEmpty())
            return;
        m_settings.language = code;
        I18n::apply(code);
    });
    langCard->setObjectName("cardLang");
    root->addWidget(langCard);

    root->addWidget(buildAppearanceCard(inner));

    QVBoxLayout *homeBody = nullptr;
    auto *homeCard = makeCard(inner, tr("INÍCIO"), tr("Página inicial"),
                              tr("Desative para esconder o catálogo da barra lateral."),
                              &homeBody);
    m_homeEnabled = new QCheckBox(tr("Mostrar a aba Início"), homeCard);
    m_homeEnabled->setChecked(m_settings.homeEnabled);
    homeBody->addWidget(m_homeEnabled);
    connect(m_homeEnabled, &QCheckBox::toggled, this, [this](bool on) {
        m_settings.homeEnabled = on;
        emit homeEnabledChanged(on);
    });
    homeCard->setObjectName("cardHome");
    root->addWidget(homeCard);

    QVBoxLayout *updBody = nullptr;
    auto *updCard = makeCard(inner, tr("ATUALIZAÇÕES"), tr("Verificar atualizações"),
                             tr("O launcher consulta cll_update.json ao iniciar."),
                             &updBody);
    m_checkUpdates = new QCheckBox(tr("Avisar sobre atualizações ao iniciar"), updCard);
    m_checkUpdates->setChecked(m_settings.checkUpdatesOnStart);
    m_checkUpdatesBtn = new QPushButton(tr("Verificar atualizações"), updCard);
    m_checkUpdatesBtn->setCursor(Qt::PointingHandCursor);
    updBody->addWidget(m_checkUpdates);
    updBody->addWidget(m_checkUpdatesBtn);
    connect(m_checkUpdates, &QCheckBox::toggled, this, [this](bool on) {
        m_settings.checkUpdatesOnStart = on;
    });
    connect(m_checkUpdatesBtn, &QPushButton::clicked, this, &SettingsPage::checkUpdatesRequested);
    updCard->setObjectName("cardUpdates");
    root->addWidget(updCard);

    // ---- 2. Plutonium client (folder + portable kit on the same card) ----
    QVBoxLayout *puBody = nullptr;
    auto *pu = makeCard(inner, tr("CLIENT"), tr("Plutonium"),
                        tr("Use o kit portatil em ./pu ou a instalacao oficial em AppData."),
                        &puBody);
    auto *puRow = new QHBoxLayout();
    puRow->setSpacing(8);
    m_plutonium = new QLineEdit(settings.plutoniumInstance, pu);
    m_plutonium->setPlaceholderText(tr("Pasta do client"));
    auto *browse = new QPushButton(tr("Procurar"), pu);
    browse->setObjectName("settingsPuBrowse");
    puRow->addWidget(m_plutonium, 1);
    puRow->addWidget(browse);
    puBody->addLayout(puRow);

    auto *kitRow = new QHBoxLayout();
    kitRow->setSpacing(10);
    m_kitHint = new QLabel(pu);
    m_kitHint->setWordWrap(true);
    m_kitHint->setObjectName("MutedHint");
    m_kitBtn = new QPushButton(tr("Baixar Plutonium Portable"), pu);
    kitRow->addWidget(m_kitHint, 1);
    kitRow->addWidget(m_kitBtn, 0, Qt::AlignBottom);
    puBody->addSpacing(2);
    puBody->addLayout(kitRow);

    connect(m_plutonium, &QLineEdit::textChanged, this, [this](const QString &t) {
        m_settings.plutoniumInstance = t;
        updateKitButton();
        emit plutoniumFolderChanged();
    });
    connect(m_plutonium, &QLineEdit::editingFinished, this, [this]() {
        const QString abs = AppSettings::resolvePath(m_plutonium->text());
        if (abs != m_plutonium->text())
            m_plutonium->setText(abs);
        m_settings.plutoniumInstance = abs;
        emit plutoniumFolderChanged();
    });
    connect(browse, &QPushButton::clicked, this, [this]() {
        const QString dir = QFileDialog::getExistingDirectory(this, tr("Pasta Plutonium"), m_plutonium->text());
        if (!dir.isEmpty())
            m_plutonium->setText(dir);
    });
    connect(m_kitBtn, &QPushButton::clicked, this, [this]() {
        if (m_settings.usingLocalPortableKit())
            return;
        ImportKitDialog dlg(this);
        dlg.exec();
        if (!dlg.installedOk())
            return;
        const QString puDir = AppSettings::localPuDir();
        m_settings.plutoniumInstance = puDir;
        m_plutonium->setText(puDir);
        m_settings.saveToIni();
        emit plutoniumFolderChanged();
        updateKitButton();
    });
    updateKitButton();
    pu->setObjectName("cardPu");
    root->addWidget(pu);

    // ---- 3. Game folders ----
    QVBoxLayout *gamesBody = nullptr;
    auto *games = makeCard(inner, tr("JOGOS"), tr("Pastas de instalacao"),
                           tr("Marque so os jogos que voce tem. Desmarcar nao apaga o caminho digitado."),
                           &gamesBody);
    folderRow(gamesBody, tr("T4 · World at War"), &AppSettings::waw);
    folderRow(gamesBody, tr("T5 · Black Ops"), &AppSettings::bo1);
    folderRow(gamesBody, tr("T6 · Black Ops II"), &AppSettings::bo2);
    folderRow(gamesBody, tr("IW5 · Modern Warfare 3"), &AppSettings::mw3);
    folderRow(gamesBody, tr("S1 · Advanced Warfare"), &AppSettings::aw);
    folderRow(gamesBody, tr("T7 · Black Ops III"), &AppSettings::bo3);
    games->setObjectName("cardGames");
    root->addWidget(games);
    root->addStretch();

    auto *actions = new QHBoxLayout();
    actions->addStretch();
    m_cancelBtn = new QPushButton(tr("Cancelar"), inner);
    m_saveBtn = new QPushButton(tr("Salvar"), inner);
    m_saveBtn->setProperty("cssClass", "primary");
    m_cancelBtn->setCursor(Qt::PointingHandCursor);
    m_saveBtn->setCursor(Qt::PointingHandCursor);
    m_cancelBtn->setMinimumHeight(40);
    m_saveBtn->setMinimumHeight(40);
    m_saveBtn->setMinimumWidth(120);
    actions->addWidget(m_cancelBtn);
    actions->addWidget(m_saveBtn);
    root->addLayout(actions);
    connect(m_saveBtn, &QPushButton::clicked, this, &SettingsPage::saveEdit);
    connect(m_cancelBtn, &QPushButton::clicked, this, &SettingsPage::cancelEdit);

    scroll->setWidget(inner);
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(scroll);
}

// Theme, accent, window material and title bar. Every control writes
// m_settings and emits lookChanged(); MainWindow applies the result, so
// Cancel and the live preview share one path.
QFrame *SettingsPage::buildAppearanceCard(QWidget *parent)
{
    QVBoxLayout *body = nullptr;
    QFrame *card = makeCard(parent, tr("TEMA"), tr("Aparência do launcher"),
                            tr("Theme, accent colour, window material and title bar."), &body);
    card->setObjectName("cardTheme");

    auto label = [&](const char *text) {
        auto *l = new QLabel(tr(text), card);
        l->setObjectName("FieldLabel");
        l->setProperty("trText", QByteArray(text));
        body->addSpacing(6);
        body->addWidget(l);
    };

    label(QT_TR_NOOP("Theme"));
    m_themeCombo = new Ui::ComboBox(card);
    m_themeCombo->setMinimumHeight(36);
    for (const QString &key : Theme::names())
        m_themeCombo->addItem(Theme::displayName(key), key);
    const int thi = m_themeCombo->findData(Theme::normalizeKey(m_settings.theme));
    m_themeCombo->setCurrentIndex(thi >= 0 ? thi : 0);
    body->addWidget(m_themeCombo);
    connect(m_themeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        const QString key = m_themeCombo->currentData().toString();
        if (key.isEmpty())
            return;
        m_settings.theme = key;
        emit lookChanged();
    });

    label(QT_TR_NOOP("Accent colour"));
    auto *swatches = new QHBoxLayout();
    swatches->setSpacing(6);
    for (const Theme::Swatch &sw : Theme::swatches()) {
        auto *b = new QPushButton(card);
        b->setObjectName("AccentSwatch");
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setFixedSize(26, 26);
        b->setToolTip(QString::fromLatin1(sw.name));
        b->setProperty("accent", QString::fromLatin1(sw.hex));
        b->setStyleSheet(QStringLiteral("QPushButton#AccentSwatch { background-color: %1; }")
                             .arg(QLatin1String(sw.hex)));
        swatches->addWidget(b);
        m_accentButtons << b;
    }
    swatches->addStretch();
    body->addLayout(swatches);

    auto *accentRow = new QHBoxLayout();
    accentRow->setSpacing(8);
    auto choice = [&](const char *text, const QString &spec) {
        auto *b = new QPushButton(tr(text), card);
        b->setObjectName("AccentChoice");
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setProperty("accent", spec);
        b->setProperty("trText", QByteArray(text));
        accentRow->addWidget(b);
        m_accentButtons << b;
    };
    choice(QT_TR_NOOP("Theme colour"), QString());
    if (Theme::systemAccent().isValid())
        choice(QT_TR_NOOP("Windows accent"), QStringLiteral("system"));
    m_accentHex = new QLineEdit(card);
    m_accentHex->setMaxLength(7);
    m_accentHex->setFixedWidth(100);
    m_accentHex->setMinimumHeight(34);
    m_accentHex->setPlaceholderText(QStringLiteral("#8aadf4"));
    m_accentHex->setToolTip(tr("Type any colour as #rrggbb."));
    accentRow->addWidget(m_accentHex);
    auto *pick = new QPushButton(tr("Pick a colour"), card);
    pick->setProperty("trText", QByteArray(QT_TR_NOOP("Pick a colour")));
    pick->setCursor(Qt::PointingHandCursor);
    accentRow->addWidget(pick);
    accentRow->addStretch();
    body->addLayout(accentRow);

    for (QPushButton *b : std::as_const(m_accentButtons))
        connect(b, &QPushButton::clicked, this, [this, b]() { setAccentSpec(b->property("accent").toString()); });
    connect(m_accentHex, &QLineEdit::editingFinished, this, [this]() {
        const QString spec = Theme::normalizeAccent(m_accentHex->text());
        if (spec.startsWith(QLatin1Char('#')))
            setAccentSpec(spec);
        else
            syncAppearance(); // not a colour: put the current one back
    });
    connect(pick, &QPushButton::clicked, this, [this]() {
        const QColor c = QColorDialog::getColor(Theme::color("ACCENT"), this, tr("Accent colour"));
        if (c.isValid())
            setAccentSpec(c.name());
    });

    label(QT_TR_NOOP("Window material"));
    m_materialCombo = new Ui::ComboBox(card);
    m_materialCombo->setMinimumHeight(36);
    fillMaterials(m_materialCombo);
    body->addWidget(m_materialCombo);
    m_materialDesc = new QLabel(card);
    m_materialDesc->setObjectName("MutedHint");
    m_materialDesc->setWordWrap(true);
    body->addWidget(m_materialDesc);
    connect(m_materialCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        const QString key = m_materialCombo->currentData().toString();
        if (key.isEmpty())
            return;
        m_settings.material = key;
        emit lookChanged();
    });

    label(QT_TR_NOOP("Tint"));
    auto *tintRow = new QHBoxLayout();
    tintRow->setSpacing(10);
    m_tint = new QSlider(Qt::Horizontal, card);
    m_tint->setRange(0, 100);
    m_tint->setPageStep(10);
    m_tint->setToolTip(tr("How much theme colour sits over the material. Lower shows more of the desktop."));
    m_tintValue = new QLabel(card);
    m_tintValue->setMinimumWidth(40);
    tintRow->addWidget(m_tint, 1);
    tintRow->addWidget(m_tintValue);
    body->addLayout(tintRow);
    // A whole stylesheet pass per pixel of drag is too slow: apply on release.
    connect(m_tint, &QSlider::valueChanged, this, [this](int v) {
        m_settings.materialTint = v;
        m_tintValue->setText(QStringLiteral("%1%").arg(v));
        if (!m_tint->isSliderDown())
            emit lookChanged();
    });
    connect(m_tint, &QSlider::sliderReleased, this, &SettingsPage::lookChanged);

    label(QT_TR_NOOP("Title bar"));
    auto *titleRow = new QHBoxLayout();
    titleRow->setSpacing(4);
    m_titleWindows = new Ui::RadioButton(tr("Windows"), card);
    m_titleBlutac = new Ui::RadioButton(tr("Blutac"), card);
    m_titleBlutac->setToolTip(tr("Blutac draws its own minimise, maximise and close buttons, "
                                 "so the material runs right to the top edge."));
    m_titleBlutac->setEnabled(WindowChrome::supportsCustomTitleBar());
    auto *titleGroup = new QButtonGroup(card);
    titleGroup->addButton(m_titleWindows);
    titleGroup->addButton(m_titleBlutac);
    titleRow->addWidget(m_titleWindows);
    titleRow->addWidget(m_titleBlutac);
    titleRow->addStretch();
    body->addLayout(titleRow);
    connect(m_titleBlutac, &QRadioButton::toggled, this, [this](bool on) {
        m_settings.customTitleBar = on;
        emit lookChanged();
    });

    m_lookHint = new QLabel(card);
    m_lookHint->setObjectName("MutedHint");
    m_lookHint->setWordWrap(true);
    body->addWidget(m_lookHint);

    m_restartRow = new QWidget(card);
    m_restartRow->setObjectName("LookRow");
    auto *restart = new QHBoxLayout(m_restartRow);
    restart->setContentsMargins(0, 6, 0, 0);
    auto *restartText = new QLabel(tr("Blutac needs a restart for this one."), m_restartRow);
    restartText->setProperty("trText", QByteArray(QT_TR_NOOP("Blutac needs a restart for this one.")));
    auto *restartBtn = new QPushButton(tr("Restart now"), m_restartRow);
    restartBtn->setProperty("trText", QByteArray(QT_TR_NOOP("Restart now")));
    restartBtn->setProperty("cssClass", "primary");
    restartBtn->setCursor(Qt::PointingHandCursor);
    restart->addWidget(restartText, 1);
    restart->addWidget(restartBtn);
    body->addWidget(m_restartRow);
    connect(restartBtn, &QPushButton::clicked, this, &SettingsPage::restartRequested);

    // The accent field and swatches show what the theme actually applied.
    connect(&ThemeHub::instance(), &ThemeHub::themeChanged, this, &SettingsPage::syncAppearance);
    syncAppearance();
    return card;
}

void SettingsPage::setAccentSpec(const QString &spec)
{
    m_settings.accent = spec;
    emit lookChanged();
}

void SettingsPage::setRestartNeeded(bool needed)
{
    m_restartNeeded = needed;
    syncAppearance();
}

void SettingsPage::syncAppearance()
{
    if (!m_materialCombo)
        return;
    const QString spec = Theme::normalizeAccent(m_settings.accent);
    for (QPushButton *b : std::as_const(m_accentButtons))
        b->setChecked(b->property("accent").toString() == spec);
    m_accentHex->setText(Theme::color("ACCENT").name());

    const Theme::Material m = Theme::materialFromKey(m_settings.material);
    const int mi = m_materialCombo->findData(Theme::materialKey(m));
    m_materialCombo->blockSignals(true);
    m_materialCombo->setCurrentIndex(mi >= 0 ? mi : 0);
    m_materialCombo->blockSignals(false);
    m_materialDesc->setText(materialBlurb(m));

    m_tint->blockSignals(true);
    m_tint->setValue(m_settings.materialTint);
    m_tint->blockSignals(false);
    m_tint->setEnabled(m != Theme::Material::Solid);
    m_tintValue->setText(QStringLiteral("%1%").arg(m_settings.materialTint));

    const bool blutac = m_settings.customTitleBar && WindowChrome::supportsCustomTitleBar();
    for (QRadioButton *r : {m_titleWindows, m_titleBlutac})
        r->blockSignals(true);
    (blutac ? m_titleBlutac : m_titleWindows)->setChecked(true);
    for (QRadioButton *r : {m_titleWindows, m_titleBlutac})
        r->blockSignals(false);

    // Blur behind does not reach the Windows caption; Mica and Acrylic do.
    const bool blurWithCaption = !blutac
        && (m == Theme::Material::Blur || m == Theme::Material::Frosted);
    m_lookHint->setText(tr("Blur and Frosted glass look best with the Blutac title bar."));
    m_lookHint->setVisible(blurWithCaption);
    m_restartRow->setVisible(m_restartNeeded);
}

QLineEdit *SettingsPage::folderRow(QVBoxLayout *layout, const QString &label,
                                   QString AppSettings::*field)
{
    const bool has = !(m_settings.*field).isEmpty();

    auto *row = new QHBoxLayout();
    row->setSpacing(8);
    auto *check = new Ui::CheckBox(label, this);
    check->setChecked(has);
    check->setMinimumWidth(qMax(check->sizeHint().width(), 190));
    auto *edit = new QLineEdit(m_settings.*field, this);
    edit->setPlaceholderText(tr("Pasta de instalacao"));
    edit->setEnabled(has);
    auto *browse = new QPushButton(tr("Procurar"), this);
    browse->setEnabled(has);
    row->addWidget(check);
    row->addWidget(edit, 1);
    row->addWidget(browse);
    layout->addSpacing(4);
    layout->addLayout(row);

    connect(check, &QCheckBox::toggled, this, [this, field, edit, browse](bool on) {
        edit->setEnabled(on);
        browse->setEnabled(on);
        // The path stays visible; unchecking only clears the effective value.
        m_settings.*field = on ? edit->text() : QString();
    });
    connect(edit, &QLineEdit::textChanged, this, [this, field, check](const QString &t) {
        if (check->isChecked())
            m_settings.*field = t;
    });
    connect(browse, &QPushButton::clicked, this, [this, edit, check]() {
        const QString dir = QFileDialog::getExistingDirectory(this, tr("Procurar"), edit->text());
        if (!dir.isEmpty()) {
            edit->setText(dir);
            check->setChecked(true);
        }
    });
    return edit;
}

void SettingsPage::updateKitButton()
{
    if (!m_kitBtn)
        return;
    const bool usingPu = m_settings.usingLocalPortableKit();
    m_kitBtn->setEnabled(!usingPu);
    if (usingPu)
        m_kitHint->setText(tr("✓ Usando kit PU Portable"));
    else
        m_kitHint->setText(tr("Baixa o kit portatil (pu.dat) para a pasta pu ao lado do programa."));
}

void SettingsPage::pullFromSettings()
{
    m_username->setText(m_settings.username);
    m_plutonium->setText(m_settings.plutoniumInstance);
    if (m_langCombo) {
        const int i = m_langCombo->findData(m_settings.language);
        if (i >= 0)
            m_langCombo->setCurrentIndex(i);
    }
    if (m_themeCombo) {
        const int i = m_themeCombo->findData(Theme::normalizeKey(m_settings.theme));
        m_themeCombo->blockSignals(true);
        m_themeCombo->setCurrentIndex(i >= 0 ? i : 0);
        m_themeCombo->blockSignals(false);
    }
    syncAppearance();
    if (m_homeEnabled) {
        m_homeEnabled->blockSignals(true);
        m_homeEnabled->setChecked(m_settings.homeEnabled);
        m_homeEnabled->blockSignals(false);
    }
    if (m_checkUpdates) {
        m_checkUpdates->blockSignals(true);
        m_checkUpdates->setChecked(m_settings.checkUpdatesOnStart);
        m_checkUpdates->blockSignals(false);
    }
    updateKitButton();
}

void SettingsPage::beginEdit()
{
    m_snapshot = m_settings;
    pullFromSettings();
}

void SettingsPage::saveEdit()
{
    m_settings.saveToIni();
    m_snapshot = m_settings;
}

void SettingsPage::cancelEdit()
{
    m_settings = m_snapshot;
    I18n::apply(m_settings.language);
    emit lookChanged();
    pullFromSettings();
    emit homeEnabledChanged(m_settings.homeEnabled);
    emit plutoniumFolderChanged();
    emit cancelled();
}

void SettingsPage::retranslate()
{
    auto fill = [](QFrame *card, const QString &k, const QString &ti, const QString &d) {
        if (!card) return;
        for (auto *l : card->findChildren<QLabel*>(QString(), Qt::FindDirectChildrenOnly)) {
            if (l->objectName() == QLatin1String("CardKicker")) l->setText(k);
            else if (l->objectName() == QLatin1String("CardTitle")) l->setText(ti);
            else if (l->objectName() == QLatin1String("CardDesc")) l->setText(d);
        }
    };
    fill(findChild<QFrame*>("cardProfile"), tr("PERFIL"), tr("Seu nickname"), QString());
    fill(findChild<QFrame*>("cardLang"), tr("IDIOMA"), tr("Idioma do programa"),
         tr("Vale para o setup e para o launcher."));
    if (auto *card = findChild<QFrame*>("cardTheme")) {
        fill(card, tr("TEMA"), tr("Aparência do launcher"),
             tr("Theme, accent colour, window material and title bar."));
        for (QWidget *w : card->findChildren<QWidget *>()) {
            const QByteArray key = w->property("trText").toByteArray();
            if (key.isEmpty())
                continue;
            if (auto *l = qobject_cast<QLabel *>(w))
                l->setText(tr(key.constData()));
            else if (auto *b = qobject_cast<QPushButton *>(w))
                b->setText(tr(key.constData()));
        }
    }
    if (m_titleWindows)
        m_titleWindows->setText(tr("Windows"));
    if (m_titleBlutac)
        m_titleBlutac->setText(tr("Blutac"));
    if (m_materialCombo) {
        m_materialCombo->blockSignals(true);
        fillMaterials(m_materialCombo);
        m_materialCombo->blockSignals(false);
        syncAppearance();
    }
    fill(findChild<QFrame*>("cardHome"), tr("INÍCIO"), tr("Página inicial"),
         tr("Desative para esconder o catálogo da barra lateral."));
    if (m_homeEnabled)
        m_homeEnabled->setText(tr("Mostrar a aba Início"));
    fill(findChild<QFrame*>("cardUpdates"), tr("ATUALIZAÇÕES"), tr("Verificar atualizações"),
         tr("O launcher consulta cll_update.json ao iniciar."));
    if (m_checkUpdates)
        m_checkUpdates->setText(tr("Avisar sobre atualizações ao iniciar"));
    if (m_checkUpdatesBtn)
        m_checkUpdatesBtn->setText(tr("Verificar atualizações"));
    if (m_saveBtn)
        m_saveBtn->setText(tr("Salvar"));
    if (m_cancelBtn)
        m_cancelBtn->setText(tr("Cancelar"));
    fill(findChild<QFrame*>("cardPu"), tr("CLIENT"), tr("Plutonium"),
         tr("Use o kit portatil em ./pu ou a instalacao oficial em AppData."));
    fill(findChild<QFrame*>("cardGames"), tr("JOGOS"), tr("Pastas de instalacao"),
         tr("Marque so os jogos que voce tem. Desmarcar nao apaga o caminho digitado."));
    if (m_username)
        m_username->setPlaceholderText(tr("e.g. Player"));
    if (m_plutonium)
        m_plutonium->setPlaceholderText(tr("Pasta do client"));
    if (auto *b = findChild<QPushButton*>("settingsPuBrowse"))
        b->setText(tr("Procurar"));
    if (m_kitBtn)
        m_kitBtn->setText(tr("Baixar Plutonium Portable"));
    updateKitButton();
    if (m_langCombo) {
        const QString cur = m_langCombo->currentData().toString();
        m_langCombo->blockSignals(true);
        m_langCombo->clear();
        for (const QString &code : I18n::codes())
            m_langCombo->addItem(I18n::displayName(code), code);
        const int i = m_langCombo->findData(cur);
        m_langCombo->setCurrentIndex(i >= 0 ? i : 0);
        m_langCombo->blockSignals(false);
    }
    if (m_themeCombo) {
        const QString cur = m_themeCombo->currentData().toString();
        m_themeCombo->blockSignals(true);
        m_themeCombo->clear();
        for (const QString &key : Theme::names())
            m_themeCombo->addItem(Theme::displayName(key), key);
        const int i = m_themeCombo->findData(cur);
        m_themeCombo->setCurrentIndex(i >= 0 ? i : 0);
        m_themeCombo->blockSignals(false);
    }
}
