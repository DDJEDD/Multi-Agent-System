#include "mainwindow.h"
#include "ui_mainwindow.h"

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
#include <cmath>
#include <utility>

// ---------------------------------------------------------------------------
// Anonymous namespace: theming (interface layer) + min_info helpers (logic
// layer). The two used to live in separate files; they are unified here.
// ---------------------------------------------------------------------------
namespace {

qreal sharedBackdropPhase()
{
    static QElapsedTimer clock;
    if (!clock.isValid())
        clock.start();
    return clock.elapsed() * 0.00008846;
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
}

void applyElevation(QWidget *widget, qreal blurRadius = 24, int yOffset = 6, int alpha = 60)
{
    auto *shadow = new QGraphicsDropShadowEffect(widget);
    shadow->setBlurRadius(blurRadius);
    shadow->setOffset(0, yOffset);
    shadow->setColor(QColor(0, 0, 0, alpha));
    widget->setGraphicsEffect(shadow);
}

constexpr int kBackdropMargin = 14;

QColor mixColor(const QColor &a, const QColor &b, qreal t)
{
    return QColor(
        a.red()   + int((b.red()   - a.red())   * t),
        a.green() + int((b.green() - a.green()) * t),
        a.blue()  + int((b.blue()  - a.blue())   * t)
        );
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

QString iconButtonStyle(const QString &hoverColor, const QString &hoverBg, int fontSizePx = 12, int radiusPx = 10)
{
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

}

// ===========================================================================
// AnimatedBackdrop / NetworkBackdrop / ToggleSwitch / AnimatedIconButton /
// AgentAvatar / ClickableLabel / ClickableFrame / SubagentCard
//
// Pure presentation classes
// ===========================================================================

AnimatedBackdrop::AnimatedBackdrop(QWidget *parent)
    : QWidget(parent)
{
    QSettings settings(QString(APP_SRC_DIR) + "/config.ini", QSettings::IniFormat);
    m_mode = settings.value("backdrop_mode", "gradient").toString();

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, [this]() {
        m_phase = sharedBackdropPhase();
        update();
    });

    if (m_mode == "image") {
        m_margin = 56;
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
        m_margin = kBackdropMargin;
        static const QStringList animatedModes = {"gradient", "aurora", "particles", "starfall"};
        if (animatedModes.contains(m_mode) && settings.value("backdrop_animation", true).toBool())
            m_timer->start(130);
    }
}

void AnimatedBackdrop::paintEvent(QPaintEvent *)
{
    QPainter painter(this);

    const QRect inner = rect().adjusted(m_margin, m_margin, -m_margin, -m_margin);
    QRegion visible(rect());
    visible -= QRegion(inner);
    painter.setClipRegion(visible);

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

    if (m_mode == "aurora") { paintAurora(painter); return; }
    if (m_mode == "particles") { paintParticles(painter); return; }
    if (m_mode == "starfall") { paintStarfall(painter); return; }
    if (m_mode == "none") return;

    paintBlobs(painter);
}

void AnimatedBackdrop::paintBlobs(QPainter &painter)
{
    struct Blob { qreal ax, ay, freqX, freqY, phaseX, phaseY, radius; QString color; };
    const qreal w = width();
    const qreal h = height();
    const qreal span = qMax(w, h);

    const QList<Blob> blobs = {
                               { w * 0.28, h * 0.26, 0.55, 0.42, 0.0, 1.4, span * 0.20, kAccent },
                               { w * 0.78, h * 0.62, 0.40, 0.60, 2.1, 0.4, span * 0.18, kAccent2 },
                               { w * 0.52, h * 0.88, 0.48, 0.35, 4.0, 2.6, span * 0.16, kAccentGreen },
                               };

    for (const auto &blob : blobs) {
        const qreal cx = blob.ax + std::sin(m_phase * blob.freqX + blob.phaseX) * w * 0.12;
        const qreal cy = blob.ay + std::cos(m_phase * blob.freqY + blob.phaseY) * h * 0.12;

        QRadialGradient gradient(cx, cy, blob.radius);
        QColor inner(blob.color); inner.setAlphaF(0.15f);
        QColor edge(blob.color); edge.setAlphaF(0.0f);
        gradient.setColorAt(0.0, inner);
        gradient.setColorAt(1.0, edge);

        painter.setPen(Qt::NoPen);
        painter.setBrush(gradient);
        painter.drawEllipse(QPointF(cx, cy), blob.radius, blob.radius);
    }
}

void AnimatedBackdrop::paintAurora(QPainter &painter)
{
    const qreal angle = m_phase * 14.0;
    const qreal rad = angle * M_PI / 180.0;
    const qreal cx = width() / 2.0;
    const qreal cy = height() / 2.0;
    const qreal len = qMax(width(), height());

    QLinearGradient gradient(cx - std::cos(rad) * len, cy - std::sin(rad) * len,
                             cx + std::cos(rad) * len, cy + std::sin(rad) * len);

    QColor edge1(kAccent); edge1.setAlphaF(0.0f);
    QColor mid1(kAccent); mid1.setAlphaF(0.20f);
    QColor mid2(kAccent2); mid2.setAlphaF(0.20f);
    QColor edge2(kAccent2); edge2.setAlphaF(0.0f);

    gradient.setColorAt(0.0, edge1);
    gradient.setColorAt(0.35, mid1);
    gradient.setColorAt(0.65, mid2);
    gradient.setColorAt(1.0, edge2);

    painter.fillRect(rect(), gradient);
}

void AnimatedBackdrop::paintParticles(QPainter &painter)
{
    struct Spot { qreal nx, ny, phase, speed; };
    static const QList<Spot> spots = {
                                      {0.02, 0.08, 0.0, 1.0}, {0.97, 0.15, 1.3, 0.8}, {0.06, 0.90, 2.6, 1.2},
                                      {0.94, 0.85, 0.7, 0.9}, {0.35, 0.03, 2.1, 1.1}, {0.65, 0.97, 3.4, 0.7},
                                      {0.03, 0.45, 1.8, 1.0}, {0.97, 0.55, 4.0, 0.8}, {0.20, 0.04, 2.9, 1.3},
                                      {0.80, 0.96, 0.3, 0.9},
                                      };

    const QStringList palette = {kAccent, kAccent2, kAccentGreen};
    const qreal w = width();
    const qreal h = height();

    painter.setPen(Qt::NoPen);
    int i = 0;
    for (const auto &spot : spots) {
        const qreal twinkle = 0.5 + 0.5 * std::sin(m_phase * spot.speed + spot.phase);
        const qreal radius = 2.5 + twinkle * 2.5;

        QColor color(palette.at(i % palette.size()));
        color.setAlphaF(0.15f + twinkle * 0.35f);
        painter.setBrush(color);
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

        QColor headColor(m.color); headColor.setAlphaF(float(0.8 * alpha));
        QColor tailColor(m.color); tailColor.setAlphaF(0.0f);

        QLinearGradient trailGradient(head, tail);
        trailGradient.setColorAt(0.0, headColor);
        trailGradient.setColorAt(1.0, tailColor);

        QPen trailPen(QBrush(trailGradient), 1.6);
        trailPen.setCapStyle(Qt::RoundCap);
        painter.setPen(trailPen);
        painter.drawLine(head, tail);

        QColor dotColor(Qt::white); dotColor.setAlphaF(float(0.9 * alpha));
        painter.setPen(Qt::NoPen);
        painter.setBrush(dotColor);
        painter.drawEllipse(head, 1.7, 1.7);
    }
}

NetworkBackdrop::NetworkBackdrop(Surface surface, QWidget *parent)
    : QWidget(parent)
    , m_surface(surface)
{
    QSettings settings(QString(APP_SRC_DIR) + "/config.ini", QSettings::IniFormat);
    m_mode = settings.value("backdrop_mode", "gradient").toString();
    static const QStringList animatedModes = {"gradient", "aurora", "particles", "starfall"};
    m_enabled = animatedModes.contains(m_mode) && settings.value("backdrop_animation", true).toBool();

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, [this]() {
        m_phase = sharedBackdropPhase();
        update();
    });
    if (m_enabled)
        m_timer->start(130);
}

