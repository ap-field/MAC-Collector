#ifndef COLLECTORWINDOW_H
#define COLLECTORWINDOW_H

#include <QMainWindow>

class CollectorWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit CollectorWindow(QWidget *parent = nullptr);
    ~CollectorWindow() override;
};
#endif // COLLECTORWINDOW_H
