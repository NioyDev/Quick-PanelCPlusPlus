#pragma once

#include "Interfaz/ISystemDisplayBrightness.h"

#include <QWidget>
#include <memory>

class QTimer;

QT_BEGIN_NAMESPACE
namespace Ui { class QuickBrightnessWindow; }
QT_END_NAMESPACE

// Popup flotante (esquina inferior derecha) con un slider para el brillo.
// Se cierra con Escape o al hacer clic fuera (comportamiento de Qt::Popup).
// El brillo se aplica con un pequeño debounce para no saturar al sistema
// mientras se arrastra el slider.
class QuickBrightnessWindow : public QWidget {
    Q_OBJECT

public:
    explicit QuickBrightnessWindow(std::shared_ptr<ISystemDisplayBrightness> brightness,
        QWidget* parent = nullptr);
    ~QuickBrightnessWindow() override;

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private slots:
    void onSliderValueChanged(int value);
    void applyPendingBrightness();

private:
    void setupIcon();
    void loadCurrentBrightness();
    void moveToBottomRight();

    static int snapToStep(int value);

    Ui::QuickBrightnessWindow* ui = nullptr;
    std::shared_ptr<ISystemDisplayBrightness> m_brightness;
    QTimer* m_applyTimer = nullptr;
    int m_pendingValue = -1;  // último valor pedido por el slider
    int m_lastApplied = -1;   // último valor enviado al sistema
};