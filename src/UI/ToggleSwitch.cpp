#include "ToggleSwitch.h"
#include <QPainter>

ToggleSwitch::ToggleSwitch(QWidget* parent)
    : QAbstractButton(parent), m_anim(this, "offset", this) {
    setCheckable(true);
    setFixedSize(40, 22);
    setCursor(Qt::PointingHandCursor);
    m_anim.setDuration(140);
    m_anim.setEasingCurve(QEasingCurve::InOutQuad);
    connect(this, &QAbstractButton::toggled, this, &ToggleSwitch::animateTo);
}

void ToggleSwitch::animateTo(bool checked) {
    const qreal end = checked ? 1.0 : 0.0;
    m_anim.stop();
    if (!isVisible()) {          // sin animación si aún no se ve (carga inicial)
        setOffset(end);
        return;
    }
    m_anim.setStartValue(m_offset);
    m_anim.setEndValue(end);
    m_anim.start();
}

void ToggleSwitch::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    if (!isEnabled())
        p.setOpacity(0.5);

    const QColor off(0x3f, 0x3f, 0x46), on(0x22, 0xc5, 0x5e);
    auto mix = [&](int a, int b) { return int(a + (b - a) * m_offset); };
    const QColor track(mix(off.red(), on.red()), mix(off.green(), on.green()), mix(off.blue(), on.blue()));

    p.setPen(Qt::NoPen);
    p.setBrush(track);
    p.drawRoundedRect(rect(), height() / 2.0, height() / 2.0);

    const int margin = 3;
    const int d = height() - margin * 2;
    const qreal x = margin + (width() - d - margin * 2) * m_offset;
    p.setBrush(Qt::white);
    p.drawEllipse(QRectF(x, margin, d, d));
}