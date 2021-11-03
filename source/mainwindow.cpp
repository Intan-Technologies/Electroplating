#include "mainwindow.h"
#include "impedancehistoryplot.h"
#include "presentimpedancesplot.h"
#include "configurationwindow.h"
#include "configurationparameters.h"
#include "globalconfigurationwindow.h"
#include "globalparameters.h"
#include "globalconstants.h"
#include "rhd2000evalboard.h"
#include "rhd2000datablock.h"
#include "rhd2000registers.h"
#include "rhd2000config.h"
#include "settings.h"
#include "signalprocessor.h"
#include "dataprocessor.h"
#include "complex.h"
#include "oneelectrode.h"
#include "boardcontrol.h"
#include "impedancemeasurecontroller.h"
#include "electroplatingboardcontrol.h"
#include "significantround.h"
#include "impedanceplot.h"

#if defined WIN32
    #include <windows.h>
#else
    #include <unistd.h>
#endif

#include <QtGui>
#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
#include <QtWidgets>
#endif

#include <QMenuBar>
#include <string>
#include <cmath>
#include <QProgressDialog>

using namespace std;

class QtProgressWrapper : public ProgressWrapper {
public:
    QtProgressWrapper(QProgressDialog& progress_) : dialog(progress_) {}

    virtual void setMaximum(int maximum) override;
    virtual void setValue(int progress) override;
    virtual int value() const override;
    virtual int maximum() const override;
    virtual bool wasCanceled() const override;

private:
    QProgressDialog& dialog;
};

//  ------------------------------------------------------------------------
void QtProgressWrapper::setMaximum(int maximum) {
    dialog.setMaximum(maximum);
}

void QtProgressWrapper::setValue(int progress) {
    dialog.setValue(progress);
}

int QtProgressWrapper::value() const {
    return dialog.value();
}

int QtProgressWrapper::maximum() const {
    return dialog.maximum();
}

bool QtProgressWrapper::wasCanceled() const {
    return dialog.wasCanceled();
}