void NetworkBackdrop::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const int radius = m_surface == Surface::Card ? 14 : 0;
    QPainterPath path;
    path.addRoundedRect(rect(), radius, radius);
    painter.setClipPath(path);

    QString fillColor = kBgPanel;
    if (m_surface == Surface::Card) fillColor = kBgCard;
    else if (m_surface == Surface::Window) fillColor = kBgWindow;
    painter.fillRect(rect(), QColor(fillColor));

    if (!m_enabled) return;

    if (m_mode == "starfall") paintStarfall(painter);
    else paintNodes(painter);
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

void NetworkBackdrop::paintNodes(QPainter &painter)
{
    struct Node { qreal nx, ny, freqX, freqY, phaseX, phaseY; };
    static const QList<Node> nodes = {
                                      {0.10, 0.15, 0.25, 0.20, 0.0, 1.1}, {0.30, 0.08, 0.22, 0.28, 1.4, 0.3},
                                      {0.55, 0.12, 0.20, 0.24, 2.6, 2.0}, {0.80, 0.20, 0.26, 0.18, 0.7, 1.6},
                                      {0.90, 0.45, 0.24, 0.22, 3.1, 0.5}, {0.75, 0.70, 0.18, 0.26, 1.9, 2.4},
                                      {0.50, 0.85, 0.22, 0.20, 0.4, 1.3}, {0.20, 0.78, 0.25, 0.24, 2.2, 0.8},
                                      {0.08, 0.50, 0.20, 0.28, 1.0, 2.7}, {0.40, 0.45, 0.23, 0.21, 2.8, 1.5},
                                      {0.62, 0.55, 0.19, 0.25, 0.2, 2.1}, {0.35, 0.65, 0.21, 0.23, 1.6, 0.6},
                                      };

    const qreal w = width();
    const qreal h = height();

    QList<QPointF> points;
    points.reserve(nodes.size());
    for (const auto &node : nodes) {
        const qreal x = node.nx * w + std::sin(m_phase * node.freqX + node.phaseX) * w * 0.06;
        const qreal y = node.ny * h + std::cos(m_phase * node.freqY + node.phaseY) * h * 0.06;
        points.append(QPointF(x, y));
    }

    const qreal linkDistance = qMin(w, h) * 0.32;
    QPen linePen;
    linePen.setWidthF(1.0);

    QList<QPointF> pulses;
    pulses.reserve(points.size());

    for (int i = 0; i < points.size(); ++i) {
        for (int j = i + 1; j < points.size(); ++j) {
            const qreal dx = points[i].x() - points[j].x();
            const qreal dy = points[i].y() - points[j].y();
            const qreal dist = std::sqrt(dx * dx + dy * dy);
            if (dist >= linkDistance) continue;

            const qreal strength = 1.0 - dist / linkDistance;
            QColor c(kBorder);
            c.setAlphaF(float(0.16 * strength));
            linePen.setColor(c);
            painter.setPen(linePen);
            painter.drawLine(points[i], points[j]);

            const qreal speed = 0.5 + qreal((i * 3 + j * 7) % 5) * 0.12;
            const qreal offset = qreal((i * 11 + j * 17) % 100) / 100.0;
            const qreal t = std::fmod(m_phase * speed * 0.15 + offset, 1.0);
            pulses.append(points[i] + (points[j] - points[i]) * t);
        }
    }

    QColor pulseColor(kAccent2);
    painter.setPen(Qt::NoPen);
    for (const auto &pulse : pulses) {
        pulseColor.setAlphaF(0.55f);
        painter.setBrush(pulseColor);
        painter.drawEllipse(pulse, 1.6, 1.6);
    }

    QColor dotColor(kAccent);
    dotColor.setAlphaF(0.35f);
    painter.setBrush(dotColor);
    for (const auto &pt : points)
        painter.drawEllipse(pt, 2.2, 2.2);
}

