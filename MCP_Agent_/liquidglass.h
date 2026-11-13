#pragma once

#include <QColor>
#include <QImage>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QRectF>
#include <QString>
#include <QtGlobal>

namespace Glass {

struct Tokens {
    bool   dark = true;
    QColor bg         = QColor("#11111b");
    QColor accent     = QColor("#89b4fa");
    QColor tint       = QColor(255, 255, 255, 20);
    QColor tintStrong = QColor(255, 255, 255, 40);
    QColor stroke     = QColor(255, 255, 255, 34);
    QColor hover      = QColor(255, 255, 255, 20);
    QColor selected   = QColor(255, 255, 255, 40);
};

namespace detail {
inline Tokens &mutableTokens()
{
    static Tokens t;
    return t;
}

// Континуальный угол (squircle): плавнее обычной дуги, как у иконок и панелей iOS
inline QPainterPath squircle(const QRectF &r, qreal radius)
{
    const qreal rr = qMin(radius, qMin(r.width(), r.height()) / 2.0);
    const qreal e  = qMin(rr * 1.32, qMin(r.width(), r.height()) / 2.0);
    const qreal k  = e * 0.30;

    const qreal L = r.left(), T = r.top(), R = r.right(), B = r.bottom();

    QPainterPath p;
    p.moveTo(L + e, T);
    p.lineTo(R - e, T);
    p.cubicTo(R - k, T, R, T + k, R, T + e);
    p.lineTo(R, B - e);
    p.cubicTo(R, B - k, R - k, B, R - e, B);
    p.lineTo(L + e, B);
    p.cubicTo(L + k, B, L, B - k, L, B - e);
    p.lineTo(L, T + e);
    p.cubicTo(L, T + k, L + k, T, L + e, T);
    p.closeSubpath();
    return p;
}
} // namespace detail

inline const Tokens &tokens() { return detail::mutableTokens(); }

// Форма стекла, чтобы обрезка и обводки в коде совпадали с отрисовкой
inline QPainterPath shape(const QRectF &r, qreal radius) { return detail::squircle(r, radius); }

inline QColor withAlpha(const QColor &c, int alpha)
{
    QColor out(c);
    out.setAlpha(qBound(0, alpha, 255));
    return out;
}

inline QString css(const QColor &c)
{
    return QString("rgba(%1,%2,%3,%4)")
    .arg(c.red()).arg(c.green()).arg(c.blue()).arg(c.alpha());
}

inline void update(const QColor &bg, const QColor &accent)
{
    Tokens &t = detail::mutableTokens();
    t.bg = bg;
    t.accent = accent;
    t.dark = bg.lightness() < 128;

    if (t.dark) {
        t.tint       = QColor(255, 255, 255, 16);
        t.tintStrong = QColor(255, 255, 255, 34);
        t.stroke     = QColor(255, 255, 255, 30);
        t.hover      = QColor(255, 255, 255, 18);
        t.selected   = QColor(255, 255, 255, 38);
    } else {
        t.tint       = QColor(255, 255, 255, 120);
        t.tintStrong = QColor(255, 255, 255, 190);
        t.stroke     = QColor(0, 0, 0, 22);
        t.hover      = QColor(255, 255, 255, 150);
        t.selected   = QColor(255, 255, 255, 210);
    }
}

// r — область стекла, radius — скругление, strength — плотность (0.5 лёгкое … 1 полное),
// backdrop/src — уже размытый фон и нужный его кусок.
inline void paint(QPainter &p, const QRectF &r, qreal radius, qreal strength,
                  const QImage &backdrop, const QRectF &src)
{
    if (r.width() < 2 || r.height() < 2)
        return;

    const Tokens &t = tokens();
    const qreal s = qBound(0.0, strength, 1.0);
    const bool dark = t.dark;

    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const QPainterPath path = detail::squircle(r, radius);

    // 1. Рассеянная тень
    if (s >= 0.75) {
        p.setPen(Qt::NoPen);
        for (int i = 6; i >= 1; --i) {
            const qreal grow = i * 1.1;
            p.setBrush(QColor(0, 0, 0, dark ? 7 : 4));
            p.drawPath(detail::squircle(r.adjusted(-grow, -grow + 2.0, grow, grow + 2.0), radius + grow));
        }
    }

    p.setClipPath(path);

    // 2. Размытый фон
    if (!backdrop.isNull() && src.width() > 0 && src.height() > 0)
        p.drawImage(r, backdrop, src);
    else
        p.fillPath(path, t.bg);

    // 3. Материал
    p.fillPath(path, dark ? QColor(18, 20, 30, int(70 * s)) : QColor(255, 255, 255, int(40 * s)));
    p.fillPath(path, withAlpha(t.tint, int(t.tint.alpha() * (0.5 + 0.5 * s))));

    // 4. Мягкий свет сверху
    QLinearGradient top(r.topLeft(), QPointF(r.left(), r.bottom()));
    top.setColorAt(0.0,  QColor(255, 255, 255, int((dark ? 16 : 55) * s)));
    top.setColorAt(0.45, QColor(255, 255, 255, 0));
    top.setColorAt(1.0,  QColor(255, 255, 255, int((dark ? 5 : 18) * s)));
    p.fillPath(path, top);

    // 5. Внутренний свет по краю
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(255, 255, 255, int((dark ? 16 : 70) * s)), 4.0));
    p.drawPath(path);

    p.setClipping(false);

    // 6. Зеркальная кромка
    QLinearGradient rim(r.topLeft(), r.bottomRight());
    rim.setColorAt(0.00, QColor(255, 255, 255, int((dark ? 150 : 235) * s)));
    rim.setColorAt(0.22, QColor(255, 255, 255, int((dark ? 40  : 110) * s)));
    rim.setColorAt(0.50, dark ? QColor(255, 255, 255, 14) : QColor(0, 0, 0, 14));
    rim.setColorAt(0.78, QColor(255, 255, 255, int((dark ? 36  : 100) * s)));
    rim.setColorAt(1.00, QColor(255, 255, 255, int((dark ? 110 : 200) * s)));

    p.setPen(QPen(QBrush(rim), 1.0));
    p.drawPath(detail::squircle(r.adjusted(0.5, 0.5, -0.5, -0.5), qMax<qreal>(0.0, radius - 0.5)));

    p.restore();
}

} // namespace Glass
