#ifndef GLOBALPARAMETERS_H
#define GLOBALPARAMETERS_H

/* Structure that holds the max number of pulses, various delays, whether or not channels 0-63 or 64-127 are present, and whether or not to use the target impedance */
struct GlobalParameters {
    int maxPulses;
    float delayMeasurementPulse;
    float delayPulseMeasurement;
    float delayChangeRef;
    float continuousZDelay;
    bool channels063Present;
    bool channels64127Present;
    bool useTargetZ;
};


#endif // GLOBALPARAMETERS_H
