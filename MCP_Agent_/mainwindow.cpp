#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "liquidglass.h"
#include <QComboBox>
#include <QButtonGroup>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QDateTime>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QTextCursor>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QShowEvent>
#include <QTimer>
#include <QRandomGenerator>
#include <QDialog>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QFontMetrics>
#include <QFontDatabase>
#include <QDir>
#include <QTabWidget>
#include <QSettings>
#include <QCheckBox>
#include <QElapsedTimer>
#include <QGraphicsDropShadowEffect>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QVariantAnimation>
#include <QEasingCurve>
#include <QPainter>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QPainterPath>
#include <QColor>
#include <QColorDialog>
#include <QFileDialog>
#include <QMovie>
#include <QStackedWidget>
#include <QGridLayout>
#include <QSet>
#include <QGuiApplication>
#include <QClipboard>
#include <QResizeEvent>
#include <QFormLayout>
#include <QSharedPointer>
#include <QRegularExpression>
#include <QCoreApplication>
#include <QStatusBar>
#include <QPointer>
#include <cmath>
#include <utility>
#include <QPointer>
#include <cmath>
#include <utility>
#include <functional>
#include <QResizeEvent>
#include <QFormLayout>
#include <QSharedPointer>
#include "telegramaccountclient.h" // можно включить, но header уже подключён в mainwindow.h

#include "telegramaccountclient.h"

// ===========================================================================
// Палитры / токены
// ===========================================================================
namespace {

qreal sharedBackdropPhase()
{
    static QElapsedTimer clock;
    if (!clock.isValid())
        clock.start();
    return clock.elapsed() * 0.00035;
}

QString kBgWindow;
QString kBgPanel;
QString kBgCard;
QString kBgInput;
QString kBgCardHover;
QString kBorder;
QString kBorderSel;
QString kTextMain;
QString kTextSubtle;
QString kTextMuted;
QString kAccent;
QString kAccent2;
QString kAccentGreen;
QString kAccentRed;

QString kStatusIdle;
QString kStatusActive;
QString kStatusBusy;
QString kStatusError;

struct ThemePalette {
    QString bgWindow, bgPanel, bgCard, bgInput, bgCardHover, border, borderSel;
    QString textMain, textSubtle, textMuted;
    QString accent, accent2, accentGreen, accentRed;
    QString statusIdle, statusActive, statusBusy, statusError;
};

struct ThemeFamily { QString id; QString label; QString lightId; QString darkId; };

QList<ThemeFamily> themeFamilies()
{
    return {
             {"neutral", "Обычная", "light", "dark"},
             {"pink",    "Pink",    "pink",  "pinkdark"},
             {"nord",    "Nord",    "nordlight", "nord"},
             {"sunset",  "Sunset",  "sunsetlight", "sunset"},
             };
}

QString themeDisplayName(const QString &themeId)
{
    if (themeId == "light")      return "Daylight";
    if (themeId == "nord")       return "Nord";
    if (themeId == "nordlight")  return "Nord Light";
    if (themeId == "sunset")     return "Sunset";
    if (themeId == "sunsetlight") return "Sunrise";
    if (themeId == "pink")       return "Sakura";
    if (themeId == "pinkdark")   return "Yozakura";
    if (themeId == "custom")     return "Пользовательская";
    return "Midnight";
}

struct BaseColors { QString bg, panel, card, border, text, accent, accent2, success, danger; };

BaseColors builtinBaseColors(const QString &themeId)
{
    if (themeId == "light")
        return { "#eef0f7", "#ffffff", "#ffffff", "#dfe2ee", "#1c1e2b", "#4c6fff", "#9c5cff", "#1fa971", "#e0446f" };
    if (themeId == "nord")
        return { "#242933", "#2e3440", "#333a47", "#434c5e", "#eceff4", "#88c0d0", "#b48ead", "#a3be8c", "#bf616a" };
    if (themeId == "nordlight")
        return { "#eceff4", "#ffffff", "#f7f9fc", "#d8dee9", "#2e3440", "#5e81ac", "#88c0d0", "#6a9955", "#b1414d" };
    if (themeId == "sunset")
        return { "#1a1420", "#241a2e", "#2b1f38", "#40304d", "#f7e9f7", "#ff8a5c", "#ff5c8a", "#7ed6a5", "#ff5c7a" };
    if (themeId == "sunsetlight")
        return { "#fdf3ea", "#ffffff", "#fff8f1", "#f0dcc8", "#3d2b1f", "#d9642a", "#c23f6e", "#4e9e6f", "#d9455f" };
    if (themeId == "pink")
        return { "#fdf0f5", "#ffffff", "#fff6fa", "#f2d7e3", "#3d1f2b", "#e0568c", "#a565c9", "#2e9e6b", "#e0483f" };
    if (themeId == "pinkdark")
        return { "#1c0f16", "#26141d", "#301a25", "#4a2536", "#f7dce8", "#ff6fa8", "#c77dff", "#4fd18f", "#ff4d6d" };
    return { "#11111b", "#181825", "#1e1e2e", "#313244", "#cdd6f4", "#89b4fa", "#cba6f7", "#a6e3a1", "#f38ba8" };
}

QString themeOverrideGroup(const QString &themeId)
{
    return themeId == "custom" ? "custom_theme" : QString("theme_%1").arg(themeId);
}

ThemePalette buildPalette(const QColor &bg, const QColor &panel, const QColor &card, const QColor &border,
                          const QColor &text, const QColor &accent, const QColor &accent2,
                          const QColor &success, const QColor &danger)
{
    const bool dark = bg.lightness() < 128;
    const QColor cardHover  = dark ? card.lighter(112) : card.darker(104);
    const QColor textSubtle = dark ? text.darker(120)  : text.lighter(140);
    const QColor textMuted  = dark ? text.darker(160)  : text.lighter(180);

    return { bg.name(), panel.name(), card.name(), card.name(), cardHover.name(), border.name(), accent.name(),
            text.name(), textSubtle.name(), textMuted.name(),
            accent.name(), accent2.name(), success.name(), danger.name(),
            textMuted.name(), success.name(), "#f9e2af", danger.name() };
}

ThemePalette paletteFor(const QString &themeName)
{
    const BaseColors base = builtinBaseColors(themeName);

    QSettings settings(QString(APP_SRC_DIR) + "/config.ini", QSettings::IniFormat);
    settings.beginGroup(themeOverrideGroup(themeName));
    const QColor bg      = QColor(settings.value("bg", base.bg).toString());
    const QColor panel   = QColor(settings.value("panel", base.panel).toString());
    const QColor card    = QColor(settings.value("card", base.card).toString());
    const QColor border  = QColor(settings.value("border", base.border).toString());
    const QColor text    = QColor(settings.value("text", base.text).toString());
    const QColor accent  = QColor(settings.value("accent", base.accent).toString());
    const QColor accent2 = QColor(settings.value("accent2", base.accent2).toString());
    const QColor success = QColor(settings.value("success", base.success).toString());
    const QColor danger  = QColor(settings.value("danger", base.danger).toString());
    settings.endGroup();

    return buildPalette(bg, panel, card, border, text, accent, accent2, success, danger);
}

void loadPalette(const QString &themeName)
{
    const ThemePalette p = paletteFor(themeName);

    kBgWindow = p.bgWindow; kBgPanel = p.bgPanel; kBgCard = p.bgCard; kBgInput = p.bgInput;
    kBgCardHover = p.bgCardHover; kBorder = p.border; kBorderSel = p.borderSel;
    kTextMain = p.textMain; kTextSubtle = p.textSubtle; kTextMuted = p.textMuted;
    kAccent = p.accent; kAccent2 = p.accent2; kAccentGreen = p.accentGreen; kAccentRed = p.accentRed;
    kStatusIdle = p.statusIdle; kStatusActive = p.statusActive; kStatusBusy = p.statusBusy; kStatusError = p.statusError;

    Glass::update(QColor(p.bgWindow), QColor(p.accent));
}

constexpr int kBackdropMargin = 6;

QColor lerpColor(const QColor &a, const QColor &b, qreal t)
{
    return QColor::fromRgbF(a.redF()   + (b.redF()   - a.redF())   * t,
                            a.greenF() + (b.greenF() - a.greenF()) * t,
                            a.blueF()  + (b.blueF()  - a.blueF())  * t,
                            a.alphaF() + (b.alphaF() - a.alphaF()) * t);
}

struct AvatarGradient { QColor start; QColor end; };
const QList<AvatarGradient> kAvatarGradients = {
    {QColor("#7c9bff"), QColor("#4c6fff")},
    {QColor("#ff9ecf"), QColor("#ff5f9e")},
    {QColor("#7fe7c4"), QColor("#22b783")},
    {QColor("#ffd479"), QColor("#f6a622")},
    {QColor("#b39bff"), QColor("#7c5cff")},
    {QColor("#7fd4ff"), QColor("#2fa3e0")},
    };

QString iconButtonStyle(const QString &hoverColor, int fontSizePx = 12, int radiusPx = 10)
{
    const QString hoverBg = Glass::css(Glass::withAlpha(QColor(hoverColor), 46));
    return QString(
               "QPushButton {"
               "   background: transparent;"
               "   border: none;"
               "   color: %1;"
               "   font-size: %4px;"
               "   border-radius: %5px;"
               "}"
               "QPushButton:hover {"
               "   background: %2;"
               "   color: %3;"
               "}"
               ).arg(kTextMuted, hoverBg, hoverColor).arg(fontSizePx).arg(radiusPx);
}

QString extractRoleFromMinInfo(const QString &minInfo)
{
    QStringList roleLines;
    for (const QString &line : minInfo.split('\n')) {
        if (line.startsWith("Имя агента:"))
            continue;
        roleLines << line;
    }
    return roleLines.join('\n').trimmed();
}

QString buildMinInfo(const QString &name, const QString &role)
{
    QString result = "Имя агента: " + name;
    const QString trimmedRole = role.trimmed();
    if (!trimmedRole.isEmpty())
        result += "\n" + trimmedRole;
    return result;
}

// Лёгкий markdown → HTML для пузырей: **жирный**, `код`, «- » списки, переводы строк.
// typing=true добавляет мигающий-подобный курсор в конец.
QString renderRich(const QString &raw, bool typing)
{
    QString s = raw.toHtmlEscaped();

    static const QRegularExpression codeRe("`([^`\n]+)`");
    static const QRegularExpression boldRe("\\*\\*(.+?)\\*\\*");
    static const QRegularExpression listRe("(^|\n)- ");

    s.replace(codeRe, QString("<span style=\"background-color:%1; font-family:monospace;\">&nbsp;\\1&nbsp;</span>").arg(kBorder));
    s.replace(boldRe, "<b>\\1</b>");
    s.replace(listRe, "\\1• ");
    s.replace("\n", "<br/>");

    if (typing)
        s += QString("<span style=\"color:%1;\">&#9613;</span>").arg(kAccent);
    return s;
}

} // anonymous namespace

// ===========================================================================
// AnimatedBackdrop
// ===========================================================================
AnimatedBackdrop::AnimatedBackdrop(QWidget *parent)
    : QWidget(parent)
{
    QSettings settings(QString(APP_SRC_DIR) + "/config.ini", QSettings::IniFormat);
    m_mode = settings.value("backdrop_mode", "gradient").toString();
    m_margin = kBackdropMargin;

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, [this]() {
        m_phase = sharedBackdropPhase();
        update();
    });

    if (m_mode == "image") {
        const QString path = settings.value("backdrop_image_path").toString();
        m_movie = new QMovie(path, QByteArray(), this);
        if (m_movie->isValid() && m_movie->frameCount() != 1) {
            connect(m_movie, &QMovie::frameChanged, this, [this]() { update(); });
            m_movie->start();
        } else {
            delete m_movie;
            m_movie = nullptr;
            m_staticImage = QPixmap(path);
        }
    } else {
        static const QStringList animatedModes = {"gradient", "aurora", "particles", "starfall"};
        if (animatedModes.contains(m_mode) && settings.value("backdrop_animation", true).toBool())
            m_timer->start(50);
    }
}

void AnimatedBackdrop::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    paintContent(painter);
}

void AnimatedBackdrop::paintContent(QPainter &painter)
{
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), QColor(kBgWindow));

    if (m_mode == "image") {
        const QPixmap frame = m_movie ? m_movie->currentPixmap() : m_staticImage;
        if (!frame.isNull()) {
            const QPixmap scaled = frame.scaled(size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
            const QPoint offset((width() - scaled.width()) / 2, (height() - scaled.height()) / 2);
            painter.drawPixmap(offset, scaled);
        }
        return;
    }

    if (m_mode == "aurora")    { paintAurora(painter);    return; }
    if (m_mode == "particles") { paintParticles(painter); return; }
    if (m_mode == "starfall")  { paintStarfall(painter);  return; }
    if (m_mode == "none")      return;

    paintBlobs(painter);
}

const QImage &AnimatedBackdrop::lowRes()
{
    const bool animated = (m_timer && m_timer->isActive()) || m_movie;
    const bool stale = m_low.isNull() || m_lowForSize != size()
                       || (animated && (!m_lowClock.isValid() || m_lowClock.elapsed() > 90));
    if (stale) {
        constexpr int kDiv = 6;                    // чем больше, тем сильнее размытие
        const int w = qMax(1, width());
        const int h = qMax(1, height());
        m_low = QImage(qMax(1, w / kDiv), qMax(1, h / kDiv), QImage::Format_ARGB32_Premultiplied);
        QPainter p(&m_low);
        p.scale(qreal(m_low.width()) / w, qreal(m_low.height()) / h);
        paintContent(p);
        m_lowForSize = size();
        m_lowClock.restart();
    }
    return m_low;
}

void AnimatedBackdrop::paintBlobs(QPainter &painter)
{
    struct Blob { qreal ax, ay, freqX, freqY, phaseX, phaseY, radius; QString color; };
    const qreal w = width();
    const qreal h = height();
    const qreal span = qMax(w, h);
    const float alpha = Glass::tokens().dark ? 0.62f : 0.50f;

    const QList<Blob> blobs = {
                               { w * 0.25, h * 0.25, 0.55, 0.42, 0.0, 1.4, span * 0.32, kAccent },
                               { w * 0.80, h * 0.60, 0.40, 0.60, 2.1, 0.4, span * 0.30, kAccent2 },
                               { w * 0.50, h * 0.90, 0.48, 0.35, 4.0, 2.6, span * 0.26, kAccentGreen },
                               { w * 0.90, h * 0.12, 0.33, 0.52, 1.1, 3.3, span * 0.22, kAccent2 },
                               };

    for (const auto &blob : blobs) {
        const qreal cx = blob.ax + std::sin(m_phase * blob.freqX + blob.phaseX) * w * 0.16;
        const qreal cy = blob.ay + std::cos(m_phase * blob.freqY + blob.phaseY) * h * 0.16;

        QRadialGradient gradient(cx, cy, blob.radius);
        QColor inner(blob.color); inner.setAlphaF(alpha);
        QColor mid(blob.color);   mid.setAlphaF(alpha * 0.45f);
        QColor edge(blob.color);  edge.setAlphaF(0.0f);
        gradient.setColorAt(0.0, inner);
        gradient.setColorAt(0.5, mid);
        gradient.setColorAt(1.0, edge);

        painter.setPen(Qt::NoPen);
        painter.setBrush(gradient);
        painter.drawEllipse(QPointF(cx, cy), blob.radius, blob.radius);
    }
}

void AnimatedBackdrop::paintAurora(QPainter &painter)
{
    const float alpha = Glass::tokens().dark ? 0.55f : 0.45f;
    for (int band = 0; band < 2; ++band) {
        const qreal angle = m_phase * 14.0 + band * 70.0;
        const qreal rad = angle * M_PI / 180.0;
        const qreal cx = width() / 2.0;
        const qreal cy = height() / 2.0;
        const qreal len = qMax(width(), height());

        QLinearGradient gradient(cx - std::cos(rad) * len, cy - std::sin(rad) * len,
                                 cx + std::cos(rad) * len, cy + std::sin(rad) * len);

        const QString c1 = band == 0 ? kAccent : kAccent2;
        const QString c2 = band == 0 ? kAccent2 : kAccentGreen;
        QColor edge1(c1); edge1.setAlphaF(0.0f);
        QColor mid1(c1);  mid1.setAlphaF(alpha);
        QColor mid2(c2);  mid2.setAlphaF(alpha);
        QColor edge2(c2); edge2.setAlphaF(0.0f);

        gradient.setColorAt(0.0, edge1);
        gradient.setColorAt(0.35, mid1);
        gradient.setColorAt(0.65, mid2);
        gradient.setColorAt(1.0, edge2);

        painter.fillRect(rect(), gradient);
    }
}

void AnimatedBackdrop::paintParticles(QPainter &painter)
{
    struct Spot { qreal nx, ny, phase, speed; };
    static const QList<Spot> spots = {
                                      {0.02, 0.08, 0.0, 1.0}, {0.97, 0.15, 1.3, 0.8}, {0.06, 0.90, 2.6, 1.2},
                                      {0.94, 0.85, 0.7, 0.9}, {0.35, 0.03, 2.1, 1.1}, {0.65, 0.97, 3.4, 0.7},
                                      {0.03, 0.45, 1.8, 1.0}, {0.97, 0.55, 4.0, 0.8}, {0.20, 0.04, 2.9, 1.3},
                                      {0.80, 0.96, 0.3, 0.9}, {0.50, 0.50, 1.0, 0.6}, {0.25, 0.65, 3.0, 0.9},
                                      {0.72, 0.30, 0.5, 1.1},
                                      };

    const QStringList palette = {kAccent, kAccent2, kAccentGreen};
    const qreal w = width();
    const qreal h = height();

    painter.setPen(Qt::NoPen);
    int i = 0;
    for (const auto &spot : spots) {
        const qreal twinkle = 0.5 + 0.5 * std::sin(m_phase * spot.speed + spot.phase);
        const qreal radius = (Glass::tokens().dark ? 70.0 : 80.0) + twinkle * 60.0;

        QColor color(palette.at(i % palette.size()));
        QRadialGradient g(QPointF(spot.nx * w, spot.ny * h), radius);
        color.setAlphaF(0.20f + twinkle * 0.30f);
        g.setColorAt(0.0, color);
        color.setAlphaF(0.0f);
        g.setColorAt(1.0, color);
        painter.setBrush(g);
        painter.drawEllipse(QPointF(spot.nx * w, spot.ny * h), radius, radius);
        ++i;
    }
}