ToggleSwitch::ToggleSwitch(QWidget *parent)
    : QWidget(parent)
{
    setFixedSize(38, 22);
    setCursor(Qt::PointingHandCursor);
    m_anim = new QPropertyAnimation(this, "knobPos", this);
    m_anim->setDuration(180);
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
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const QRectF track(0, 0, width(), height());
    const qreal radius = height() / 2.0;
    const QColor trackColor = mixColor(QColor(kBorder), QColor(kAccentGreen), m_knobPos);

    painter.setPen(Qt::NoPen);
    painter.setBrush(trackColor);
    painter.drawRoundedRect(track, radius, radius);

    const qreal knobDiameter = height() - 4;
    const qreal knobX = 2 + m_knobPos * (width() - knobDiameter - 4);
    painter.setBrush(Qt::white);
    painter.drawEllipse(QRectF(knobX, 2, knobDiameter, knobDiameter));
}

AnimatedIconButton::AnimatedIconButton(const QString &glyph, const QString &baseColor, const QString &hoverColor, QWidget *parent)
    : QPushButton(parent)
    , m_glyph(glyph)
    , m_baseColor(baseColor)
    , m_hoverColor(hoverColor)
{
    setCursor(Qt::PointingHandCursor);
    m_anim = new QPropertyAnimation(this, "spin", this);
    m_anim->setDuration(320);
    m_anim->setEasingCurve(QEasingCurve::OutBack);
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
    m_paletteIndex = trimmed.isEmpty() ? 0 : (qHash(trimmed) % kAvatarGradients.size());
    update();
}

void AgentAvatar::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const auto &grad = kAvatarGradients.at(m_paletteIndex);
    QLinearGradient gradient(0, 0, width(), height());
    gradient.setColorAt(0, grad.start);
    gradient.setColorAt(1, grad.end);

    painter.setPen(Qt::NoPen);
    painter.setBrush(gradient);
    painter.drawEllipse(rect());

    QFont font = painter.font();
    font.setBold(true);
    font.setPointSize(qMax(9, height() / 3));
    painter.setFont(font);
    painter.setPen(Qt::white);
    painter.drawText(rect(), Qt::AlignCenter, m_initials);
}

ClickableLabel::ClickableLabel(QWidget *parent) : QLabel(parent) { setCursor(Qt::PointingHandCursor); }
ClickableLabel::ClickableLabel(const QString &text, QWidget *parent) : QLabel(text, parent) { setCursor(Qt::PointingHandCursor); }

void ClickableLabel::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) emit clicked();
    QLabel::mousePressEvent(event);
}

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
    QString bg = "transparent";
    QString borderLeft = "3px solid transparent";
    if (m_selected) {
        bg = kBgCardHover;
        borderLeft = QString("3px solid %1").arg(kBorderSel);
    } else if (m_hovered) {
        bg = kBgCard;
    }
    setStyleSheet(QString(
                      "ClickableFrame {"
                      "   background-color: %1;"
                      "   border: none;"
                      "   border-left: %2;"
                      "   border-top-right-radius: 8px;"
                      "   border-bottom-right-radius: 8px;"
                      "}"
                      ).arg(bg, borderLeft));
}

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
                                  "QLineEdit:focus { color: #ffffff; }"
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
    m_settingsButton->setStyleSheet(iconButtonStyle(kAccent, "rgba(137, 180, 250, 0.15)", 16, 13));
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
    QString bg = "transparent";
    QString borderLeft = "3px solid transparent";
    if (m_selected) {
        bg = kBgCardHover;
        borderLeft = QString("3px solid %1").arg(kBorderSel);
    } else if (m_hovered) {
        bg = kBgCard;
    }
    setStyleSheet(QString(
                      "SubagentCard {"
                      "   background-color: %1;"
                      "   border: none;"
                      "   border-left: %2;"
                      "   border-top-right-radius: 8px;"
                      "   border-bottom-right-radius: 8px;"
                      "}"
                      ).arg(bg, borderLeft));
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
// CodeBlockWidget — сворачиваемый блок кода внутри пузыря сообщения
// ===========================================================================
namespace {

class CodeBlockWidget : public QFrame
{
public:
    explicit CodeBlockWidget(const QString &code, QWidget *parent = nullptr)
        : QFrame(parent)
    {
        auto *outer = new QVBoxLayout(this);
        outer->setContentsMargins(0, 6, 0, 0);
        outer->setSpacing(0);

        m_header = new ClickableFrame(this);
        m_header->setStyleSheet(QString(
                                    "ClickableFrame {"
                                    "   background-color: %1;"
                                    "   border: 1px solid %2;"
                                    "   border-top-left-radius: 10px;"
                                    "   border-top-right-radius: 10px;"
                                    "   border-left: none;"
                                    "}"
                                    ).arg(kBgCard, kBorder));

        auto *headerLayout = new QHBoxLayout(m_header);
        headerLayout->setContentsMargins(10, 6, 8, 6);
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
        copyBtn->setStyleSheet(iconButtonStyle(kAccent, "rgba(137, 180, 250, 0.15)"));
        connect(copyBtn, &QPushButton::clicked, this, [code]() {
            QGuiApplication::clipboard()->setText(code);
        });

        headerLayout->addWidget(m_chevron);
        headerLayout->addWidget(label, 1);
        headerLayout->addWidget(copyBtn);
        connect(m_header, &ClickableFrame::clicked, this, [this]() { toggle(); });

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
                                  "   background-color: %1;"
                                  "   color: %2;"
                                  "   border: 1px solid %3;"
                                  "   border-top: none;"
                                  "   border-bottom-left-radius: 10px;"
                                  "   border-bottom-right-radius: 10px;"
                                  "   padding: 8px;"
                                  "   font-size: 12px;"
                                  "}"
                                  "QScrollBar:vertical { background: transparent; width: 8px; margin: 0px; }"
                                  "QScrollBar::handle:vertical { background: %3; border-radius: 4px; min-height: 24px; }"
                                  ).arg(kBgWindow, kTextMain, kBorder));

        containerLayout->addWidget(m_body);