/* Constructor */
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    /* Connect to and initialize board */

    //Set up board control variables
    ebc = new ElectroplatingBoardControl();
    boardControl = new BoardControl();
    boardControl->create();
    connected = false;
    signalSources = new SignalSources;

    //Connect to board - if unsuccessful, delete variables and exit
    connectToBoard();
    if (!connected) {
        delete ebc;
        delete boardControl;
        exit(EXIT_FAILURE);
    }

    //Initialize the board
    boardControl->evalBoard->initialize();
    boardControl->evalBoard->setDataSource(0, Rhd2000EvalBoard::PortA1);
    boardControl->configure16DigitalOutputs();
    boardControl->evalBoardMode = boardControl->evalBoard->getBoardMode();
    boardControl->evalBoard->enableDac(0, true);
    boardControl->evalBoard->setDacManual(0);
    boardControl->evalBoard->selectDacDataStream(0, 8);
    boardControl->analogOutputs.dacs[0].enabled = true;
    boardControl->analogOutputs.dacs[0].channel = 31;
    boardControl->analogOutputs.dacs[0].dataStream = 8;
    ebc->setVoltage(0);
    boardControl->analogOutputs.setDacManualVolts(0);
    boardControl->updateAnalogOutputSource(0);
    boardControl->updateDACManual();

    //Select a sampling rate
    changeSampleRate(Rhd2000EvalBoard::SampleRate20000Hz);

    //Set up filter parameters
    signalProcessor = new SignalProcessor();
    notchFilterFrequency = 60.0;
    notchFilterBandwidth = 10.0;
    notchFilterEnabled = false;
    signalProcessor->setNotchFilterEnabled(notchFilterEnabled);
    highpassFilterFrequency = 250.0;
    highpassFilterEnabled = false;
    signalProcessor->setHighpassFilterEnabled(highpassFilterEnabled);

    //Update the board's bandwidth and sample rate
    boardControl->updateBandwidth();
    boardControl->changeSampleRate(boardControl->evalBoard->getSampleRateEnum());

    //Scan port to identify the connected chip
    scanPort();

    bool first64 = boardControl->dataStreams.physicalDataStreams[0].getNumChannels() == 64;
    bool second64 = boardControl->dataStreams.physicalDataStreams[1].getNumChannels() == 64;

    if (!(first64 && second64)) {
        QMessageBox::information(this, tr("No 128-channel Headstage Detected"),
                                 tr("No 128-channel headstage is connected to the Intan Electroplating Board."
                                    "<p>Connnect headstage module, then restart the application."));
        delete signalProcessor;
        delete ebc;
        delete boardControl;
        exit(EXIT_FAILURE);
    }

    /* Set up First Column of GUI */
    QVBoxLayout *firstColumn = new QVBoxLayout;

    //Set up "Selected Channel" Group Box
    QGroupBox *selectedChannelGroupBox = new QGroupBox(tr("Selected Channel"));
    selectedChannelSpinBox = new QSpinBox();
    selectedChannelSpinBox->setRange(0, 127);
    selectedChannelSpinBoxLabel = new QLabel(tr("N/A"));
    QHBoxLayout *selectedChannelGroupBoxLayout = new QHBoxLayout;
    selectedChannelGroupBoxLayout->addWidget(selectedChannelSpinBoxLabel);
    selectedChannelGroupBoxLayout->addWidget(selectedChannelSpinBox);
    selectedChannelGroupBox->setLayout(selectedChannelGroupBoxLayout);

    //Set up "Manual Pulse" Group Box
    QGroupBox *manualPulseGroupBox = new QGroupBox(tr("Manual Pulse"));
    QVBoxLayout *manualPulseGroupBoxLayout = new QVBoxLayout;
    manualChannelLabel = new QLabel(tr("Channel: ") + "0");
    manualModeLabel = new QLabel("Mode: Constant Current");
    manualValueLabel = new QLabel("Value: 0 nA");
    manualDurationLabel = new QLabel("Duration: 1 s");
    manualConfigureButton = new QPushButton(tr("Configure..."));
    manualApplyButton = new QPushButton(tr("Apply"));
    manualPulseGroupBoxLayout->addWidget(manualChannelLabel);
    manualPulseGroupBoxLayout->addWidget(manualModeLabel);
    manualPulseGroupBoxLayout->addWidget(manualValueLabel);
    manualPulseGroupBoxLayout->addWidget(manualDurationLabel);
    manualPulseGroupBoxLayout->addWidget(manualConfigureButton);
    manualPulseGroupBoxLayout->addWidget(manualApplyButton);
    manualPulseGroupBox->setLayout(manualPulseGroupBoxLayout);

    //Set up "Automatic Plating" Group Box
    QGroupBox *automaticPlatingGroupBox = new QGroupBox(tr("Automatic Plating"));
    QVBoxLayout *automaticPlatingGroupBoxLayout = new QVBoxLayout;
    automaticModeLabel = new QLabel("Mode: Constant Current");
    automaticValueLabel = new QLabel("Value: 0 nA");
    automaticInitialDurationLabel = new QLabel("Duration: 1 s");
    automaticConfigureButton = new QPushButton(tr("Configure..."));
    QGroupBox *targetImpedanceGroupBox = new QGroupBox(tr("Target Impedance"));
    QHBoxLayout *targetImpedanceGroupBoxLayout = new QHBoxLayout;
    targetImpedance = new QLineEdit();
    QLabel *targetImpedanceUnits = new QLabel(tr("kOhms"));
    targetImpedanceGroupBoxLayout->addWidget(targetImpedance);
    targetImpedanceGroupBoxLayout->addWidget(targetImpedanceUnits);
    targetImpedanceGroupBox->setLayout(targetImpedanceGroupBoxLayout);
    QGroupBox *runGroupBox = new QGroupBox(tr("Run"));
    QVBoxLayout *runGroupBoxLayout = new QVBoxLayout;
    runAllButton = new QRadioButton(tr("Run All"));
    runSelectedChannelButton = new QRadioButton(tr("Run Selected Channel (0)"));
    run063Button = new QRadioButton(tr("Run 0-63"));
    run64127Button = new QRadioButton(tr("Run 64-127"));
    QHBoxLayout *runCustomRow = new QHBoxLayout;
    runCustomButton = new QRadioButton(tr("Run"));
    customLowSpinBox = new QSpinBox();
    customLowSpinBox->setRange(0, 127);
    customLowSpinBox->setEnabled(false);
    connect(runCustomButton, SIGNAL(toggled(bool)), this, SLOT(runCustomChanged()));
    QLabel *runCustomDash = new QLabel("-");
    customHighSpinBox = new QSpinBox();
    customHighSpinBox->setRange(0, 127);
    customHighSpinBox->setValue(1);
    customHighSpinBox->setEnabled(false);
    runCustomRow->addWidget(runCustomButton);
    runCustomRow->addWidget(customLowSpinBox);
    runCustomRow->addWidget(runCustomDash);
    runCustomRow->addWidget(customHighSpinBox);
    runCustomRow->addStretch();
    runGroupBoxLayout->addWidget(runAllButton);
    runGroupBoxLayout->addWidget(runSelectedChannelButton);
    runGroupBoxLayout->addWidget(run063Button);
    runGroupBoxLayout->addWidget(run64127Button);
    runGroupBoxLayout->addLayout(runCustomRow);
    runGroupBox->setLayout(runGroupBoxLayout);
    automaticRunButton = new QPushButton(tr("Run"));
    automaticPlatingGroupBoxLayout->addWidget(automaticModeLabel);
    automaticPlatingGroupBoxLayout->addWidget(automaticValueLabel);
    automaticPlatingGroupBoxLayout->addWidget(automaticInitialDurationLabel);
    automaticPlatingGroupBoxLayout->addWidget(automaticConfigureButton);
    automaticPlatingGroupBoxLayout->addWidget(targetImpedanceGroupBox);
    automaticPlatingGroupBoxLayout->addWidget(runGroupBox);
    automaticPlatingGroupBoxLayout->addWidget(automaticRunButton);
    automaticPlatingGroupBox->setLayout(automaticPlatingGroupBoxLayout);

    //Set up "Plot Settings" Group Box
    QGroupBox *plotSettingsGroupBox = new QGroupBox(tr("Plot Settings"));
    QVBoxLayout *plotSettingsGroupBoxLayout = new QVBoxLayout;
    showGrid = new QCheckBox(tr("Show Grid"));
    plotSettingsGroupBoxLayout->addWidget(showGrid);
    plotSettingsGroupBox->setLayout(plotSettingsGroupBoxLayout);

    //Add group boxes to First Column
    firstColumn->addWidget(selectedChannelGroupBox);
    firstColumn->addStretch();
    firstColumn->addWidget(manualPulseGroupBox);
    firstColumn->addStretch();
    firstColumn->addWidget(automaticPlatingGroupBox);
    firstColumn->addStretch();
    firstColumn->addWidget(plotSettingsGroupBox);

    //Set default state to "Run All"
    runAllButton->setChecked(true);

    /* Set up Second Column of GUI */
    QVBoxLayout *secondColumn = new QVBoxLayout;
    //Set up First Row
    QHBoxLayout *firstRow = new QHBoxLayout;
    //Set up "Impedance Display" Group Box
    QGroupBox *impedanceDisplayGroupBox = new QGroupBox(tr("Impedance Display"));
    QHBoxLayout *impedanceDisplayGroupBoxLayout = new QHBoxLayout;
    magnitudeButton = new QRadioButton(tr("Magnitudes (ohms)"));
    phaseButton = new QRadioButton(tr("Phases (degrees)"));
    impedanceDisplayGroupBoxLayout->addWidget(magnitudeButton);
    impedanceDisplayGroupBoxLayout->addWidget(phaseButton);
    impedanceDisplayGroupBox->setLayout(impedanceDisplayGroupBoxLayout);
    firstRow->addWidget(impedanceDisplayGroupBox);

    //Set default state to "Magnitude"
    magnitudeButton->setChecked(true);

    //Connect Magnitude button's 'toggled' signal to impedanceDisplayChanged slot
    connect(magnitudeButton, SIGNAL(toggled(bool)), this, SLOT(impedanceDisplayChanged()));

    //Set up "Read All Impedances" and "Continuous Z Scan" buttons
    readAllImpedancesButton = new QPushButton(tr("Read All Impedances"));
    continuousZScanButton = new QPushButton(tr("Continuous Z Scan"));
    firstRow->addWidget(readAllImpedancesButton);
    firstRow->addWidget(continuousZScanButton);
    secondColumn->addLayout(firstRow);

    //Set up present impedance widget
    currentZ = new ImpedancePlot(this);
    currentZ->title = "Impedance Magnitudes (0 of 0 below threshold)";
    currentZ->xLabel = "Channel";
    currentZ->yLabel = "Impedance (Ohms)";
    currentZ->setDomain(false, 127);
    currentZ->setRange(false);
    currentZ->redrawPlot();

    //Connect currentZ's 'clickedXUnits' signal to findClosestChannel slot
    connect(currentZ, SIGNAL(clickedXUnits(double)), this, SLOT(findClosestChannel(double)));

    //Set up impedance history widget
    Zhistory = new ImpedancePlot(this);
    Zhistory->title = "Impedance History (Channel 0)";
    Zhistory->xLabel = "Time (seconds)";
    Zhistory->yLabel = "Impedance (Ohms)";
    Zhistory->setDomain(true, 1);
    Zhistory->setRange(false);
    Zhistory->redrawPlot();

    //Add impedance plots to Second Column
    secondColumn->addWidget(currentZ);
    secondColumn->addWidget(Zhistory);

    //Add two columns to window's main layout
    QHBoxLayout *mainLayout = new QHBoxLayout;
    mainLayout->addLayout(firstColumn);
    mainLayout->addLayout(secondColumn);
    QWidget *mainWidget = new QWidget;
    mainWidget->setLayout(mainLayout);

    /* Set up menus and add actions to them */
    //Create "Settings" actions
    configureAction = new QAction(tr("Configure"), this);
    saveSettingsAction = new QAction(tr("Save Settings"), this);
    loadSettingsAction = new QAction(tr("Load Settings"), this);
    saveImpedancesAction = new QAction(tr("Save Impedances"), this);

    //Connect "Settings" actions to their respective slots
    connect(configureAction, SIGNAL(triggered()), this, SLOT(configure()));
    connect(saveSettingsAction, SIGNAL(triggered()), this, SLOT(saveSettings()));
    connect(loadSettingsAction, SIGNAL(triggered()), this, SLOT(loadSettings()));
    connect(saveImpedancesAction, SIGNAL(triggered()), this, SLOT(saveImpedances()));

    //Create "Help" actions
    intanWebsiteAction = new QAction(tr("Visit Intan Website..."), this);
    aboutAction = new QAction(tr("About Intan GUI..."), this);

    //Connect "Help" actions to their respective slots
    connect(intanWebsiteAction, SIGNAL(triggered()), this, SLOT(openIntanWebsite()));
    connect(aboutAction, SIGNAL(triggered()), this, SLOT(about()));

    //Add "Settings" actions to menu and add menu to menu bar
    settingsMenu = menuBar()->addMenu(tr("&Settings"));
    settingsMenu->addAction(configureAction);
    settingsMenu->addAction(saveSettingsAction);
    settingsMenu->addAction(loadSettingsAction);
    settingsMenu->addSeparator();
    settingsMenu->addAction(saveImpedancesAction);

    //Add "Help" actions to menu and add menu to menu bar
    helpMenu = menuBar()->addMenu(tr("Help"));
    helpMenu->addAction(intanWebsiteAction);
    helpMenu->addAction(aboutAction);

    /* Set up and initialize settings */
    //Create and initialize parameters
    manualParameters = new ConfigurationParameters;
    automaticParameters = new ConfigurationParameters;
    globalParameters = new GlobalParameters;
    initializeParams();

    //Create and initialize settings
    settings = new Settings;
    initializeSettings();

    //Create data processor
    dataProcessor = new DataProcessor();

    //Connect final signals & slots
    connect(manualConfigureButton, SIGNAL(clicked()), this, SLOT(manualConfigureSlot()));
    connect(manualApplyButton, SIGNAL(clicked()), this, SLOT(manualApplySlot()));
    connect(automaticConfigureButton, SIGNAL(clicked()), this, SLOT(automaticConfigureSlot()));
    connect(automaticRunButton, SIGNAL(clicked()), this, SLOT(automaticRunSlot()));
    connect(readAllImpedancesButton, SIGNAL(clicked()), this, SLOT(readAllImpedancesSlot()));
    connect(continuousZScanButton, SIGNAL(clicked()), this, SLOT(continuousZScanSlot()));
    connect(targetImpedance, SIGNAL(textChanged(QString)), this, SLOT(targetImpedanceChanged(QString)));
    connect(selectedChannelSpinBox, SIGNAL(valueChanged(int)), this, SLOT(selectedChannelChanged()));
    connect(showGrid, SIGNAL(toggled(bool)), this, SLOT(showGridChanged(bool)));

    //Set Target Impedance (after signal connection so that it calls the appropriate slot)
    targetImpedance->setText("100");
    targetImpedanceLine = targetImpedance->text().toDouble() * 1000;

    setCentralWidget(mainWidget);

    adjustSize();
    QRect screenRect = QApplication::desktop()->screenGeometry();
    //Set this main widget to main window
    setCentralWidget(mainWidget);
    adjustSize();

    //If the screen height has less than 100 pixels to spare at the current size of mainWidget, or
    //if the screen width has less than 100 pixels to spare, put mainWidget inside a QScrollArea

    if ((screenRect.height() < mainWidget->height() + 100) ||
            (screenRect.width() < mainWidget->width() + 100)) {
        QScrollArea *scrollArea = new QScrollArea;
        scrollArea->setWidget(mainWidget);
        setCentralWidget(scrollArea);

        if (screenRect.height() < mainWidget->height() + 100)
            setMinimumHeight(screenRect.height() - 100);
        else
            setMinimumHeight(mainWidget->height() + 50);

        if (screenRect.width() < mainWidget->width() + 100)
            setMinimumWidth(screenRect.width() - 100);
        else
            setMinimumWidth(mainWidget->width() + 50);
    }
}