void AnimatedBackdrop::paintStarfall(QPainter &painter)
{
    struct Meteor { qreal startX, startY, angle, trailLen, speed, offset; QString color; };
    static const QList<Meteor> meteors = {
                                           {0.10, -0.05, 32.0, 0.20, 0.55, 0.00, kAccent},
                                           {0.42, -0.08, 28.0, 0.16, 0.42, 0.40, kAccent2},
                                           {0.68, -0.04, 36.0, 0.22, 0.65, 0.70, kAccent},
                                           {0.25, -0.10, 30.0, 0.14, 0.38, 0.20, kAccentGreen},
                                           {0.85, -0.06, 34.0, 0.24, 0.50, 0.85, kAccent2},
                                           };

    const qreal w = width();
    const qreal h = height();
    const qreal diag = std::sqrt(w * w + h * h);

    for (const auto &m : meteors) {
        const qreal cycle = std::fmod(m_phase * m.speed + m.offset, 1.0);
        if (cycle > 0.55) continue;

        const qreal rad = m.angle * M_PI / 180.0;
        const qreal travel = (cycle / 0.55) * diag * 1.25;
        const QPointF head(m.startX * w + std::cos(rad) * travel, m.startY * h + std::sin(rad) * travel);
        const QPointF tail(head.x() - std::cos(rad) * diag * m.trailLen, head.y() - std::sin(rad) * diag * m.trailLen);

        const qreal fadeIn = qMin(1.0, cycle / 0.08);
        const qreal fadeOut = qMin(1.0, (0.55 - cycle) / 0.15);
        const qreal alpha = qMin(fadeIn, fadeOut);
        if (alpha <= 0.02) continue;

        QColor headColor(m.color); headColor.setAlphaF(float(0.9 * alpha));
        QColor tailColor(m.color); tailColor.setAlphaF(0.0f);

        QLinearGradient trailGradient(head, tail);
        trailGradient.setColorAt(0.0, headColor);
        trailGradient.setColorAt(1.0, tailColor);

        QPen trailPen(QBrush(trailGradient), 2.4);
        trailPen.setCapStyle(Qt::RoundCap);
        painter.setPen(trailPen);
        painter.drawLine(head, tail);

        QColor dotColor(Qt::white); dotColor.setAlphaF(float(0.95 * alpha));
        painter.setPen(Qt::NoPen);
        painter.setBrush(dotColor);
        painter.drawEllipse(head, 2.4, 2.4);
    }
}

// ===========================================================================
// NetworkBackdrop — стеклянная панель
// ===========================================================================
NetworkBackdrop::NetworkBackdrop(Surface surface, QWidget *parent)
    : QWidget(parent)
    , m_surface(surface)
{
    QSettings settings(QString(APP_SRC_DIR) + "/config.ini", QSettings::IniFormat);
    m_mode = settings.value("backdrop_mode", "gradient").toString();
    // сетку узлов убрали: поверх стекла анимируется только «звездопад»
    m_enabled = (m_mode == "starfall") && settings.value("backdrop_animation", true).toBool();

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, [this]() {
        m_phase = sharedBackdropPhase();
        update();
    });
    if (m_enabled)
        m_timer->start(90);
}

void NetworkBackdrop::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    qreal inset = 0, radius = 20, strength = 1.0;
    switch (m_surface) {
    case Surface::Panel:  inset = 7; radius = 34;  strength = 1.0;  break;
    case Surface::Window: inset = 7; radius = 34;  strength = 0.8;  break;
    case Surface::Card:   inset = 0; radius = 26;  strength = 0.55; break;
    case Surface::Pill:   inset = 0; radius = 999; strength = 0.9;  break; // полная капсула
    }

    const QRectF r = QRectF(rect()).adjusted(inset, inset, -inset, -inset);
    if (r.width() < 8 || r.height() < 8)      // схлопнутая панель кода
        return;

    AnimatedBackdrop *bd = nullptr;
    for (QWidget *w = parentWidget(); w && !bd; w = w->parentWidget())
        bd = qobject_cast<AnimatedBackdrop *>(w);

    if (bd && bd->width() > 0 && bd->height() > 0) {
        const QImage &low = bd->lowRes();
        const QPoint o = mapTo(bd, QPoint(0, 0));
        const qreal sx = qreal(low.width())  / bd->width();
        const qreal sy = qreal(low.height()) / bd->height();
        const QRectF src((o.x() + r.x()) * sx, (o.y() + r.y()) * sy, r.width() * sx, r.height() * sy);
        Glass::paint(painter, r, radius, strength, low, src);
    } else {
        Glass::paint(painter, r, radius, strength, QImage(), QRectF());
    }

    if (m_glow > 0.001) {                       // фокус поля ввода: тонкое кольцо, без «неона»
        painter.setBrush(Qt::NoBrush);
        QColor halo(kAccent);
        halo.setAlphaF(float(0.14 * m_glow));
        painter.setPen(QPen(halo, 5.0));
        painter.drawPath(Glass::shape(r.adjusted(1, 1, -1, -1), radius));
        QColor ring(kAccent);
        ring.setAlphaF(float(0.60 * m_glow));
        painter.setPen(QPen(ring, 1.4));
        painter.drawPath(Glass::shape(r.adjusted(0.8, 0.8, -0.8, -0.8), radius));
    }

    if (!m_enabled || m_mode != "starfall") return;

    painter.setClipPath(Glass::shape(r, radius));
    paintStarfall(painter);
}

void NetworkBackdrop::paintStarfall(QPainter &painter)
{
    struct Meteor { qreal startX, startY, angle, trailLen, speed, offset; QString color; };
    static const QList<Meteor> meteors = {
                                           {0.05, -0.06, 32.0, 0.22, 0.50, 0.00, kAccent},
                                           {0.30, -0.10, 28.0, 0.18, 0.40, 0.35, kAccent2},
                                           {0.55, -0.05, 34.0, 0.24, 0.58, 0.65, kAccent},
                                           {0.78, -0.08, 30.0, 0.16, 0.36, 0.15, kAccentGreen},
                                           {0.92, -0.06, 36.0, 0.20, 0.46, 0.85, kAccent2},
                                           {0.18, -0.12, 26.0, 0.14, 0.32, 0.55, kAccent},
                                           };

    const qreal w = width();
    const qreal h = height();
    const qreal diag = std::sqrt(w * w + h * h);

    for (const auto &m : meteors) {
        const qreal cycle = std::fmod(m_phase * m.speed + m.offset, 1.0);
        if (cycle > 0.55) continue;

        const qreal rad = m.angle * M_PI / 180.0;
        const qreal travel = (cycle / 0.55) * diag * 1.25;
        const QPointF head(m.startX * w + std::cos(rad) * travel, m.startY * h + std::sin(rad) * travel);
        const QPointF tail(head.x() - std::cos(rad) * diag * m.trailLen, head.y() - std::sin(rad) * diag * m.trailLen);

        const qreal fadeIn = qMin(1.0, cycle / 0.08);
        const qreal fadeOut = qMin(1.0, (0.55 - cycle) / 0.15);
        const qreal alpha = qMin(fadeIn, fadeOut);
        if (alpha <= 0.02) continue;

        QColor headColor(m.color); headColor.setAlphaF(float(0.75 * alpha));
        QColor tailColor(m.color); tailColor.setAlphaF(0.0f);

        QLinearGradient trailGradient(head, tail);
        trailGradient.setColorAt(0.0, headColor);
        trailGradient.setColorAt(1.0, tailColor);

        QPen trailPen(QBrush(trailGradient), 1.4);
        trailPen.setCapStyle(Qt::RoundCap);
        painter.setPen(trailPen);
        painter.drawLine(head, tail);

        QColor dotColor(Qt::white); dotColor.setAlphaF(float(0.85 * alpha));
        painter.setPen(Qt::NoPen);
        painter.setBrush(dotColor);
        painter.drawEllipse(head, 1.5, 1.5);
    }
}

// Сетка узлов больше не используется в интерфейсе; метод оставлен, чтобы не менять заголовок.
void NetworkBackdrop::paintNodes(QPainter &painter)
{
void NetworkBackdrop::paintNodes(QPainter &painter)
{
    Q_UNUSED(painter);
}
}

// ===========================================================================
// ToggleSwitch — тумблер в стиле iOS
// ===========================================================================
ToggleSwitch::ToggleSwitch(QWidget *parent)
    : QWidget(parent)
{
    setFixedSize(46, 28);
    setCursor(Qt::PointingHandCursor);
    m_anim = new QPropertyAnimation(this, "knobPos", this);
    m_anim->setDuration(260);
    m_anim->setEasingCurve(QEasingCurve::OutCubic);
}

void ToggleSwitch::setChecked(bool checked, bool animate)
{
    if (m_checked == checked) return;
    m_checked = checked;
    const qreal target = checked ? 1.0 : 0.0;
    if (animate) {
        m_anim->stop();
        m_anim->setStartValue(m_knobPos);
        m_anim->setEndValue(target);
        m_anim->start();
    } else {
        setKnobPos(target);
    }
}

void ToggleSwitch::setKnobPos(qreal pos) { m_knobPos = pos; update(); }

void ToggleSwitch::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        setChecked(!m_checked);
        emit toggled(m_checked);
    }
    QWidget::mousePressEvent(event);
}

void ToggleSwitch::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const qreal t = qBound(0.0, m_knobPos, 1.0);
    const QRectF track = QRectF(rect()).adjusted(1, 1, -1, -1);
    const qreal r = track.height() / 2.0;

    const QColor off = Glass::tokens().dark ? QColor(255, 255, 255, 40) : QColor(0, 0, 0, 38);
    p.setPen(Qt::NoPen);
    p.setBrush(lerpColor(off, QColor(kAccentGreen), t));
    p.drawRoundedRect(track, r, r);

    const qreal d = track.height() - 4;
    const QRectF knob(track.left() + 2 + t * (track.width() - d - 4), track.top() + 2, d, d);

    p.setBrush(QColor(0, 0, 0, 45));                       // мягкая тень ручки
    p.drawEllipse(knob.translated(0, 1.5).adjusted(-0.5, -0.5, 0.5, 0.5));
    p.setBrush(Qt::white);
    p.drawEllipse(knob);
}

// ===========================================================================
// AnimatedIconButton
// ===========================================================================
AnimatedIconButton::AnimatedIconButton(const QString &glyph, const QString &baseColor, const QString &hoverColor, QWidget *parent)
    : QPushButton(parent)
    , m_glyph(glyph)
    , m_baseColor(baseColor)
    , m_hoverColor(hoverColor)
{
    setCursor(Qt::PointingHandCursor);
    m_anim = new QPropertyAnimation(this, "spin", this);
    m_anim->setDuration(380);
    m_anim->setEasingCurve(QEasingCurve::OutCubic);
}

void AnimatedIconButton::setSpin(qreal degrees) { m_spin = degrees; update(); }

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void AnimatedIconButton::enterEvent(QEnterEvent *event)
#else
void AnimatedIconButton::enterEvent(QEvent *event)
#endif
{
    m_anim->stop();
    m_anim->setStartValue(m_spin);
    m_anim->setEndValue(90.0);
    m_anim->start();
    QPushButton::enterEvent(event);
}

void AnimatedIconButton::leaveEvent(QEvent *event)
{
    m_anim->stop();
    m_anim->setStartValue(m_spin);
    m_anim->setEndValue(0.0);
    m_anim->start();
    QPushButton::leaveEvent(event);
}

void AnimatedIconButton::paintEvent(QPaintEvent *event)
{
    QPushButton::paintEvent(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.translate(rect().center());
    painter.rotate(m_spin);

    painter.setPen(QColor(underMouse() ? m_hoverColor : m_baseColor));
    painter.drawText(QRect(-width() / 2, -height() / 2, width(), height()), Qt::AlignCenter, m_glyph);
}

// ===========================================================================
// AgentAvatar — плоский градиентный кружок с инициалами
// ===========================================================================
AgentAvatar::AgentAvatar(QWidget *parent)
    : QWidget(parent)
{
    setFixedSize(40, 40);
    setAgentName("?");
}

void AgentAvatar::setAgentName(const QString &name)
{
    const QString trimmed = name.trimmed();
    const QStringList parts = trimmed.split(' ', Qt::SkipEmptyParts);

    QString initials;
    if (!parts.isEmpty())
        initials += parts.first().at(0).toUpper();
    if (parts.size() > 1)
        initials += parts.at(1).at(0).toUpper();
    else if (!parts.isEmpty() && parts.first().size() > 1)
        initials += parts.first().at(1).toUpper();

    m_initials = initials.isEmpty() ? "?" : initials;
    m_paletteIndex = trimmed.isEmpty() ? 0 : int(qHash(trimmed) % uint(kAvatarGradients.size()));
    update();
}

void AgentAvatar::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const auto &grad = kAvatarGradients.at(m_paletteIndex);
    const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);

    QLinearGradient gradient(r.topLeft(), r.bottomRight());
    gradient.setColorAt(0, grad.start);
    gradient.setColorAt(1, grad.end);
    painter.setPen(Qt::NoPen);
    painter.setBrush(gradient);
    painter.drawEllipse(r);

    QFont font = painter.font();
    font.setWeight(QFont::DemiBold);
    font.setPointSize(qMax(9, height() / 3));
    painter.setFont(font);
    painter.setPen(Qt::white);
    painter.drawText(rect(), Qt::AlignCenter, m_initials);
}

// ===========================================================================
// ClickableLabel / ClickableFrame
// ===========================================================================
ClickableLabel::ClickableLabel(QWidget *parent) : QLabel(parent) { setCursor(Qt::PointingHandCursor); }
ClickableLabel::ClickableLabel(const QString &text, QWidget *parent) : QLabel(text, parent) { setCursor(Qt::PointingHandCursor); }

void ClickableLabel::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) emit clicked();
    QLabel::mousePressEvent(event);
}

namespace {
QString glassCardStyle(const char *selector, bool selected, bool hovered)
{
    const auto &g = Glass::tokens();
    QString bg = "transparent";
    QString border = "1px solid transparent";
    if (selected) {
        bg = Glass::css(g.selected);
        border = "1px solid " + Glass::css(g.stroke);
    } else if (hovered) {
        bg = Glass::css(g.hover);
    }
    return QString("%1 { background-color: %2; border: %3; border-radius: 18px; }")
        .arg(QString::fromLatin1(selector), bg, border);
}
} // namespace

ClickableFrame::ClickableFrame(QWidget *parent)
    : QFrame(parent)
{
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_Hover, true);
    applyStyle();
}

void ClickableFrame::setSelected(bool selected) { m_selected = selected; applyStyle(); }

void ClickableFrame::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) emit clicked();
    QFrame::mousePressEvent(event);
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void ClickableFrame::enterEvent(QEnterEvent *event)
#else
void ClickableFrame::enterEvent(QEvent *event)
#endif
{
    m_hovered = true;
    applyStyle();
    QFrame::enterEvent(event);
}

void ClickableFrame::leaveEvent(QEvent *event)
{
    m_hovered = false;
    applyStyle();
    QFrame::leaveEvent(event);
}

void ClickableFrame::applyStyle()
{
    setStyleSheet(glassCardStyle("ClickableFrame", m_selected, m_hovered));
}

// ===========================================================================
// SubagentCard
// ===========================================================================
SubagentCard::SubagentCard(const QString &id, const QString &name, QWidget *parent)
    : QFrame(parent)
    , m_id(id)
{
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_Hover, true);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(10, 8, 8, 8);
    outer->setSpacing(4);
    m_outer = outer;

    auto *topRow = new QHBoxLayout();
    topRow->setSpacing(8);

    m_avatar = new AgentAvatar(this);
    m_avatar->setFixedSize(32, 32);
    m_avatar->setAgentName(name);

    m_statusDot = new QLabel("●", this);
    m_statusDot->setFixedWidth(10);
    m_statusDot->setStyleSheet(QString("font-size: 11px; border: none; background: transparent; color: %1;").arg(kStatusIdle));

    m_nameEdit = new QLineEdit(name, this);
    m_nameEdit->setPlaceholderText("Имя агента");
    m_nameEdit->setToolTip("Имя агента");
    m_nameEdit->setFrame(false);
    m_nameEdit->setStyleSheet(QString(
                                  "QLineEdit {"
                                  "   background: transparent;"
                                  "   border: none;"
                                  "   color: %1;"
                                  "   font-size: 14px;"
                                  "   font-weight: 600;"
                                  "   padding: 0px;"
                                  "}"
                                  ).arg(kTextMain));
    connect(m_nameEdit, &QLineEdit::editingFinished, this, [this]() {
        m_avatar->setAgentName(m_nameEdit->text());
        emit nameEdited(m_id, m_nameEdit->text());
    });

    m_toggleSwitch = new ToggleSwitch(this);
    m_toggleSwitch->setToolTip("Включить/выключить агента");
    m_toggleSwitch->setChecked(true, false);
    connect(m_toggleSwitch, &ToggleSwitch::toggled, this, [this](bool enabled) {
        m_enabled = enabled;
        applyStatusStyle();
        emit enabledToggled(m_id, enabled);
    });

    m_settingsButton = new AnimatedIconButton("⚙", kTextMuted, kAccent, this);
    m_settingsButton->setFixedSize(28, 28);
    m_settingsButton->setToolTip("Настройки агента");
    m_settingsButton->setStyleSheet(iconButtonStyle(kAccent, 16, 14));
    connect(m_settingsButton, &QPushButton::clicked, this, [this]() {
        emit settingsRequested(m_id);
    });

    topRow->addWidget(m_avatar);
    topRow->addWidget(m_statusDot);
    topRow->addWidget(m_nameEdit, 1);
    topRow->addWidget(m_toggleSwitch);
    topRow->addWidget(m_settingsButton);
    outer->addLayout(topRow);

    m_roleLabel = new ClickableLabel(this);
    m_roleLabel->setStyleSheet(QString(
                                   "font-size: 11px; border: none; background: transparent; color: %1; padding-left: 22px;"
                                   ).arg(kTextSubtle));
    m_roleLabel->setToolTip("Нажмите, чтобы открыть историю переписки");
    connect(m_roleLabel, &ClickableLabel::clicked, this, [this]() { emit clicked(m_id); });
    outer->addWidget(m_roleLabel);

    m_statusLabel = new ClickableLabel("Ожидание", this);
    m_statusLabel->setStyleSheet(QString(
                                     "font-size: 11px; border: none; background: transparent; color: %1; padding-left: 22px;"
                                     ).arg(kTextMuted));
    m_statusLabel->setToolTip("Нажмите, чтобы открыть историю переписки");
    connect(m_statusLabel, &ClickableLabel::clicked, this, [this]() { emit clicked(m_id); });
    outer->addWidget(m_statusLabel);

    setRole(QString());
    applyCardStyle();
    applyStatusStyle();
}

