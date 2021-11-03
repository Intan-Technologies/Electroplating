#include "electroplatingboardcontrol.h"

#include <QtCore>


/* Constructor */
ElectroplatingBoardControl::ElectroplatingBoardControl()
{
    currentSourceEnable = false;
    currentSinkEnable = false;
    currentModeEnable = false;
    resistorSelection = 0;
    elecTest1 = false;
    elecTest2 = false;
    dataSource = 0; //Port A, MISO 1
    effectiveChannel = 0;
    setVoltage(0);
    setCurrent(0);
    setPlatingChannel(0);
    setZCheckChannel(0);
    dacManualActual = 0;
    voltageActual = 0;
    currentActual = 0;
    rangeSel0 = 0;
    rangeSel1 = 0;
    setChannel(0);
    for (int index = 0; index < 16; index++) {
        digitalOutputs[index] = 0;
    }
    resistors[0] = 100e6;
    resistors[1] = 10e6;
    resistors[2] = 1e6;
    resistors[3] = 100e3;
    pulseReferenceSelection = 0;
    zCheckMode = true;
}


/* Public - Set to current mode with the given current */
void ElectroplatingBoardControl::setCurrent(double value)
{
    bool good = false;

    double voltages[4];
    for (int i = 0; i < 4; i++) {
        voltages[i] = value * resistors[i];
    }

    for (int i = 0; i < 4; i++) {
        double v = voltages[i];
        if ((v >= -1.0001) && (v <= 1.0001)) {
            resistorSelection = i;
            good = true;

            if (v >= 0) {
                pulseReferenceSelection = false;
                dacManualDesired = 3.3 - v;
            }
            else {
                pulseReferenceSelection = true;
                dacManualDesired = v * -1;
            }

            if (dacManualDesired > 3.3)
                dacManualDesired = 3.3;
            if (dacManualDesired < 0)
                dacManualDesired = 0;
            break;
        }
    }

    if (!good)
        qDebug() << "Couldn't set value; valid range is -10uA..10uA";

    currentSourceEnable = (value >= 0);
    currentSinkEnable = (value < 0);
    currentModeEnable = true;
    zCheckMode = false;
}


/* Public - Set to voltage mode with the given voltage */
void ElectroplatingBoardControl::setVoltage(double value)
{
    currentSourceEnable = false;
    currentSinkEnable = false;
    currentModeEnable = false;

    if (value >= 0) {
        pulseReferenceSelection = false;
        dacManualDesired = value;
    }
    else {
        pulseReferenceSelection = true;
        dacManualDesired = 3.3 + value;
    }

    if (dacManualDesired > 3.3)
        dacManualDesired = 3.3;
    if (dacManualDesired < 0)
        dacManualDesired = 0;
    zCheckMode = false;
}


/* Public - Set to plating mode, with the given channel (value) */
void ElectroplatingBoardControl::setPlatingChannel(int value)
{
    setChannel(value);

    if (value <= 63) {
        elecTest1 = true;
        elecTest2 = false;
    }
    else {
        elecTest1 = false;
        elecTest2 = true;
    }

    if (currentModeEnable) {
        currentSourceEnable = !(getReferenceSelection());
        currentSinkEnable = getReferenceSelection();
    }
    zCheckMode = false;
}


/* Public - Set to zcheck mode, with the given channel (value) */
void ElectroplatingBoardControl::setZCheckChannel(int value)
{
    setChannel(value);

    elecTest1 = false;
    elecTest2 = false;
    currentSourceEnable = false;
    currentSinkEnable = false;
    zCheckMode = true;
}


/* Public - Set data source and effective channel, based on the desired headstage channel */
void ElectroplatingBoardControl::setChannel(int channel)
{
    if ((channel < 0) || (channel > 127))
        qDebug() << "Value must be 0...127";

    if (channel <= 63) {
        dataSource = 0;
        effectiveChannel = channel;
    }
    else {
        dataSource = 1;
        effectiveChannel = channel - 64;
    }
}


/* Public - Get the desired headstage channel, from the data source and effective channel */
int ElectroplatingBoardControl::getChannel()
{
    int value = 64 * dataSource + effectiveChannel;
    return value;
}


/* Public - Get best achievable DacManual, given hardware constraints */
double ElectroplatingBoardControl::getDacManualActual()
{
    double num_steps = pow(2, 16);
    double step_size = 3.3/num_steps;
    int best_achievable_int = round(dacManualDesired/step_size);
    return best_achievable_int * step_size;
}


/* Public - Get best achievable voltage, given hardware constraints */
double ElectroplatingBoardControl::getVoltageActual()
{
    double value;
    if (currentSourceEnable || currentSinkEnable)
        value = 0;
    else {
        if (getReferenceSelection())
            //Negative
            value = getDacManualActual() - 3.3;
        else
            //Positive
            value = getDacManualActual();
    }
    return value;
}


/* Public - Get best achievable current, given hardware constraints */
double ElectroplatingBoardControl::getCurrentActual()
{
    double value;
    if (currentSourceEnable)
        value = (3.3 - getDacManualActual()) / resistors[resistorSelection];
    else if (currentSinkEnable)
        value = (-1 * getDacManualActual()) / resistors[resistorSelection];
    else
        value = 0;
    return value;
}


/* Public - Get the value of the RANGE_SEL_0 control line */
int ElectroplatingBoardControl::getRangeSel0()
{
    return resistorSelection & 1;
}


/* Public - Get the value of the RANGE_SEL_1 control line */
int ElectroplatingBoardControl::getRangeSel1()
{
    return ((resistorSelection & 2) >> 1);
}


/* Public - Get the value of the REF_SEL control line */
bool ElectroplatingBoardControl::getReferenceSelection()
{
    bool value;
    if (zCheckMode)
        value = false;
    else
        value = pulseReferenceSelection;
    return value;
}


/* Public - Get the values of the digital outputs for these settings */
void ElectroplatingBoardControl::getDigitalOutputs(bool out[])
{
    bool value[16];
    for (int i = 0; i < 16; i++) {
        value[i] = 0;
    }
    value[0] = currentSinkEnable;
    value[1] = currentSourceEnable;
    value[2] = currentModeEnable;
    value[3] = (getRangeSel0() == 1) ? true : false;
    value[4] = (getRangeSel1() == 1) ? true : false;
    value[5] = elecTest1;
    value[6] = elecTest2;
    value[7] = getReferenceSelection();

    for (int i = 0; i < 16; i++) {
        out[i] = value[i];
    }
}