/* Destructor */
MainWindow::~MainWindow()
{
    delete dataProcessor;
    delete settings;
    delete globalParameters;
    delete automaticParameters;
    delete manualParameters;
    delete Zhistory;
    delete currentZ;
    delete signalProcessor;
    delete signalSources;
    delete boardControl;
    delete ebc;
}


/* ***** SLOTS ***** */

/* If the user changed whether or not run custom is checked, enable/disable custom spinboxes */
void MainWindow::runCustomChanged()
{
    bool enable = runCustomButton->isChecked();
    customLowSpinBox->setEnabled(enable);
    customHighSpinBox->setEnabled(enable);
}


/* If the user has changed impedance display (phase or magnitude), redraw the plots */
void MainWindow::impedanceDisplayChanged()
{
    redrawImpedance();
}


/* When the currentZ plot is clicked, set the "Selected" channel to the channel closest to the x-position of the click */
void MainWindow::findClosestChannel(double clickXPosition)
{
    double currentDistance;
    double closestDistance = 100000; //Since we're looking for the minimum of this value, initialize it with a value much higher than possible
    int closestChannel;

    //Store current impedances in 'impedances'
    QVector<ElectrodeImpedance> impedances = dataProcessor->get_impedances();

    //If no impedances have been measured, or if the click was outside the range of the graph (set to -1), return without changing the channel
    if (impedances.size() == 0 || clickXPosition == -1)
        return;

    //Loop through all members of 'impedances' finding the channel closest to the click
    for (int i = 0; i < impedances.size(); i++) {
        currentDistance = abs(clickXPosition - impedances.at(i).index);
        if (currentDistance < closestDistance) {
            closestDistance = currentDistance;
            closestChannel = impedances.at(i).index;
        }
    }

    //Set the "Selected" channel to the closest channel
    selectedChannelSpinBox->setValue(closestChannel);
}


/* Open a new Global Configuration Window, and pass it globalParameters to save (if OK is clicked) */
void MainWindow::configure()
{
    GlobalConfigurationWindow globalConfig(globalParameters);
}


/* Save settings to .set file */
void MainWindow::saveSettings()
{
    //Get filename
    QString saveSettingsFileName;
    saveSettingsFileName = QFileDialog::getSaveFileName(this,
                                                        tr("Select Settings Filename"), ".",
                                                        tr("Intan Settings Files (*.set)"));

    //If user canceled the save operation, return
    if (saveSettingsFileName.length() == 0)
        return;

    /* Update settings with the current GUI entries */
    //Automatic pulse settings
    settings->automaticIsVoltageMode = (automaticParameters->electroplatingMode == ConstantVoltage);
    settings->automaticValue = automaticParameters->actualValue;
    settings->automaticDesired = automaticParameters->desiredValue;
    settings->automaticDuration = automaticParameters->duration;

    //Manual pulse settings
    settings->manualIsVoltageMode = (manualParameters->electroplatingMode == ConstantVoltage);
    settings->manualValue = manualParameters->actualValue;
    settings->manualDesired = manualParameters->desiredValue;
    settings->manualDuration = manualParameters->duration;

    //Global settings
    settings->threshold = (targetImpedance->text().toDouble()) * 1000;
    settings->maxPulses = globalParameters->maxPulses;
    settings->delayBeforePulse = globalParameters->delayMeasurementPulse;
    settings->delayAfterPulse = globalParameters->delayPulseMeasurement;
    settings->delayChangeRef = globalParameters->delayChangeRef;
    settings->delayZScan = globalParameters->continuousZDelay;
    settings->channels0to63 = globalParameters->channels063Present;
    settings->channels64to127 = globalParameters->channels64127Present;
    settings->useTargetImpedance = globalParameters->useTargetZ;

    //State of GUI
    settings->selected = selectedChannelSpinBox->value();
    settings->displayMagnitudes = magnitudeButton->isChecked();
    settings->showGrid = showGrid->isChecked();

    //Save settings
    dataProcessor->save_settings(saveSettingsFileName, *settings);
}