QString SubagentCard::name() const { return m_nameEdit->text(); }

void SubagentCard::setName(const QString &name)
{
    if (m_nameEdit->text() != name) m_nameEdit->setText(name);
    m_avatar->setAgentName(name);
}

void SubagentCard::setRole(const QString &role) { m_role = role; refreshRolePreview(); }
void SubagentCard::setStatus(Status status) { m_status = status; applyStatusStyle(); }

void SubagentCard::setAgentEnabled(bool enabled)
{
    m_enabled = enabled;
    m_toggleSwitch->setChecked(enabled);
    applyStatusStyle();
}

void SubagentCard::setCollapsed(bool collapsed)
{
    m_collapsed = collapsed;
    m_statusDot->setVisible(!collapsed);
    m_roleLabel->setVisible(!collapsed);
    m_statusLabel->setVisible(!collapsed);
    m_nameEdit->setVisible(!collapsed);
    m_toggleSwitch->setVisible(!collapsed);
    m_settingsButton->setVisible(!collapsed);
    m_outer->setContentsMargins(collapsed ? 6 : 10, 8, collapsed ? 6 : 8, 8);
}

void SubagentCard::setSelected(bool selected) { m_selected = selected; applyCardStyle(); }

void SubagentCard::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) emit clicked(m_id);
    QFrame::mousePressEvent(event);
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void SubagentCard::enterEvent(QEnterEvent *event)
#else
void SubagentCard::enterEvent(QEvent *event)
#endif
{
    m_hovered = true;
    applyCardStyle();
    QFrame::enterEvent(event);
}

void SubagentCard::leaveEvent(QEvent *event)
{
    m_hovered = false;
    applyCardStyle();
    QFrame::leaveEvent(event);
}

void SubagentCard::applyCardStyle()
{
    setStyleSheet(glassCardStyle("SubagentCard", m_selected, m_hovered));
}

void SubagentCard::applyStatusStyle()
{
    QString color;
    QString text;
    switch (m_status) {
    case Status::Idle:   color = kStatusIdle;   text = "Ожидание";  break;
    case Status::Active: color = kStatusActive; text = "Активен";   break;
    case Status::Busy:   color = kStatusBusy;   text = "Выполняет"; break;
    case Status::Error:  color = kStatusError;  text = "Ошибка";    break;
    }
    if (!m_enabled) color = kTextMuted;
    m_statusDot->setStyleSheet(QString("font-size: 11px; border: none; background: transparent; color: %1;").arg(color));
    m_statusLabel->setText(text);
    m_statusLabel->setStyleSheet(QString(
                                     "font-size: 11px; border: none; background: transparent; color: %1; padding-left: 22px;"
                                     ).arg(color));
}

void SubagentCard::refreshRolePreview()
{
    QString preview = m_role.trimmed().isEmpty() ? QString("Роль не задана") : m_role;
    preview.replace('\n', ' ');
    const QFontMetrics metrics(m_roleLabel->font());
    m_roleLabel->setText(metrics.elidedText(preview, Qt::ElideRight, 208));
}

// ===========================================================================
// Виджеты чата: код-блок, пузырь с «печатью», индикатор набора, кнопка отправки
// ===========================================================================
namespace {

class ClickRow : public QFrame
{
public:
    std::function<void()> onClick;
    explicit ClickRow(QWidget *parent = nullptr) : QFrame(parent) { setCursor(Qt::PointingHandCursor); }

protected:
    void mousePressEvent(QMouseEvent *e) override
    {
        if (e->button() == Qt::LeftButton && onClick)
            onClick();
        QFrame::mousePressEvent(e);
    }
};

// Сворачиваемый блок кода внутри пузыря
class CodeBlockWidget : public QFrame
{
public:
    explicit CodeBlockWidget(const QString &code, QWidget *parent = nullptr)
        : QFrame(parent)
    {
        const auto &g = Glass::tokens();

        auto *outer = new QVBoxLayout(this);
        outer->setContentsMargins(0, 8, 0, 0);
        outer->setSpacing(0);

        m_header = new ClickRow(this);
        m_header->setObjectName("codeHeader");
        m_header->setStyleSheet(QString(
                                    "QFrame#codeHeader {"
                                    "   background-color: %1;"
                                    "   border: 1px solid %2;"
                                    "   border-top-left-radius: 12px;"
                                    "   border-top-right-radius: 12px;"
                                    "}"
                                    ).arg(Glass::css(g.tint), Glass::css(g.stroke)));

        auto *headerLayout = new QHBoxLayout(m_header);
        headerLayout->setContentsMargins(12, 6, 8, 6);
        headerLayout->setSpacing(6);

        m_chevron = new QLabel("▸", m_header);
        m_chevron->setStyleSheet(QString(
                                     "color: %1; border: none; background: transparent; font-size: 11px;"
                                     ).arg(kAccent));

        auto *label = new QLabel("Код", m_header);
        label->setStyleSheet(QString(
                                 "color: %1; border: none; background: transparent; font-size: 11px; font-weight: 600;"
                                 ).arg(kTextSubtle));

        auto *copyBtn = new AnimatedIconButton("⧉", kTextMuted, kAccent, m_header);
        copyBtn->setFixedSize(24, 24);
        copyBtn->setToolTip("Скопировать код");
        copyBtn->setStyleSheet(iconButtonStyle(kAccent));
        connect(copyBtn, &QPushButton::clicked, this, [code]() {
            QGuiApplication::clipboard()->setText(code);
        });

        headerLayout->addWidget(m_chevron);
        headerLayout->addWidget(label, 1);
        headerLayout->addWidget(copyBtn);
        m_header->onClick = [this]() { toggle(); };

        m_container = new QWidget(this);
        m_container->setMinimumHeight(0);
        m_container->setMaximumHeight(0);
        m_container->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

        auto *containerLayout = new QVBoxLayout(m_container);
        containerLayout->setContentsMargins(0, 0, 0, 0);
        containerLayout->setSpacing(0);

        m_body = new QPlainTextEdit(m_container);
        m_body->setPlainText(code);
        m_body->setReadOnly(true);
        m_body->setLineWrapMode(QPlainTextEdit::NoWrap);
        m_body->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
        m_body->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        m_body->setStyleSheet(QString(
                                  "QPlainTextEdit {"
                                  "   background-color: rgba(0,0,0,%4);"
                                  "   color: %1;"
                                  "   border: 1px solid %2;"
                                  "   border-top: none;"
                                  "   border-bottom-left-radius: 12px;"
                                  "   border-bottom-right-radius: 12px;"
                                  "   padding: 8px;"
                                  "   font-size: 12px;"
                                  "}"
                                  "QScrollBar:vertical { background: transparent; width: 8px; margin: 0px; }"
                                  "QScrollBar::handle:vertical { background: %2; border-radius: 4px; min-height: 24px; }"
                                  "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }"
                                  ).arg(kTextMain, Glass::css(g.stroke)).arg(g.dark ? 90 : 18));

        containerLayout->addWidget(m_body);

        outer->addWidget(m_header);
        outer->addWidget(m_container);

        const int lineH = QFontMetrics(m_body->font()).lineSpacing();
        const int lines = qMin(int(code.count('\n')) + 1, 14);
        m_expandedHeight = lines * lineH + 22;
        m_body->setFixedHeight(m_expandedHeight);

        m_anim = new QPropertyAnimation(m_container, "maximumHeight", this);
        m_anim->setDuration(300);
        m_anim->setEasingCurve(QEasingCurve::OutCubic);
    }

    void toggle()
    {
        m_expanded = !m_expanded;
        m_chevron->setText(m_expanded ? "▾" : "▸");

        m_anim->stop();
        m_anim->setStartValue(m_container->maximumHeight());
        m_anim->setEndValue(m_expanded ? m_expandedHeight : 0);
        m_anim->start();
    }

private:
    ClickRow *m_header;
    QLabel *m_chevron;
    QWidget *m_container;
    QPlainTextEdit *m_body;
    QPropertyAnimation *m_anim;
    int m_expandedHeight = 0;
    bool m_expanded = false;
};

// Пузырь сообщения: всплытие + «живая» печать текста
class ChatBubble : public QFrame
{
public:
    ChatBubble(const QString &author, const QString &text, const QString &time,
               bool isUser, const QString &code, bool typewriter,
               std::function<void()> onGrow, QWidget *parent = nullptr)
        : QFrame(parent)
        , m_text(text)
        , m_code(code)
        , m_onGrow(std::move(onGrow))
    {
        const auto &g = Glass::tokens();

        m_outer = new QHBoxLayout(this);
        m_outer->setContentsMargins(0, 0, 0, 0);
        m_outer->setSpacing(10);

        auto *avatar = new AgentAvatar(this);
        avatar->setFixedSize(30, 30);
        avatar->setAgentName(author);

        auto *bubble = new QFrame(this);
        bubble->setObjectName("bubble");
        bubble->setMaximumWidth(460);

        // как в iMessage: свои сообщения — однотонный акцент, чужие — стекло; нижний угол у «хвостика» подрезан
        const QString bg = isUser ? kAccent : Glass::css(g.tintStrong);
        const QString border = isUser ? QString("transparent") : Glass::css(g.stroke);
        bubble->setStyleSheet(QString(
                                  "QFrame#bubble {"
                                  "   background: %1;"
                                  "   border: 1px solid %2;"
                                  "   border-radius: 20px;"
                                  "   %3"
                                  "}"
                                  ).arg(bg, border,
                                       isUser ? "border-bottom-right-radius: 6px;"
                                              : "border-bottom-left-radius: 6px;"));

        m_bubbleLayout = new QVBoxLayout(bubble);
        m_bubbleLayout->setContentsMargins(15, 10, 15, 11);
        m_bubbleLayout->setSpacing(4);

        auto *headerRow = new QHBoxLayout();
        headerRow->setSpacing(10);
        auto *authorLabel = new QLabel(author, bubble);
        authorLabel->setStyleSheet(QString("font-size: 11px; font-weight: 700; color: %1; border:none; background:transparent;")
                                       .arg(isUser ? "rgba(255,255,255,0.9)" : kAccent));
        auto *timeLabel = new QLabel(time, bubble);
        timeLabel->setStyleSheet(QString("font-size: 10px; color: %1; border:none; background:transparent;")
                                     .arg(isUser ? "rgba(255,255,255,0.65)" : kTextMuted));
        headerRow->addWidget(authorLabel);
        headerRow->addStretch();
        headerRow->addWidget(timeLabel);
        m_bubbleLayout->addLayout(headerRow);

        if (!text.trimmed().isEmpty()) {
            m_label = new QLabel(bubble);
            m_label->setTextFormat(Qt::RichText);
            m_label->setWordWrap(true);
            m_label->setTextInteractionFlags(Qt::TextSelectableByMouse);
            QFont f = m_label->font();
            f.setPixelSize(14);
            m_label->setFont(f);
            m_label->setStyleSheet(QString("color: %1; border:none; background:transparent;")
                                       .arg(isUser ? "#ffffff" : kTextMain));

            // фиксируем итоговую ширину заранее, чтобы пузырь не «дёргался» во время печати
            const QFontMetrics fm(f);
            int w = 0;
            for (const QString &line : text.split('\n'))
                w = qMax(w, fm.horizontalAdvance(line));
            m_label->setMinimumWidth(qMin(w + 12, 420));

            m_bubbleLayout->addWidget(m_label);
        }

        if (isUser) {
            m_outer->addStretch(1);
            m_outer->addWidget(bubble);
            m_outer->addWidget(avatar, 0, Qt::AlignTop);
        } else {
            m_outer->addWidget(avatar, 0, Qt::AlignTop);
            m_outer->addWidget(bubble);
            m_outer->addStretch(1);
        }

        if (m_label && typewriter) {
            m_label->setText(renderRich(QString(), true));
            startTyping();
        } else {
            if (m_label)
                m_label->setText(renderRich(m_text, false));
            addCode();
        }
    }

    void playEntrance()
    {
        auto *effect = new QGraphicsOpacityEffect(this);
        effect->setOpacity(0.0);
        setGraphicsEffect(effect);

        auto *anim = new QVariantAnimation(this);
        anim->setDuration(420);
        anim->setStartValue(0.0);
        anim->setEndValue(1.0);
        anim->setEasingCurve(QEasingCurve::OutCubic);

        connect(anim, &QVariantAnimation::valueChanged, this, [this, effect](const QVariant &v) {
            const qreal t = v.toReal();
            effect->setOpacity(t);
            m_outer->setContentsMargins(0, int((1.0 - t) * 24), 0, 0);
        });

        QPointer<ChatBubble> self(this);
        connect(anim, &QVariantAnimation::finished, this, [self]() {
            if (!self) return;
            self->setGraphicsEffect(nullptr);
            self->m_outer->setContentsMargins(0, 0, 0, 0);
        });

        anim->start(QAbstractAnimation::DeleteWhenStopped);
    }

private:
    void startTyping()
    {
        m_pos = 0;
        m_timer = new QTimer(this);
        m_timer->setInterval(18);
        connect(m_timer, &QTimer::timeout, this, [this]() { advance(); });
        m_timer->start();
    }

    void advance()
    {
        const int n = m_text.size();
        const int step = qMax(1, n / 150) + int(QRandomGenerator::global()->bounded(2));
        m_pos = qMin(n, m_pos + step);
        if (m_pos < n && m_pos > 0 && m_text.at(m_pos - 1).isHighSurrogate())
            ++m_pos;

        m_label->setText(renderRich(m_text.left(m_pos), m_pos < n));
        if (m_onGrow) m_onGrow();

        if (m_pos >= n) {
            m_timer->stop();
            addCode();
            if (m_onGrow) m_onGrow();
            return;
        }

        // небольшие «вдохи» на знаках препинания
        const QChar last = m_text.at(m_pos - 1);
        m_timer->setInterval((last == '.' || last == '!' || last == '?' || last == '\n') ? 150
                             : (last == ',' || last == ':' || last == ';') ? 70 : 18);
    }

    void addCode()
    {
        if (m_codeAdded || m_code.trimmed().isEmpty()) return;
        m_codeAdded = true;
        m_bubbleLayout->addWidget(new CodeBlockWidget(m_code, this->findChild<QFrame *>("bubble")));
    }

    QString m_text;
    QString m_code;
    std::function<void()> m_onGrow;
    QHBoxLayout *m_outer = nullptr;
    QVBoxLayout *m_bubbleLayout = nullptr;
    QLabel *m_label = nullptr;
    QTimer *m_timer = nullptr;
    int m_pos = 0;
    bool m_codeAdded = false;
};

// Три прыгающие точки
class TypingDots : public QWidget
{
public:
    explicit TypingDots(QWidget *parent = nullptr) : QWidget(parent)
    {
        setFixedSize(68, 38);
        m_anim = new QVariantAnimation(this);
        m_anim->setDuration(1200);
        m_anim->setStartValue(0.0);
        m_anim->setEndValue(1.0);
        m_anim->setLoopCount(-1);
        connect(m_anim, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
            m_t = v.toReal();
            update();
        });
    }

protected:
    void showEvent(QShowEvent *e) override { QWidget::showEvent(e); m_anim->start(); }
    void hideEvent(QHideEvent *e) override { QWidget::hideEvent(e); m_anim->stop(); }

    void paintEvent(QPaintEvent *) override
    {
        const auto &g = Glass::tokens();
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        p.setPen(QPen(g.stroke, 1.0));
        p.setBrush(g.tintStrong);
        p.drawRoundedRect(r, r.height() / 2.0, r.height() / 2.0);

        for (int i = 0; i < 3; ++i) {
            const qreal phase = std::fmod(m_t - i * 0.16 + 1.0, 1.0);
            const qreal bounce = qMax(0.0, std::sin(phase * 2.0 * M_PI));
            QColor c(kTextMain);
            c.setAlpha(int(110 + 145 * bounce));
            p.setPen(Qt::NoPen);
            p.setBrush(c);
            const qreal cx = width() / 2.0 + (i - 1) * 14.0;
            const qreal cy = height() / 2.0 - bounce * 5.0;
            p.drawEllipse(QPointF(cx, cy), 3.6, 3.6);
        }
    }

private:
    QVariantAnimation *m_anim = nullptr;
    qreal m_t = 0.0;
};

class TypingRow : public QWidget
{
public:
    explicit TypingRow(QWidget *parent = nullptr) : QWidget(parent)
    {
        auto *lay = new QHBoxLayout(this);
        lay->setContentsMargins(0, 0, 0, 0);
        lay->setSpacing(10);
        m_avatar = new AgentAvatar(this);
        m_avatar->setFixedSize(30, 30);
        lay->addWidget(m_avatar, 0, Qt::AlignTop);
        lay->addWidget(new TypingDots(this));
        lay->addStretch(1);
        setVisible(false);
    }

    void setAgentName(const QString &name) { m_avatar->setAgentName(name); }

