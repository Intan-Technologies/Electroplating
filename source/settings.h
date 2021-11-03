#ifndef SETTINGS_H
#define SETTINGS_H

struct Settings {
    bool automaticIsVoltageMode;
    double automaticValue;
    double automaticDesired;
    double automaticDuration;
    bool manualIsVoltageMode;
    double manualValue;
    double manualDesired;
    double manualDuration;
    double threshold;
    int maxPulses;
    double delayBeforePulse;
    double delayAfterPulse;
    double delayChangeRef;
    double delayZScan;
    bool channels0to63;
    bool channels64to127;
    bool useTargetImpedance;
    int selected;
    bool displayMagnitudes;
    bool showGrid;
};


#endif // SETTINGS_H
