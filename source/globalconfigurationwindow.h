#ifndef GLOBALCONFIGURATIONWINDOW_H
#define GLOBALCONFIGURATIONWINDOW_H

#include <QDialog>
#include <QLineEdit>

/* GlobalConfigurationWindow is a class that creates a window to configure the global parameters for all pulses and reading of impedances */

struct GlobalParameters;
class QCheckBox;
class QRadioButton;
class GlobalConfigurationWindow : public QDialog
{
    Q_OBJECT
public:
    explicit GlobalConfigurationWindow(GlobalParameters *parameters, QWidget *parent = 0); //Constructor

private:
    void accept(); //Reimplemented slot that is called when the user accepts the dialog. Passes current user inputs into 'params' structure, so they are accessible by the parent
    void reject(); //Reimplemented slot that is called when the user rejects the dialog
    GlobalParameters *params; //Structure that holds the max number of pulses, various delays, whether or not channels 0-63 or 64-127 are present, and whether or not to use the target impedance
    QLineEdit *maxPulses;
    QLineEdit *delayMeasurementPulse;
    QLineEdit *delayPulseMeasurement;
    QLineEdit *delayChangeRef;
    QLineEdit *continuousZDelay;
    QCheckBox *channels063Present;
    QCheckBox *channels64127Present;
    QRadioButton *useTargetZ;
    QRadioButton *noTargetZ;

};

#endif // GLOBALCONFIGURATIONWINDOW_H
