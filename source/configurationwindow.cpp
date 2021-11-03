#include "configurationwindow.h"
#include "configurationparameters.h"
#include "electroplatingboardcontrol.h"
#include "significantround.h"

#include <QtGui>
#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
#include <QtWidgets>
#endif

#include <QDialog>


/* Constructor */
ConfigurationWindow::ConfigurationWindow(ConfigurationParameters* parameters, ElectroplatingBoardControl *ebcObj, QWidget *parent) :
    QDialog(parent)
{
    //Initialize params, ebc, and valueActual
    params = parameters;
    ebc = ebcObj;
    valueActual = 0;

    setWindowTitle("Configuration");
    QVBoxLayout *mainLayout = new QVBoxLayout;

    /* Set up "Electroplating Mode" label and combo box */
    QLabel *electroplatingModeLabel = new QLabel(tr("Electroplating Mode"));
    electroplatingModeComboBox = new QComboBox;
    electroplatingModeComboBox->addItem("Constant Current");
    electroplatingModeComboBox->addItem("Constant Voltage");

    //Connect "Electroplating Mode" combo box's currentIndexChanged signal to the electroplatingModeChanged slot
    connect(electroplatingModeComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(electroplatingModeChanged()));

    /* Set up "Current Parameters" group box (containing "Sign" label and combo box, "Desired Value" label, lineEdit, and units, and "Best Achievable Value" label and result */
    currentParametersGroupBox = new QGroupBox(tr("Current Parameters"));
    QHBoxLayout *currentParametersGroupBoxLayout = new QHBoxLayout;

    //Sign Column includes "Sign" label and combo box
    QVBoxLayout *signColumn = new QVBoxLayout;
    QLabel *signLabel = new QLabel("Sign");
    signComboBox = new QComboBox;
    signComboBox->addItem("-");
    signComboBox->addItem("+");
    signColumn->addWidget(signLabel);
    signColumn->addWidget(signComboBox);

    //Desired Value Column includes "Desired Value" label, line edit, and units label
    QVBoxLayout *desiredValueColumn = new QVBoxLayout;
    QLabel *desiredValueLabel = new QLabel("Desired Value");
    QHBoxLayout *desiredValueRow = new QHBoxLayout;
    desiredValue = new QLineEdit;
    desiredValueValidator = new QDoubleValidator(0, 10000, 6);
    desiredValue->setValidator(desiredValueValidator);
    desiredValueUnits = new QLabel("nA (max 10,000 nA)");
    desiredValueRow->addWidget(desiredValue);
    desiredValueRow->addWidget(desiredValueUnits);
    desiredValueColumn->addWidget(desiredValueLabel);
    desiredValueColumn->addLayout(desiredValueRow);

    //Connect desired value and sign combo box 'changed' signals to the valuesChanged slot
    connect(signComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(valuesChanged()));
    connect(desiredValue, SIGNAL(textChanged(QString)), this, SLOT(valuesChanged()));

    //Best Achievable Column includes "Best Achievable Value" label and value
    QVBoxLayout *bestAchievableValueColumn = new QVBoxLayout;
    QLabel *bestAchievableValueLabel = new QLabel("Best Achievable Value");
    bestAchievableValue = new QLabel("0 nA");
    bestAchievableValueColumn->addWidget(bestAchievableValueLabel);
    bestAchievableValueColumn->addWidget(bestAchievableValue);

    //Add the three columns to a horizontal layout, and set the group box's layout to that layout
    currentParametersGroupBoxLayout->addLayout(signColumn);
    currentParametersGroupBoxLayout->addLayout(desiredValueColumn);
    currentParametersGroupBoxLayout->addLayout(bestAchievableValueColumn);
    currentParametersGroupBox->setLayout(currentParametersGroupBoxLayout);

    /* Set up "Duration" label, combo box, and units */
    QLabel *durationLabel = new QLabel("Duration");
    QHBoxLayout *durationRow = new QHBoxLayout;

    //Give 'duration' line edit a validator from 0.1 to 100, with 6 decimals
    duration = new QLineEdit;
    durationValidator = new QDoubleValidator(0.1, 100, 6);
    duration->setValidator(durationValidator);
    durationUnits = new QLabel("second(s) (0.1 - 100)");
    durationRow->addWidget(duration);
    durationRow->addWidget(durationUnits);

    //Connect duration value 'changed' signal to the valuesChanged slot
    connect(duration, SIGNAL(textChanged(QString)), this, SLOT(valuesChanged()));

    //Set up "OK" and "cancel" buttons
    buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    //Add all of the set up widgets to the main layout of the window
    mainLayout->addWidget(electroplatingModeLabel);
    mainLayout->addWidget(electroplatingModeComboBox);
    mainLayout->addWidget(currentParametersGroupBox);
    mainLayout->addWidget(durationLabel);
    mainLayout->addLayout(durationRow);
    mainLayout->addWidget(buttonBox);

    //Initialize each of the widgets with the value from parameters
    electroplatingModeComboBox->setCurrentIndex(parameters->electroplatingMode);
    signComboBox->setCurrentIndex(parameters->sign);
    desiredValue->setText(QString::number(abs(parameters->desiredValue)));
    duration->setText(QString::number(parameters->duration));

    setLayout(mainLayout);
    exec();
}