/* Load settings from a .set file */
void MainWindow::loadSettings()
{
    //Get filename
    QString loadSettingsFileName;
    loadSettingsFileName = QFileDialog::getOpenFileName(this,
                                                        tr("Select Settings Filename"), ".",
                                                        tr("Intan Settings Files (*.set)"));
    dataProcessor->load_settings(loadSettingsFileName, *settings);

    //If user canceled the load operation, return
    if (loadSettingsFileName.length() == 0)
        return;

    /* Load settings from .set file */
    //Automatic pulse settings
    automaticParameters->electroplatingMode = (settings->automaticIsVoltageMode ? ConstantVoltage : ConstantCurrent);
    automaticParameters->desiredValue = settings->automaticDesired;
    automaticParameters->actualValue = settings->automaticValue;
    if (settings->automaticValue < 0)
        automaticParameters->sign = Negative;
    else
        automaticParameters->sign = Positive;
    automaticParameters->duration = settings->automaticDuration;

    //Manual pulse settings
    manualParameters->electroplatingMode = (settings->manualIsVoltageMode ? ConstantVoltage : ConstantCurrent);
    manualParameters->desiredValue = settings->manualDesired;
    manualParameters->actualValue = settings->manualValue;
    if (settings->manualValue < 0)
        manualParameters->sign = Negative;
    else
        manualParameters->sign = Positive;
    manualParameters->duration = settings->manualDuration;

    //Global settings
    targetImpedance->setText(QString::number(settings->threshold / 1000));
    globalParameters->maxPulses = settings->maxPulses;
    globalParameters->delayMeasurementPulse = settings->delayBeforePulse;
    globalParameters->delayPulseMeasurement = settings->delayAfterPulse;
    globalParameters->delayChangeRef = settings->delayChangeRef;
    globalParameters->continuousZDelay = settings->delayZScan;
    globalParameters->channels063Present = settings->channels0to63;
    globalParameters->channels64127Present = settings->channels64to127;
    globalParameters->useTargetZ = settings->useTargetImpedance;

    //State of GUI
    selectedChannelSpinBox->setValue(settings->selected);
    magnitudeButton->setChecked(settings->displayMagnitudes);
    showGrid->setChecked(settings->showGrid);

    //Update manual labels to reflect any changes after loading
    updateManualLabels();
    //Update automatic labels to reflect any changes after loading
    updateAutomaticLabels();
}


/* Save impedances to a .csv file */
void MainWindow::saveImpedances()
{
    //Get filename
    QString saveImpedancesFileName;
    saveImpedancesFileName = QFileDialog::getSaveFileName(this,
                                                          tr("Save Impedance Data As"), ".",
                                                          tr("Comma Separated Values File (*.csv)"));

    //If user canceled the save operation, return
    if (saveImpedancesFileName.length() == 0)
        return;

    //Save impedances
    dataProcessor->save_impedances(saveImpedancesFileName);
}


/* Open Intan's website in the user's default internet browser */
void MainWindow::openIntanWebsite()
{
    QDesktopServices::openUrl(QUrl("http://www.intantech.com", QUrl::TolerantMode));
}


/* Pop up dialog displaying information about this program */
void MainWindow::about()
{
    QMessageBox::about(this, tr("About Intan Technologies RHD2000 Electroplating Interface"),
            tr("<h2>Intan Technologies RHD2000 Electroplating Interface</h2>"
               "<p>Version 1.03"
               "<p>Copyright &copy; 2021 Intan Technologies"
               "<p>This application controls the RHD2000 "
               "Electroplating Board from Intan Technologies.  The C++/Qt source code "
               "for this application is freely available from Intan Technologies. "
               "For more information visit <i>http://www.intantech.com</i>."
#ifdef __APPLE__
               "<p>This program is developed and tested for macOS using Qt 5.12.10. "
#else
               "<p>This program is developed and tested for Windows using Qt 5.12.8. "
#endif
               "<p>This program is free software: you can redistribute it and/or modify "
               "it under the terms of the GNU Lesser General Public License as published "
               "by the Free Software Foundation, either version 3 of the License, or "
               "(at your option) any later version."
               "<p>This program is distributed in the hope that it will be useful, "
               "but WITHOUT ANY WARRANTY; without even the implied warranty of "
               "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the "
               "GNU Lesser General Public License for more details."
               "<p>You should have received a copy of the GNU Lesser General Public License "
               "along with this program.  If not, see <i>http://www.gnu.org/licenses/</i>."));
}


/* Open a new Configuration Window, and pass it manualParameters to save (if OK is clicked) */
void MainWindow::manualConfigureSlot()
{
    ConfigurationWindow manualConfig(manualParameters, ebc);
    //Update labels in main window to reflect the changed manual pulse parameters
    updateManualLabels();
}


/* Read impedance, apply a manual pulse, and read impedance again for the currently selected channel */
void MainWindow::manualApplySlot()
{
    //Set up a progress dialog to inform the user of the pulse operation
    QProgressDialog *manualProgress = new QProgressDialog("Measuring Pre-Pulse Impedance", "Abort", 0, 1, this);
    manualProgress->setWindowTitle(" ");
    manualProgress->setMinimumDuration(0);
    manualProgress->setModal(true);
    manualProgress->setMaximum(5);
    manualProgress->setValue(0);

    //Clear this channel's history
    dataProcessor->Electrodes[selectedChannelSpinBox->value()]->reset_time();

    //Measure before pulsing
    readImpedance(selectedChannelSpinBox->value(), manualProgress);

    //Delay before pulsing
    manualProgress->setLabelText("Delaying Before Pulse");
    manualProgress->setValue(1);
    QApplication::processEvents();
    sleep(globalParameters->delayMeasurementPulse * 1000);

    //Pulse
    manualProgress->setLabelText("Applying Pulse");
    manualProgress->setValue(2);
    QApplication::processEvents();
    pulse(selectedChannelSpinBox->value(), manualParameters->electroplatingMode, manualParameters->actualValue, manualParameters->duration);

    //Delay after pulsing
    manualProgress->setLabelText("Delaying After Pulse");
    manualProgress->setValue(3);
    QApplication::processEvents();
    sleep(globalParameters->delayPulseMeasurement * 1000);

    //Measure after pulsing
    manualProgress->setLabelText("Measuring Post-Pulse Impedance");
    manualProgress->setValue(4);
    QApplication::processEvents();
    readImpedance(selectedChannelSpinBox->value(), manualProgress);

    //Delete progress dialog
    delete manualProgress;

    //Reading impedances this way leaves the LEDs on, so turn them off
    int ledArray[8] = {0,0,0,0,0,0,0,0};
    boardControl->evalBoard->setLedDisplay(ledArray);
}


/* Open a new Configuration Window, and pass it automaticParameters to save (if OK is clicked) */
void MainWindow::automaticConfigureSlot()
{
    ConfigurationWindow automaticConfig(automaticParameters, ebc);
    //Update labels in main window to reflect the changed automatic pulse parameters
    updateAutomaticLabels();
}