    void setActive(bool on)
    {
        if (on == isVisible()) return;
        if (!on) { setVisible(false); setGraphicsEffect(nullptr); return; }

        auto *effect = new QGraphicsOpacityEffect(this);
        effect->setOpacity(0.0);
        setGraphicsEffect(effect);
        setVisible(true);

        auto *a = new QPropertyAnimation(effect, "opacity", this);
        a->setDuration(260);
        a->setStartValue(0.0);
        a->setEndValue(1.0);
        a->setEasingCurve(QEasingCurve::OutCubic);
        QPointer<TypingRow> self(this);
        connect(a, &QPropertyAnimation::finished, this, [self]() {
            if (self) self->setGraphicsEffect(nullptr);
        });
        a->start(QAbstractAnimation::DeleteWhenStopped);
    }

private:
    AgentAvatar *m_avatar = nullptr;
};

// Круглая плоская кнопка отправки
class SendButton : public QPushButton
{
public:
    explicit SendButton(QWidget *parent = nullptr) : QPushButton(parent)
    {
        setFixedSize(44, 44);
        setCursor(Qt::PointingHandCursor);
        setToolTip("Отправить");
        m_anim = new QVariantAnimation(this);
        m_anim->setDuration(220);
        m_anim->setEasingCurve(QEasingCurve::OutCubic);
        connect(m_anim, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
            m_hover = v.toReal();
            update();
        });
    }

    void setReady(bool r)
    {
        if (m_ready == r) return;
        m_ready = r;
        update();
    }

protected:
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    void enterEvent(QEnterEvent *e) override
#else
    void enterEvent(QEvent *e) override
#endif
    {
        animateHover(1.0);
        QPushButton::enterEvent(e);
    }
    void leaveEvent(QEvent *e) override { animateHover(0.0); QPushButton::leaveEvent(e); }
    void mousePressEvent(QMouseEvent *e) override { m_pressed = true; update(); QPushButton::mousePressEvent(e); }
    void mouseReleaseEvent(QMouseEvent *e) override { m_pressed = false; update(); QPushButton::mouseReleaseEvent(e); }

    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.translate(rect().center());
        const qreal s = 1.0 + 0.04 * m_hover - (m_pressed ? 0.08 : 0.0);
        p.scale(s, s);

        const qreal R = 18.0;
        p.setPen(Qt::NoPen);
        if (m_ready) {
            QLinearGradient g(0, -R, 0, R);
            g.setColorAt(0, QColor(kAccent).lighter(110));
            g.setColorAt(1, QColor(kAccent));
            p.setBrush(g);
        } else {
            p.setBrush(Glass::tokens().tintStrong);
        }
        p.drawEllipse(QPointF(0, 0), R, R);

        QPen arrow(m_ready ? QColor(Qt::white) : QColor(kTextSubtle), 2.4);
        arrow.setCapStyle(Qt::RoundCap);
        arrow.setJoinStyle(Qt::RoundJoin);
        p.setPen(arrow);
        p.setBrush(Qt::NoBrush);
        p.drawLine(QPointF(0, 7), QPointF(0, -7));
        p.drawLine(QPointF(-6, -1), QPointF(0, -7));
        p.drawLine(QPointF(6, -1), QPointF(0, -7));
    }

private:
    void animateHover(qreal target)
    {
        m_anim->stop();
        m_anim->setStartValue(m_hover);
        m_anim->setEndValue(target);
        m_anim->start();
    }

    QVariantAnimation *m_anim = nullptr;
    qreal m_hover = 0.0;
    bool m_pressed = false;
    bool m_ready = false;
};

} // anonymous namespace

// ===========================================================================
// MainWindow
// ===========================================================================