/* Private - Display the best achievable value close to the desired value entered by the user */
void ConfigurationWindow::updateBestAchievableValue()
{
    //Get the new value
    double newValue = desiredValue->text().toDouble();

    //Limit it to the allowed values
    if ((ElectroplatingMode) electroplatingModeComboBox->currentIndex() == ConstantVoltage) {
        //For voltage, 0 ... 3.3V
        if (newValue > 3.3)
            newValue = 3.3;
        if (newValue < 0)
            newValue = 0;
    }
    else {
        //For current, -10uA ... +10uA, i.e., -10,000 nA ... +10,000 nA
        if (newValue > 10000)
            newValue = 10000;
        if (newValue < -10000)
            newValue = -10000;
    }

    //Store the magnitude (multiplied by the sign) into signedValue;
    double signedValue = (signComboBox->currentIndex() == Negative) ? newValue * -1 : newValue;
    QString valueActualString;

    //Get actual possible value from ebc logic (voltage or current)
    if ((ElectroplatingMode) electroplatingModeComboBox->currentIndex() == ConstantVoltage) {
        //From ebc, set Voltage to signedValue
        ebc->setVoltage(signedValue);
        //Then, retrieve VoltageActual from ebc
        valueActual = ebc->getVoltageActual();
        valueActual = roundf(valueActual * 100) / 100;
        valueActualString = (QString::number(valueActual, 'g', 3));
        valueActualString.append(" V");
        bestAchievableValue->setText(valueActualString);
    }
    else {
        //From ebc, set current signedValue
        ebc->setCurrent(signedValue * 1e-9);
        //Then, retrieve CurrentActual from ebc (possibly do scaling)
        valueActual = ebc->getCurrentActual();
        valueActual = significantRound(valueActual / 1e-9);
        if (abs(valueActual) >= 1000)
            valueActualString = QString::number((int) valueActual);
        else {
            valueActualString = (QString::number(valueActual, 'g', 3));
        }
        valueActualString.append(" nA");
        bestAchievableValue->setText(valueActualString);
    }
}

/* Private - Slot that is called when the desired value, sign, or duration is changed. Provides feedback to the user if the values are not valid */
void ConfigurationWindow::valuesChanged()
{
    int unusedPos = 1; //Throw-away variable just used to be able to call 'validate()'

    QString desiredText = desiredValue->text();
    QValidator::State desired = desiredValueValidator->validate(desiredText, unusedPos);

    QString durText = duration->text();
    QValidator::State dur = durationValidator->validate(durText, unusedPos);

    if ((desired == QDoubleValidator::Acceptable) &&
            dur == QDoubleValidator::Acceptable)
        buttonBox->button(QDialogButtonBox::Ok)->setEnabled(true);
    else
        buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);

    if (desired != QDoubleValidator::Acceptable)
        desiredValueUnits->setStyleSheet("color: red;");
    else
        desiredValueUnits->setStyleSheet("color: black;");

    updateBestAchievableValue();

    if (dur != QDoubleValidator::Acceptable)
        durationUnits->setStyleSheet("color: red;");
    else
        durationUnits->setStyleSheet("color: black;");
}

/* Private - Slot that is called when the desired mode (current or voltage) is changed. Updates units and value range based on voltage or current */
void ConfigurationWindow::electroplatingModeChanged()
{
    if (electroplatingModeComboBox->currentIndex() == ConstantCurrent) {
        currentParametersGroupBox->setTitle("Current Parameters");
        desiredValueValidator->setRange(0, 10000);
        desiredValueUnits->setText("nA (max 10,000 nA)");
        bestAchievableValue->setText("0 nA");
    }
    else {
        currentParametersGroupBox->setTitle("Voltage Parameters");
        desiredValueValidator->setRange(0, 3.3, 4);
        desiredValueUnits->setText("Volts (max 3.3 V)");
        bestAchievableValue->setText("0 V");
    }
    valuesChanged();
}

/* Private - Reimplemented slot that is called when the user accepts the dialog. Passes current user inputs into 'params' structure, so they are accessible by the parent */
void ConfigurationWindow::accept()
{
    params->electroplatingMode = (ElectroplatingMode) electroplatingModeComboBox->currentIndex();
    params->sign = (Sign) signComboBox->currentIndex();
    params->desiredValue = desiredValue->text().toFloat();
    params->actualValue = valueActual;
    params->duration = roundf(duration->text().toFloat() * 10000) / 10000;

    done(Accepted);
}

/* Private - Reimplemented slot that is called when the user rejects the dialog */
void ConfigurationWindow::reject()
{
    done(Rejected);
}
