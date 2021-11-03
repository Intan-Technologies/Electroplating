#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include "rhd2000evalboard.h"
#include "complex.h"
#include <complex>
#include "configurationparameters.h"
#include <QProgressDialog>

struct ConfigurationParameters;
struct GlobalParameters;
struct Settings;
class ImpedancePlot;
class GlobalConfigurationWindow;
class ConfigurationWindow;
class QLabel;
class QSpinBox;
class QRadioButton;
class QPushButton;
class QCheckBox;
class SignalProcessor;
class DataProcessor;
class QLineEdit;
class BoardControl;
class ElectroplatingBoardControl;
class SignalSources;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = 0); //Constructor
    ~MainWindow(); //Destructor

private slots:
    void runCustomChanged(); //If the user changed whether or not run custom is checked, enable/disable custom spinboxes
    void impedanceDisplayChanged(); //If the user has changed impedance display (phase or magnitude), redraw the plots
    void findClosestChannel(double clickXPosition); //When the currentZ plot is clicked, set the "Selected" channel to the channel closest to the x-position of the click
    void configure(); //Open a new Global Configuration Window, and pass it globalParameters to save (if OK is clicked)
    void saveSettings(); //Save settings to .set file
    void loadSettings(); //Load settings from a .set file
    void saveImpedances(); //Save impedances to a .csv file
    void openIntanWebsite(); //Open Intan's website in the user's default internet browser
    void about(); //Pop up dialog displaying information about this program
    void manualConfigureSlot(); //Open a new Configuration Window, and pass it manualParameters to save (if OK is clicked)
    void manualApplySlot(); //Read impedance, apply a manual pulse, and read impedance again for the currently selected channel
    void automaticConfigureSlot(); //Open a new Configuration Window, and pass it automaticParameters to save (if OK is clicked) FINISHED
    void automaticRunSlot(); //Apply automatic electroplating pulses to all desired channels, reading and plating in a loop for each channel
    void readAllImpedancesSlot(); //Read all 128 channels' impedances sequentially
    void continuousZScanSlot(); //Read the currently selected channel's impedance continuously until user clicks 'Cancel'
    void targetImpedanceChanged(QString impedance); //If the user has changed the target impedance, redraw the plots
    void selectedChannelChanged(); //If the user has changed the select channel, update the GUI and impedance plots to reflect the new channel
    void showGridChanged(bool grid); //If the user has toggled the show grid checkbox, update the impedance plots to reflect the new setting
    void timerupdate();
    void sleeptimerupdate();

private:
    void connectToBoard(); //Connect to Opal Kelly XEM6010 board and upload .bit file
    void scanPort(); //Scan SPI Port to identify all connected RHD2000 amplifier chips
    void changeSampleRate(Rhd2000EvalBoard::AmplifierSampleRate sampleRate); //Upload and execute board commands to change sample rate
    void initializeParams(); //Initialize manual, automatic, and global parameters with default values
    void initializeSettings(); //Initialize settings with default values
    void redrawImpedance(); //Update and redraw impedance plots, as well as GUI impedance display widget
    void setImpedanceTitle(); //Update impedance plot "currentZ" title
    void updatePresentPlotMode(); //Update impedance plot "currentZ" title, y label, and y range depending on the display of magnitude or phase
    void drawAllImpedances(); //Draw "currentZ" impedances plot
    void drawImpedanceHistory(); //Draw "Zhistory" impedances plot
    void updateManualLabels(); //Update mainwindow's labels when Manual values are changed
    void updateAutomaticLabels(); //Update mainwindow's labels when Automatic values are changed
    void readImpedance(int index, QProgressDialog *progress); //Read one impedance
    void pulse(int selected, ElectroplatingMode mode, double value, double duration); //Pulse either current or voltage (depending on mode), on the "selected" channel, with the "value" magnitude, for "duration" seconds
    bool setRefDigitalOutput(bool values[16]); //Sets the vref digital output; pausing if the reference changes from 0 V to 3.3 V or vice versa
    void setNonrefDigitalValues(bool values[16]); //Sets the digital outputs, excluding the reference voltage
    void plateOneAutomatically(int index, QProgressDialog *progress); //Plate one channel automatically, with a maximum number of loops 'maxPulses'
    bool keepGoing(int count, int index); //Helper function used to determine if we should keep looping
    void setAllEnabled(bool enabled); //Enable or disable all user-interactable widgets
    void sleep(double ms); //Sleep (suspend execution of current thread) for a given amount of ms

    /* Board Control variables */
    ElectroplatingBoardControl *ebc;
    BoardControl *boardControl;
    bool connected;
    bool firstRead = true;

    bool timerdone;
    bool sleeptimerdone;
    SignalSources *signalSources;

    /* Signal Processor variables */
    SignalProcessor *signalProcessor;
    double notchFilterFrequency;
    double notchFilterBandwidth;
    bool notchFilterEnabled;
    double highpassFilterFrequency;
    bool highpassFilterEnabled;

    /* Selected Channel widgets */
    QSpinBox *selectedChannelSpinBox;
    QLabel *selectedChannelSpinBoxLabel;

    /* Manual widgets */
    QLabel *manualChannelLabel;
    QLabel *manualModeLabel;
    QLabel *manualValueLabel;
    QLabel *manualDurationLabel;
    QPushButton *manualConfigureButton;
    QPushButton *manualApplyButton;

    /* Automatic widgets */
    QLabel *automaticModeLabel;
    QLabel *automaticValueLabel;
    QLabel *automaticInitialDurationLabel;
    QPushButton *automaticConfigureButton;

    /* Run widgets */
    QLineEdit *targetImpedance;
    QRadioButton *runAllButton;
    QRadioButton *runSelectedChannelButton;
    QRadioButton *run063Button;
    QRadioButton *run64127Button;
    QPushButton *automaticRunButton;
    QRadioButton *magnitudeButton;
    QRadioButton *phaseButton;
    QPushButton *readAllImpedancesButton;
    QPushButton *continuousZScanButton;
    QRadioButton *runCustomButton;
    QSpinBox *customLowSpinBox;
    QSpinBox *customHighSpinBox;

    /* Impedance Plot widgets */
    ImpedancePlot *currentZ;
    ImpedancePlot *Zhistory;

    /* Actions to add to menus */
    QAction *configureAction;
    QAction *saveSettingsAction;
    QAction *loadSettingsAction;
    QAction *saveImpedancesAction;
    QAction *intanWebsiteAction;
    QAction *aboutAction;

    /* Menus to add to menu bar */
    QMenu *settingsMenu;
    QMenu *helpMenu;

    /* Parameters */
    ConfigurationParameters *manualParameters;
    ConfigurationParameters *automaticParameters;
    GlobalParameters *globalParameters;

    /* Settings */
    Settings *settings;

    /* Data Processor */
    DataProcessor *dataProcessor;

    /* Y position of target impedance */
    double targetImpedanceLine;

    /* Show Grid checkbox */
    QCheckBox *showGrid;
};

#endif // MAINWINDOW_H