MainWindow::MainWindow(agents *agentManager, QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_agentManager(agentManager)
{
    QSettings themeSettings(QString(APP_SRC_DIR) + "/config.ini", QSettings::IniFormat);
    loadPalette(themeSettings.value("theme", "dark").toString());

    ui->setupUi(this);

    connect(m_agentManager, &agents::requestSendMessagesToUIDelayed,
            this, &MainWindow::handleAgentReplyForUI);
    connect(m_agentManager, &agents::requestThinkingContext,
            this, &MainWindow::handleThinkingContext);

    setWindowTitle("Agent Console");
    resize(1120, 720);
    setStyleSheet(QString("QMainWindow { background-color: %1; }").arg(kBgWindow));

    m_mainAgentName = "Главный агент";

    buildUi();

    startTime.start();
    uptimeTimer = new QTimer(this);
    connect(uptimeTimer, &QTimer::timeout, this, &MainWindow::updateUptime);
    uptimeTimer->start(1000);

    mainStatusCard->setSelected(true);
    refreshComposerPlaceholder();

    loadAgentsFromDisk();

    appendMainMessage("Главный агент подключён. Ожидаю команд.", false);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    if (m_introPlayed) return;
    m_introPlayed = true;

    setWindowOpacity(0.0);
    auto *a = new QPropertyAnimation(this, "windowOpacity", this);
    a->setDuration(520);
    a->setStartValue(0.0);
    a->setEndValue(1.0);
    a->setEasingCurve(QEasingCurve::OutCubic);
    a->start(QAbstractAnimation::DeleteWhenStopped);
}

qint64 MainWindow::uiChatIdFor(const QString &agentId)
{
    auto it = m_uiChatIds.constFind(agentId);
    if (it != m_uiChatIds.constEnd())
        return it.value();

    static qint64 nextId = -1;
    const qint64 id = nextId--;
    m_uiChatIds.insert(agentId, id);
    return id;
}

void MainWindow::handleAgentReplyForUI(qint64 chatId, const QList<DelayedMessage> &messages)
{
    QString agentId;
    bool fromUi = false;
    for (auto it = m_uiChatIds.constBegin(); it != m_uiChatIds.constEnd(); ++it) {
        if (it.value() == chatId) {
            agentId = it.key();
            fromUi = true;
            break;
        }
    }
    if (fromUi)
        m_pending.remove(agentId);

    for (const DelayedMessage &msg : messages) {
        if (!msg.text.isEmpty() || !msg.code.isEmpty())
            receiveAgentReply(agentId, msg.text, msg.code);

        if (!msg.code.isEmpty()) {
            const QString author = agentId.isEmpty() ? m_mainAgentName : subagentName(agentId);
            showCodePanel(msg.code, "Код — " + author);
        }
    }

    updateTypingIndicator();
    refreshAgentsFromDisk();
}

void MainWindow::loadAgentsFromDisk()
{
    const QStringList existing = m_agentManager->listAgents();

    if (existing.contains(m_mainAgentName)) {
        m_mainAgentRole = extractRoleFromMinInfo(m_agentManager->getFile(m_mainAgentName, "min_info"));
    } else {
        m_agentManager->createAgent(m_mainAgentName, QString());
        m_agentManager->editFile(m_mainAgentName, buildMinInfo(m_mainAgentName, QString()), "min_info");
    }
    m_agentFolderNames[QString()] = m_mainAgentName;
    setMainAgentName(m_mainAgentName);
    setMainAgentRole(m_mainAgentRole);

    for (const QString &agentName : existing) {
        if (agentName == m_mainAgentName)
            continue;
        const QString role = extractRoleFromMinInfo(m_agentManager->getFile(agentName, "min_info"));
        const QString id = addSubagent(agentName, role);
        m_agentFolderNames[id] = agentName;
        m_agentEnabled.insert(id, true);
    }
}

void MainWindow::refreshAgentsFromDisk()
{
    const QStringList existing = m_agentManager->listAgents();
    const QSet<QString> onDisk(existing.begin(), existing.end());
    const QString mainBackendName = m_agentFolderNames.value(QString(), m_mainAgentName);

    for (const QString &agentName : existing) {
        if (agentName == mainBackendName)
            continue;
        if (m_agentFolderNames.values().contains(agentName))
            continue;

        const QString role = extractRoleFromMinInfo(m_agentManager->getFile(agentName, "min_info"));
        const QString id = addSubagent(agentName, role);
        m_agentFolderNames[id] = agentName;
        m_agentEnabled.insert(id, true);
    }

    const QStringList ids = m_cards.keys();
    for (const QString &id : ids) {
        const QString backendName = m_agentFolderNames.value(id);
        if (!backendName.isEmpty() && !onDisk.contains(backendName))
            removeSubagent(id);
    }

    for (auto it = m_agentFolderNames.constBegin(); it != m_agentFolderNames.constEnd(); ++it) {
        const QString &id = it.key();
        const QString &backendName = it.value();
        if (id.isEmpty() || backendName.isEmpty() || !m_cards.contains(id))
            continue;
        setSubagentRole(id, extractRoleFromMinInfo(m_agentManager->getFile(backendName, "min_info")));
    }
}

void MainWindow::syncAgentName(const QString &backendName, const QString &newName)
{
    if (backendName.isEmpty())
        return;
    const QString role = extractRoleFromMinInfo(m_agentManager->getFile(backendName, "min_info"));
    m_agentManager->editFile(backendName, buildMinInfo(newName, role), "min_info");
}

// ===========================================================================
// Страница настроек
// ===========================================================================
QWidget *MainWindow::buildSettingsPage()
{
    auto *page = new NetworkBackdrop(NetworkBackdrop::Surface::Window, this);

    auto *scroll = new QScrollArea(page);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet(QString(
                              "QScrollArea { background: transparent; border: none; }"
                              "QScrollBar:vertical { background: transparent; width: 8px; margin: 0px; }"
                              "QScrollBar::handle:vertical { background: %1; border-radius: 4px; min-height: 24px; }"
                              "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }"
                              ).arg(Glass::css(Glass::tokens().stroke)));
    scroll->viewport()->setStyleSheet("background: transparent;");

    auto *content = new QWidget(scroll);
    content->setStyleSheet("background: transparent;");
    auto *outer = new QVBoxLayout(content);
    outer->setContentsMargins(42, 36, 42, 36);
    outer->setSpacing(12);

    auto *backRow = new QHBoxLayout();
    auto *backButton = new QPushButton("← Назад к чату", content);
    backButton->setCursor(Qt::PointingHandCursor);
    backButton->setStyleSheet(QString(
                                  "QPushButton { background: transparent; color: %1; border: none; font-size: 13px; font-weight: 600; padding: 4px 0px; }"
                                  "QPushButton:hover { color: %2; }"
                                  ).arg(kTextSubtle, kAccent));
    connect(backButton, &QPushButton::clicked, this, &MainWindow::showMainView);
    backRow->addWidget(backButton);
    backRow->addStretch(1);
    outer->addLayout(backRow);

    auto *pageTitle = new QLabel("Настройки", content);
    pageTitle->setStyleSheet(QString("color: %1; font-size: 26px; font-weight: 700; border: none; background: transparent;").arg(kTextMain));
    outer->addWidget(pageTitle);

    auto pillButtonStyle = [](bool active) {
        const auto &g = Glass::tokens();
        return QString(
                   "QPushButton {"
                   "   background-color: %1;"
                   "   color: %2;"
                   "   border: 1px solid %3;"
                   "   border-radius: 18px;"
                   "   padding: 10px 18px;"
                   "   font-size: 13px;"
                   "   font-weight: 600;"
                   "}"
                   "QPushButton:hover { border-color: %4; }")
            .arg(active ? kAccent : Glass::css(g.tint),
                 active ? QString("#ffffff") : kTextMain,
                 active ? kAccent : Glass::css(g.stroke),
                 kAccent);
    };

    auto addEyebrow = [&](const QString &text) {
        auto *label = new QLabel(text, content);
        label->setStyleSheet(QString("color: %1; font-size: 10px; font-weight: 700; border: none; background: transparent; margin-top: 14px; letter-spacing: 1px;").arg(kTextMuted));
        outer->addWidget(label);
    };

    QSettings settings(QString(APP_SRC_DIR) + "/config.ini", QSettings::IniFormat);

    addEyebrow("ТЕМА ОФОРМЛЕНИЯ");
    const QString currentTheme = settings.value("theme", "dark").toString();
    const bool currentIsDark = QColor(builtinBaseColors(currentTheme).bg).lightness() < 128;

    auto smallLabelStyle = [&]() {
        return QString("color: %1; font-size: 13px; border: none; background: transparent;").arg(kTextMuted);
    };

    for (const auto &family : themeFamilies()) {
        auto *familyRow = new QHBoxLayout();
        familyRow->setSpacing(8);

        const bool familyActive = (currentTheme == family.lightId || currentTheme == family.darkId);
        const bool isLight = familyActive && currentTheme == family.lightId;

        auto *familyBtn = new QPushButton(family.label, content);
        familyBtn->setFixedWidth(110);
        familyBtn->setCursor(Qt::PointingHandCursor);
        familyBtn->setStyleSheet(pillButtonStyle(familyActive));

        auto *sunLabel = new QLabel("☀", content);
        sunLabel->setStyleSheet(smallLabelStyle());

        // включён = тёмная версия темы
        auto *modeToggle = new ToggleSwitch(content);
        modeToggle->setToolTip("Светлая / тёмная версия темы");
        modeToggle->setChecked(familyActive ? !isLight : currentIsDark, false);

        auto *moonLabel = new QLabel("🌙", content);
        moonLabel->setStyleSheet(smallLabelStyle());

        connect(familyBtn, &QPushButton::clicked, this, [this, family, modeToggle]() {
            switchTheme(modeToggle->isChecked() ? family.darkId : family.lightId);
        });
        connect(modeToggle, &ToggleSwitch::toggled, this, [this, family](bool dark) {
            switchTheme(dark ? family.darkId : family.lightId);
        });

        const QString editTargetId = familyActive ? currentTheme : family.darkId;
        auto *editBtn = new AnimatedIconButton("✎", kTextMuted, kAccent, content);
        editBtn->setFixedSize(28, 28);
        editBtn->setToolTip("Редактировать цвета темы «" + themeDisplayName(editTargetId) + "»");
        editBtn->setStyleSheet(iconButtonStyle(kAccent, 12, 14));
        connect(editBtn, &QPushButton::clicked, this, [this, editTargetId]() { openThemeCreator(editTargetId); });

        familyRow->addWidget(familyBtn);
        familyRow->addWidget(sunLabel);
        familyRow->addWidget(modeToggle);
        familyRow->addWidget(moonLabel);
        familyRow->addWidget(editBtn);
        familyRow->addStretch(1);
        outer->addLayout(familyRow);
    }

    if (settings.childGroups().contains("custom_theme")) {
        auto *customRow = new QHBoxLayout();
        customRow->setSpacing(8);

        auto *customBtn = new QPushButton(themeDisplayName("custom"), content);
        customBtn->setCursor(Qt::PointingHandCursor);
        customBtn->setStyleSheet(pillButtonStyle(currentTheme == "custom"));
        connect(customBtn, &QPushButton::clicked, this, [this]() { switchTheme("custom"); });

        auto *editCustomBtn = new AnimatedIconButton("✎", kTextMuted, kAccent, content);
        editCustomBtn->setFixedSize(28, 28);
        editCustomBtn->setToolTip("Редактировать цвета пользовательской темы");
        editCustomBtn->setStyleSheet(iconButtonStyle(kAccent, 12, 14));
        connect(editCustomBtn, &QPushButton::clicked, this, [this]() { openThemeCreator("custom"); });

        customRow->addWidget(customBtn);
        customRow->addWidget(editCustomBtn);
        customRow->addStretch(1);
        outer->addLayout(customRow);
    }

    auto *createThemeBtn = new QPushButton("Создать новую тему…", content);
    createThemeBtn->setCursor(Qt::PointingHandCursor);
    createThemeBtn->setStyleSheet(pillButtonStyle(false));
    connect(createThemeBtn, &QPushButton::clicked, this, [this]() { openThemeCreator("custom"); });
    outer->addWidget(createThemeBtn, 0, Qt::AlignLeft);

    addEyebrow("ФОН ПРИЛОЖЕНИЯ");
    auto *bgRow = new QHBoxLayout();
    bgRow->setSpacing(10);
    const QString currentBackdropMode = settings.value("backdrop_mode", "gradient").toString();

    auto addBackdropModeButton = [&](const QString &modeId, const QString &label) {
        auto *btn = new QPushButton(label, content);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(pillButtonStyle(currentBackdropMode == modeId));
        connect(btn, &QPushButton::clicked, this, [this, modeId]() {
            QSettings s(QString(APP_SRC_DIR) + "/config.ini", QSettings::IniFormat);
            s.setValue("backdrop_mode", modeId);
            rebuildUiLive();
        });
        bgRow->addWidget(btn);
    };

    addBackdropModeButton("gradient", "Блики");
    addBackdropModeButton("aurora", "Аврора");
    addBackdropModeButton("particles", "Частицы");
    addBackdropModeButton("starfall", "Звездопад");

    auto *imageBtn = new QPushButton("Своё изображение или GIF…", content);
    imageBtn->setCursor(Qt::PointingHandCursor);
    imageBtn->setStyleSheet(pillButtonStyle(currentBackdropMode == "image"));
    connect(imageBtn, &QPushButton::clicked, this, &MainWindow::openBackgroundPicker);
    bgRow->addWidget(imageBtn);

    auto *noneBtn = new QPushButton("Без фона", content);
    noneBtn->setCursor(Qt::PointingHandCursor);
    noneBtn->setStyleSheet(pillButtonStyle(currentBackdropMode == "none"));
    connect(noneBtn, &QPushButton::clicked, this, [this]() {
        QSettings s(QString(APP_SRC_DIR) + "/config.ini", QSettings::IniFormat);
        s.setValue("backdrop_mode", "none");
        rebuildUiLive();
    });
    bgRow->addWidget(noneBtn);
    bgRow->addStretch(1);
    outer->addLayout(bgRow);

    auto *animCheck = new QCheckBox("Анимация фона включена", content);
    animCheck->setCursor(Qt::PointingHandCursor);
    animCheck->setStyleSheet(QString("QCheckBox { color: %1; font-size: 12px; border: none; background: transparent; }").arg(kTextMain));
    animCheck->setChecked(settings.value("backdrop_animation", true).toBool());
    connect(animCheck, &QCheckBox::toggled, this, [this](bool enabled) {
        QSettings s(QString(APP_SRC_DIR) + "/config.ini", QSettings::IniFormat);
        s.setValue("backdrop_animation", enabled);
        rebuildUiLive();
    });
    outer->addWidget(animCheck);

    addEyebrow("ПОДКЛЮЧЕНИЕ");
    auto *apiBtn = new QPushButton("API-ключи…", content);
    apiBtn->setCursor(Qt::PointingHandCursor);
    apiBtn->setStyleSheet(pillButtonStyle(false));
    connect(apiBtn, &QPushButton::clicked, this, &MainWindow::openAppSettings);
    outer->addWidget(apiBtn, 0, Qt::AlignLeft);

    outer->addStretch(1);
    scroll->setWidget(content);

    auto *pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(7, 7, 7, 7);
    pageLayout->addWidget(scroll);

    return page;
}

void MainWindow::switchTheme(const QString &themeName)
{
    QSettings settings(QString(APP_SRC_DIR) + "/config.ini", QSettings::IniFormat);
    if (settings.value("theme", "dark").toString() == themeName)
        return;

    settings.setValue("theme", themeName);
    rebuildUiLive();
}

void MainWindow::rebuildUiLive()
{
    QTimer::singleShot(0, this, &MainWindow::performUiRebuild);
}

void MainWindow::performUiRebuild()
{
    QSettings settings(QString(APP_SRC_DIR) + "/config.ini", QSettings::IniFormat);
    loadPalette(settings.value("theme", "dark").toString());

    struct AgentSnapshot { QString id; QString name; QString role; bool enabled; };
    QList<AgentSnapshot> snapshot;
    for (auto it = m_cards.constBegin(); it != m_cards.constEnd(); ++it)
        snapshot.append({ it.key(), it.value()->name(), it.value()->role(), m_agentEnabled.value(it.key(), true) });

    const QString activeId = m_activeAgentId;
    const int activePage = pageStack->currentIndex();

    QWidget *oldCentral = centralWidget();
    setStyleSheet(QString("QMainWindow { background-color: %1; }").arg(kBgWindow));
    buildUi();
    if (oldCentral) oldCentral->deleteLater();

    m_cards.clear();
    for (const auto &snap : snapshot) {
        auto *card = createCardWidget(snap.id, snap.name);
        card->setRole(snap.role);
        card->setAgentEnabled(snap.enabled);
        subagentsLayout->insertWidget(subagentsLayout->count() - 1, card);
        m_cards.insert(snap.id, card);
    }
    subagentCountLabel->setText(QString::number(m_cards.size()));

    if (m_agentListCollapsed) {
        sidebarPanel->setFixedWidth(kSidebarCollapsedWidth);
        applyCollapsedVisualState();
    }

    setMainAgentRole(m_mainAgentRole);

    if (!activeId.isEmpty() && m_cards.contains(activeId))
        selectSubagent(activeId);
    else
        selectMainAgent();

    if (activePage == 1)
        showSettings();

    updateTypingIndicator();
}

void MainWindow::buildUi()
{
    auto *backdrop = new AnimatedBackdrop(this);

    auto *shell = new QFrame(backdrop);
    shell->setObjectName("shell");
    shell->setStyleSheet("QFrame#shell { background: transparent; border: none; }");

    auto *shellLayout = new QVBoxLayout(shell);
    shellLayout->setContentsMargins(0, 0, 0, 0);
    shellLayout->setSpacing(0);

    pageStack = new QStackedWidget(shell);
    pageStack->addWidget(buildMainView());
    pageStack->addWidget(buildSettingsPage());
    shellLayout->addWidget(pageStack, 1);

    auto *backdropLayout = new QVBoxLayout(backdrop);
    const int backdropMargin = backdrop->contentMargin();
    backdropLayout->setContentsMargins(backdropMargin, backdropMargin, backdropMargin, backdropMargin);
    backdropLayout->addWidget(shell);

    setCentralWidget(backdrop);
}

void MainWindow::fadeToPage(int index)
{
    if (!pageStack || pageStack->currentIndex() == index)
        return;

    pageStack->setCurrentIndex(index);
    QWidget *w = pageStack->currentWidget();
    if (!w) return;

    auto *effect = new QGraphicsOpacityEffect(w);
    effect->setOpacity(0.0);
    w->setGraphicsEffect(effect);

    auto *anim = new QPropertyAnimation(effect, "opacity", w);
    anim->setDuration(260);
    anim->setStartValue(0.0);
    anim->setEndValue(1.0);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    QPointer<QWidget> guard(w);
    connect(anim, &QPropertyAnimation::finished, w, [guard]() {
        if (guard) guard->setGraphicsEffect(nullptr);
    });
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void MainWindow::showSettings() { fadeToPage(1); }
void MainWindow::showMainView() { fadeToPage(0); }

QWidget *MainWindow::buildMainView()
{
    auto *view = new QWidget(this);
    auto *layout = new QHBoxLayout(view);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    layout->addWidget(buildSidebar());
    layout->addWidget(buildChatArea(), 1);

    codePanelSeparator = new QFrame(view);
    codePanelSeparator->setFixedWidth(0);
    codePanelSeparator->setStyleSheet("background: transparent; border: none;");
    codePanelSeparator->setVisible(false);
    layout->addWidget(codePanelSeparator);

    layout->addWidget(buildCodePanel());

    return view;
}

QWidget *MainWindow::buildSidebar()
{
    auto *panel = new NetworkBackdrop(NetworkBackdrop::Surface::Panel, this);
    sidebarPanel = panel;
    panel->setFixedWidth(kSidebarExpandedWidth);

    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(21, 23, 21, 21);
    layout->setSpacing(10);

    mainStatusCard = new ClickableFrame(panel);
    connect(mainStatusCard, &ClickableFrame::clicked, this, &MainWindow::selectMainAgent);

    auto *statusLayout = new QVBoxLayout(mainStatusCard);
    statusLayout->setContentsMargins(10, 8, 10, 8);
    statusLayout->setSpacing(4);

    auto *mainRow = new QHBoxLayout();
    mainRow->setSpacing(8);

    mainAvatar = new AgentAvatar(mainStatusCard);
    mainAvatar->setFixedSize(34, 34);
    mainAvatar->setAgentName(m_mainAgentName);

    agentStatusDot = new QLabel("●", mainStatusCard);
    agentStatusDot->setFixedWidth(10);
    agentStatusDot->setStyleSheet(QString("color: %1; font-size: 11px; border: none; background: transparent;").arg(kAccentGreen));

    mainNameEdit = new QLineEdit(m_mainAgentName, mainStatusCard);
    mainNameEdit->setToolTip("Имя главного агента");
    mainNameEdit->setFrame(false);
    mainNameEdit->setStyleSheet(QString(
                                    "QLineEdit {"
                                    "   background: transparent;"
                                    "   border: none;"
                                    "   color: %1;"
                                    "   font-size: 13px;"
                                    "   font-weight: 600;"
                                    "   padding: 0px;"
                                    "}"
                                    ).arg(kTextMain));
    connect(mainNameEdit, &QLineEdit::editingFinished, this, [this]() {
        const QString oldName = m_mainAgentName;
        const QString newName = mainNameEdit->text().trimmed();

        if (newName.isEmpty()) {
            setMainAgentName(oldName);
            return;
        }
        if (newName != oldName) {
            const QString oldBackendName = m_agentFolderNames.value(QString(), oldName);
            m_agentManager->changeAgentName(oldBackendName, newName);
            syncAgentName(newName, newName);
            m_agentFolderNames[QString()] = newName;
            setMainAgentName(newName);
            emit mainAgentRenamed(m_mainAgentName);
        }
    });

    mainSettingsButton = new AnimatedIconButton("⚙", kTextMuted, kAccent, mainStatusCard);
    mainSettingsButton->setFixedSize(28, 28);
    mainSettingsButton->setToolTip("Настройки главного агента");
    mainSettingsButton->setStyleSheet(iconButtonStyle(kAccent, 16, 14));
    connect(mainSettingsButton, &QPushButton::clicked, this, &MainWindow::openMainAgentSettings);

    mainRow->addWidget(mainAvatar);
    mainRow->addWidget(agentStatusDot);
    mainRow->addWidget(mainNameEdit, 1);
    mainRow->addWidget(mainSettingsButton);
    statusLayout->addLayout(mainRow);

    auto *metaRow = new QHBoxLayout();
    metaRow->setSpacing(8);

    mainRoleLabel = new ClickableLabel(mainStatusCard);
    mainRoleLabel->setStyleSheet(QString(
                                     "font-size: 11px; border: none; background: transparent; color: %1; padding-left: 22px;"
                                     ).arg(kTextSubtle));
    mainRoleLabel->setToolTip("Роль главного агента");
    connect(mainRoleLabel, &ClickableLabel::clicked, this, &MainWindow::selectMainAgent);

    uptimeLabel = new ClickableLabel("00:00:00", mainStatusCard);
    uptimeLabel->setStyleSheet(QString("font-size: 10px; border: none; background: transparent; color: %1;").arg(kTextMuted));
    uptimeLabel->setToolTip("Время работы приложения");
    connect(uptimeLabel, &ClickableLabel::clicked, this, &MainWindow::selectMainAgent);

    metaRow->addWidget(mainRoleLabel, 1);
    metaRow->addWidget(uptimeLabel, 0, Qt::AlignRight);
    statusLayout->addLayout(metaRow);

    layout->addWidget(mainStatusCard);

    auto *sep = new QFrame(panel);
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet(QString("background-color: %1; border: none; max-height: 1px;").arg(Glass::css(Glass::tokens().stroke)));
    layout->addWidget(sep);

    auto *headerRow = new QHBoxLayout();
    headerRow->setSpacing(8);

    auto *title = new QLabel("АГЕНТЫ", panel);
    title->setStyleSheet(QString("color: %1; font-size: 10px; font-weight: 700; border: none; background: transparent; letter-spacing: 1px;").arg(kTextMuted));
    sidebarTitleLabel = title;

    subagentCountLabel = new QLabel("0", panel);
    subagentCountLabel->setAlignment(Qt::AlignCenter);
    subagentCountLabel->setFixedSize(20, 18);
    subagentCountLabel->setStyleSheet(QString(
                                          "color: %1; font-size: 10px; font-weight: bold; background-color: %2; border-radius: 9px;"
                                          ).arg(kTextSubtle, Glass::css(Glass::tokens().tintStrong)));

    refreshAgentsButton = new AnimatedIconButton("⟳", kTextMuted, kAccent, panel);
    refreshAgentsButton->setFixedSize(24, 24);
    refreshAgentsButton->setToolTip("Обновить список агентов");
    refreshAgentsButton->setStyleSheet(iconButtonStyle(kAccent, 12, 12));
    connect(refreshAgentsButton, &QPushButton::clicked, this, &MainWindow::refreshAgentsFromDisk);

    addSubagentButton = new QPushButton("+", panel);
    addSubagentButton->setFixedSize(24, 24);
    addSubagentButton->setCursor(Qt::PointingHandCursor);
    addSubagentButton->setToolTip("Добавить агента");
    addSubagentButton->setStyleSheet(iconButtonStyle(kAccent, 14, 12));
    connect(addSubagentButton, &QPushButton::clicked, this, [this]() {
        const QString id = addSubagent();
        const QString backendName = m_agentFolderNames.value(id);

        m_agentManager->createAgent(backendName, QString());
        m_agentManager->editFile(backendName, buildMinInfo(backendName, QString()), "min_info");

        emit addSubagentRequested();
        selectSubagent(id);
    });

    collapseListButton = new AnimatedIconButton("◂", kTextMuted, kAccent, panel);
    collapseListButton->setFixedSize(24, 24);
    collapseListButton->setToolTip("Свернуть боковую панель");
    collapseListButton->setStyleSheet(iconButtonStyle(kAccent, 12, 12));
    connect(collapseListButton, &QPushButton::clicked, this, &MainWindow::toggleAgentListCollapsed);

    headerRow->addWidget(title);
    headerRow->addWidget(subagentCountLabel);
    headerRow->addStretch();
    headerRow->addWidget(collapseListButton);
    headerRow->addWidget(refreshAgentsButton);
    headerRow->addWidget(addSubagentButton);
    layout->addLayout(headerRow);

    subagentsScroll = new QScrollArea(panel);
    subagentsScroll->setWidgetResizable(true);
    subagentsScroll->setFrameShape(QFrame::NoFrame);
    subagentsScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    subagentsScroll->setStyleSheet(QString(
                                       "QScrollArea { background: transparent; border: none; }"
                                       "QScrollBar:vertical { background: transparent; width: 8px; margin: 0px; }"
                                       "QScrollBar::handle:vertical { background: %1; border-radius: 4px; min-height: 24px; }"
                                       "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }"
                                       ).arg(Glass::css(Glass::tokens().stroke)));
    subagentsScroll->viewport()->setStyleSheet("background: transparent;");

    auto *scrollContent = new QWidget(subagentsScroll);
    scrollContent->setStyleSheet("background: transparent;");
    subagentsLayout = new QVBoxLayout(scrollContent);
    subagentsLayout->setContentsMargins(0, 0, 4, 0);
    subagentsLayout->setSpacing(6);
    subagentsLayout->addStretch(1);

    subagentsScroll->setWidget(scrollContent);
    layout->addWidget(subagentsScroll, 1);

    auto *sep2 = new QFrame(panel);
    sep2->setFrameShape(QFrame::HLine);
    sep2->setStyleSheet(QString("background-color: %1; border: none; max-height: 1px;").arg(Glass::css(Glass::tokens().stroke)));
    layout->addWidget(sep2);

    settingsEntryButton = new AnimatedIconButton("⚙", kTextMuted, kAccent, panel);
    settingsEntryButton->setFixedSize(38, 38);
    settingsEntryButton->setToolTip("Настройки приложения");
    settingsEntryButton->setStyleSheet(iconButtonStyle(kAccent, 20, 19));
    connect(settingsEntryButton, &QPushButton::clicked, this, &MainWindow::showSettings);
    layout->addWidget(settingsEntryButton, 0, Qt::AlignLeft);

    return panel;
}

// ===========================================================================
// Область чата
// ===========================================================================
QWidget *MainWindow::buildChatArea()
{
    auto *panel = new NetworkBackdrop(NetworkBackdrop::Surface::Window, this);

    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(23, 21, 23, 23);
    layout->setSpacing(12);

    // --- шапка: стеклянная «таблетка» ---
    auto *headerGlass = new NetworkBackdrop(NetworkBackdrop::Surface::Pill, panel);
    headerGlass->setFixedHeight(58);
    auto *headerRow = new QHBoxLayout(headerGlass);
    headerRow->setContentsMargins(14, 8, 18, 8);
    headerRow->setSpacing(12);

    chatHeaderAvatar = new AgentAvatar(headerGlass);
    chatHeaderAvatar->setFixedSize(38, 38);
    chatHeaderAvatar->setAgentName(m_mainAgentName);

    auto *titleCol = new QVBoxLayout();
    titleCol->setSpacing(0);
    chatHeaderLabel = new QLabel(m_mainAgentName, headerGlass);
    chatHeaderLabel->setStyleSheet(QString("color: %1; font-size: 16px; font-weight: 700; border: none; background: transparent;").arg(kTextMain));
    chatHeaderSub = new QLabel("● в сети", headerGlass);
    chatHeaderSub->setStyleSheet(QString("color: %1; font-size: 11px; border: none; background: transparent;").arg(kAccentGreen));
    titleCol->addWidget(chatHeaderLabel);
    titleCol->addWidget(chatHeaderSub);

    headerRow->addWidget(chatHeaderAvatar);
    headerRow->addLayout(titleCol);
    headerRow->addStretch(1);
    layout->addWidget(headerGlass);

    // --- лента сообщений ---
    auto *chatBackdropContainer = new NetworkBackdrop(NetworkBackdrop::Surface::Card, panel);
    auto *chatBackdropLayout = new QVBoxLayout(chatBackdropContainer);
    chatBackdropLayout->setContentsMargins(2, 2, 2, 2);

    chatScroll = new QScrollArea(chatBackdropContainer);
    chatScroll->setWidgetResizable(true);
    chatScroll->setFrameShape(QFrame::NoFrame);
    chatScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    chatScroll->setStyleSheet(QString(
                                  "QScrollArea { background: transparent; border: none; }"
                                  "QScrollBar:vertical { background: transparent; width: 8px; margin: 4px 2px; }"
                                  "QScrollBar::handle:vertical { background: %1; border-radius: 3px; min-height: 28px; }"
                                  "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }"
                                  ).arg(Glass::css(Glass::tokens().stroke)));
    chatScroll->viewport()->setStyleSheet("background: transparent;");

    chatBubblesHost = new QWidget(chatScroll);
    chatBubblesHost->setStyleSheet("background: transparent;");
    chatBubblesLayout = new QVBoxLayout(chatBubblesHost);
    chatBubblesLayout->setContentsMargins(18, 18, 18, 18);
    chatBubblesLayout->setSpacing(14);
    chatBubblesLayout->addStretch(1);

    typingRow = new TypingRow(chatBubblesHost);
    chatBubblesLayout->addWidget(typingRow);

    chatScroll->setWidget(chatBubblesHost);
    chatBackdropLayout->addWidget(chatScroll);
    layout->addWidget(chatBackdropContainer, 1);

    connect(chatScroll->verticalScrollBar(), &QScrollBar::valueChanged, this, [this](int v) {
        if (m_autoScrolling) return;
        m_stickBottom = v >= chatScroll->verticalScrollBar()->maximum() - 40;
    });

    // --- чип «думаю…» ---
    thinkingChip = new QFrame(panel);
    thinkingChip->setStyleSheet(QString(
                                    "QFrame {"
                                    "   background: %1;"
                                    "   border: 1px solid %2;"
                                    "   border-radius: 15px;"
                                    "}"
                                    ).arg(Glass::css(Glass::tokens().tintStrong), Glass::css(Glass::tokens().stroke)));

    auto *chipLayout = new QHBoxLayout(thinkingChip);
    chipLayout->setContentsMargins(14, 7, 16, 7);
    chipLayout->setSpacing(8);

    thinkingDot = new QLabel("●", thinkingChip);
    thinkingDot->setStyleSheet(QString("color: %1; font-size: 12px; border:none; background:transparent;").arg(kAccent));

    thinkingText = new QLabel("Думаю…", thinkingChip);
    thinkingText->setStyleSheet(QString("color: %1; font-size: 12px; font-weight: 600; border:none; background:transparent;").arg(kTextMain));

    chipLayout->addWidget(thinkingDot);
    chipLayout->addWidget(thinkingText, 1);

    thinkingOpacity = new QGraphicsOpacityEffect(thinkingChip);
    thinkingOpacity->setOpacity(0.0);
    thinkingChip->setGraphicsEffect(thinkingOpacity);
    thinkingChip->setVisible(false);

    thinkingFade = new QPropertyAnimation(thinkingOpacity, "opacity", this);
    thinkingFade->setDuration(260);
    thinkingFade->setEasingCurve(QEasingCurve::OutCubic);

    thinkingPulse = new QVariantAnimation(this);
    thinkingPulse->setDuration(650);
    thinkingPulse->setStartValue(0.25);
    thinkingPulse->setEndValue(1.0);
    thinkingPulse->setEasingCurve(QEasingCurve::InOutSine);
    thinkingPulse->setLoopCount(-1);
    connect(thinkingPulse, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        QColor c(kAccent);
        c.setAlphaF(value.toReal());
        thinkingDot->setStyleSheet(QString("color: %1; font-size: 12px; border:none; background:transparent;")
                                       .arg(c.name(QColor::HexArgb)));
    });

    thinkingHideTimer = new QTimer(this);
    thinkingHideTimer->setSingleShot(true);
    thinkingHideTimer->setInterval(3000);
    connect(thinkingHideTimer, &QTimer::timeout, this, [this]() {
        thinkingFade->stop();
        thinkingFade->setStartValue(thinkingOpacity->opacity());
        thinkingFade->setEndValue(0.0);
        thinkingFade->start();
        thinkingPulse->stop();
    });

    layout->addWidget(thinkingChip, 0, Qt::AlignLeft);

    // --- поле ввода: стеклянная капсула с тонким кольцом при фокусе ---
    inputGlass = new NetworkBackdrop(NetworkBackdrop::Surface::Pill, panel);
    inputGlowAnim = new QPropertyAnimation(inputGlass, "glow", inputGlass);
    inputGlowAnim->setDuration(260);
    inputGlowAnim->setEasingCurve(QEasingCurve::OutCubic);

    auto *inputLayout = new QHBoxLayout(inputGlass);
    inputLayout->setContentsMargins(18, 9, 9, 9);
    inputLayout->setSpacing(8);

    messageInput = new QPlainTextEdit(inputGlass);
    messageInput->setFixedHeight(44);
    messageInput->setFrameShape(QFrame::NoFrame);
    messageInput->setStyleSheet(QString(
                                    "QPlainTextEdit {"
                                    "   background: transparent;"
                                    "   border: none;"
                                    "   color: %1;"
                                    "   font-size: 14px;"
                                    "   padding-top: 4px;"
                                    "}"
                                    ).arg(kTextMain));
    messageInput->viewport()->setStyleSheet("background: transparent;");
    messageInput->installEventFilter(this);

    auto *send = new SendButton(inputGlass);
    sendButton = send;
    connect(sendButton, &QPushButton::clicked, this, &MainWindow::handleSendClicked);

    connect(messageInput, &QPlainTextEdit::textChanged, this, [this, send]() {
        send->setReady(!messageInput->toPlainText().trimmed().isEmpty());
        const int lines = qMax(1, int(messageInput->document()->size().height()));
        const int lineH = QFontMetrics(messageInput->font()).lineSpacing() + 2;
        messageInput->setFixedHeight(qBound(44, lines * lineH + 16, 150));
    });

    inputLayout->addWidget(messageInput, 1);
    inputLayout->addWidget(sendButton, 0, Qt::AlignBottom);

    layout->addWidget(inputGlass);

    return panel;
}

void MainWindow::handleThinkingContext(const QString &stage, const QString &message)
{
    Q_UNUSED(stage);
    if (!thinkingChip) return;

    QString shortMsg = message;
    shortMsg.replace('\n', ' ');
    const QFontMetrics metrics(thinkingText->font());
    thinkingText->setText(metrics.elidedText("🧠 " + shortMsg, Qt::ElideRight, 360));

    thinkingChip->setVisible(true);
    thinkingFade->stop();
    thinkingFade->setStartValue(thinkingOpacity->opacity());
    thinkingFade->setEndValue(1.0);
    thinkingFade->start();

    if (thinkingPulse->state() != QAbstractAnimation::Running)
        thinkingPulse->start();

    thinkingHideTimer->start();

    refreshAgentsFromDisk();
}

void MainWindow::appendMainMessage(const QString &text, bool isUser, const QString &code) { appendHistory(QString(), text, isUser, code); }
void MainWindow::appendSubagentMessage(const QString &id, const QString &text, bool isUser, const QString &code) { appendHistory(id, text, isUser, code); }

void MainWindow::setAgentActive(bool active)
{
    agentStatusDot->setStyleSheet(QString("color: %1; font-size: 11px; border: none; background: transparent;")
                                      .arg(active ? kAccentGreen : kAccentRed));
    agentStatusDot->setToolTip(active ? "Активен" : "Офлайн");
}

void MainWindow::setMainAgentName(const QString &name)
{
    m_mainAgentName = name.trimmed().isEmpty() ? QString("Главный агент") : name.trimmed();
    if (mainNameEdit->text() != m_mainAgentName)
        mainNameEdit->setText(m_mainAgentName);
    mainAvatar->setAgentName(m_mainAgentName);
    if (m_activeAgentId.isEmpty()) {
        chatHeaderLabel->setText(m_mainAgentName);
        chatHeaderAvatar->setAgentName(m_mainAgentName);
    }
    refreshComposerPlaceholder();
}

void MainWindow::setMainAgentRole(const QString &role)
{
    m_mainAgentRole = role;
    if (mainRoleLabel) {
        QString preview = role.trimmed().isEmpty() ? QString("Роль не задана") : role.trimmed();
        preview.replace('\n', ' ');
        const QFontMetrics metrics(mainRoleLabel->font());
        mainRoleLabel->setText(metrics.elidedText(preview, Qt::ElideRight, 170));
    }
}

SubagentCard *MainWindow::createCardWidget(const QString &id, const QString &name)
{
    auto *card = new SubagentCard(id, name, subagentsScroll->widget());
    card->setCollapsed(m_agentListCollapsed);

    connect(card, &SubagentCard::clicked, this, &MainWindow::selectSubagent);
    connect(card, &SubagentCard::settingsRequested, this, &MainWindow::openSubagentSettings);
    connect(card, &SubagentCard::nameEdited, this, [this](const QString &cardId, const QString &newName) {
        const QString trimmed = newName.trimmed();
        const QString oldBackendName = m_agentFolderNames.value(cardId);

        if (trimmed.isEmpty()) {
            setSubagentName(cardId, oldBackendName);
            return;
        }
        if (trimmed != oldBackendName) {
            if (!oldBackendName.isEmpty()) {
                m_agentManager->changeAgentName(oldBackendName, trimmed);
                syncAgentName(trimmed, trimmed);
            }
            m_agentFolderNames.insert(cardId, trimmed);
        }
        emit subagentRenamed(cardId, trimmed);
        if (cardId == m_activeAgentId) {
            chatHeaderLabel->setText(trimmed);
            chatHeaderAvatar->setAgentName(trimmed);
        }
    });
    connect(card, &SubagentCard::enabledToggled, this, [this](const QString &cardId, bool enabled) {
        m_agentEnabled.insert(cardId, enabled);
        if (cardId == m_activeAgentId)
            updateTypingIndicator();
    });

    // плавное появление карточки
    auto *effect = new QGraphicsOpacityEffect(card);
    effect->setOpacity(0.0);
    card->setGraphicsEffect(effect);
    auto *fade = new QPropertyAnimation(effect, "opacity", card);
    fade->setDuration(380);
    fade->setStartValue(0.0);
    fade->setEndValue(1.0);
    fade->setEasingCurve(QEasingCurve::OutCubic);
    QPointer<SubagentCard> guard(card);
    connect(fade, &QPropertyAnimation::finished, card, [guard]() {
        if (guard) guard->setGraphicsEffect(nullptr);
    });
    fade->start(QAbstractAnimation::DeleteWhenStopped);

    return card;
}

QString MainWindow::addSubagent(const QString &name, const QString &role)
{
    ++m_subagentSeq;
    const QString id = QString("agent-%1").arg(m_subagentSeq);
    const QString displayName = name.isEmpty() ? QString("Агент %1").arg(m_subagentSeq) : name;

    auto *card = createCardWidget(id, displayName);
    card->setRole(role);

    subagentsLayout->insertWidget(subagentsLayout->count() - 1, card);
    m_cards.insert(id, card);
    m_histories.insert(id, {});
    m_agentFolderNames.insert(id, displayName);
    m_agentEnabled.insert(id, true);
    subagentCountLabel->setText(QString::number(m_cards.size()));

    return id;
}

void MainWindow::removeSubagent(const QString &id)
{
    if (auto *card = m_cards.take(id)) {
        card->deleteLater();
        m_histories.remove(id);
        m_agentFolderNames.remove(id);
        m_agentEnabled.remove(id);
        m_pending.remove(id);
        subagentCountLabel->setText(QString::number(m_cards.size()));
        if (m_activeAgentId == id)
            selectMainAgent();
    }
}

void MainWindow::toggleAgentListCollapsed()
{
    m_agentListCollapsed = !m_agentListCollapsed;

    const int targetWidth = m_agentListCollapsed ? kSidebarCollapsedWidth : kSidebarExpandedWidth;
    auto *anim = new QVariantAnimation(this);
    anim->setDuration(320);
    anim->setStartValue(sidebarPanel->width());
    anim->setEndValue(targetWidth);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    connect(anim, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        sidebarPanel->setFixedWidth(value.toInt());
    });
    anim->start(QAbstractAnimation::DeleteWhenStopped);

    applyCollapsedVisualState();
}