/* Apply automatic electroplating pulses to all desired channels, reading and plating in a loop for each channel */
void MainWindow::automaticRunSlot()
{
    //Set up a progress dialog to inform the user of the pulse operation
    QProgressDialog *automaticProgress = new QProgressDialog("Plating Automatically", "Abort", 0, 1, this);
    automaticProgress->setWindowTitle(" ");
    automaticProgress->setMinimumDuration(0);
    automaticProgress->setModal(true);

    //From the currently selected radio button, determine how many and what channels need to be plated
    int channelStart, channelEnd;
    if (runAllButton->isChecked()) {
        channelStart = 0;
        channelEnd = 128;
        automaticProgress->setMaximum(128);
    }
    else if (runSelectedChannelButton->isChecked()) {
        channelStart = selectedChannelSpinBox->value();
        channelEnd = selectedChannelSpinBox->value() + 1;
        automaticProgress->setMaximum(1);
    }
    else if (run063Button->isChecked()) {
        channelStart = 0;
        channelEnd = 64;
        automaticProgress->setMaximum(64);
    }
    else if (run64127Button->isChecked()) {
        channelStart = 64;
        channelEnd = 128;
        automaticProgress->setMaximum(64);
    }
    else if (runCustomButton->isChecked()) {
        channelStart = qMin(customHighSpinBox->value(), customLowSpinBox->value());
        channelEnd = qMax(customHighSpinBox->value(), customLowSpinBox->value()) + 1;
        automaticProgress->setMaximum(channelEnd - channelStart);
    }

    //Plate the appropriate channels
    for (int i = channelStart; i < channelEnd; i++) {
        //Update progress dialog
        automaticProgress->setLabelText("Plating Channel " + QString::number(i) + " Automatically");
        automaticProgress->setValue(i - channelStart);
        QApplication::processEvents();

        //Set the "Selected" channel to the channel that is being plated
        selectedChannelSpinBox->setValue(i);

        //Plate channel
        plateOneAutomatically(i, automaticProgress);

        if (automaticProgress->wasCanceled())
            break;
    }

    //Delete progress dialog
    delete automaticProgress;

    //Reading impedances this way leaves the LEDs on, so turn them off
    int ledArray[8] = {0,0,0,0,0,0,0,0};
    boardControl->evalBoard->setLedDisplay(ledArray);
}


/* Read all 128 channels' impedances sequentially */
void MainWindow::readAllImpedancesSlot()
{
    //Don't want the user clicking on other buttons while we read
    setAllEnabled(false);

    //Set up a progress dialog to inform the user of the read operation
    QProgressDialog *readAllProgress = new QProgressDialog("Measuring Electrode Impedances", "Abort", 0, 1, this);
    readAllProgress->setWindowTitle(" ");
    readAllProgress->setMinimumDuration(0);
    readAllProgress->setModal(true);
    readAllProgress->setMaximum(128);
    readAllProgress->show();

    //Read all 128 channels
    for (int i = 0; i < 128; i++) {

        QApplication::processEvents();

        //If the user clicked 'Cancel', set the 'Selected' channel to the current channel, and exit loop
        if (readAllProgress->wasCanceled()) {
            if (i > 0)
                selectedChannelSpinBox->setValue(i - 1);
            readAllProgress->close();
            break;
        }

        //Update progress dialog
        readAllProgress->setLabelText("Measuring Channel " + QString::number(i));
        readAllProgress->setValue(i);
        //update();
        QApplication::processEvents();
        QTimer *timer = new QTimer(this);
        timerdone = false;
        connect(timer, SIGNAL(timeout()), this, SLOT(timerupdate()));
        timer->start(50);

        while (!timerdone) {
            QApplication::processEvents();
        }

        //Clear the history for the electrode and measure it.
        dataProcessor->Electrodes[i]->reset_time();
        readImpedance(i, readAllProgress);

        //Update the display
        selectedChannelSpinBox->setValue(i);
        currentZ->repaint();
        //readAllProgress->setValue(i + 1);
        //readAllProgress->show();
        //update();
    }
    QApplication::processEvents();

    //Delete progress dialog
    delete readAllProgress;

    //Reading impedances this way leaves the LEDs on, so turn them off
    int ledArray[8] = {0,0,0,0,0,0,0,0};
    boardControl->evalBoard->setLedDisplay(ledArray);

    //Now re-enable the buttons
    setAllEnabled(true);

}

void MainWindow::timerupdate()
{
    timerdone = true;
}

void MainWindow::sleeptimerupdate()
{
    sleeptimerdone = true;
}


/* Read the currently selected channel's impedance continuously until user clicks 'Cancel' */
void MainWindow::continuousZScanSlot()
{
    //Don't want anyone pushing any other buttons while this is running
    setAllEnabled(false);

    //Set up a progress dialog to inform the user of the reading operation and allow user to cancel
    QProgressDialog *continuousProgress = new QProgressDialog("Scanning Continuously", "Abort", 0, 1, this);
    continuousProgress->setWindowTitle(" ");
    continuousProgress->setMinimumDuration(0);
    continuousProgress->setModal(true);
    continuousProgress->setMaximum(1);
    continuousProgress->setValue(0);

    //Reset the history
    dataProcessor->Electrodes[selectedChannelSpinBox->value()]->reset_time();

    while (true) {
        //Update progress dialog
        continuousProgress->setLabelText("Reading Impedance");
        QApplication::processEvents();

        //Read impedance
        readImpedance(selectedChannelSpinBox->value(), continuousProgress);

        //Pause
        continuousProgress->setLabelText("Pausing");
        QApplication::processEvents();
        sleep(globalParameters->continuousZDelay * 1000);
        QApplication::processEvents();

        //If user clicks 'Cancel', exit loop
        if (continuousProgress->wasCanceled())
            break;
    }

    //Delete progress dialog
    delete continuousProgress;

    //Reading impedances this way leaves the LEDs on, so turn them off
    int ledArray[8] = {0,0,0,0,0,0,0,0};
    boardControl->evalBoard->setLedDisplay(ledArray);

    //Now re-enable the buttons
    setAllEnabled(true);
}


/* If the user has changed the target impedance, inform the currentZ and Zhistory plots of the new threshold */
void MainWindow::targetImpedanceChanged(QString impedance)
{
    redrawImpedance();
}


/* If the user has changed the select channel, update the GUI and impedance plots to reflect the new channel */
void MainWindow::selectedChannelChanged()
{
    manualChannelLabel->setText("Channel: " + selectedChannelSpinBox->text());
    runSelectedChannelButton->setText("Run Selected Channel (" + selectedChannelSpinBox->text() + ")");

    Zhistory->title = "Impedance History (Channel " + selectedChannelSpinBox->text() + ")";

    redrawImpedance();
}


/* If the user has toggled the show grid checkbox, update the impedance plots to reflect the new setting */
void MainWindow::showGridChanged(bool grid)
{
    currentZ->plotGrid(grid);
    Zhistory->plotGrid(grid);

    redrawImpedance();
}

/* ***** FUNCTIONS ***** */

