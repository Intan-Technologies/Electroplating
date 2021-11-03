#include "impedancehistoryplot.h"
#include <QtGui>

ImpedanceHistoryPlot::ImpedanceHistoryPlot(QWidget *parent) :
    QWidget(parent)
{
    title = new QLabel("Impedance History (Channel 0)");
    timeLabel = new QLabel("Time (seconds)");

    QString impedanceString("Impedance (Ohms)");

    QFontMetrics metrics = this->fontMetrics();
    int metricsHeight = metrics.height();
    int metricsWidth = metrics.width(impedanceString);

    QPixmap verticalLabelPixmap(metricsHeight * 2, metricsWidth * 2);
    verticalLabelPixmap.fill(Qt::white);
    //verticalLabelPixmap.fill(parent->palette().color(QWidget::backgroundRole()));
    QPainter painter(&verticalLabelPixmap);
    painter.rotate(270);
    painter.drawText(QPoint(-1.5 * metricsWidth, 1.5 * metricsHeight), impedanceString);

    QLabel *verticalImpedanceLabel = new QLabel;
    verticalImpedanceLabel->setPixmap(verticalLabelPixmap);

    QVBoxLayout *labelColumn = new QVBoxLayout;
    labelColumn->addWidget(verticalImpedanceLabel);

    placeHolderButton = new QPushButton(tr("Impedance History"));
    QVBoxLayout *graphColumn = new QVBoxLayout;
    graphColumn->addWidget(title);
    graphColumn->setAlignment(title, Qt::AlignHCenter);
    graphColumn->addWidget(placeHolderButton);
    graphColumn->setAlignment(placeHolderButton, Qt::AlignHCenter);
    graphColumn->addWidget(timeLabel);
    graphColumn->setAlignment(timeLabel, Qt::AlignHCenter);

    QHBoxLayout *mainLayout = new QHBoxLayout;

    //
    QGroupBox *firstBox = new QGroupBox;
    firstBox->setLayout(labelColumn);

    QGroupBox *secondBox = new QGroupBox;
    secondBox->setLayout(graphColumn);
    //

    mainLayout->addWidget(firstBox, 1);
    mainLayout->addWidget(secondBox, 30);
    setLayout(mainLayout);
}

void ImpedanceHistoryPlot::changeTargetImpedance(double target)
{
    targetImpedance = target;
}
