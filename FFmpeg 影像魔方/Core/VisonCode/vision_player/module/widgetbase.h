
#ifndef _WIDGET_BASE_H_
#define _WIDGET_BASE_H_

#include <QWidget>
class VWidgetBase : public QWidget
{
    Q_OBJECT
public:
    explicit VWidgetBase(QWidget *parent = NULL);
    ~VWidgetBase();

    void setTitleBar(QWidget *titleBar);

signals:
    void windowStateChanged(Qt::WindowStates windowStates);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    bool nativeEvent(const QByteArray &eventType, void *message, long *result) override;

private:
    void InitBase();
    bool winMouseEvent(MSG *msg, long *result);
    void thisEvent(QObject *watched, QEvent *event);      // 当前窗口事件处理
    //void titleBarEvent(QObject *watched, QEvent *event);  // 标题栏事件处理

private:
    QWidget* m_titleBar;
};

#endif //_WIDGET_BASE_H_