void MainWindow::applyCollapsedVisualState()
{
    mainNameEdit->setVisible(!m_agentListCollapsed);
    mainSettingsButton->setVisible(!m_agentListCollapsed);
    agentStatusDot->setVisible(!m_agentListCollapsed);
    sidebarTitleLabel->setVisible(!m_agentListCollapsed);
    subagentCountLabel->setVisible(!m_agentListCollapsed);
    if (mainRoleLabel) mainRoleLabel->setVisible(!m_agentListCollapsed);
    if (uptimeLabel) uptimeLabel->setVisible(!m_agentListCollapsed);

    if (auto *mainRow = qobject_cast<QVBoxLayout *>(mainStatusCard->layout()))
        mainRow->setContentsMargins(m_agentListCollapsed ? 6 : 10, 8, m_agentListCollapsed ? 6 : 10, 8);

    for (SubagentCard *card : std::as_const(m_cards))
        card->setCollapsed(m_agentListCollapsed);

    collapseListButton->setToolTip(m_agentListCollapsed ? "Развернуть боковую панель" : "Свернуть боковую панель");
}

void MainWindow::setSubagentStatus(const QString &id, AgentStatus status)
{
    auto *card = m_cards.value(id, nullptr);
    if (!card) return;
    switch (status) {
    case AgentStatus::Idle:   card->setStatus(SubagentCard::Status::Idle);   break;
    case AgentStatus::Active: card->setStatus(SubagentCard::Status::Active); break;
    case AgentStatus::Busy:   card->setStatus(SubagentCard::Status::Busy);   break;
    case AgentStatus::Error:  card->setStatus(SubagentCard::Status::Error);  break;
    }
}

void MainWindow::setSubagentName(const QString &id, const QString &name)
{
    auto *card = m_cards.value(id, nullptr);
    if (!card) return;
    card->setName(name);
    if (id == m_activeAgentId) {
        chatHeaderLabel->setText(name);
        chatHeaderAvatar->setAgentName(name);
    }
}

void MainWindow::setSubagentRole(const QString &id, const QString &role)
{
    if (auto *card = m_cards.value(id, nullptr))
        card->setRole(role);
}

QString MainWindow::subagentName(const QString &id) const
{
    auto *card = m_cards.value(id, nullptr);
    return card ? card->name() : QString();
}

QString MainWindow::subagentRole(const QString &id) const
{
    const QString backendName = m_agentFolderNames.value(id);
    if (backendName.isEmpty())
        return QString();
    return extractRoleFromMinInfo(m_agentManager->getFile(backendName, "min_info"));
}

