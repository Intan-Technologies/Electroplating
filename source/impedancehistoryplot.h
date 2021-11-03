#ifndef IMPEDANCEHISTORYPLOT_H
#define IMPEDANCEHISTORYPLOT_H

#include <QWidget>

class QLabel;
class QPushButton;

class ImpedanceHistoryPlot : public QWidget
{
    Q_OBJECT
public:
    explicit ImpedanceHistoryPlot(QWidget *parent = 0);
    void changeTargetImpedance(double target);

private:
    QPushButton* placeHolderButton;
    double targetImpedance;

    QLabel *title;
    //QLabel *impedanceLabel;
    QLabel *timeLabel;
    QRect frame;
    QPixmap pixmap;

signals:

public slots:

};

#endif // IMPEDANCEHISTORYPLOT_H
