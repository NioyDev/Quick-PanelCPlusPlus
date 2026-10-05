#pragma once
#include <QAbstractButton>
#include <QPropertyAnimation>

class ToggleSwitch : public QAbstractButton {
    Q_OBJECT
        Q_PROPERTY(qreal offset READ offset WRITE setOffset)

public:
    explicit ToggleSwitch(QWidget* parent = nullptr);
    QSize sizeHint() const override { return QSize(40, 22); }

    qreal offset() const { return m_offset; }
    void setOffset(qreal value) { m_offset = value; update(); }

protected:
    void paintEvent(QPaintEvent*) override;

private:
    void animateTo(bool checked);

    qreal m_offset = 0.0;   // 0 = apagado, 1 = encendido
    QPropertyAnimation m_anim;
};