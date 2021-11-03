#ifndef PRESENTIMPEDANCESPLOT_H
#define PRESENTIMPEDANCESPLOT_H

#include <QWidget>

class QLabel;
class QPushButton;
class QPainter;

class PresentImpedancesPlot : public QWidget
{
    Q_OBJECT
public:
    explicit PresentImpedancesPlot(QWidget *parent = 0);
    void setImpedanceDisplayMag(bool mag);
    void changeTargetImpedance(double target);

private:
    void updateVerticalPixmap(bool mag);

    QPushButton* placeHolderButton;
    double targetImpedance;
    int metricsHeight;
    int metricsMagWidth;
    int metricsPhaseWidth;

    QPixmap *verticalLabelPixmap;
    QLabel *verticalImpedanceLabel;
    QPainter *painter;
    QLabel *title;
    QString *titleMag;
    QString *titlePhase;
    QString *impedanceMag;
    QString *impedancePhase;
    //QLabel *impedanceLabel;
    QLabel *channelLabel;
    QRect frame;
    QPixmap pixmap;

signals:

public slots:

};

#endif // PRESENTIMPEDANCESPLOT_H