        outer->addWidget(m_header);
        outer->addWidget(m_container);

        const int lineH = QFontMetrics(m_body->font()).lineSpacing();
        const int lines = qMin(code.count('\n') + 1, 14);
        m_expandedHeight = lines * lineH + 20;

        m_body->setFixedHeight(m_expandedHeight);

        m_anim = new QPropertyAnimation(m_container, "maximumHeight", this);
        m_anim->setDuration(220);
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
    ClickableFrame *m_header;
    QLabel *m_chevron;
    QWidget *m_container;
    QPlainTextEdit *m_body;
    QPropertyAnimation *m_anim;
    int m_expandedHeight = 0;
    bool m_expanded = false;
};

class ChatBubble : public QFrame
{
public:
    ChatBubble(const QString &author, const QString &text, const QString &time,
               bool isUser, const QString &code, QWidget *parent = nullptr)
        : QFrame(parent)
    {
        auto *outer = new QHBoxLayout(this);
        outer->setContentsMargins(0, 0, 0, 0);
        outer->setSpacing(10);

        auto *avatar = new AgentAvatar(this);
        avatar->setFixedSize(30, 30);
        avatar->setAgentName(author);

        auto *bubble = new QFrame(this);
        bubble->setObjectName("bubble");
        bubble->setMaximumWidth(440);
        const QString bg = isUser
                               ? QString("qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 %1, stop:1 %2)").arg(kAccent, kAccent2)
                               : kBgWindow;
        bubble->setStyleSheet(QString(
                                  "QFrame#bubble {"
                                  "   background: %1;"
                                  "   border: 1px solid %2;"
                                  "   border-radius: 14px;"
                                  "}"
                                  ).arg(bg, isUser ? "transparent" : kBorder));

        auto *bubbleLayout = new QVBoxLayout(bubble);
        bubbleLayout->setContentsMargins(14, 10, 14, 10);
        bubbleLayout->setSpacing(3);

        auto *headerRow = new QHBoxLayout();
        headerRow->setSpacing(8);
        auto *authorLabel = new QLabel(author, bubble);
        authorLabel->setStyleSheet(QString("font-size: 11px; font-weight: 700; color: %1; border:none; background:transparent;")
                                       .arg(isUser ? "rgba(255,255,255,0.85)" : kAccent));
        auto *timeLabel = new QLabel(time, bubble);
        timeLabel->setStyleSheet(QString("font-size: 10px; color: %1; border:none; background:transparent;")
                                     .arg(isUser ? "rgba(255,255,255,0.6)" : kTextMuted));
        headerRow->addWidget(authorLabel);
        headerRow->addStretch();
        headerRow->addWidget(timeLabel);
        bubbleLayout->addLayout(headerRow);

        if (!text.trimmed().isEmpty()) {
            auto *textLabel = new QLabel(bubble);
            textLabel->setText(text.toHtmlEscaped().replace("\n", "<br/>"));
            textLabel->setTextFormat(Qt::RichText);
            textLabel->setWordWrap(true);
            textLabel->setStyleSheet(QString("font-size: 13px; color: %1; border:none; background:transparent;")
                                         .arg(isUser ? "#ffffff" : kTextMain));
            bubbleLayout->addWidget(textLabel);
        }

        if (!code.trimmed().isEmpty()) {
            auto *codeBlock = new CodeBlockWidget(code, bubble);
            bubbleLayout->addWidget(codeBlock);
        }

        if (isUser) {
            outer->addStretch(1);
            outer->addWidget(bubble);
            outer->addWidget(avatar, 0, Qt::AlignTop);
        } else {
            outer->addWidget(avatar, 0, Qt::AlignTop);
            outer->addWidget(bubble);
            outer->addStretch(1);
        }
    }

    void playEntrance()
    {
        auto *effect = new QGraphicsOpacityEffect(this);
        setGraphicsEffect(effect);

        auto *anim = new QPropertyAnimation(effect, "opacity", this);
        anim->setDuration(240);
        anim->setStartValue(0.0);
        anim->setEndValue(1.0);
        anim->setEasingCurve(QEasingCurve::OutCubic);

        QPointer<ChatBubble> self(this);
        connect(anim, &QPropertyAnimation::finished, this, [self]() {
            if (self)
                self->setGraphicsEffect(nullptr);
        });

        anim->start(QAbstractAnimation::DeleteWhenStopped);
    }
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
    resize(1080, 700);
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
    const QString agentId = m_uiChatIds.key(chatId);

