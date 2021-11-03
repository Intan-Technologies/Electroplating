#include "dataprocessor.h"
#include "globalconstants.h"
#include "oneelectrode.h"
#include "settings.h"
#include "electrodeimpedance.h"
#include <QFile>
#include <QMessageBox>
#include <QtCore>

using namespace std;

/* Constructor */
DataProcessor::DataProcessor()
{
    //Allocate memory for 128 'OneElectrode' objects
    for (int i = 0; i < 128; i++) {
        Electrodes[i] = new OneElectrode;
    }
}

/* Destructor */
DataProcessor::~DataProcessor()
{
    //Free memory for 128 'OneElectrode' objects
    for (int i = 0; i < 128; i++) {
        delete Electrodes[i];
    }
}

/* Public - Gets the most recently measured impedances, returning both indices of electrodes whose impedances have been measured & impedances as complex numbers */
QVector<ElectrodeImpedance> DataProcessor::get_impedances()
{
    std::complex<double> impedances_tmp[128];
    int valid[128];
    QVector<ElectrodeImpedance> impedances;

    //Initialize impedances' real and imaginary components to 0, and valid to 0
    for (int i = 0; i < 128; i++) {
        impedances_tmp[i].real(0);
        impedances_tmp[i].imag(0);
        valid[i] = 0;
    }

    //If this electrode has a non-empty impedance history, set it to valid, and get the most recently measured impedance on this electrode
    for (int i = 0; i < 128; i++) {
        if (!Electrodes[i]->ImpedanceHistory.isEmpty()) {
            valid[i] = 1;
            impedances_tmp[i] = Electrodes[i]->get_current_impedance();
        }
    }

    //For all the valid electrodes, append the impedance and index to 'impedances' QVector
    for (int i = 0; i < 128; i++) {
        if (valid[i] == 1) {
            ElectrodeImpedance thisElectrode;
            thisElectrode.impedance = impedances_tmp[i];
            thisElectrode.index = i;
            impedances.append(thisElectrode);
        }
    }
    return impedances;
}

/* Public - Save the current impedances to 'filename' */
void DataProcessor::save_impedances(QString filename)
{
    //Save the current impedances to 'filename'.
    QFile impedancesFile(filename);
    if (!impedancesFile.open(QIODevice::WriteOnly)) {
        QMessageBox::critical(0, "Cannot Save Impedances File",
                              "Cannot open new csv file for writing.");
        return;
    }

    /* Save Impedances */
    //Set up data stream
    QDataStream outStream(&impedancesFile);
    outStream.setVersion(QDataStream::Qt_4_8);
    outStream.setByteOrder(QDataStream::LittleEndian);
    outStream.setFloatingPointPrecision(QDataStream::SinglePrecision);

    //Write the 'label' row, listing the labels associated with each entry of data
    QByteArray label = "Channel Number,Channel Name,Port,Enabled,Impedance Magnitude at 1000 Hz (ohms),Impedance Phase at 1000 Hz (degrees),Series RC equivalent R (Ohms),Series RC equivalent C (Farads)\n";
    outStream.writeRawData(label.constData(), label.length());

    //For each electrode with a non-empty impedance history, write data
    for (int i = 0; i < 128; i++) {
        if (!Electrodes[i]->ImpedanceHistory.empty()) {
            std::complex<double> impedance = Electrodes[i]->get_current_impedance();
            QByteArray data;
            //Append channel number, channel name (identical to channel number), port, and enabled (always true) into 'data'
            data.append(QString("A-%1,A-%2,Port A,1,").arg(QString::number(i), 3, QLatin1Char('0')).arg(QString::number(i), 3, QLatin1Char('0')));

            //Append impedance magnitude into 'data'
            data.append(QString("%1,").arg(QString::number(std::abs(impedance), 'e', 2)));

            //Append impedance phase into 'data'
            data.append(QString("%1,").arg(QString::number(std::arg(impedance) * RADIANS_TO_DEGREES, 'f', 0)));

            //Append equivalent R into 'data'
            data.append(QString("%1,").arg(QString::number(impedance.real(), 'e', 2)));

            //Append equivalent C into 'data'
            double frequency = 1000;
            data.append(QString("%1\n").arg(QString::number(1/(-2 * PI * frequency * impedance.imag()), 'e', 2)));

            //Write 'data' to file
            outStream.writeRawData(data.constData(), data.length());
        }
    }
    impedancesFile.close();
}