/* Connect to Opal Kelly XEM6010 board and upload .bit file */
void MainWindow::connectToBoard()
{
    int usb_type = 0;

    if (connected) {
        boardControl->evalBoard->resetBoard();
    }

    else {
        //Open Opal Kelly XEM6010 board
        usb_type = boardControl->evalBoard->open();

        //Make sure USB connection is established
        QMessageBox::StandardButton r;
        if (usb_type == -1) {
            r = QMessageBox::question(this, tr("Cannot load Opal Kelly FrontPanel DLL"),
                                      tr("Opal Kelly USB drivers not installed.  "
                                         "<p>To use the Electroplating Board, load the correct Opal Kelly drivers, then restart the application."
                                         "<p>Visit http://www.intantech.com for more information."),
                                      QMessageBox::Ok | QMessageBox::Cancel);
            return;
        }
        else if (usb_type != 1) {
            r = QMessageBox::question(this, tr("Electroplating Board Not Found"),
                                      tr("Intan Technologies Electroplating Board not found on any USB port.  "
                                         "<p>To use the Electroplating Board, connect the device to a USB port, then restart the application."
                                         "<p>Visit http://www.intantech.com for more information."),
                                      QMessageBox::Ok | QMessageBox::Cancel);
            return;
        }

        //Load Rhythm FPGA configuration bitfile (provided by Intan Technologies).
        string bitfilename = QString(QCoreApplication::applicationDirPath() + "/main.bit").toStdString();

        if (!boardControl->evalBoard->uploadFpgaBitfile(bitfilename)) {
            QMessageBox::critical(this, tr("Hardware Configuration File Upload Error"),
                                  tr("Cannot upload configuration file to Intan Electroplating Board. Make sure file main.bit "
                                     "is in the same directory as the executable file."));
            return;
        }

        //Make sure currently connected board is an Electroplating Board (board mode value of 2)
        int mode = 0;
        mode = boardControl->evalBoard->getBoardMode();
        if (mode != 2) {
            r = QMessageBox::question(this, tr("Electroplating Board Not Found"),
                                      tr("Intan Technologies Electroplating Board not found on any USB port.  "
                                         "<p>To use the Electroplating Board, connect the device to a USB port, then restart the application."
                                         "<p>Visit http://www.intantech.com for more information."),
                                      QMessageBox::Ok | QMessageBox::Cancel);
            return;
        }

        //Make sure a 128-channel headstage is connected
        bool headstage = false;
        headstage = signalSources->signalPort[0].enabled;

        //Reset LEDs
        int array[] = {0, 0, 0, 0, 0, 0, 0, 0};
        boardControl->evalBoard->setLedDisplay(array);
    }
    connected = true;
}


/* Scan SPI Port to identify all connected RHD2000 amplifier chips */
void MainWindow::scanPort()
{
    // Scan SPI Ports.
    boardControl->getChipIds(0);
    boardControl->dataStreams.autoConfigureDataStreams();

    // Configure SignalProcessor object for the required number of data streams
    signalProcessor->allocateMemory(boardControl->evalBoard->getNumEnabledDataStreams());
}


/* Upload and execute board commands to change sample rate */
void MainWindow::changeSampleRate(Rhd2000EvalBoard::AmplifierSampleRate sampleRate) {
    boardControl->changeSampleRate(sampleRate);

    //Set up an RHD2000 register object using this sample rate to optimize MUX-related register settings
    Rhd2000Registers chipRegisters(boardControl->boardSampleRate);

    int commandSequenceLength;
    vector<int> commandList;

    commandSequenceLength = chipRegisters.createCommandListRegisterConfig(commandList, false);
    boardControl->evalBoard->uploadCommandList(commandList, Rhd2000EvalBoard::AuxCmd3, 0);
    boardControl->evalBoard->selectAuxCommandLength(Rhd2000EvalBoard::AuxCmd3, 0, commandSequenceLength - 1);
    boardControl->evalBoard->selectAuxCommandBank(Rhd2000EvalBoard::PortA, Rhd2000EvalBoard::AuxCmd3, 0);
    boardControl->auxCmds.commandSlots[Rhd2000EvalBoard::AuxCmd3].selectBank(0);
    boardControl->updateCommandSlots();
}


/* Initialize manual, automatic, and global parameters with default values */
void MainWindow::initializeParams()
{
    manualParameters->electroplatingMode = ConstantCurrent;
    manualParameters->sign = Negative;
    manualParameters->desiredValue = 0;
    manualParameters->actualValue = 0;
    manualParameters->duration = 1;

    automaticParameters->electroplatingMode = ConstantCurrent;
    automaticParameters->sign = Negative;
    automaticParameters->desiredValue = 0;
    automaticParameters->actualValue = 0;
    automaticParameters->duration = 1;

    globalParameters->maxPulses = 10;
    globalParameters->delayMeasurementPulse = 0;
    globalParameters->delayPulseMeasurement = 0;
    globalParameters->delayChangeRef = 0.1f;
    globalParameters->continuousZDelay = 2;
    globalParameters->channels063Present = true;
    globalParameters->channels64127Present = true;
    globalParameters->useTargetZ = true;
}


/* Initialize settings with default values */
void MainWindow::initializeSettings()
{
    settings->automaticIsVoltageMode = false;
    settings->automaticValue = 0;
    settings->automaticDesired = 0;
    settings->automaticDuration = 1;

    settings->manualIsVoltageMode = false;
    settings->manualValue = 0;
    settings->manualDesired = 0;
    settings->manualDuration = 1;

    settings->threshold = 100000;
    settings->maxPulses = 10;
    settings->delayBeforePulse = 0;
    settings->delayAfterPulse = 0;
    settings->delayChangeRef = 0.1;
    settings->delayZScan = 2;

    settings->channels0to63 = true;
    settings->channels64to127 = true;
    settings->useTargetImpedance = true;

    settings->selected = 0;
    settings->displayMagnitudes = true;
    settings->showGrid = false;
}


/* Update and redraw impedance plots, as well as GUI impedance display widget */
void MainWindow::redrawImpedance()
{
    setImpedanceTitle();
    updatePresentPlotMode();
    drawAllImpedances();
    drawImpedanceHistory();

    //display the current impedance as text
    QString str = "";

    OneElectrode *electrode = dataProcessor->Electrodes[selectedChannelSpinBox->value()];
    if (electrode->ImpedanceHistory.size() == 0)
        selectedChannelSpinBoxLabel->setText("N/A");
    else {
        if (magnitudeButton->isChecked()) {
            double z = std::abs(electrode->get_current_impedance());
            if (z < 1000)
                str = QString::number(significantRound(z)) + " Ohms";
            else if (z < 1e6)
                str = QString::number(significantRound(z / 1000)) + " kOhms";
            else
                str = QString::number(significantRound(z / 1e6)) + " MOhms";
        }
        else {
            double theta = std::arg(electrode->get_current_impedance());
            str = QString::number(significantRound(theta * RADIANS_TO_DEGREES)) + " degrees";
        }
        selectedChannelSpinBoxLabel->setText(str);
    }
}


/* Update impedance plot "currentZ" title */
void MainWindow::setImpedanceTitle()
{
    int numBelow = 0;
    int numTotal;
    QVector<ElectrodeImpedance> impedances = dataProcessor->get_impedances();
    numTotal = impedances.size();
    for (int i = 0; i < impedances.size(); i++) {
        if (std::abs(impedances.at(i).impedance) <= targetImpedance->text().toDouble() * 1000)
            numBelow++;
    }
    currentZ->title = "Impedance Magnitudes (" + QString::number(numBelow) + " of " + QString::number(numTotal) + " below threshold)";
}


/* Update impedance plot "currentZ" title, y label, and y range depending on the display of magnitude or phase */
void MainWindow::updatePresentPlotMode()
{
    if (magnitudeButton->isChecked()) {
        setImpedanceTitle();
        currentZ->yLabel = "Impedance (Ohms)";
        currentZ->setRange(false);
    }
    else {
        currentZ->title = "Impedance Phases";
        currentZ->yLabel = "Phase (Degrees)";
        currentZ->setRange(true);
    }
}


