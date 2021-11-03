#ifndef CONFIGURATIONPARAMETERS_H
#define CONFIGURATIONPARAMETERS_H

/* Configuration Parameters */
/* Structure used to store various parameters of how an electroplating pulse should be applied */

/* ElectroplatingMode: Refers to the pulse having a constant current or a constant voltage, both of whose magnitudes are represented by 'actualValue' */
enum ElectroplatingMode {
    ConstantCurrent,
    ConstantVoltage
};

/* Sign: Refers to the pulse as having either positive or negative current or voltage */
enum Sign {
    Negative,
    Positive
};

struct ConfigurationParameters {
    ElectroplatingMode electroplatingMode; //Constant Current or Constant Voltage
    Sign sign; //Positive or Negative
    float desiredValue; //User-desired current or voltage magnitude
    float actualValue; //Closest possible hardware-possible current or voltage magnitude
    float duration; //Duration of pulse
};

#endif // CONFIGURATIONPARAMETERS_H
