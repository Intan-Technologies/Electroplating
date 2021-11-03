#include "oneelectrode.h"
#include <complex>
#include <QVector>
#include <QtCore>


/* Constructor */
OneElectrode::OneElectrode()
{
    elapsedTimer = new QElapsedTimer;
    elapsedTimer->start();
    reset_time();
}


/* Clears history and sets InitialTime */
void OneElectrode::reset_time()
{
    elapsedTimer->restart();
    InitialTime = elapsedTimer->elapsed();

    ImpedanceHistory.remove(0, ImpedanceHistory.size());
    MeasurementTimes.remove(0, MeasurementTimes.size());
    PulseTimes.remove(0, PulseTimes.size());
    PulseDurations.remove(0, PulseDurations.size());
}


/* Adds 'value' to the list of impedance measurements */
void OneElectrode::add_measurement(std::complex<double> value)
{
    double time;
    ImpedanceHistory.append(value);
    if (MeasurementTimes.isEmpty()) {
        elapsedTimer->restart();
        InitialTime = elapsedTimer->elapsed();
        time = 0;
    }
    else
        time = get_elapsed_time();

    MeasurementTimes.append(time);
}


/* Adds a pulse of duration 'duration' to the list of pulses */
void OneElectrode::add_pulse(double duration)
{
    double time;
    time = get_elapsed_time();
    PulseTimes.append(time);
    PulseDurations.append(duration);
}


/* Return Impedance History */
std::complex<double> OneElectrode::get_current_impedance()
{
    return ImpedanceHistory.last();
}


/* Return the amount of elapsed time since InitialTime */
double OneElectrode::get_elapsed_time()
{
    return (elapsedTimer->elapsed() - InitialTime)/1000;
}
