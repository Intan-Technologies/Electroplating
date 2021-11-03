#ifndef ELECTROPLATINGBOARDCONTROL_H
#define ELECTROPLATINGBOARDCONTROL_H

/* ElectroplatingBoardControl is a class that allows the setting and getting of various parameters used to control the electroplating board
 *
 * Calculations for the electroplating board
 *
 * The electroplating board has 8 digital control lines:
 * 0    I_SINK_EN
 * 1    I_SOURCE_EN
 * 2    I_MODE_EN
 * 3    RANGE_SEL_0
 * 4    RANGE_SEL_1
 * 5    ELEC_TEST1
 * 6    ELEC_TEST2
 * 7    REF_SEL
 *
 * See the datasheet for descriptions of what they do.
 *
 * This class encapsulates the details of the operation of the conrol lines. Instead of setting them directly,
 * you set Voltage or Current and PlatingChannel or ZCheckChannel. */

class ElectroplatingBoardControl
{

public:
    ElectroplatingBoardControl(); //Constructor
    void setCurrent(double value); //Set to current mode with the given current
    void setVoltage(double value); //Set to voltage mode with the given voltage
    void setPlatingChannel(int channel); //Set to plating mode, with the given channel (value)
    void setZCheckChannel(int channel); //Set to zcheck mode, with the given channel (value)
    void setChannel(int channel); //Set data source and effective channel, based on the desired headstage channel
    int getChannel(); //Get the desired headstage channel, from the data source and effective channel
    double getDacManualActual(); //Get best achievable DacManual, given hardware constraints
    double getVoltageActual(); //Get best achievable voltage, given hardware constraints
    double getCurrentActual(); //Get best achievable current, given hardware constraints
    int getRangeSel0(); //Get the value of the RANGE_SEL_0 control line
    int getRangeSel1(); //Get the value of the RANGE_SEL_1 control line
    bool getReferenceSelection(); //Get the value of the REF_SEL control line
    void getDigitalOutputs(bool out[16]); //Get the values of the digital outputs for these settings

    bool currentSourceEnable; //Value of I_SOURCE_EN control line
    bool currentSinkEnable; //Value of I_SINK_EN control line
    bool currentModeEnable; //Value of I_MODE_EN control line
    int resistorSelection; //Which of the four resistors to use: 0 -> 100 MOhm ... 1 -> 10 MOhm ... 2 -> 1 MOhm ... 3 -> 100 kOhm ... The values RangeSel0 and RangeSel1 are derived from this
    bool elecTest1; //Value of ELEC_TEST1 control line
    bool elecTest2; //Value of ELEC_TEST2 control line
    double dacManualDesired; //The voltage you need to set DacManual
    // For example, if you want to electroplate with 1.0 V, this would be 1.0
    // If you want to electroplate with -1.0, the reference would be +3.3 V, and this value would be 2.3 (2.3 - 3.3 = -1.0).

    int dataSource; //Which datasource to use when measuring impedance: 0 (i.e., Port A MISO 1) for channels 0-63 ... 1 (i.e., Port A MISO 2) for channels 64-127
    int effectiveChannel; //Which channel to pass to impedance measurement calls. For example, channel 127 would be DataSource = 1, EffectiveChannel = 63.
    double resistors[4]; //Array of 4 resistors: 100M, 10M, 1M, and 100k
    bool zCheckMode; //Stores if board should do impedance checking (true) or not
    double voltage; //Stores voltage
    double current; //Stores current
    double platingChannel; //Stores the channel to be plated
    double zCheckChannel; //Stores the channel to have its impedance checked
    double dacManualActual; //Best achievable DacManual, given hardware constraints
    double voltageActual; //Best achievable voltage, given hardware constraints
    double currentActual; //Best achievable current, given hardware constraints
    int rangeSel0; //Value of the RANGE_SEL_0 control line
    int rangeSel1; //Value of the RANGE_SEL_1 control line
    int channel; //Current ZCheck or Plating channel (0-127)
    bool digitalOutputs[16]; //Values of the digital outputs for these settings
    bool pulseReferenceSelection; //Value of the REF_SEL control line
};

#endif // ELECTROPLATINGBOARDCONTROL_H
