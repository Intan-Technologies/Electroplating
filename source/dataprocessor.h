#ifndef DATAPROCESSOR_H
#define DATAPROCESSOR_H

#include <QVector>
#include "electrodeimpedance.h"

/* DataProcessor is a class that saves impedances, and saves and loads settings */

class OneElectrode;
struct Settings;
class DataProcessor
{
public:
    DataProcessor(); //Constructor
    ~DataProcessor(); //Destructor
    QVector<ElectrodeImpedance> get_impedances(); //Gets the most recently measured impedances, returning both indices of electrodes whose impedances have been measured & impedances as complex numbers
    void save_impedances(QString filename); //Save the current impedances to 'filename'
    void save_settings(QString filename, Settings &settings); //Save the current settings to 'filename'
    void load_settings(QString filename, Settings &settings); //Load settings from 'filename'

    OneElectrode *Electrodes[128];

private:
    int Selected; //Selected channel (0-127)
    bool DisplayMagnitudes; //True for magnitudes, false for phases
};

#endif // DATAPROCESSOR_H