    for (const DelayedMessage &msg : messages) {
        if (!msg.text.isEmpty() || !msg.code.isEmpty())
            receiveAgentReply(agentId, msg.text, msg.code);

        if (!msg.code.isEmpty()) {
            const QString author = agentId.isEmpty() ? m_mainAgentName : subagentName(agentId);
            showCodePanel(msg.code, "Код — " + author);
        }
    }

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

QWidget *MainWindow::buildSettingsPage()
{
    auto *page = new QWidget(this);
    page->setStyleSheet(QString("background-color: %1;").arg(kBgWindow));

    auto *scroll = new QScrollArea(page);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet(QString(
                              "QScrollArea { background: transparent; border: none; }"
                              "QScrollBar:vertical { background: transparent; width: 8px; margin: 0px; }"
                              "QScrollBar::handle:vertical { background: %1; border-radius: 4px; min-height: 24px; }"
                              "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }"
                              ).arg(kBorder));

    auto *content = new QWidget(scroll);
    content->setStyleSheet("background: transparent;");
    auto *outer = new QVBoxLayout(content);
    outer->setContentsMargins(36, 30, 36, 30);
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
    pageTitle->setStyleSheet(QString("color: %1; font-size: 20px; font-weight: 700; border: none; background: transparent;").arg(kTextMain));
    outer->addWidget(pageTitle);

    auto pillButtonStyle = [](bool active) {
        return QString(
                   "QPushButton {"
                   "   background-color: %1;"
                   "   color: %2;"
                   "   border: 1px solid %3;"
                   "   border-radius: 10px;"
                   "   padding: 10px 18px;"
                   "   font-size: 13px;"
                   "   font-weight: 600;"
                   "}"
                   "QPushButton:hover { border-color: %4; }"
                   ).arg(active ? kAccent : kBgCard, active ? "#ffffff" : kTextMain, active ? kAccent : kBorder, kAccent);
    };

    auto addEyebrow = [&](const QString &text) {
        auto *label = new QLabel(text, content);
        label->setStyleSheet(QString("color: %1; font-size: 10px; font-weight: 700; border: none; background: transparent; margin-top: 12px;").arg(kTextMuted));
        outer->addWidget(label);
    };

    QSettings settings(QString(APP_SRC_DIR) + "/config.ini", QSettings::IniFormat);

    addEyebrow("ТЕМА ОФОРМЛЕНИЯ");
    const QString currentTheme = settings.value("theme", "dark").toString();

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

        auto *modeToggle = new ToggleSwitch(content);
        modeToggle->setToolTip("Светлая / тёмная версия темы");
        if (isLight) modeToggle->setChecked(false, false);

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
        editBtn->setFixedSize(26, 26);
        editBtn->setToolTip("Редактировать цвета темы «" + themeDisplayName(editTargetId) + "»");
        editBtn->setStyleSheet(iconButtonStyle(kAccent, "rgba(137, 180, 250, 0.15)"));
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
        editCustomBtn->setFixedSize(26, 26);
        editCustomBtn->setToolTip("Редактировать цвета пользовательской темы");
        editCustomBtn->setStyleSheet(iconButtonStyle(kAccent, "rgba(137, 180, 250, 0.15)"));
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
    pageLayout->setContentsMargins(0, 0, 0, 0);
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
}

void MainWindow::buildUi()
{
    auto *backdrop = new AnimatedBackdrop(this);

    auto *shell = new QFrame(backdrop);
    shell->setObjectName("shell");
    shell->setStyleSheet(QString(
                             "QFrame#shell {"
                             "   background-color: %1;"
                             "   border: 1px solid %2;"
                             "}"
                             ).arg(kBgWindow, kBorder));

    auto *shellLayout = new QVBoxLayout(shell);
    shellLayout->setContentsMargins(1, 1, 1, 1);
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

void MainWindow::showSettings() { pageStack->setCurrentIndex(1); }
void MainWindow::showMainView() { pageStack->setCurrentIndex(0); }

QWidget *MainWindow::buildMainView()
{
    auto *view = new QWidget(this);
    auto *layout = new QHBoxLayout(view);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    layout->addWidget(buildSidebar());

    auto *vSep = new QFrame(view);
    vSep->setFrameShape(QFrame::VLine);
    vSep->setStyleSheet(QString("background-color: %1; border: none; max-width: 1px;").arg(kBorder));
    layout->addWidget(vSep);

    layout->addWidget(buildChatArea(), 1);

    codePanelSeparator = new QFrame(view);
    codePanelSeparator->setFrameShape(QFrame::VLine);
    codePanelSeparator->setStyleSheet(QString("background-color: %1; border: none; max-width: 1px;").arg(kBorder));
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
    layout->setContentsMargins(14, 16, 14, 14);
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
                                    "QLineEdit:focus { color: #ffffff; }"
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
    mainSettingsButton->setStyleSheet(iconButtonStyle(kAccent, "rgba(137, 180, 250, 0.15)", 16, 13));
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
    sep->setStyleSheet(QString("background-color: %1; border: none; max-height: 1px;").arg(kBorder));
    layout->addWidget(sep);

    auto *headerRow = new QHBoxLayout();
    headerRow->setSpacing(8);

    auto *title = new QLabel("АГЕНТЫ", panel);
    title->setStyleSheet(QString("color: %1; font-size: 10px; font-weight: 700; border: none; background: transparent;").arg(kTextMuted));
    sidebarTitleLabel = title;

    subagentCountLabel = new QLabel("0", panel);
    subagentCountLabel->setAlignment(Qt::AlignCenter);
    subagentCountLabel->setFixedSize(18, 18);
    subagentCountLabel->setStyleSheet(QString(
                                          "color: %1; font-size: 10px; font-weight: bold; background-color: %2; border-radius: 9px;"
                                          ).arg(kTextSubtle, kBgCard));

    refreshAgentsButton = new AnimatedIconButton("⟳", kTextMuted, kAccent, panel);
    refreshAgentsButton->setFixedSize(22, 22);
    refreshAgentsButton->setToolTip("Обновить список агентов");
    refreshAgentsButton->setStyleSheet(iconButtonStyle(kAccent, "rgba(137, 180, 250, 0.15)"));
    connect(refreshAgentsButton, &QPushButton::clicked, this, &MainWindow::refreshAgentsFromDisk);

    addSubagentButton = new QPushButton("+", panel);
    addSubagentButton->setFixedSize(22, 22);
    addSubagentButton->setCursor(Qt::PointingHandCursor);
    addSubagentButton->setToolTip("Добавить агента");
    addSubagentButton->setStyleSheet(iconButtonStyle(kAccent, "rgba(137, 180, 250, 0.15)"));
    connect(addSubagentButton, &QPushButton::clicked, this, [this]() {
        const QString id = addSubagent();
        const QString backendName = m_agentFolderNames.value(id);

        m_agentManager->createAgent(backendName, QString());
        m_agentManager->editFile(backendName, buildMinInfo(backendName, QString()), "min_info");

        emit addSubagentRequested();
        selectSubagent(id);
    });

    collapseListButton = new AnimatedIconButton("◂", kTextMuted, kAccent, panel);
    collapseListButton->setFixedSize(22, 22);
    collapseListButton->setToolTip("Свернуть боковую панель");
    collapseListButton->setStyleSheet(iconButtonStyle(kAccent, "rgba(137, 180, 250, 0.15)"));
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
    subagentsScroll->setStyleSheet(
        "QScrollArea { background: transparent; border: none; }"
        "QScrollBar:vertical { background: transparent; width: 8px; margin: 0px; }"
        "QScrollBar::handle:vertical { background: #313244; border-radius: 4px; min-height: 24px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }"
        );
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
    sep2->setStyleSheet(QString("background-color: %1; border: none; max-height: 1px;").arg(kBorder));
    layout->addWidget(sep2);

    settingsEntryButton = new AnimatedIconButton("⚙", kTextMuted, kAccent, panel);
    settingsEntryButton->setFixedSize(36, 36);
    settingsEntryButton->setToolTip("Настройки приложения");
    settingsEntryButton->setStyleSheet(iconButtonStyle(kAccent, "rgba(137, 180, 250, 0.15)", 20, 16));
    connect(settingsEntryButton, &QPushButton::clicked, this, &MainWindow::showSettings);
    layout->addWidget(settingsEntryButton, 0, Qt::AlignLeft);

    return panel;
}

QWidget *MainWindow::buildChatArea()
{
    auto *panel = new NetworkBackdrop(NetworkBackdrop::Surface::Window, this);

    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(20, 18, 20, 20);
    layout->setSpacing(12);

    auto *headerRow = new QHBoxLayout();
    headerRow->setSpacing(10);

    chatHeaderAvatar = new AgentAvatar(panel);
    chatHeaderAvatar->setFixedSize(32, 32);
    chatHeaderAvatar->setAgentName(m_mainAgentName);

    chatHeaderLabel = new QLabel(m_mainAgentName, panel);
    chatHeaderLabel->setStyleSheet(QString("color: %1; font-size: 16px; font-weight: 700; border: none; background: transparent;").arg(kTextMain));

    headerRow->addWidget(chatHeaderAvatar);
    headerRow->addWidget(chatHeaderLabel);
    headerRow->addStretch(1);
    layout->addLayout(headerRow);

    auto *chatBackdropContainer = new NetworkBackdrop(NetworkBackdrop::Surface::Card, panel);
    auto *chatBackdropLayout = new QVBoxLayout(chatBackdropContainer);
    chatBackdropLayout->setContentsMargins(0, 0, 0, 0);

    chatScroll = new QScrollArea(chatBackdropContainer);
    chatScroll->setWidgetResizable(true);
    chatScroll->setFrameShape(QFrame::NoFrame);
    chatScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    chatScroll->setStyleSheet(QString(
                                  "QScrollArea {"
                                  "   background: transparent;"
                                  "   border: 1px solid %1;"
                                  "   border-radius: 14px;"
                                  "}"
                                  "QScrollBar:vertical { background: transparent; width: 8px; margin: 0px; }"
                                  "QScrollBar::handle:vertical { background: %1; border-radius: 4px; min-height: 24px; }"
                                  "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }"
                                  ).arg(kBorder));
    chatScroll->viewport()->setStyleSheet("background: transparent;");

    chatBubblesHost = new QWidget(chatScroll);
    chatBubblesHost->setStyleSheet("background: transparent;");
    chatBubblesLayout = new QVBoxLayout(chatBubblesHost);
    chatBubblesLayout->setContentsMargins(16, 16, 16, 16);
    chatBubblesLayout->setSpacing(14);
    chatBubblesLayout->addStretch(1);

    chatScroll->setWidget(chatBubblesHost);
    chatBackdropLayout->addWidget(chatScroll);
    layout->addWidget(chatBackdropContainer, 1);

    thinkingChip = new QFrame(panel);
    thinkingChip->setStyleSheet(QString(
                                    "QFrame {"
                                    "   background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 %1, stop:1 %2);"
                                    "   border-radius: 12px;"
                                    "}"
                                    ).arg(kBgCard, kBgCardHover));
    applyElevation(thinkingChip, 18, 3, 55);

    auto *chipLayout = new QHBoxLayout(thinkingChip);
    chipLayout->setContentsMargins(12, 7, 14, 7);
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
    thinkingFade->setDuration(220);
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

    auto *inputFrame = new QFrame(panel);
    inputFrame->setStyleSheet(QString(
                                  "QFrame {"
                                  "   background-color: %1;"
                                  "   border: 1px solid %2;"
                                  "   border-radius: 14px;"
                                  "}"
                                  ).arg(kBgInput, kBorder));
    applyElevation(inputFrame, 20, 4, 45);
    auto *inputLayout = new QHBoxLayout(inputFrame);
    inputLayout->setContentsMargins(12, 8, 8, 8);
    inputLayout->setSpacing(8);

    messageInput = new QPlainTextEdit(inputFrame);
    messageInput->setFixedHeight(56);
    messageInput->setFrameShape(QFrame::NoFrame);
    messageInput->setStyleSheet(QString(
                                    "QPlainTextEdit {"
                                    "   background: transparent;"
                                    "   border: none;"
                                    "   color: %1;"
                                    "   font-size: 13px;"
                                    "}"
                                    ).arg(kTextMain));
    messageInput->installEventFilter(this);

    sendButton = new QPushButton("➤", inputFrame);
    sendButton->setFixedSize(40, 40);
    sendButton->setCursor(Qt::PointingHandCursor);
    sendButton->setToolTip("Отправить");
    sendButton->setStyleSheet(QString(
                                  "QPushButton {"
                                  "   background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 %1, stop:1 %2);"
                                  "   color: #ffffff;"
                                  "   border: none;"
                                  "   border-radius: 20px;"
                                  "   font-size: 15px;"
                                  "   font-weight: bold;"
                                  "}"
                                  "QPushButton:hover { background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 %2, stop:1 %1); }"
                                  "QPushButton:pressed { background: %2; }"
                                  ).arg(kAccent, kAccent2));
    applyElevation(sendButton, 16, 3, 70);
    connect(sendButton, &QPushButton::clicked, this, &MainWindow::handleSendClicked);

    inputLayout->addWidget(messageInput, 1);
    inputLayout->addWidget(sendButton, 0, Qt::AlignBottom);

    layout->addWidget(inputFrame);

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
    });

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
    anim->setDuration(240);
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
    if (watched == messageInput && event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if ((keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter)
            && !(keyEvent->modifiers() & Qt::ShiftModifier)) {
            handleSendClicked();
            return true;
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
        renderMessage(entry);
}

void MainWindow::renderMessage(const ChatEntry &entry)
{
    const QString author = entry.isUser
                               ? "Вы"
                               : (m_activeAgentId.isEmpty() ? m_mainAgentName : subagentName(m_activeAgentId));

    auto *bubble = new ChatBubble(author, entry.text, entry.time, entry.isUser, entry.code, chatBubblesHost);
    chatBubblesLayout->addWidget(bubble);
    bubble->playEntrance();

    QTimer::singleShot(0, this, [this]() {
        QScrollBar *bar = chatScroll->verticalScrollBar();
        bar->setValue(bar->maximum());
    });
}

void MainWindow::redrawActiveHistory()
{
    while (chatBubblesLayout->count() > 1) {
        QLayoutItem *item = chatBubblesLayout->takeAt(1);
        delete item->widget();
        delete item;
    }

    const auto entries = m_histories.value(m_activeAgentId);
    for (const auto &entry : entries)
        renderMessage(entry);
}

MainWindow::AgentSettingsResult MainWindow::runAgentSettingsDialog(const QString &title, const QString &backendAgentName, QString &name, QString &role, bool allowDelete)
{
    QDialog dialog(this);
    dialog.setWindowTitle(title);
    dialog.setMinimumWidth(560);
    dialog.setMinimumHeight(520);
    dialog.setStyleSheet(QString(
                             "QDialog { background-color: %1; }"
                             "QLabel { color: %2; font-size: 12px; border: none; background: transparent; }"
                             "QLineEdit, QTextEdit {"
                             "   background-color: %3;"
                             "   color: %2;"
                             "   border: 1px solid %4;"
                             "   border-radius: 8px;"
                             "   padding: 8px;"
                             "   font-size: 13px;"
                             "}"
                             "QLineEdit:focus, QTextEdit:focus { border-color: %5; }"
                             "QTabWidget::pane { border: 1px solid %4; border-radius: 8px; }"
                             "QTabBar::tab { background-color: %3; color: %2; padding: 6px 12px; }"
                             "QTabBar::tab:selected { color: %5; }"
                             "QPushButton {"
                             "   background-color: %3;"
                             "   color: %2;"
                             "   border: 1px solid %4;"
                             "   border-radius: 8px;"
                             "   padding: 6px 16px;"
                             "   font-size: 13px;"
                             "}"
                             "QPushButton:hover { border-color: %5; color: %5; }"
                             ).arg(kBgWindow, kTextMain, kBgCard, kBorder, kAccent));

    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(10);

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

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Save)->setText("Сохранить");
    buttons->button(QDialogButtonBox::Cancel)->setText("Отмена");
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (allowDelete) {
        auto *deleteButton = new QPushButton("Удалить агента", &dialog);
        deleteButton->setStyleSheet(QString(
                                        "QPushButton { color: %1; border: 1px solid %1; border-radius: 8px; padding: 6px 16px; background: transparent; }"
                                        "QPushButton:hover { background: rgba(243, 139, 168, 0.15); }"
                                        ).arg(kAccentRed));
        connect(deleteButton, &QPushButton::clicked, &dialog, [&dialog]() {
            if (QMessageBox::question(&dialog, "Удалить агента",
                                      "Удалить этого агента? Файлы агента будут удалены с диска.") == QMessageBox::Yes)
                dialog.done(2);
        });
        buttons->addButton(deleteButton, QDialogButtonBox::DestructiveRole);
    }

    layout->addWidget(nameLabel);
    layout->addWidget(nameEdit);
    layout->addWidget(roleLabel);
    layout->addWidget(roleEdit);
    layout->addWidget(promptLabel);
    layout->addWidget(promptTabs, 1);
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

    return AgentSettingsResult::Saved;
}

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
                             "QPushButton { border: 1px solid %3; border-radius: 8px; padding: 6px 16px; color: %2; background-color: %4; }"
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
            swatch->setStyleSheet(QString("background-color: %1; border: 1px solid rgba(128,128,128,0.4); border-radius: 8px;")
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
                             "   border-radius: 8px;"
                             "   padding: 8px;"
                             "   font-size: 13px;"
                             "}"
                             "QLineEdit:focus { border-color: %5; }"
                             "QCheckBox { color: %2; font-size: 12px; }"
                             "QPushButton {"
                             "   background-color: %3;"
                             "   color: %2;"
                             "   border: 1px solid %4;"
                             "   border-radius: 8px;"
                             "   padding: 6px 16px;"
                             "   font-size: 13px;"
                             "}"
                             "QPushButton:hover { border-color: %5; color: %5; }"
                             ).arg(kBgWindow, kTextMain, kBgCard, kBorder, kAccent));

    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(10);

    auto *tokenLabel = new QLabel("Telegram Bot Token", &dialog);
    auto *tokenEdit = new QLineEdit(settings.value("telegram_token").toString(), &dialog);
    tokenEdit->setPlaceholderText("123456789:AAExampleTokenValue");
    tokenEdit->setEchoMode(QLineEdit::Password);

    auto *geminiLabel = new QLabel("Gemini API Key", &dialog);
    auto *geminiEdit = new QLineEdit(settings.value("gemini_api_key").toString(), &dialog);
    geminiEdit->setPlaceholderText("AIzaExampleKeyValue");
    geminiEdit->setEchoMode(QLineEdit::Password);

    auto *showCheckbox = new QCheckBox("Показать значения", &dialog);
    connect(showCheckbox, &QCheckBox::toggled, &dialog, [tokenEdit, geminiEdit](bool checked) {
        tokenEdit->setEchoMode(checked ? QLineEdit::Normal : QLineEdit::Password);
        geminiEdit->setEchoMode(checked ? QLineEdit::Normal : QLineEdit::Password);
    });

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Save)->setText("Сохранить");
    buttons->button(QDialogButtonBox::Cancel)->setText("Отмена");
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    layout->addWidget(tokenLabel);
    layout->addWidget(tokenEdit);
    layout->addWidget(geminiLabel);
    layout->addWidget(geminiEdit);
    layout->addWidget(showCheckbox);
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted)
        return;

    const QString botToken = tokenEdit->text().trimmed();
    const QString geminiApiKey = geminiEdit->text().trimmed();

    settings.setValue("telegram_token", botToken);
    settings.setValue("gemini_api_key", geminiApiKey);

    emit appTelegramTokenChanged(botToken);
    emit appGeminiTokenChanged(geminiApiKey);
}

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

