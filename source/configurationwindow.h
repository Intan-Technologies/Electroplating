#ifndef CONFIGURATIONWINDOW_H
#define CONFIGURATIONWINDOW_H

#include <QWidget>
#include <QDialog>
#include <QGroupBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QLabel>
#include "electroplatingboardcontrol.h"

/* ConfigurationWindow is a class that creates a window to configure a pulse for either manual or automatic pulsing */

struct ConfigurationParameters;
class ConfigurationWindow : public QDialog
{
    Q_OBJECT

public:
    explicit ConfigurationWindow(ConfigurationParameters *parameters, ElectroplatingBoardControl *ebcObj, QWidget *parent = 0); //Constructor

private:
    void updateBestAchievableValue(); //Display the best achievable value close to the desired value entered by the user

    ConfigurationParameters *params; //Structure that holds the mode, sign, value (actual and desired), and duration of the pulse
    ElectroplatingBoardControl *ebc; //Class that allows simplified control of the electroplating board (used to find the best achievable value from the user's desired value)

    QGroupBox *currentParametersGroupBox;
    QComboBox *electroplatingModeComboBox;
    QComboBox *signComboBox;
    QDialogButtonBox *buttonBox;
    QLineEdit *desiredValue;
    QDoubleValidator *desiredValueValidator;
    QLineEdit *duration;
    QDoubleValidator *durationValidator;
    QLabel *desiredValueUnits;
    QLabel *bestAchievableValue;
    QLabel *durationUnits;
    double valueActual;

private slots:
    void valuesChanged(); //Slot that is called when the desired value, sign, or duration is changed. Provides feedback to the user if the values are not valid
    void electroplatingModeChanged(); //Slot that is called when the desired mode (current or voltage) is changed. Updates units and value range based on voltage or current
    void accept(); //Reimplemented slot that is called when the user accepts the dialog. Passes current user inputs into 'params' structure, so they are accessible by the parent
    void reject(); //Reimplemented slot that is called when the user rejects the dialog

};

#endif // CONFIGURATIONWINDOW_H