/* Public - Save settings to 'filename' */
void DataProcessor::save_settings(QString filename, Settings &settings)
{
    //Save the current settings to 'filename'.
    QFile settingsFile(filename);
    if (!settingsFile.open(QIODevice::WriteOnly)) {
        QMessageBox::critical(0, "Cannot Save Settings File",
                              "Cannot open new settings file for writing.");
        return;
    }

    /* Save Settings */
    //Set up data stream
    QDataStream outStream(&settingsFile);
    outStream.setVersion(QDataStream::Qt_4_8);
    outStream.setByteOrder(QDataStream::LittleEndian);
    outStream.setFloatingPointPrecision(QDataStream::SinglePrecision);

    //Write general settings file numbers
    outStream << (quint32) SETTINGS_FILE_MAGIC_NUMBER;
    outStream << (qint16) SETTINGS_FILE_MAIN_VERSION_NUMBER;
    outStream << (qint16) SETTINGS_FILE_SECONDARY_VERSION_NUMBER;

    //Write automatic pulse settings
    outStream << (qint16) settings.automaticIsVoltageMode;
    outStream << settings.automaticValue;
    outStream << settings.automaticDesired;
    outStream << settings.automaticDuration;

    //Write manual pulse settings
    outStream << (qint16) settings.manualIsVoltageMode;
    outStream << settings.manualValue;
    outStream << settings.manualDesired;
    outStream << settings.manualDuration;

    //Write global settings
    outStream << settings.threshold;
    outStream << (qint16) settings.maxPulses;
    outStream << settings.delayBeforePulse;
    outStream << settings.delayAfterPulse;
    outStream << settings.delayChangeRef;
    outStream << settings.delayZScan;
    outStream << (qint16) settings.channels0to63;
    outStream << (qint16) settings.channels64to127;
    outStream << (qint16) settings.useTargetImpedance;
    outStream << (qint16) settings.selected;
    outStream << (qint16) settings.showGrid;

    settingsFile.close();
}

/* Public - Load settings from 'filename' */
void DataProcessor::load_settings(QString filename, Settings &settings)
{
    //Load settings from 'filename'
    QFile settingsFile(filename);
    if (!settingsFile.open(QIODevice::ReadOnly)) {
        QMessageBox::critical(0, "Cannot Load Settings File",
                              "Cannot open new settings file for reading.");
        return;
    }

    /* Load settings */
    QDataStream inStream(&settingsFile);
    inStream.setVersion(QDataStream::Qt_4_8);
    inStream.setByteOrder(QDataStream::LittleEndian);
    inStream.setFloatingPointPrecision(QDataStream::SinglePrecision);

    //Set up variables to read data into
    qint16 tempQint16;
    quint32 tempQuint32;
    int versionMain, versionSecondary;

    //Check the magic number at the beginning of the file to make sure the file is valid
    inStream >> tempQuint32;
    if (tempQuint32 != SETTINGS_FILE_MAGIC_NUMBER) {
        QMessageBox::critical(0, "Cannot Parse Settings File",
                              "Selected file is not a valid settings file.");
        return;
    }

    //Read the main and secondary versions
    inStream >> tempQint16;
    versionMain = tempQint16;
    inStream >> tempQint16;
    versionSecondary = tempQint16;

    //Eventually check version number here for compatibility issues - not currently necessary

    //Read automatic pulse settings
    inStream >> tempQint16;
    settings.automaticIsVoltageMode = (bool) tempQint16;
    inStream >> settings.automaticValue;
    inStream >> settings.automaticDesired;
    inStream >> settings.automaticDuration;

    //Read manual pulse settings
    inStream >> tempQint16;
    settings.manualIsVoltageMode = (bool) tempQint16;
    inStream >> settings.manualValue;
    inStream >> settings.manualDesired;
    inStream >> settings.manualDuration;

    //Read global settings
    inStream >> settings.threshold;
    inStream >> tempQint16;
    settings.maxPulses = tempQint16;
    inStream >> settings.delayBeforePulse;
    inStream >> settings.delayAfterPulse;
    inStream >> settings.delayChangeRef;
    inStream >> settings.delayZScan;
    inStream >> tempQint16;
    settings.channels0to63 = (bool) tempQint16;
    inStream >> tempQint16;
    settings.channels64to127 = (bool) tempQint16;
    inStream >> tempQint16;
    settings.useTargetImpedance = (bool) tempQint16;
    inStream >> tempQint16;
    settings.selected = tempQint16;
    inStream >> tempQint16;
    settings.showGrid = tempQint16;

    settingsFile.close();
}