bool MainWindow::hasSubagent(const QString &id) const { return m_cards.contains(id); }
QStringList MainWindow::subagentIds() const { return m_cards.keys(); }

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == messageInput) {
        if (event->type() == QEvent::KeyPress) {
            auto *keyEvent = static_cast<QKeyEvent *>(event);
            if ((keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter)
                && !(keyEvent->modifiers() & Qt::ShiftModifier)) {
                handleSendClicked();
                return true;
            }
        } else if ((event->type() == QEvent::FocusIn || event->type() == QEvent::FocusOut) && inputGlowAnim) {
            inputGlowAnim->stop();
            inputGlowAnim->setStartValue(inputGlass->glow());
            inputGlowAnim->setEndValue(event->type() == QEvent::FocusIn ? 1.0 : 0.0);
            inputGlowAnim->start();
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::selectMainAgent()
{
    m_activeAgentId.clear();
    mainStatusCard->setSelected(true);
    for (SubagentCard *card : std::as_const(m_cards))
        card->setSelected(false);
    chatHeaderLabel->setText(m_mainAgentName);
    chatHeaderAvatar->setAgentName(m_mainAgentName);
    refreshComposerPlaceholder();
    redrawActiveHistory();
    updateTypingIndicator();
    showMainView();
    emit mainAgentSelected();
}

void MainWindow::selectSubagent(const QString &id)
{
    auto *target = m_cards.value(id, nullptr);
    if (!target) return;
    m_activeAgentId = id;
    mainStatusCard->setSelected(false);
    for (SubagentCard *card : std::as_const(m_cards))
        card->setSelected(card->id() == id);
    chatHeaderLabel->setText(target->name());
    chatHeaderAvatar->setAgentName(target->name());
    refreshComposerPlaceholder();
    redrawActiveHistory();
    updateTypingIndicator();
    showMainView();
    emit subagentSelected(id);
}

void MainWindow::refreshComposerPlaceholder()
{
    const QString targetName = m_activeAgentId.isEmpty() ? m_mainAgentName : subagentName(m_activeAgentId);
    messageInput->setPlaceholderText(
        QString("Написать агенту «%1»…  (Enter — отправить, Shift+Enter — новая строка)").arg(targetName));
}

void MainWindow::appendHistory(const QString &agentId, const QString &text, bool isUser, const QString &code)
{
    ChatEntry entry{text, isUser, QDateTime::currentDateTime().toString("hh:mm"), code};
    m_histories[agentId].append(entry);
    if (agentId == m_activeAgentId)
        renderMessage(entry, true);
}

void MainWindow::renderMessage(const ChatEntry &entry, bool animate)
{
    const QString author = entry.isUser
                               ? "Вы"
                               : (m_activeAgentId.isEmpty() ? m_mainAgentName : subagentName(m_activeAgentId));

    auto *bubble = new ChatBubble(author, entry.text, entry.time, entry.isUser, entry.code,
                                  animate && !entry.isUser,
                                  [this]() { if (m_stickBottom) scrollChatToBottom(false); },
                                  chatBubblesHost);
    // перед индикатором набора (он всегда последний)
    chatBubblesLayout->insertWidget(chatBubblesLayout->count() - 1, bubble);
    if (animate)
        bubble->playEntrance();

    if (m_stickBottom)
        QTimer::singleShot(0, this, [this, animate]() { scrollChatToBottom(animate); });
}

void MainWindow::scrollChatToBottom(bool smooth)
{
    if (!chatScroll) return;
    QScrollBar *bar = chatScroll->verticalScrollBar();

    if (!smooth) {
        bar->setValue(bar->maximum());
        return;
    }

    if (m_scrollAnim)
        m_scrollAnim->stop();

    auto *anim = new QPropertyAnimation(bar, "value", this);
    anim->setDuration(380);
    anim->setStartValue(bar->value());
    anim->setEndValue(bar->maximum());
    anim->setEasingCurve(QEasingCurve::OutCubic);
    m_autoScrolling = true;
    connect(anim, &QPropertyAnimation::finished, this, [this]() {
        m_autoScrolling = false;
        m_stickBottom = true;
    });
    m_scrollAnim = anim;
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void MainWindow::redrawActiveHistory()
{
    // [stretch, пузыри..., typingRow] — удаляем только пузыри
    while (chatBubblesLayout->count() > 2) {
        QLayoutItem *item = chatBubblesLayout->takeAt(1);
        delete item->widget();
        delete item;
    }

    m_stickBottom = true;
    const auto entries = m_histories.value(m_activeAgentId);
    for (const auto &entry : entries)
        renderMessage(entry, false);

    QTimer::singleShot(0, this, [this]() { scrollChatToBottom(false); });
}

void MainWindow::updateTypingIndicator()
{
    if (!typingRow) return;

    const bool on = m_pending.contains(m_activeAgentId)
                    && (m_activeAgentId.isEmpty() || m_agentEnabled.value(m_activeAgentId, true));
    auto *row = static_cast<TypingRow *>(typingRow);
    row->setAgentName(m_activeAgentId.isEmpty() ? m_mainAgentName : subagentName(m_activeAgentId));
    row->setActive(on);

    if (chatHeaderSub) {
        const bool enabled = m_activeAgentId.isEmpty() || m_agentEnabled.value(m_activeAgentId, true);
        if (on) {
            chatHeaderSub->setText("печатает…");
            chatHeaderSub->setStyleSheet(QString("color: %1; font-size: 11px; border: none; background: transparent;").arg(kAccent));
        } else if (!enabled) {
            chatHeaderSub->setText("● выключен");
            chatHeaderSub->setStyleSheet(QString("color: %1; font-size: 11px; border: none; background: transparent;").arg(kTextMuted));
        } else {
            chatHeaderSub->setText("● в сети");
            chatHeaderSub->setStyleSheet(QString("color: %1; font-size: 11px; border: none; background: transparent;").arg(kAccentGreen));
        }
    }

    if (on && m_stickBottom)
        QTimer::singleShot(0, this, [this]() { scrollChatToBottom(true); });
}

// ===========================================================================
// Диалоги агента
// ===========================================================================
MainWindow::AgentSettingsResult MainWindow::runAgentSettingsDialog(const QString &title, const QString &backendAgentName, QString &name, QString &role, bool allowDelete)
{
    QDialog dialog(this);
    dialog.setWindowTitle(title);
    dialog.setMinimumWidth(580);
    dialog.setMinimumHeight(540);
    dialog.setStyleSheet(QString(
                             "QDialog { background-color: %1; }"
                             "QLabel { color: %2; font-size: 12px; border: none; background: transparent; }"
                             "QLineEdit, QTextEdit {"
                             "   background-color: %3;"
                             "   color: %2;"
                             "   border: 1px solid %4;"
                             "   border-radius: 12px;"
                             "   padding: 8px;"
                             "   font-size: 13px;"
                             "}"
                             "QLineEdit:focus, QTextEdit:focus { border-color: %5; }"
                             "QTabWidget::pane { border: 1px solid %4; border-radius: 12px; }"
                             "QTabBar::tab { background-color: %3; color: %2; padding: 6px 14px; border-radius: 8px; margin: 2px; }"
                             "QTabBar::tab:selected { color: %5; }"
                             "QPushButton {"
                             "   background-color: %3;"
                             "   color: %2;"
                             "   border: 1px solid %4;"
                             "   border-radius: 12px;"
                             "   padding: 6px 18px;"
                             "   font-size: 13px;"
                             "}"
                             "QPushButton:hover { border-color: %5; color: %5; }"
                             ).arg(kBgWindow, kTextMain, kBgCard, kBorder, kAccent));

    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(20, 14, 20, 16);
    layout->setSpacing(8);

    constexpr int kSwH = 36, kSwPad = 3;
    constexpr int kItemH = kSwH - kSwPad * 2;
    struct SwitchBar : QWidget {
        std::function<void()> onResize;
        using QWidget::QWidget;
        void resizeEvent(QResizeEvent *e) override {
            QWidget::resizeEvent(e);
            if (onResize) onResize();
        }
    };

    const auto &gt = Glass::tokens();
    auto *pageSwitch = new SwitchBar(&dialog);
    pageSwitch->setObjectName("cyberSwitch");
    pageSwitch->setAttribute(Qt::WA_StyledBackground, true);
    pageSwitch->setFixedHeight(kSwH);
    pageSwitch->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    pageSwitch->setStyleSheet(QString(
                                  "#cyberSwitch { background: %1; border: 1px solid %2; border-radius: 18px; }"
                                  "#cyberHighlight {"
                                  "   border-radius: 15px;"
                                  "   border: 1px solid %2;"
                                  "   background: %3;"
                                  "}"
                                  "#cyberSwitch QPushButton {"
                                  "   background: transparent; border: none; border-radius: 15px; padding: 0;"
                                  "   color: %4; font-size: 12px; font-weight: 600; outline: none;"
                                  "}"
                                  "#cyberSwitch QPushButton:hover { color: %5; }"
                                  "#cyberSwitch QPushButton:checked { color: %5; }")
                                  .arg(Glass::css(gt.tint), Glass::css(gt.stroke),
                                       Glass::css(gt.tintStrong),
                                       kTextMuted, kTextMain));
    auto *highlight = new QFrame(pageSwitch);
    highlight->setObjectName("cyberHighlight");
    highlight->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    highlight->setGeometry(kSwPad, kSwPad, 1, kItemH);

    auto *switchGroup = new QButtonGroup(pageSwitch);
    const QStringList switchTexts = {"General", "Services"};
    QList<QPushButton *> switchButtons;
    for (int i = 0; i < 2; ++i) {
        auto *btn = new QPushButton(switchTexts[i], pageSwitch);
        btn->setCheckable(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFocusPolicy(Qt::TabFocus);
        switchGroup->addButton(btn, i);
        switchButtons.append(btn);
    }
    switchButtons[0]->setChecked(true);

    auto *slide = new QPropertyAnimation(highlight, "pos", pageSwitch);
    slide->setDuration(260);
    slide->setEasingCurve(QEasingCurve::OutCubic);

    pageSwitch->onResize = [=, this]() {
        const int itemW = (pageSwitch->width() - kSwPad * 2) / 2;
        for (int i = 0; i < 2; ++i)
            switchButtons[i]->setGeometry(kSwPad + i * itemW, kSwPad, itemW, kItemH);
        slide->stop();
        highlight->setGeometry(kSwPad + qMax(0, switchGroup->checkedId()) * itemW, kSwPad, itemW, kItemH);
    };

    auto *pages = new QStackedWidget(&dialog);

    // --- General ---
    auto *generalPage = new QWidget(pages);
    auto *generalLayout = new QVBoxLayout(generalPage);
    generalLayout->setContentsMargins(0, 0, 0, 0);
    generalLayout->setSpacing(6);

    auto *nameLabel = new QLabel("Имя", &dialog);
    auto *nameEdit = new QLineEdit(name, &dialog);
    nameEdit->setPlaceholderText("Название агента");

    auto *roleLabel = new QLabel("Роль (короткая заметка, отображается бейджем под именем)", &dialog);
    auto *roleEdit = new QTextEdit(&dialog);
    roleEdit->setPlainText(role);
    roleEdit->setPlaceholderText("Короткая заметка про агента…");
    roleEdit->setMaximumHeight(60);

    auto *promptLabel = new QLabel("Системные промпты", &dialog);
    auto *promptTabs = new QTabWidget(&dialog);

    static const QList<QPair<QString, QString>> promptParts = {
                                                                {"soul", "Soul"},
                                                                {"system_prompt", "System Prompt"},
                                                                };

    QMap<QString, QTextEdit *> promptEdits;
    for (const auto &part : promptParts) {
        auto *edit = new QTextEdit(promptTabs);
        edit->setPlainText(m_agentManager->getFile(backendAgentName, part.first));
        promptTabs->addTab(edit, part.second);
        promptEdits.insert(part.first, edit);
    }

    generalLayout->addWidget(nameLabel);
    generalLayout->addWidget(nameEdit);
    generalLayout->addWidget(roleLabel);
    generalLayout->addWidget(roleEdit);
    generalLayout->addWidget(promptLabel);
    generalLayout->addWidget(promptTabs, 1);

    // --- Services ---
    auto *servicesPage = new QWidget(pages);
    auto *servicesLayout = new QVBoxLayout(servicesPage);
    servicesLayout->setContentsMargins(0, 0, 0, 0);
    servicesLayout->setSpacing(6);

    struct ServiceDef { QString id; QString name; };
    static const QList<ServiceDef> kServices = {
    struct ServiceDef { QString id; QString name; };
    static const QList<ServiceDef> kServices = {
                                                {"instagram", "Instagram"},
                                                {"telegram",  "Telegram"},
                                                {"whatsapp",  "WhatsApp"},
                                                {"discord",   "Discord"},
                                                };

    auto serviceCreds = QSharedPointer<QMap<QString, QMap<QString, QString>>>::create();
    auto serviceToggles = QSharedPointer<QMap<QString, ToggleSwitch *>>::create();

    const QString dotsStyle = QString(
                                  "QPushButton {"
                                  "   background: transparent; border: none; color: %1;"
                                  "   font-size: 18px; font-weight: 700; padding: 0 4px;"
                                  "}"
                                  "QPushButton:hover { color: %2; }"
                                  ).arg(kTextMain, kAccent);

    for (const auto &svc : kServices) {
        auto *row = new QWidget(servicesPage);
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(4, 4, 4, 4);
        rowLayout->setSpacing(8);

        auto *label = new QLabel(svc.name, row);

        auto *toggle = new ToggleSwitch(row);
        serviceToggles->insert(svc.id, toggle);
        auto *dotsButton = new QPushButton(QString::fromUtf8("\xE2\x8B\xAF"), row); // ⋯
        dotsButton->setStyleSheet(dotsStyle);
        dotsButton->setCursor(Qt::PointingHandCursor);
        dotsButton->setFixedSize(28, 28);

        rowLayout->addWidget(label);
        rowLayout->addStretch(1);
        rowLayout->addWidget(toggle);
        rowLayout->addWidget(dotsButton);
        servicesLayout->addWidget(row);

        connect(dotsButton, &QPushButton::clicked, &dialog, [this, &dialog, svc, serviceCreds]() {
            QDialog credsDialog(&dialog);
            credsDialog.setWindowTitle(svc.name);
            credsDialog.setMinimumWidth(340);

            auto *form = new QFormLayout(&credsDialog);
            form->setContentsMargins(16, 16, 16, 16);
            form->setSpacing(8);

            QMap<QString, QLineEdit *> fields;
            auto addField = [&](const QString &key, const QString &labelText, bool password = false) {
                auto *edit = new QLineEdit((*serviceCreds)[svc.id].value(key), &credsDialog);
                if (password)
                    edit->setEchoMode(QLineEdit::Password);
                form->addRow(labelText, edit);
                fields.insert(key, edit);
            };

            if (svc.id == "telegram") {
                addField("api_id", "API ID");
                addField("api_hash", "API Hash", true);
                addField("phone", "Номер телефону");

                auto *connectTelegramButton =
                    new QPushButton("Підключити Telegram", &credsDialog);

                connectTelegramButton->setCursor(Qt::PointingHandCursor);
                form->addRow(QString(), connectTelegramButton);

                connect(connectTelegramButton,
                        &QPushButton::clicked,
                        &credsDialog,
                        [this, &fields, svc, serviceCreds]() {
                            const QString apiIdText =
                                fields.value("api_id")->text().trimmed();

                            const QString apiHash =
                                fields.value("api_hash")->text().trimmed();

                            const QString phone =
                                fields.value("phone")->text().trimmed();

                            bool ok = false;
                            const qint32 apiId =
                                apiIdText.toInt(&ok);

                            if (!ok || apiId <= 0) {
                                QMessageBox::warning(
                                    this,
                                    "Telegram",
                                    "API ID має бути додатним числом.");
                                return;
                            }

                            if (apiHash.isEmpty()) {
                                QMessageBox::warning(
                                    this,
                                    "Telegram",
                                    "API Hash не може бути порожнім.");
                                return;
                            }

                            if (phone.isEmpty()) {
                                QMessageBox::warning(
                                    this,
                                    "Telegram",
                                    "Номер телефону не може бути порожнім.");
                                return;
                            }

                            (*serviceCreds)[svc.id]["api_id"] = apiIdText;
                            (*serviceCreds)[svc.id]["api_hash"] = apiHash;
                            (*serviceCreds)[svc.id]["phone"] = phone;

                            if (m_telegramAccountClient) {
                                m_telegramAccountClient->deleteLater();
                                m_telegramAccountClient = nullptr;
                            }

                            QString accountName = m_mainAgentName;

                            if (!m_activeAgentId.isEmpty()) {
                                const QString selectedName =
                                    subagentName(m_activeAgentId);

                                if (!selectedName.trimmed().isEmpty())
                                    accountName = selectedName;
                            }

                            QString safeAccountName = accountName;

                            safeAccountName.replace(
                                QRegularExpression("[^A-Za-zА-Яа-яЇїІіЄєҐґ0-9_-]"),
                                "_");

                            if (safeAccountName.isEmpty())
                                safeAccountName = "default_agent";

                            const QString sessionsRoot =
                                QCoreApplication::applicationDirPath()
                                + "/sessions";

                            const QString sessionDirectory =
                                sessionsRoot + "/" + safeAccountName;

                            if (!QDir().mkpath(sessionDirectory)) {
                                QMessageBox::critical(
                                    this,
                                    "Telegram",
                                    "Не вдалося створити папку сесії:\n"
                                        + sessionDirectory);
                                return;
                            }

                            m_telegramAccountClient =
                                new TelegramAccountClient(this, this);

                            connect(m_telegramAccountClient,
                                    &TelegramAccountClient::statusChanged,
                                    this,
                                    [this](const QString &status) {
                                        statusBar()->showMessage(status, 10000);
                                    });

                            connect(m_telegramAccountClient,
                                    &TelegramAccountClient::authorizationReady,
                                    this,
                                    [this, accountName]() {
                                        QMessageBox::information(
                                            this,
                                            "Telegram",
                                            QString("Акаунт для «%1» успішно підключено.")
                                                .arg(accountName));
                                    });

                            connect(m_telegramAccountClient,
                                    &TelegramAccountClient::authorizationError,
                                    this,
                                    [this](const QString &message) {
                                        QMessageBox::critical(
                                            this,
                                            "Помилка Telegram",
                                            message);
                                    });

                            m_telegramAccountClient->startAuthorization(
                                apiId,
                                apiHash,
                                phone,
                                sessionDirectory);
                        });
            } else {
                // TODO: заміни на поля, потрібні саме цьому сервісу
                addField("login", "Логін / токен");
                addField("password", "Пароль", true);
            }

            auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &credsDialog);
            buttons->button(QDialogButtonBox::Save)->setText("Сохранить");
            buttons->button(QDialogButtonBox::Cancel)->setText("Отмена");
            connect(buttons, &QDialogButtonBox::accepted, &credsDialog, &QDialog::accept);
            connect(buttons, &QDialogButtonBox::rejected, &credsDialog, &QDialog::reject);
            form->addRow(buttons);

            if (credsDialog.exec() == QDialog::Accepted) {
                for (auto it = fields.constBegin(); it != fields.constEnd(); ++it)
                    (*serviceCreds)[svc.id][it.key()] = it.value()->text();
            }
        });
    }

    servicesLayout->addStretch(1);

    pages->addWidget(generalPage);
    pages->addWidget(servicesPage);
    for (int i = 0; i < 2; ++i) {
        connect(switchButtons[i], &QPushButton::clicked, pageSwitch, [=, this]() {
            const int itemW = (pageSwitch->width() - kSwPad * 2) / 2;
            slide->stop();
            slide->setStartValue(highlight->pos());
            slide->setEndValue(QPoint(kSwPad + i * itemW, kSwPad));
            slide->start();
            pages->setCurrentIndex(i);
        });
    }

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Save)->setText("Сохранить");
    buttons->button(QDialogButtonBox::Cancel)->setText("Отмена");
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (allowDelete) {
        auto *deleteButton = new QPushButton("Удалить агента", &dialog);
        deleteButton->setStyleSheet(QString(
                                        "QPushButton { color: %1; border: 1px solid %1; border-radius: 12px; padding: 6px 16px; background: transparent; }"
                                        "QPushButton:hover { background: %2; }"
                                        ).arg(kAccentRed, Glass::css(Glass::withAlpha(QColor(kAccentRed), 40))));
        connect(deleteButton, &QPushButton::clicked, &dialog, [&dialog]() {
            if (QMessageBox::question(&dialog, "Удалить агента",
                                      "Удалить этого агента? Файлы агента будут удалены с диска.") == QMessageBox::Yes)
                dialog.done(2);
        });
        buttons->addButton(deleteButton, QDialogButtonBox::DestructiveRole);
    }

    layout->addWidget(pageSwitch);
    layout->addWidget(pages, 1);
    layout->addWidget(buttons);

    const int result = dialog.exec();
    if (result == 2)
        return AgentSettingsResult::DeleteRequested;
    if (result != QDialog::Accepted)
        return AgentSettingsResult::Cancelled;

    name = nameEdit->text().trimmed();
    role = roleEdit->toPlainText();

    for (auto it = promptEdits.constBegin(); it != promptEdits.constEnd(); ++it)
        m_agentManager->editFile(backendAgentName, it.value()->toPlainText(), it.key());

    // TODO: тут же сохрани выбор сервисов и данные подключения, например:
    // for (auto it = serviceToggles->constBegin(); it != serviceToggles->constEnd(); ++it)
    //     if (it.value()->isChecked())
    //         m_agentManager->connectService(backendAgentName, it.key(), (*serviceCreds)[it.key()]);

    return AgentSettingsResult::Saved;
}

// ===========================================================================
// Фон / тема / API
// ===========================================================================
void MainWindow::openBackgroundPicker()
{
    const QString path = QFileDialog::getOpenFileName(
        this, "Выберите изображение или GIF", QDir::homePath(),
        "Изображения (*.png *.jpg *.jpeg *.gif *.webp *.bmp)");

    if (path.isEmpty()) return;

    QSettings settings(QString(APP_SRC_DIR) + "/config.ini", QSettings::IniFormat);
    settings.setValue("backdrop_mode", "image");
    settings.setValue("backdrop_image_path", path);
    rebuildUiLive();
}

void MainWindow::openThemeCreator(const QString &themeId)
{
    const ThemePalette current = paletteFor(themeId);
    QMap<QString, QColor> colors;
    colors["bg"]      = QColor(current.bgWindow);
    colors["panel"]   = QColor(current.bgPanel);
    colors["card"]    = QColor(current.bgCard);
    colors["border"]  = QColor(current.border);
    colors["text"]    = QColor(current.textMain);
    colors["accent"]  = QColor(current.accent);
    colors["accent2"] = QColor(current.accent2);
    colors["success"] = QColor(current.accentGreen);
    colors["danger"]  = QColor(current.accentRed);

    QDialog dialog(this);
    dialog.setWindowTitle("Редактировать тему: " + themeDisplayName(themeId));
    dialog.setMinimumWidth(420);
    dialog.setStyleSheet(QString(
                             "QDialog { background-color: %1; }"
                             "QLabel { color: %2; font-size: 12px; border: none; background: transparent; }"
                             "QPushButton { border: 1px solid %3; border-radius: 12px; padding: 6px 16px; color: %2; background-color: %4; }"
                             "QPushButton:hover { border-color: %5; }"
                             ).arg(kBgWindow, kTextMain, kBorder, kBgCard, kAccent));

    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(14);

    auto *hint = new QLabel("Выберите ключевые цвета — остальные оттенки (мягкий текст, ховеры) подберутся автоматически.", &dialog);
    hint->setWordWrap(true);
    hint->setStyleSheet(QString("color: %1; font-size: 11px; border: none; background: transparent;").arg(kTextMuted));
    layout->addWidget(hint);

    auto *grid = new QGridLayout();
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(10);

    struct Row { QString key; QString label; };
    const QList<Row> rows = {
        {"bg", "Фон окна"}, {"panel", "Фон панели"}, {"card", "Фон карточек"},
        {"border", "Границы"}, {"text", "Текст"},
        {"accent", "Акцент 1"}, {"accent2", "Акцент 2"},
        {"success", "Успех"}, {"danger", "Ошибка"}
    };

    int row = 0;
    for (const auto &item : rows) {
        const QString key = item.key;

        auto *label = new QLabel(item.label, &dialog);
        auto *swatch = new QPushButton(&dialog);
        swatch->setFixedSize(72, 28);
        swatch->setCursor(Qt::PointingHandCursor);

        auto refreshSwatch = [swatch, &colors, key]() {
            swatch->setStyleSheet(QString("background-color: %1; border: 1px solid rgba(128,128,128,0.4); border-radius: 14px;")
                                      .arg(colors[key].name()));
        };
        refreshSwatch();

        connect(swatch, &QPushButton::clicked, &dialog, [&dialog, &colors, key, refreshSwatch]() {
            const QColor picked = QColorDialog::getColor(colors[key], &dialog, "Выберите цвет");
            if (picked.isValid()) {
                colors[key] = picked;
                refreshSwatch();
            }
        });

        grid->addWidget(label, row, 0);
        grid->addWidget(swatch, row, 1);
        ++row;
    }

    layout->addLayout(grid);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Save)->setText("Сохранить и применить");
    buttons->button(QDialogButtonBox::Cancel)->setText("Отмена");
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted)
        return;

    QSettings saveSettings(QString(APP_SRC_DIR) + "/config.ini", QSettings::IniFormat);
    saveSettings.beginGroup(themeOverrideGroup(themeId));
    for (auto it = colors.constBegin(); it != colors.constEnd(); ++it)
        saveSettings.setValue(it.key(), it.value().name());
    saveSettings.endGroup();
    saveSettings.setValue("theme", themeId);

    rebuildUiLive();
}
void MainWindow::openAppSettings()
{
    QSettings settings(QString(APP_SRC_DIR) + "/config.ini", QSettings::IniFormat);

    QDialog dialog(this);
    dialog.setWindowTitle("Настройки приложения");
    dialog.setMinimumWidth(440);
    dialog.setStyleSheet(QString(
                             "QDialog { background-color: %1; }"
                             "QLabel { color: %2; font-size: 12px; border: none; background: transparent; }"
                             "QLineEdit {"
                             "   background-color: %3;"
                             "   color: %2;"
                             "   border: 1px solid %4;"
                             "   border-radius: 12px;"
                             "   padding: 8px;"
                             "   font-size: 13px;"
                             "}"
                             "QLineEdit:focus { border-color: %5; }"
                             "QCheckBox { color: %2; font-size: 12px; }"
                             "QPushButton {"
                             "   background-color: %3;"
                             "   color: %2;"
                             "   border: 1px solid %4;"
                             "   border-radius: 12px;"
                             "   padding: 6px 18px;"
                             "   font-size: 13px;"
                             "}"
                             "QPushButton:hover { border-color: %5; color: %5; }"
                             "QComboBox {"
                             "   background-color: %3; color: %2; border: 1px solid %4;"
                             "   border-radius: 12px; padding: 8px; font-size: 13px;"
                             "}"
                             "QComboBox:focus { border-color: %5; }"
                             "QComboBox QAbstractItemView { background-color: %3; color: %2; selection-background-color: %5; }"
                             ).arg(kBgWindow, kTextMain, kBgCard, kBorder, kAccent));

    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(10);

    auto *tokenLabel = new QLabel("Telegram Bot Token", &dialog);
    auto *tokenEdit = new QLineEdit(settings.value("telegram_token").toString(), &dialog);
    tokenEdit->setPlaceholderText("123456789:AAExampleTokenValue");
    tokenEdit->setEchoMode(QLineEdit::Password);

    auto *orKeyLabel = new QLabel("OpenRouter API Key", &dialog);
    auto *orKeyEdit = new QLineEdit(settings.value("openrouter_api_key").toString(), &dialog);
    orKeyEdit->setPlaceholderText("sk-or-v1-...");
    orKeyEdit->setEchoMode(QLineEdit::Password);

    auto *modelLabel = new QLabel("Модель (OpenRouter)", &dialog);
    auto *modelBox = new QComboBox(&dialog);
    modelBox->setEditable(true);
    modelBox->addItems({
        "openai/gpt-4o-mini",
        "anthropic/claude-sonnet-4.5",
        "google/gemini-2.5-flash",
        "deepseek/deepseek-chat",
        "meta-llama/llama-3.3-70b-instruct"
    });
    modelBox->setCurrentText(settings.value("ai_model", "openai/gpt-4o-mini").toString());

    auto *showCheckbox = new QCheckBox("Показать значения", &dialog);
    connect(showCheckbox, &QCheckBox::toggled, &dialog, [tokenEdit, orKeyEdit](bool checked) {
        const auto mode = checked ? QLineEdit::Normal : QLineEdit::Password;
        tokenEdit->setEchoMode(mode);
        orKeyEdit->setEchoMode(mode);
    });
    });

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Save)->setText("Сохранить");
    buttons->button(QDialogButtonBox::Cancel)->setText("Отмена");
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    layout->addWidget(tokenLabel);
    layout->addWidget(tokenEdit);
    layout->addWidget(orKeyLabel);
    layout->addWidget(orKeyEdit);
    layout->addWidget(modelLabel);
    layout->addWidget(modelBox);
    layout->addWidget(showCheckbox);
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted)
        return;

    const QString botToken = tokenEdit->text().trimmed();
    const QString aiKey = orKeyEdit->text().trimmed();
    const QString aiModel = modelBox->currentText().trimmed();

    settings.setValue("telegram_token", botToken);
    settings.setValue("openrouter_api_key", aiKey);
    if (!aiModel.isEmpty())
        settings.setValue("ai_model", aiModel);
    settings.remove("cf_account_id");
    settings.remove("cf_api_token");
    settings.remove("groq_api_key");
    settings.remove("gemini_api_key");

    emit appTelegramTokenChanged(botToken);
    emit appAiKeyChanged(aiKey);
    if (!aiModel.isEmpty())
        emit appAiModelChanged(aiModel);
