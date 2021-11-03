#include "presentimpedancesplot.h"
#include <QtGui>

PresentImpedancesPlot::PresentImpedancesPlot(QWidget *parent) :
    QWidget(parent)
{
    titleMag = new QString("Present Impedance Magnitudes (0 of 0 below threshold)");
    titlePhase = new QString("Current Impedance Phases");
    impedanceMag = new QString("Impedance (Ohms)");
    impedancePhase = new QString("Phase (degrees)");

    title = new QLabel(*titleMag);
    channelLabel = new QLabel("Channel");


    QFontMetrics metrics = this->fontMetrics();
    metricsHeight = metrics.height();
    metricsMagWidth = metrics.width(*impedanceMag);
    metricsPhaseWidth = metrics.width(*impedancePhase);

    verticalLabelPixmap = new QPixmap(metricsHeight * 2, metricsMagWidth * 2);
    verticalLabelPixmap->fill(Qt::white);
    //verticalLabelPixmap.fill(parent->palette().color(QWidget::backgroundRole()));
    painter = new QPainter(verticalLabelPixmap);
    painter->rotate(270);
    painter->drawText(QPoint(-1.5 * metricsMagWidth, 1.5 * metricsHeight), *impedanceMag);

    verticalImpedanceLabel = new QLabel;
    verticalImpedanceLabel->setPixmap(*verticalLabelPixmap);

    QVBoxLayout *labelColumn = new QVBoxLayout;
    labelColumn->addWidget(verticalImpedanceLabel);

    placeHolderButton = new QPushButton(tr("Present Impedances: Magnitude"));
    QVBoxLayout *graphColumn = new QVBoxLayout;
    graphColumn->addWidget(title);
    graphColumn->setAlignment(title, Qt::AlignHCenter);
    graphColumn->addWidget(placeHolderButton);
    graphColumn->setAlignment(placeHolderButton, Qt::AlignHCenter);
    graphColumn->addWidget(channelLabel);
    graphColumn->setAlignment(channelLabel, Qt::AlignHCenter);

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

void PresentImpedancesPlot::setImpedanceDisplayMag(bool mag)
{
    if (mag) {
        title->setText(*titleMag);
        placeHolderButton->setText("Present Impedances: Magnitude");
    }
    else {
        title->setText(*titlePhase);

        placeHolderButton->setText("Present Impedances: Phase");
    }
    updateVerticalPixmap(mag);
}

void PresentImpedancesPlot::updateVerticalPixmap(bool mag)
{
    verticalLabelPixmap->fill(Qt::white);
    if (mag)
        painter->drawText(QPoint(-1.5 * metricsMagWidth, 1.5 * metricsHeight), *impedanceMag);
    else
        painter->drawText(QPoint(-1.5 * metricsPhaseWidth, 1.5 * metricsHeight), *impedancePhase);
    verticalImpedanceLabel->setPixmap(*verticalLabelPixmap);
}

void PresentImpedancesPlot::changeTargetImpedance(double target)
{
    targetImpedance = target;
}
