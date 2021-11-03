#ifndef ELECTRODEIMPEDANCE_H
#define ELECTRODEIMPEDANCE_H

#include <complex>

//Structure used to store the complex impedance of a channel, as well as an index representing which channel that impedance is associated with

struct ElectrodeImpedance {
    int index;
    std::complex<double> impedance;
};

#endif // IMPEDANCE_H