void MainWindow::openMainAgentSettings()
{
    const QString oldBackendName = m_agentFolderNames.value(QString(), m_mainAgentName);

    QString name = m_mainAgentName;
    QString role = m_mainAgentRole;
    if (runAgentSettingsDialog("Настройки главного агента", oldBackendName, name, role, false) != AgentSettingsResult::Saved)
        return;

    const QString finalName = name.isEmpty() ? oldBackendName : name;
    QString effectiveBackendName = oldBackendName;

    if (!oldBackendName.isEmpty() && finalName != oldBackendName) {
        m_agentManager->changeAgentName(oldBackendName, finalName);
        m_agentFolderNames[QString()] = finalName;
        effectiveBackendName = finalName;
    }

    setMainAgentName(finalName);
    setMainAgentRole(role);
    m_agentManager->editFile(effectiveBackendName, buildMinInfo(m_mainAgentName, role), "min_info");

    emit mainAgentRenamed(m_mainAgentName);
    emit mainAgentRoleChanged(m_mainAgentRole);
}

void MainWindow::openSubagentSettings(const QString &id)
{
    auto *card = m_cards.value(id, nullptr);
    if (!card) return;

    const QString oldBackendName = m_agentFolderNames.value(id, card->name());

    QString name = card->name();
    QString role = subagentRole(id);
    const AgentSettingsResult result = runAgentSettingsDialog("Настройки агента", oldBackendName, name, role, true);

    if (result == AgentSettingsResult::DeleteRequested) {
        if (!oldBackendName.isEmpty())
            m_agentManager->deleteAgent(oldBackendName);
        m_agentFolderNames.remove(id);
        emit subagentRemoveRequested(id);
        removeSubagent(id);
        return;
    }
    if (result != AgentSettingsResult::Saved)
        return;

    const QString finalName = name.isEmpty() ? card->name() : name;
    QString effectiveBackendName = oldBackendName;

    if (!oldBackendName.isEmpty() && finalName != oldBackendName) {
        m_agentManager->changeAgentName(oldBackendName, finalName);
        m_agentFolderNames.insert(id, finalName);
        effectiveBackendName = finalName;
    }

    setSubagentName(id, finalName);
    setSubagentRole(id, role);
    m_agentManager->editFile(effectiveBackendName, buildMinInfo(finalName, role), "min_info");

    emit subagentRenamed(id, finalName);
    emit subagentRoleChanged(id, role);
}

// ===========================================================================
// Отправка / ответы
// ===========================================================================
void MainWindow::handleSendClicked()
{
    const QString text = messageInput->toPlainText().trimmed();
    if (text.isEmpty())
        return;

    m_stickBottom = true;

    const bool enabled = m_activeAgentId.isEmpty() || m_agentEnabled.value(m_activeAgentId, true);
    if (!enabled) {
        appendHistory(m_activeAgentId, text, true);
        appendHistory(m_activeAgentId, "Агент выключен.", false);
        messageInput->clear();
        return;
    }

    appendHistory(m_activeAgentId, text, true);

    const QString backendName = m_agentFolderNames.value(m_activeAgentId, m_mainAgentName);
    const qint64 chatId = uiChatIdFor(m_activeAgentId);

    // индикатор «печатает…» до прихода ответа (страховка — 90 секунд)
    const QString pendingId = m_activeAgentId;
    m_pending.insert(pendingId);
    updateTypingIndicator();
    QTimer::singleShot(90000, this, [this, pendingId]() {
        if (m_pending.remove(pendingId))
            updateTypingIndicator();
    });

    m_agentManager->reqAgent(text, chatId, backendName, MessageSource::UI);

    emit messageSubmitted(m_activeAgentId, backendName, text);
    messageInput->clear();
}

void MainWindow::receiveAgentReply(const QString &agentId, const QString &text, const QString &code)
{
    appendHistory(agentId, text, false, code);
}

void MainWindow::updateUptime()
{
    const qint64 secs = startTime.elapsed() / 1000;
    const int hours = int(secs / 3600);
    const int mins = int((secs % 3600) / 60);
    const int seconds = int(secs % 60);
    if (uptimeLabel) {
        uptimeLabel->setText(QString("%1:%2:%3")
                                 .arg(hours, 2, 10, QChar('0'))
                                 .arg(mins, 2, 10, QChar('0'))
                                 .arg(seconds, 2, 10, QChar('0')));
    }
}

// ===========================================================================
// Панель кода (с эффектом «печати»)
// ===========================================================================
QWidget *MainWindow::buildCodePanel()
{
    auto *panel = new NetworkBackdrop(NetworkBackdrop::Surface::Panel, this);
    codePanelContainer = panel;
    panel->setFixedWidth(0);

    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(23, 23, 23, 23);
    layout->setSpacing(10);

    auto *headerRow = new QHBoxLayout();
    headerRow->setSpacing(8);

    codePanelTitle = new QLabel("Код", panel);
    codePanelTitle->setStyleSheet(QString(
                                      "color: %1; font-size: 14px; font-weight: 700; border: none; background: transparent;"
                                      ).arg(kTextMain));

    auto *copyBtn = new AnimatedIconButton("⧉", kTextMuted, kAccent, panel);
    copyBtn->setFixedSize(28, 28);
    copyBtn->setToolTip("Скопировать код");
    copyBtn->setStyleSheet(iconButtonStyle(kAccent, 12, 14));
    connect(copyBtn, &QPushButton::clicked, this, [this]() {
        QGuiApplication::clipboard()->setText(codePanelBody->toPlainText());
    });

    auto *closeBtn = new AnimatedIconButton("✕", kTextMuted, kAccentRed, panel);
    closeBtn->setFixedSize(28, 28);
    closeBtn->setToolTip("Закрыть панель кода");
    closeBtn->setStyleSheet(iconButtonStyle(kAccentRed, 12, 14));
    connect(closeBtn, &QPushButton::clicked, this, &MainWindow::hideCodePanel);

    headerRow->addWidget(codePanelTitle, 1);
    headerRow->addWidget(copyBtn);
    headerRow->addWidget(closeBtn);
    layout->addLayout(headerRow);

    codePanelBody = new QPlainTextEdit(panel);
    codePanelBody->setReadOnly(true);
    codePanelBody->setLineWrapMode(QPlainTextEdit::NoWrap);
    codePanelBody->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    codePanelBody->setStyleSheet(QString(
                                     "QPlainTextEdit {"
                                     "   background-color: rgba(0,0,0,%4);"
                                     "   color: %1;"
                                     "   border: 1px solid %2;"
                                     "   border-radius: 16px;"
                                     "   padding: 12px;"
                                     "   font-size: 12px;"
                                     "}"
                                     "QScrollBar:vertical { background: transparent; width: 8px; margin: 0px; }"
                                     "QScrollBar::handle:vertical { background: %2; border-radius: 4px; min-height: 24px; }"
                                     "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }"
                                     ).arg(kTextMain, Glass::css(Glass::tokens().stroke)).arg(Glass::tokens().dark ? 90 : 18));
    layout->addWidget(codePanelBody, 1);

    codeTypewriterCursorBlink = new QTimer(this);
    codeTypewriterCursorBlink->setInterval(430);
    connect(codeTypewriterCursorBlink, &QTimer::timeout, this, [this]() {
        m_typewriterCursorVisible = !m_typewriterCursorVisible;
        updateTypewriterCursorDisplay();
    });

    return panel;
}

void MainWindow::showCodePanel(const QString &code, const QString &title)
{
    codePanelTitle->setText(title.isEmpty() ? "Код" : title);

    if (codeTypewriterTimer && codeTypewriterTimer->isActive())
        codeTypewriterTimer->stop();

    codePanelBody->clear();
    m_typewriterFullText = code;
    m_typewriterPos = 0;
    m_typewriterCursorVisible = true;
    m_typewriterCursorDrawn = false;

    if (!codeTypewriterTimer) {
        codeTypewriterTimer = new QTimer(this);
        connect(codeTypewriterTimer, &QTimer::timeout, this, &MainWindow::advanceCodeTypewriter);
    }

    const bool wasOpen = m_codePanelOpen;
    codePanelSeparator->setVisible(true);
    m_codePanelOpen = true;

    if (!wasOpen) {
        auto *anim = new QVariantAnimation(this);
        anim->setDuration(380);
        anim->setEasingCurve(QEasingCurve::OutCubic);
        anim->setStartValue(0);
        anim->setEndValue(kCodePanelWidth);
        connect(anim, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
            codePanelContainer->setFixedWidth(value.toInt());
        });
        anim->start(QAbstractAnimation::DeleteWhenStopped);
    } else {
        codePanelContainer->setFixedWidth(kCodePanelWidth);
    }

    if (codeTypewriterCursorBlink)
        codeTypewriterCursorBlink->start();

    codeTypewriterTimer->start(8);
}

void MainWindow::advanceCodeTypewriter()
{
    if (m_typewriterPos >= m_typewriterFullText.size()) {
        codeTypewriterTimer->stop();
        if (codeTypewriterCursorBlink)
            codeTypewriterCursorBlink->stop();
        m_typewriterCursorVisible = false;
        updateTypewriterCursorDisplay();
        return;
    }

    // убираем курсор перед вставкой, иначе символ «▌» застрянет посреди текста
    if (m_typewriterCursorDrawn) {
        m_typewriterCursorVisible = false;
        updateTypewriterCursorDisplay();
        m_typewriterCursorVisible = true;
    }

    const int chunk = 2 + int(QRandomGenerator::global()->bounded(5)); // 2..6 символов
    const int end = qMin(m_typewriterPos + chunk, int(m_typewriterFullText.size()));

    QTextCursor cursor(codePanelBody->document());
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(m_typewriterFullText.mid(m_typewriterPos, end - m_typewriterPos));

    m_typewriterPos = end;

    QScrollBar *bar = codePanelBody->verticalScrollBar();
    bar->setValue(bar->maximum());
}

void MainWindow::updateTypewriterCursorDisplay()
{
    if (!codePanelBody) return;

    QTextCursor cursor(codePanelBody->document());
    cursor.movePosition(QTextCursor::End);

    if (m_typewriterCursorDrawn) {
        cursor.movePosition(QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor);
        cursor.removeSelectedText();
    }

    if (m_typewriterCursorVisible && m_typewriterPos < m_typewriterFullText.size()) {
        cursor.insertText("▌");
        m_typewriterCursorDrawn = true;
    } else {
        m_typewriterCursorDrawn = false;
    }
}

void MainWindow::hideCodePanel()
{
    if (!m_codePanelOpen) return;
    m_codePanelOpen = false;

    if (codeTypewriterTimer)
        codeTypewriterTimer->stop();
    if (codeTypewriterCursorBlink)
        codeTypewriterCursorBlink->stop();

    auto *anim = new QVariantAnimation(this);
    anim->setDuration(320);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    anim->setStartValue(codePanelContainer->width());
    anim->setEndValue(0);

    connect(anim, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        codePanelContainer->setFixedWidth(value.toInt());
    });

    connect(anim, &QVariantAnimation::finished, this, [this]() {
        codePanelSeparator->setVisible(false);
    });

    anim->start(QAbstractAnimation::DeleteWhenStopped);
}
