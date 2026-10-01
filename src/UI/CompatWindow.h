#pragma once

#include <QMainWindow>
#include <QCloseEvent>
#include <QTimer>
#include <memory>
#include "Interfaz/ISystemControl.h"

QT_BEGIN_NAMESPACE
namespace Ui { class CompatWindow; }
QT_END_NAMESPACE

class CompatWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit CompatWindow(std::shared_ptr<ISystemControl> systemControl, QWidget* parent = nullptr);
    ~CompatWindow() override;
    void showTemporary(int durationMs = 3000);

private slots:
    void onVolumeSliderChanged(int value);
    void onBtnPrevClicked();
    void onBtnPlayPauseClicked();
    void onBtnNextClicked();

    void updateMediaInfo(const MediaInfo& info);
    void updateVolume(int volume);

private:
    void positionTopLeft();
    Ui::CompatWindow* ui;
    std::shared_ptr<ISystemControl> m_systemControl;
    QTimer* m_hideTimer;

protected:
	void closeEvent(QCloseEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
};