/* Draw "currentZ" impedances plot */
void MainWindow::drawAllImpedances()
{
    QVector<ElectrodeImpedance> impedances = dataProcessor->get_impedances();
    if (magnitudeButton->isChecked()) {
        //Draw green target impedance line
        double targetImpedanceOhms = targetImpedance->text().toDouble() * 1000;
        currentZ->plotLine(0, targetImpedanceOhms, 127 + 1, targetImpedanceOhms, Qt::green);
        //Draw each point
        for (int i = 0; i < impedances.size(); i++) {
            bool selected = false;
            ClipState state = InRange;
            if (selectedChannelSpinBox->value() == impedances[i].index)
                selected = true;
            if (std::abs(impedances[i].impedance) >= 10000000)
                state = ClipHigh;
            else if (std::abs(impedances[i].impedance) <= 10000)
                state = ClipLow;
            currentZ->plotPoint(impedances[i].index, std::abs(impedances[i].impedance), state, selected);
        }
    }
    else {
        //Draw gray 0 degree and - 90 degree lines
        currentZ->plotLine(0, 0, 127, 0, Qt::darkGray);
        currentZ->plotLine(0, -PI/2, 127, -PI/2, Qt::darkGray);
        for (int i = 0; i < impedances.size(); i++) {
            bool selected = false;
            if (selectedChannelSpinBox->value() == impedances[i].index)
                selected = true;
            currentZ->plotPoint(impedances[i].index, std::arg(impedances[i].impedance), InRange, selected);
        }
    }
    currentZ->redrawPlot();
}


/* Draw "Zhistory" impedances plot */
void MainWindow::drawImpedanceHistory()
{
    //Set x axis scale
    OneElectrode *electrode = new OneElectrode;
    electrode = dataProcessor->Electrodes[selectedChannelSpinBox->value()];
    if (electrode->MeasurementTimes.size() == 0 || electrode->MeasurementTimes.last() < 1) {
        Zhistory->setDomain(true, 1);
    }
    else {
        Zhistory->setDomain(true, electrode->MeasurementTimes.last());
    }

    //Set y axis scale
    //If this channel has no impedances stored, just show the default scaling
    if (electrode->ImpedanceHistory.size() == 0) {
        Zhistory->setRange(false);
    }
    //If this channel does have impedances stored, find the minimum y value, convert it to the lowest exponent, and set the range to that.
    else {
        double ymin = std::abs(electrode->ImpedanceHistory.first());
        for (int i = 0; i < electrode->ImpedanceHistory.size(); i++) {
            if ((std::abs(electrode->ImpedanceHistory.at(i)) < ymin) && (std::abs(electrode->ImpedanceHistory.at(i)) > 1))
                ymin = std::abs(electrode->ImpedanceHistory.at(i));
        }
        if (ymin < 10000) {
            Zhistory->setRange(false, floor(log10(ymin)), 7);
        }
        else
            Zhistory->setRange(false);
    }

    //Draw green target impedance line
    double targetImpedanceOhms = targetImpedance->text().toDouble() * 1000;
    if (electrode->MeasurementTimes.size() == 0 || electrode->MeasurementTimes.last() < 1) {
        Zhistory->plotLine(0, targetImpedanceOhms, 1 + 1, targetImpedanceOhms, Qt::green);
    }
    else {
        Zhistory->plotLine(0, targetImpedanceOhms, electrode->MeasurementTimes.last() + 1, targetImpedanceOhms, Qt::green);
    }
    //Draw each point
    for (int i = 0; i < electrode->ImpedanceHistory.size(); i++) {
        ClipState state = InRange;
        if (std::abs(electrode->ImpedanceHistory.at(i)) >= 10000000)
            state = ClipHigh;
        Zhistory->plotPoint(electrode->MeasurementTimes.at(i), std::abs(electrode->ImpedanceHistory.at(i)), state, true);
    }
    //Draw a blue line connecting each point
    for (int i = 0; i < electrode->ImpedanceHistory.size() - 1; i++) {
        Zhistory->plotLine(electrode->MeasurementTimes.at(i), std::abs(electrode->ImpedanceHistory.at(i)), electrode->MeasurementTimes.at(i + 1), std::abs(electrode->ImpedanceHistory.at(i + 1)), Qt::blue);
    }
    //Draw red pulse lines
    for (int i = 0; i < electrode->PulseDurations.size(); i++) {
        Zhistory->plotLine(electrode->PulseTimes.at(i), 5, electrode->PulseTimes.at(i) + electrode->PulseDurations.at(i), 5, Qt::red);
    }

    Zhistory->redrawPlot();
}


/* Update mainwindow's labels when Manual values are changed */
void MainWindow::updateManualLabels()
{
    if (manualParameters->electroplatingMode == ConstantCurrent)
        manualModeLabel->setText("Mode: Constant Current");
    else
        manualModeLabel->setText("Mode: Constant Voltage");
    manualValueLabel->setText((QString)"Value: " + QString::number(manualParameters->actualValue) + ((manualParameters->electroplatingMode == ConstantCurrent) ? (QString)" nA" : (QString)" V"));
    if (manualParameters->actualValue == 0)
        manualValueLabel->setText((QString)"Value: 0" + ((manualParameters->electroplatingMode == ConstantCurrent) ? (QString)" nA" : (QString)" V"));
    manualDurationLabel->setText("Duration: " + QString::number(manualParameters->duration) + " s");
}


/* Update mainwindow's labels when Automatic values are changed */
void MainWindow::updateAutomaticLabels()
{
    if (automaticParameters->electroplatingMode == ConstantCurrent)
        automaticModeLabel->setText("Mode: Constant Current");
    else
        automaticModeLabel->setText("Mode: Constant Voltage");
    automaticValueLabel->setText((QString)"Value: " + QString::number(automaticParameters->actualValue) + ((automaticParameters->electroplatingMode == ConstantCurrent) ? (QString)" nA" : (QString)" V"));
    if (automaticParameters->actualValue == 0)
        automaticValueLabel->setText((QString)"Value: 0" + ((automaticParameters->electroplatingMode == ConstantCurrent) ? (QString)" nA" : (QString)" V"));
    automaticInitialDurationLabel->setText("Duration: " + QString::number(automaticParameters->duration) + " s");
}


/* Read one impedance */
void MainWindow::readImpedance(int index, QProgressDialog *progress)
{
    bool enabled[] = {true, true, false, false, false, false, false, false};

    boardControl->dataStreams.configureDataStreams(enabled);
    boardControl->updateDataStreams();

    QtProgressWrapper progressWrapper(*progress);

    ImpedanceMeasureController *impedanceMeasureController = new ImpedanceMeasureController(*boardControl, progressWrapper, nullptr, true);

    if (firstRead) {
        delete impedanceMeasureController;
        impedanceMeasureController = new ImpedanceMeasureController(*boardControl, progressWrapper, nullptr, false);
    }

    firstRead = false;

    int datasource = index / 64;
    int channel = index % 64;

    bool okay_to_read;
    if (datasource == 0)
        okay_to_read = globalParameters->channels063Present;
    else
        okay_to_read = globalParameters->channels64127Present;

    if (okay_to_read) {
        std::complex<double> impedance = impedanceMeasureController->measureOneImpedance((Rhd2000EvalBoard::BoardDataSource)datasource, channel);
        dataProcessor->Electrodes[index]->add_measurement(impedance);
    }

    else
        dataProcessor->Electrodes[index]->reset_time();

    redrawImpedance();
}


