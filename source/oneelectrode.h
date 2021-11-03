#ifndef ONEELECTRODE_H
#define ONEELECTRODE_H
#include <QVector>
#include <complex>

class QElapsedTimer;

/* One Electrode is a class built to store the data from one electrode
 * Used for storing data from the given electrode only, no control functions. */

class OneElectrode
{
public:
    OneElectrode(); //Constructor
    void reset_time(); //Clears history and sets InitialTime
    void add_measurement(std::complex<double> value); //Adds 'value' to the list of impedance measurements
    void add_pulse(double duration); //Adds a pulse of duration 'duration' to the list of pulses
    std::complex<double> get_current_impedance(); //Return Impedance History
    double get_elapsed_time(); //Return the amount of elapsed time since InitialTime

    QVector<std::complex<double>> ImpedanceHistory; //Array of measured complex impedance values (also see MeasurementTimes)
    QVector<double> MeasurementTimes; //Array of times (in seconds) when impedance was measured (also see ImpedanceHistory, InitialTime)
    QVector<double> PulseTimes; //Array of times (in seconds) when pulses were applied (also see PulseDurations, InitialTime)
    QVector<double> PulseDurations; //Array of durations of applied pulses (also see PulseTimes)
    double InitialTime; //Absolute time that corresponds to 0. Reset this with reset_time(). All MeasurementTimes and PulseTimes are seconds after this (also see MeasurementTimes, PulseTimes, reset_time)

private:
    QElapsedTimer *elapsedTimer;
};

#endif // ONEELECTRODE_H