void MainWindow::handleSendClicked()
{
    const QString text = messageInput->toPlainText().trimmed();
    if (text.isEmpty())
        return;

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
    const int hours = secs / 3600;
    const int mins = (secs % 3600) / 60;
    const int seconds = secs % 60;
    if (uptimeLabel) {
        uptimeLabel->setText(QString("%1:%2:%3")
                                 .arg(hours, 2, 10, QChar('0'))
                                 .arg(mins, 2, 10, QChar('0'))
                                 .arg(seconds, 2, 10, QChar('0')));
    }
}

// ИСПРАВЛЕННАЯ ВЕРСИЯ: buildCodePanel с setFixedWidth
QWidget *MainWindow::buildCodePanel()
{
    auto *panel = new NetworkBackdrop(NetworkBackdrop::Surface::Panel, this);
    codePanelContainer = panel;
    panel->setFixedWidth(0);  // ← используем setFixedWidth вместо setMaximumWidth

    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(16, 16, 16, 16);
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
    copyBtn->setStyleSheet(iconButtonStyle(kAccent, "rgba(137, 180, 250, 0.15)"));
    connect(copyBtn, &QPushButton::clicked, this, [this]() {
        QGuiApplication::clipboard()->setText(codePanelBody->toPlainText());
    });

    auto *closeBtn = new AnimatedIconButton("✕", kTextMuted, kAccentRed, panel);
    closeBtn->setFixedSize(28, 28);
    closeBtn->setToolTip("Закрыть панель кода");
    closeBtn->setStyleSheet(iconButtonStyle(kAccentRed, "rgba(243, 139, 168, 0.15)"));
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
                                     "   background-color: %1;"
                                     "   color: %2;"
                                     "   border: 1px solid %3;"
                                     "   border-radius: 10px;"
                                     "   padding: 10px;"
                                     "   font-size: 12px;"
                                     "}"
                                     "QScrollBar:vertical { background: transparent; width: 8px; margin: 0px; }"
                                     "QScrollBar::handle:vertical { background: %3; border-radius: 4px; min-height: 24px; }"
                                     ).arg(kBgWindow, kTextMain, kBorder));
    layout->addWidget(codePanelBody, 1);

    // Мигающий курсор в конце "печатаемого" текста — усиливает эффект живой печати
    codeTypewriterCursorBlink = new QTimer(this);
    codeTypewriterCursorBlink->setInterval(430);
    connect(codeTypewriterCursorBlink, &QTimer::timeout, this, [this]() {
        m_typewriterCursorVisible = !m_typewriterCursorVisible;
        updateTypewriterCursorDisplay();
    });

    return panel;
}

