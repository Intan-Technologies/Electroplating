#include "globalconfigurationwindow.h"
#include "globalparameters.h"

#include <QtGui>
#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
#include <QtWidgets>
#endif

#include <QDialog>

GlobalConfigurationWindow::GlobalConfigurationWindow(GlobalParameters *parameters, QWidget *parent) :
    QDialog(parent)
{
    //Initialize params
    params = parameters;

    setWindowTitle("Global Settings");
    QVBoxLayout *mainLayout = new QVBoxLayout;

    /* Set up "Pulses" group box (containing "maxPulses" label and line edit) */
    QGroupBox *pulsesGroupBox = new QGroupBox(tr("Pulses"));
    QHBoxLayout *pulsesGroupBoxLayout = new QHBoxLayout;

    //Create "Max Pulses" label and line edit
    QLabel *maxPulsesLabel = new QLabel(tr("Maximum number of pulses for automated plating"));
    maxPulses = new QLineEdit;

    //Add "Max Pulses" label and line edits to the group box
    pulsesGroupBoxLayout->addWidget(maxPulsesLabel);
    pulsesGroupBoxLayout->addWidget(maxPulses);
    pulsesGroupBox->setLayout(pulsesGroupBoxLayout);

    /* Set up "Delays" group box (containing four "Delay" rows of labels and line edits) */
    QGroupBox *delaysGroupBox = new QGroupBox(tr("Delays"));
    QVBoxLayout *delaysGroupBoxLayout = new QVBoxLayout;

    //Create row for 1st delay - delay between measurement and pulse
    QHBoxLayout *delayMeasurementPulseRow = new QHBoxLayout;
    QLabel *delayMeasurementPulseLabel = new QLabel(tr("Delay between measurement and pulse (in seconds)"));
    delayMeasurementPulse = new QLineEdit;
    delayMeasurementPulseRow->addWidget(delayMeasurementPulseLabel);
    delayMeasurementPulseRow->addWidget(delayMeasurementPulse);
    delaysGroupBoxLayout->addLayout(delayMeasurementPulseRow);

    //Create row for 2nd delay - delay between pulse and measurement
    QHBoxLayout *delayPulseMeasurementRow = new QHBoxLayout;
    QLabel *delayPulseMeasurementLabel = new QLabel(tr("Delay between pulse and measurement (in seconds)"));
    delayPulseMeasurement = new QLineEdit;
    delayPulseMeasurementRow->addWidget(delayPulseMeasurementLabel);
    delayPulseMeasurementRow->addWidget(delayPulseMeasurement);
    delaysGroupBoxLayout->addLayout(delayPulseMeasurementRow);

    //Create row for 3rd delay - delay when changing reference voltage (in seconds)
    QHBoxLayout *delayChangeRefRow = new QHBoxLayout;
    QLabel *delayChangeRefLabel = new QLabel(tr("Delay when changing reference voltage (in seconds)"));
    delayChangeRef = new QLineEdit;
    delayChangeRefRow->addWidget(delayChangeRefLabel);
    delayChangeRefRow->addWidget(delayChangeRef);
    delaysGroupBoxLayout->addLayout(delayChangeRefRow);

    //Create row for 4th delay - continuous z-scan delay
    QHBoxLayout *continuousZDelayRow = new QHBoxLayout;
    QLabel *continuousZDelayLabel = new QLabel(tr("Continuous z-scan delay (in seconds)"));
    continuousZDelay = new QLineEdit;
    continuousZDelayRow->addWidget(continuousZDelayLabel);
    continuousZDelayRow->addWidget(continuousZDelay);
    delaysGroupBoxLayout->addLayout(continuousZDelayRow);

    //Add 4 rows to the "Delays" group box
    delaysGroupBox->setLayout(delaysGroupBoxLayout);

    /* Set up "Electrodes" group box (containing channels "0-63" and "64-127" check boxes */
    QGroupBox *electrodesGroupBox = new QGroupBox(tr("Electrodes"));
    QVBoxLayout *electrodesGroupBoxLayout = new QVBoxLayout;

    //Create two checkbox buttons representing chanenls 0-63 and 64-127
    channels063Present = new QCheckBox(tr("Channels 0-63 are present"));
    channels64127Present = new QCheckBox(tr("Channels 64-127 are present"));

    //Add two checkbox buttons to "Electrodes" group box
    electrodesGroupBoxLayout->addWidget(channels063Present);
    electrodesGroupBoxLayout->addWidget(channels64127Present);
    electrodesGroupBox->setLayout(electrodesGroupBoxLayout);

    /* Set up "Target Impedance" group box (containing "useTargetZ" and "noTargetZ" radio buttons) */
    QGroupBox *targetImpedanceGroupBox = new QGroupBox(tr("Target Impedance"));
    QVBoxLayout *targetImpedanceGroupBoxLayout = new QVBoxLayout;

    //Create two radio buttons representing whether or not target impedance should be used
    useTargetZ = new QRadioButton(tr("Stop plating when target impedance is reached"));
    noTargetZ = new QRadioButton(tr("Plate without using target impedance"));

    //Add two radio buttons to "Target Impedance" group box
    targetImpedanceGroupBoxLayout->addWidget(useTargetZ);
    targetImpedanceGroupBoxLayout->addWidget(noTargetZ);
    targetImpedanceGroupBox->setLayout(targetImpedanceGroupBoxLayout);

    /* Set up "OK" and "cancel" buttons */
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    /* Add all of these boxes to the main layout of the window */
    mainLayout->addWidget(pulsesGroupBox);
    mainLayout->addWidget(delaysGroupBox);
    mainLayout->addWidget(electrodesGroupBox);
    mainLayout->addWidget(targetImpedanceGroupBox);
    mainLayout->addWidget(buttonBox);

    //Initialize each of the widgets with the value from parameters
    maxPulses->setText(QString::number(parameters->maxPulses));
    delayMeasurementPulse->setText(QString::number(parameters->delayMeasurementPulse));
    delayPulseMeasurement->setText(QString::number(parameters->delayPulseMeasurement));
    delayChangeRef->setText(QString::number(parameters->delayChangeRef));
    continuousZDelay->setText(QString::number(parameters->continuousZDelay));
    channels063Present->setChecked(parameters->channels063Present);
    channels64127Present->setChecked(parameters->channels64127Present);
    if (parameters->useTargetZ == true) {
        useTargetZ->setChecked(true);
        noTargetZ->setChecked(false);
    }
    else {
        useTargetZ->setChecked(false);
        noTargetZ->setChecked(true);
    }

    setLayout(mainLayout);
    exec();
}


/* Public - Reimplemented slot that is called when the user accepts the dialog. Passes current user inputs into 'params' structure, so they are accessible by the parent */
void GlobalConfigurationWindow::accept() {
    params->maxPulses = maxPulses->text().toFloat();
    params->delayMeasurementPulse = delayMeasurementPulse->text().toFloat();
    params->delayPulseMeasurement = delayPulseMeasurement->text().toFloat();
    params->delayChangeRef = delayChangeRef->text().toFloat();
    params->continuousZDelay = continuousZDelay->text().toFloat();
    params->channels063Present = channels063Present->isChecked();
    params->channels64127Present = channels64127Present->isChecked();
    params->useTargetZ = useTargetZ->isChecked();

    done(Accepted);
}


/* Public - Reimplemented slot that is called when the user rejects the dialog */
void GlobalConfigurationWindow::reject() {
    done(Rejected);
}
