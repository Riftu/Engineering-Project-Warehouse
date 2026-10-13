#ifndef CONTROLBAR_H
#define CONTROLBAR_H

#include <QVariant>
#include <QWidget>

namespace Ui {
class ControlBar;
}

class ControlBar : public QWidget
{
    Q_OBJECT

public:
    explicit ControlBar(QWidget *parent = nullptr);
    ~ControlBar();

    void SetPlayTotalTime(uint32_t total);
    void SetPlayTime(uint32_t current);
    void ControlBarReset();

signals:
    void ControlBarSignal(int iType, QVariant data);

private slots:
    void on_bt_play_clicked();
    void on_bt_setup_clicked();
    void on_bt_next_clicked();
    void on_bt_previous_clicked();
    void on_bt_volume_clicked();
    void on_volumeSlider_valueChanged(int value);
    void on_play_process_sliderPressed();
    void on_play_process_sliderReleased();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    Ui::ControlBar *ui;
    uint32_t m_Total;
    bool m_bPress;
    int m_iCount;
    uint32_t m_iStart;
    uint32_t m_iEnd;
};

#endif // CONTROLBAR_H