// ИСПРАВЛЕННАЯ ВЕРСИЯ: showCodePanel — теперь код "печатается" быстро, как будто его набирает ИИ
void MainWindow::showCodePanel(const QString &code, const QString &title)
{
    codePanelTitle->setText(title.isEmpty() ? "Код" : title);

    // Если уже что-то печаталось — останавливаем и начинаем печатать новый код с нуля
    if (codeTypewriterTimer && codeTypewriterTimer->isActive())
        codeTypewriterTimer->stop();

    codePanelBody->clear();
    m_typewriterFullText = code;
    m_typewriterPos = 0;
    m_typewriterCursorVisible = true;

    if (!codeTypewriterTimer) {
        codeTypewriterTimer = new QTimer(this);
        connect(codeTypewriterTimer, &QTimer::timeout, this, &MainWindow::advanceCodeTypewriter);
    }

    const bool wasOpen = m_codePanelOpen;
    codePanelSeparator->setVisible(true);
    m_codePanelOpen = true;

    if (!wasOpen) {
        auto *anim = new QVariantAnimation(this);
        anim->setDuration(260);
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

    // Печатаем быстрыми "пачками" символов — эффект живого набора текста ИИ.
    // Интервал короткий (8 мс), поэтому даже большой код печатается за доли секунды,
    // но глаз успевает заметить анимацию.
    codeTypewriterTimer->start(8);
}

// Печатает очередную "пачку" символов кода в панель, имитируя быстрый набор текста ИИ
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

    // Случайный размер "пачки" символов за тик — придаёт печати живой, неравномерный ритм
    const int chunk = 2 + QRandomGenerator::global()->bounded(5); // 2..6 символов
    const int end = qMin(m_typewriterPos + chunk, m_typewriterFullText.size());

    QTextCursor cursor(codePanelBody->document());
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(m_typewriterFullText.mid(m_typewriterPos, end - m_typewriterPos));

    m_typewriterPos = end;

    QScrollBar *bar = codePanelBody->verticalScrollBar();
    bar->setValue(bar->maximum());
}

// Показывает/скрывает мигающий "курсор" в конце уже напечатанного текста
void MainWindow::updateTypewriterCursorDisplay()
{
    if (!codePanelBody) return;

    QTextCursor cursor(codePanelBody->document());
    cursor.movePosition(QTextCursor::End);

    // Убираем предыдущий символ курсора, если он был нарисован
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

// ИСПРАВЛЕННАЯ ВЕРСИЯ: hideCodePanel с QVariantAnimation
void MainWindow::hideCodePanel()
{
    if (!m_codePanelOpen) return;
    m_codePanelOpen = false;

    if (codeTypewriterTimer)
        codeTypewriterTimer->stop();
    if (codeTypewriterCursorBlink)
        codeTypewriterCursorBlink->stop();

    auto *anim = new QVariantAnimation(this);
    anim->setDuration(260);
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