/* Pulse either current or voltage (depending on mode), on the "selected" channel, with the "value" magnitude, for "duration" seconds */
void MainWindow::pulse(int selected, ElectroplatingMode mode, double value, double duration)
{
    //Record that we are pulsing
    dataProcessor->Electrodes[selected]->add_pulse(duration);

    //Figure out settings
    if (mode == ConstantVoltage) {
        ebc->setVoltage(value);
        boardControl->analogOutputs.setDacManualVolts(ebc->getDacManualActual());
    }
    else {
        ebc->setCurrent(value/1e9);
        boardControl->analogOutputs.setDacManualVolts(ebc->getDacManualActual());
    }

    //Plating below. Note that we set up the chip first, set the digital outputs, pulse, set the digital
    //outputs to turn plating off, set the chip. IN THAT ORDER. Settings digital outputs is instantaneous,
    //sending information to the board takes time.

    //Start plating
    ebc->setPlatingChannel(selected);
    boardControl->updateAnalogOutputSource(0);
    boardControl->updateDACManual();
    boardControl->beginPlating(ebc->effectiveChannel);
    bool out[16];
    ebc->getDigitalOutputs(out);
    bool wait = setRefDigitalOutput(out);
    boardControl->updateDigitalOutputs();
    if (wait)
        sleep(globalParameters->delayChangeRef * 1000);

    ebc->getDigitalOutputs(out);
    setNonrefDigitalValues(out);
    boardControl->updateDigitalOutputs();

    //Plate for the given duration
    sleep(duration * 1000);

    //Stop plating
    ebc->setZCheckChannel(selected);

    ebc->getDigitalOutputs(out);
    setNonrefDigitalValues(out);
    boardControl->updateDigitalOutputs();

    ebc->getDigitalOutputs(out);
    setRefDigitalOutput(out);
    boardControl->updateDigitalOutputs();

    //Leave DacManual at 0 V or 0 current
    if (ebc->getReferenceSelection())
        boardControl->evalBoard->setDacManual(3.3);
    else
        boardControl->evalBoard->setDacManual(0);

    boardControl->endImpedanceMeasurement();
}


/* Sets the vref digital output; pausing if the reference changes from 0 V to 3.3 V or vice versa */
bool MainWindow::setRefDigitalOutput(bool values[16])
{
    bool result = false;
    bool boolValues[16];
    for (int i = 0; i < 16; i++) {
        if (boardControl->digitalOutputs.values[i] == 1)
            boolValues[i] = true;
        else
            boolValues[i] = false;
    }
    //If REF_sel CHANGES
    if (boolValues[7] != values[7]) {
        //Change just that, and let it equilibrate
        if (values[7] == true)
            boardControl->digitalOutputs.values[7] = 1;
        else
            boardControl->digitalOutputs.values[7] = 0;
        sleep(globalParameters->delayChangeRef * 1000);
        result = true;
    }
    return result;
}


/* Sets the digital outputs, excluding the reference voltage */
void MainWindow::setNonrefDigitalValues(bool values[16])
{
    bool tmp[16];
    for (int i = 0; i < 16; i++) {
        tmp[i] = values[i];
    }
    if (boardControl->digitalOutputs.values[7] == 1)
        tmp[7] = true;
    else
        tmp[7] = false;
    for (int i = 0; i < 16; i++) {
        if (tmp[i])
            boardControl->digitalOutputs.values[i] = 1;
        else
            boardControl->digitalOutputs.values[i] = 0;
    }
}


/* Plate one channel automatically, with a maximum number of loops 'maxPulses' */
void MainWindow::plateOneAutomatically(int index, QProgressDialog *progress)
{
    QString originalLabelText = progress->labelText();

    //Don't want people hitting other buttons while plating
    setAllEnabled(false);

    //Clear history for the given electrode, and take a reading before we start
    dataProcessor->Electrodes[index]->reset_time();
    readImpedance(index, progress);

    //Make sure at start that target impedance hasn't already been reached
    if (globalParameters->useTargetZ) {
        //Keep going as long as the impedance is above the target
        QVector<ElectrodeImpedance> impedances = dataProcessor->get_impedances();

        int impedancesIndex = 0;
        for (int i = 0; i < impedances.size(); i++) {
            if (impedances[i].index == index) {
                impedancesIndex = i;
                break;
            }
        }

        double last_impedance_magnitude = abs(impedances[impedancesIndex].impedance);
        double target = targetImpedance->text().toDouble() * 1000;
        if (last_impedance_magnitude <= target) {
            setAllEnabled(true);
            return;
        }
    }

    //Basic algorith is simple:
    //while (impedance > target)
    //  plate for fixed time
    //  remeasure impedance

    //Do at most 'maxPulses' iterations; we don't want an infinite loop if one of the electrodes just isn't working
    int count = 0;
    do {
        if (progress->wasCanceled())
            break;

        //Delay before
        progress->setLabelText(originalLabelText + " - Pulsing");
        QApplication::processEvents();
        sleep(globalParameters->delayMeasurementPulse * 1000);
        QApplication::processEvents();

        if (progress->wasCanceled())
            break;

        //Pulse
        QApplication::processEvents();
        pulse(index, automaticParameters->electroplatingMode, automaticParameters->actualValue, automaticParameters->duration);
        QApplication::processEvents();

        if (progress->wasCanceled())
            break;

        //Delay after
        progress->setLabelText(originalLabelText + " - Measuring Impedance");
        QApplication::processEvents();
        sleep(globalParameters->delayPulseMeasurement * 1000);
        QApplication::processEvents();

        if (progress->wasCanceled())
            break;

        //Measure
        QApplication::processEvents();
        readImpedance(index, progress);
        QApplication::processEvents();

        //Refresh figure...
        count++;

    } while (keepGoing(count, index));

    //Re-enable things
    setAllEnabled(true);
}


/* Helper function used to determine if we should keep looping */
bool MainWindow::keepGoing(int count, int index)
{
    if (count >= globalParameters->maxPulses) {
        //Hit the maximum number of pulses; time to stop
        return false;
    }
    else {
        if (globalParameters->useTargetZ) {
            //Keep going as long as the impedance is above the target
            QVector<ElectrodeImpedance> impedances = dataProcessor->get_impedances();

            int impedancesIndex = 0;
            for (int i = 0; i < impedances.size(); i++) {
                if (impedances[i].index == index) {
                    impedancesIndex = i;
                    break;
                }
            }

            double last_impedance_magnitude = abs(impedances[impedancesIndex].impedance);
            double target = targetImpedance->text().toDouble() * 1000;
            return (last_impedance_magnitude > target);
        }
        else
            //Keep going until we hit MaxPulses limit above
            return true;
    }
}


/* Enable or disable all user-interactable widgets */
void MainWindow::setAllEnabled(bool enabled)
{
    selectedChannelSpinBox->setEnabled(enabled);
    manualConfigureButton->setEnabled(enabled);
    manualApplyButton->setEnabled(enabled);
    automaticConfigureButton->setEnabled(enabled);
    automaticRunButton->setEnabled(enabled);
    readAllImpedancesButton->setEnabled(enabled);
    continuousZScanButton->setEnabled(enabled);
    runAllButton->setEnabled(enabled);
    runSelectedChannelButton->setEnabled(enabled);
    run063Button->setEnabled(enabled);
    run64127Button->setEnabled(enabled);
    runCustomButton->setEnabled(enabled);
    magnitudeButton->setEnabled(enabled);
    phaseButton->setEnabled(enabled);
    // These spinboxes also depend on if runCustomButton is currently selected:
    bool enabledSpinBoxes = runCustomButton->isChecked() && enabled;
    customLowSpinBox->setEnabled(enabledSpinBoxes);
    customHighSpinBox->setEnabled(enabledSpinBoxes);
}

/* Sleep (suspend execution of current thread) for a given amount of ms */
void MainWindow::sleep(double ms)
{
    QTimer *timer = new QTimer();
    sleeptimerdone = false;
    connect(timer, SIGNAL(timeout()), this, SLOT(sleeptimerupdate()));
    timer->start(ms);
    while (!sleeptimerdone) {
        QApplication::processEvents();
    }
    delete timer;
}
