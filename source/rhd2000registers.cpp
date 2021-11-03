//----------------------------------------------------------------------------------
// rhd2000registers.cpp
//
// Intan Technoloies RHD2000 Rhythm Interface API
// Rhd2000Registers Class
// Version 1.4 (26 February 2014)
//
// Copyright (c) 2013-2014 Intan Technologies LLC
//
// This software is provided 'as-is', without any express or implied warranty.
// In no event will the authors be held liable for any damages arising from the
// use of this software.
//
// Permission is granted to anyone to use this software for any applications that
// use Intan Technologies integrated circuits, and to alter it and redistribute it
// freely.
//
// See http://www.intantech.com for documentation and product information.
//----------------------------------------------------------------------------------

#include "rhd2000registers.h"

#define register_t some_unique_name   // Under Linux, types.h defines register_t to be something, which becomes incompatible with our register_t
#include <iostream>
#undef register_t

#include <iomanip>
#include <cmath>
#include <vector>
#include <queue>
#include <string.h>

#include "common.h"

using std::cerr;
using std::endl;
using std::vector;
using Rhd2000RegisterInternals::register_t;

namespace Rhd2000RegisterInternals {
    register0_t::register0_t() {
        adcReferenceBw = 3;         // ADC reference generator bandwidth (0 [highest BW] - 3 [lowest BW]);
        // always set to 3

        // ampFastSettle is set via setFastSettle()

        ampVrefEnable = 1;          // enable amplifier voltage references (0 = power down; 1 = enable);
        // 1 = normal operation
        adcComparatorBias = 3;      // ADC comparator preamp bias current (0 [lowest] - 3 [highest], only
        // valid for comparator select = 2,3); always set to 3
        adcComparatorSelect = 2;    // ADC comparator select; always set to 2
    }

    register1_t::register1_t() {
        vddSenseEnable = 1;         // supply voltage sensor enable (0 = disable; 1 = enable)
        // adcBufferBias is set in setSampleRate()
        unused = 0;
    }

    register2_t::register2_t() {
        // muxBias is set in defineSampleRate()
        unused = 0;
    }

    register3_t::register3_t() {
        muxLoad = 0;             // MUX capacitance load at ADC input (0 [min CL] - 7 [max CL]); LSB = 3 pF

        // digOut and digOutHiZ are set in setDigOutHiZ()

        tempS1 = 0;                 // temperature sensor S1 (0-1); 0 = power saving mode when temperature sensor is
        // not in use
        tempS2 = 0;                 // temperature sensor S2 (0-1); 0 = power saving mode when temperature sensor is
        // not in use
        tempEn = 0;                 // temperature sensor enable (0 = disable; 1 = enable)
    }

    register4_t::register4_t() {
        weakMiso = 1;               // weak MISO (0 = MISO line is HiZ when CS is inactive; 1 = MISO line is weakly
        // driven when CS is inactive)
        twosComp = 0;               // two's complement ADC results (0 = unsigned offset representation; 1 = signed
        // representation)
        absMode = 0;                // absolute value mode (0 = normal output; 1 = output passed through abs(x) function)

        // dspEn and dspCutoffFreq are set via enableDsp() and setDspCutoffFreq(), respectively
    }

    register5_t::register5_t() {
        unused = 0;
        zcheckDacPower = 1;         // impedance testing DAC power-up (0 = power down; 1 = power up)
        zcheckLoad = 0;             // impedance testing dummy load (0 = normal operation; 1 = insert 60 pF to ground)
        zcheckConnAll = 0;          // impedance testing connect all (0 = normal operation; 1 = connect all electrodes together)

        // zcheckScale, zcheckSelPol, and zCheckEn are set by setZcheckScale(),
        // setZcheckPolarity(), and enableZcheck(), respectively
    }

    register6_t::register6_t() {
        zcheckDac = 128;  // midrange
    }

    register7_t::register7_t() {
        unused = 0;
        // zcheckSelect is set by setZcheckChannel()
    }

    register8_t::register8_t() {
        unused = 0;
        offChipRH1 = 0;             // bandwidth resistor RH1 on/off chip (0 = on chip; 1 = off chip)
        // rH1Dac1 is set by setUpperBandwidth()
    }

    register9_t::register9_t() {
        unused = 0;
        adcAux1En = 1;              // enable ADC aux1 input (when RH1 is on chip) (0 = disable; 1 = enable)
        // rH1Dac2 is set by setUpperBandwidth()
    }

    register10_t::register10_t() {
        unused = 0;
        offChipRH2 = 0;             // bandwidth resistor RH2 on/off chip (0 = on chip; 1 = off chip)
        // rH2Dac1 is set by setUpperBandwidth()
    }

    register11_t::register11_t() {
        unused = 0;
        adcAux2En = 1;              // enable ADC aux2 input (when RH2 is on chip) (0 = disable; 1 = enable)
        // rH2Dac2 is set by setUpperBandwidth()
    }

    register12_t::register12_t() {
        offChipRL = 0;              // bandwidth resistor RL on/off chip (0 = on chip; 1 = off chip)
        // rLDac1 is set by setLowerBandwidth()
    }

    register13_t::register13_t() {
        adcAux3En = 1;              // enable ADC aux3 input (when RL is on chip) (0 = disable; 1 = enable)

        // rLDac2 and rLDac3 are set by setLowerBandwidth()
    }

    /** \brief Constructor.

        Set RHD2000 register variables to default values.
     */
    typed_register_t::typed_register_t() {
        // Constructors for the various registers set default values for almost everything.

        // Zero everything beyond r13
        char* thisp = reinterpret_cast<char*>(this);
        char* aPwrp = reinterpret_cast<char*>(&aPwr);
        memset(aPwrp, 0, sizeof(*this) - (aPwrp - thisp));
    }

    /** \brief Return the Intan chip ID.

        @returns    The Intan chip ID stored in ROM register 63.  If the data is invalid
        (due to a SPI communication channel with the wrong delay or a chip not present)
        then return -1.  
        
        @param[out] register59Value     The value of ROM register 59 is also returned.  This register
        has a value of 0 on RHD2132 and RHD2216 chips, but in RHD2164 chips it is used
        to align the DDR MISO A/B data from the SPI bus.  (Register 59 has a value of 53
        on MISO A and a value of 58 on MISO B.)

     */
    typed_register_t::ChipId typed_register_t::deviceId(int &register59Value) const {
        bool intanChipPresent;

        // First, check ROM registers 32-36 to verify that they hold 'INTAN', and
        // the initial chip name ROM registers 24-26 that hold 'RHD'.
        // This is just used to verify that we are getting good data over the SPI
        // communication channel.
        intanChipPresent = (romCompany[0] == 'I' &&
                            romCompany[1] == 'N' &&
                            romCompany[2] == 'T' &&
                            romCompany[3] == 'A' &&
                            romCompany[4] == 'N' &&
                            romChipName[0] == 'R' &&
                            romChipName[1] == 'H' &&
                            romChipName[2] == 'D');

        // If the SPI communication is bad, return -1.  Otherwise, return the Intan
        // chip ID number stored in ROM register 63.
        if (!intanChipPresent) {
            register59Value = -1;
            return typed_register_t::CHIP_ID_NONE;
        } else {
            register59Value = romMisoABMarker; // Register 59
            return static_cast<typed_register_t::ChipId>(romChipId); // chip ID (Register 63)
        }
    }
}

/** \brief Constructor.  

    Set RHD2000 register variables to default values.

	@param[in] sampleRate	Sampling rate in Hz.  Possibly returned by RhdEvalBoard::GetSampleRate()
 */
Rhd2000Registers::Rhd2000Registers(double sampleRate) 
    : registers()
{
    defineSampleRate(sampleRate);

    setFastSettle(false);       // amplifier fast settle (off = normal operation)

    // muxBias = 40;            // ADC input MUX bias current (0 [highest current] - 63 [lowest current]);
    // This value should be set according to ADC sampling rate; set in setSampleRate()


    setDigOutHiZ();             // auxiliary digital output state

    enableDsp(true);            // DSP offset removal enable/disable
    setDspCutoffFreq(1.0);      // DSP offset removal HPF cutoff freqeuncy

    setZcheckScale(ZcheckCs100fF);  // impedance testing scale factor (100 fF, 1.0 pF, or 10.0 pF)
    setZcheckPolarity(ZcheckPositiveInput); // impedance testing polarity select (RHD2216 only) (0 = test positive inputs;
                                // 1 = test negative inputs)
    enableZcheck(false);        // impedance testing enable/disable

    setZcheckChannel(0);        // impedance testing amplifier select (0-63)

    setUpperBandwidth(10000.0); // set upper bandwidth of amplifiers
    setLowerBandwidth(1.0);     // set lower bandwidth of amplifiers

    powerUpAllAmps();           // turn on all amplifiers
}

/** \brief Defines RHD2000 per-channel sampling rate so that certain sampling-rate-dependent registers are set correctly.

    This function does not change the sampling rate of the FPGA; for this, use Rhd2000EvalBoard::setSampleRate.

	@param[in] newSampleRate	Sampling rate in Hz.  Possibly returned by RhdEvalBoard::GetSampleRate()
*/
void Rhd2000Registers::defineSampleRate(double newSampleRate)
{
    sampleRate = newSampleRate;

    if (sampleRate < 3334.0) {
        registers.r2.muxBias = 40;
        registers.r1.adcBufferBias = 32;
    } else if (sampleRate < 4001.0) {
		registers.r2.muxBias = 40;
		registers.r1.adcBufferBias = 16;
    } else if (sampleRate < 5001.0) {
		registers.r2.muxBias = 40;
		registers.r1.adcBufferBias = 8;
    } else if (sampleRate < 6251.0) {
		registers.r2.muxBias = 32;
		registers.r1.adcBufferBias = 8;
    } else if (sampleRate < 8001.0) {
		registers.r2.muxBias = 26;
		registers.r1.adcBufferBias = 8;
    } else if (sampleRate < 10001.0) {
		registers.r2.muxBias = 18;
		registers.r1.adcBufferBias = 4;
    } else if (sampleRate < 12501.0) {
		registers.r2.muxBias = 16;
		registers.r1.adcBufferBias = 3;
    } else if (sampleRate < 15001.0) {
		registers.r2.muxBias = 7;
		registers.r1.adcBufferBias = 3;
    } else {
		registers.r2.muxBias = 4;
		registers.r1.adcBufferBias = 2;
    }
}

/** \brief Enable or disable amplifier fast settle function.

    The fast settle function drives amplifiers to baseline if enabled.

	@param[in] enabled	true = enable, false = disable
 */
void Rhd2000Registers::setFastSettle(bool enabled)
{
	registers.r0.ampFastSettle = (enabled ? 1 : 0);
}

/** \brief Sets the auxiliary digital output variable to indicate a low output. */
void Rhd2000Registers::setDigOutLow()
{
    registers.r3.digOut = 0;
    registers.r3.digOutHiZ = 0;
}

/** \brief Sets the auxiliary digital output variable to indicate a high output. */
void Rhd2000Registers::setDigOutHigh()
{
    registers.r3.digOut = 1;
    registers.r3.digOutHiZ = 0;
}

/** \brief Sets the auxiliary digital output variable to indicate a high-impedance (HiZ) output. */
void Rhd2000Registers::setDigOutHiZ()
{
    registers.r3.digOut = 0;
    registers.r3.digOutHiZ = 1;
}

/** \brief Enables or disables ADC auxiliary input 1. */
void Rhd2000Registers::enableAux1(bool enabled)
{
    registers.r9.adcAux1En = (enabled ? 1 : 0);
}

/** \brief Enables or disables ADC auxiliary input 2. */
void Rhd2000Registers::enableAux2(bool enabled)
{
	registers.r11.adcAux2En = (enabled ? 1 : 0);
}

/** \brief Enables or disables ADC auxiliary input 3. */
void Rhd2000Registers::enableAux3(bool enabled)
{
	registers.r13.adcAux3En = (enabled ? 1 : 0);
}

/** \brief Enables or disables DSP offset removal filter. */
void Rhd2000Registers::enableDsp(bool enabled)
{
    registers.r4.dspEn = (enabled ? 1 : 0);
}

/** \brief Sets the DSP offset removal filter cutoff frequency.

    The hardware will set the cutoff frequency as close to the requested new cutoff frequency as possible.

    @param[in] newDspCutoffFreq  Requested DSP offset removal cutoff frequency (in Hz).

    @returns The actual cutoff frequency (in Hz).
 */
double Rhd2000Registers::setDspCutoffFreq(double newDspCutoffFreq)
{
    int n;
    double x, fCutoff[16], logNewDspCutoffFreq, logFCutoff[16], minLogDiff;
    const double Pi = 2*acos(0.0);

    fCutoff[0] = 0.0;   // We will not be using fCutoff[0], but we initialize it to be safe

    logNewDspCutoffFreq = log10(newDspCutoffFreq);

    // Generate table of all possible DSP cutoff frequencies
    for (n = 1; n < 16; ++n) {
        x = pow(2.0, (double) n);
        fCutoff[n] = sampleRate * log(x / (x - 1.0)) / (2*Pi);
        logFCutoff[n] = log10(fCutoff[n]);
        // LOG(true) << "  fCutoff[" << n << "] = " << fCutoff[n] << " Hz" << endl;
    }

    // Now find the closest value to the requested cutoff frequency (on a logarithmic scale)
    if (newDspCutoffFreq > fCutoff[1]) {
        registers.r4.dspCutoffFreq = 1;
    } else if (newDspCutoffFreq < fCutoff[15]) {
        registers.r4.dspCutoffFreq = 15;
    } else {
        minLogDiff = 10000000.0;
        for (n = 1; n < 16; ++n) {
            if (abs(logNewDspCutoffFreq - logFCutoff[n]) < minLogDiff) {
                minLogDiff = abs(logNewDspCutoffFreq - logFCutoff[n]);
                registers.r4.dspCutoffFreq = n;
            }
        }
    }

    return fCutoff[registers.r4.dspCutoffFreq];
}

/** \brief Returns the current value of the DSP offset removal cutoff frequency (in Hz). */
double Rhd2000Registers::getDspCutoffFreq() const
{
    double x;
    const double Pi = 2*acos(0.0);

    x = pow(2.0, (double) registers.r4.dspCutoffFreq);

    return sampleRate * log(x / (x - 1.0)) / (2*Pi);
}

/** \brief Enables or disables impedance checking mode. */
void Rhd2000Registers::enableZcheck(bool enabled)
{
    registers.r5.zcheckEn = (enabled ? 1: 0);
}

/** \brief Power up or down impedance checking DAC */
void Rhd2000Registers::setZcheckDacPower(bool enabled)
{
    registers.r5.zcheckDacPower = (enabled ? 1 : 0);
}

/** \brief Select the series capacitor C<SUB>S</SUB> used to convert the voltage waveform generated by the on-chip
    DAC into an AC current waveform that stimulates a selected electrode for impedance testing.

    @param[in] scale    Which series capacitor to use, specified by the ZcheckCs enumeration.
*/
void Rhd2000Registers::setZcheckScale(ZcheckCs scale)
{
    switch (scale) {
        case ZcheckCs100fF:
            registers.r5.zcheckScale = 0x00;     // Cs = 0.1 pF
            break;
        case ZcheckCs1pF:
            registers.r5.zcheckScale = 0x01;     // Cs = 1.0 pF
            break;
        case ZcheckCs10pF:
            registers.r5.zcheckScale = 0x03;     // Cs = 10.0 pF
            break;
    }
}

/** \brief Converts a ZCheckCs value to Farads.

    @param[in] capacitanceEnum    Input value as enum.
    @returns  Capacitance in Farads.
*/
double Rhd2000Registers::getCapacitance(Rhd2000Registers::ZcheckCs capacitanceEnum) {
    switch (capacitanceEnum) {
    case ZcheckCs100fF:
        return 0.1e-12;
    case ZcheckCs1pF:
        return 1.0e-12;
    case ZcheckCs10pF:
        return 10.0e-12;
    }
    return -1; // Not a valid value
}

/** \brief Select impedance testing of positive or negative amplifier inputs (RHD2216 only).

    @param[in] polarity Which polarity of inputs to test.
 */
void Rhd2000Registers::setZcheckPolarity(ZcheckPolarity polarity)
{
    switch (polarity) {
        case ZcheckPositiveInput:
            registers.r5.zcheckSelPol = 0;
            break;
    case ZcheckNegativeInput:
            registers.r5.zcheckSelPol = 1;
            break;
    }
}

/** \brief Select the amplifier channel for impedance testing.

    @param[in] channel  Amplifier channel (0-63)
	@return The value of the channel parameter, or -1 if the parameter value is invalid.
 */
int Rhd2000Registers::setZcheckChannel(int channel)
{
    if (channel < 0 || channel > 63) {
        return -1;
    } else {
        registers.r7.zcheckSelect = channel;
		return registers.r7.zcheckSelect;
    }
}

/** \brief Returns true if a selected amplifier is powered.

    @param[in] channel  Amplifier channel (0-63)
    @return  true = powered up, false = powered down
*/
bool Rhd2000Registers::getAmpPowered(int channel) {
    if (channel >= 0 && channel <= 63) {
        unsigned int byteIndex = channel / 8,
                     bitIndex = channel % 8;
        register_t bit = (1 << bitIndex);

        return (registers.aPwr[byteIndex] & bit) == 0 ? false : true;
    }
    return false;
}

/** \brief Power up or down a selected amplifier on chip.

    @param[in] channel  Amplifier channel (0-63)
    @param[in] powered  true = power up, false = power down
 */
void Rhd2000Registers::setAmpPowered(int channel, bool powered)
{
    if (channel >= 0 && channel <= 63) {
		unsigned int byteIndex = channel / 8,
					 bitIndex  = channel % 8;
		register_t bit = (1 << bitIndex);
		register_t bitmask = ~bit;

		registers.aPwr[byteIndex] = (registers.aPwr[byteIndex] & bitmask) | (powered ? bit : 0);
    }
}

/** \brief Power up all amplifiers on chip 
 */
void Rhd2000Registers::powerUpAllAmps()
{
    for (int channel = 0; channel < 8; ++channel) {
		registers.aPwr[channel] = 0xFF;
    }
}

/** Power down all amplifiers on chip
 */
void Rhd2000Registers::powerDownAllAmps()
{
    for (int channel = 0; channel < 8; ++channel) {
		registers.aPwr[channel] = 0;
    }
}

/** \brief Returns the value of one register stored in the current class instance.

    This function does not initiate communication to the RHD2000 chip to read the current value.

    @param[in] reg  Register to read from (0 - 21)
	@returns Value of the register.
 */
int Rhd2000Registers::getRegisterValue(int reg) const
{
	if (reg >= 0 && reg <= 21) {
        const register_t* pRegisters = reinterpret_cast<const register_t*>(&registers);
        return pRegisters[reg];
	}
    return -1;
}

// Returns the value of the RH1 resistor (in ohms) corresponding to a particular upper
// bandwidth value (in Hz).
double Rhd2000Registers::rH1FromUpperBandwidth(double upperBandwidth) const
{
    double log10f = log10(upperBandwidth);

    return 0.9730 * pow(10.0, (8.0968 - 1.1892 * log10f + 0.04767 * log10f * log10f));
}

// Returns the value of the RH2 resistor (in ohms) corresponding to a particular upper
// bandwidth value (in Hz).
double Rhd2000Registers::rH2FromUpperBandwidth(double upperBandwidth) const
{
    double log10f = log10(upperBandwidth);

    return 1.0191 * pow(10.0, (8.1009 - 1.0821 * log10f + 0.03383 * log10f * log10f));
}

// Returns the value of the RL resistor (in ohms) corresponding to a particular lower
// bandwidth value (in Hz).
double Rhd2000Registers::rLFromLowerBandwidth(double lowerBandwidth) const
{
    double log10f = log10(lowerBandwidth);

    if (lowerBandwidth < 4.0) {
        return 1.0061 * pow(10.0, (4.9391 - 1.2088 * log10f + 0.5698 * log10f * log10f +
                                   0.1442 * log10f * log10f * log10f));
    } else {
        return 1.0061 * pow(10.0, (4.7351 - 0.5916 * log10f + 0.08482 * log10f * log10f));
    }
}

// Returns the amplifier upper bandwidth (in Hz) corresponding to a particular value
// of the resistor RH1 (in ohms).
double Rhd2000Registers::upperBandwidthFromRH1(double rH1) const
{
    double a, b, c;

    a = 0.04767;
    b = -1.1892;
    c = 8.0968 - log10(rH1/0.9730);

    return pow(10.0, ((-b - sqrt(b * b - 4 * a * c))/(2 * a)));
}

// Returns the amplifier upper bandwidth (in Hz) corresponding to a particular value
// of the resistor RH2 (in ohms).
double Rhd2000Registers::upperBandwidthFromRH2(double rH2) const
{
    double a, b, c;

    a = 0.03383;
    b = -1.0821;
    c = 8.1009 - log10(rH2/1.0191);

    return pow(10.0, ((-b - sqrt(b * b - 4 * a * c))/(2 * a)));
}

// Returns the amplifier lower bandwidth (in Hz) corresponding to a particular value
// of the resistor RL (in ohms).
double Rhd2000Registers::lowerBandwidthFromRL(double rL) const
{
    double a, b, c;

    // Quadratic fit below is invalid for values of RL less than 5.1 kOhm
    if (rL < 5100.0) {
        rL = 5100.0;
    }

    if (rL < 30000.0) {
        a = 0.08482;
        b = -0.5916;
        c = 4.7351 - log10(rL/1.0061);
    } else {
        a = 0.3303;
        b = -1.2100;
        c = 4.9873 - log10(rL/1.0061);
    }

    return pow(10.0, ((-b - sqrt(b * b - 4 * a * c))/(2 * a)));
}

/** \brief Sets the on-chip RH1 and RH2 DAC values appropriately to set a particular amplifier 
            upper bandwidth.
            
    The hardware will set the upper bandwidth as close to the requested value as possible.

    @param[in] upperBandwidth   Requested upper bandwidth (in Hz).
    @returns    An estimate of the actual upper bandwidth achieved.
 */
double Rhd2000Registers::setUpperBandwidth(double upperBandwidth)
{
    const double RH1Base = 2200.0;
    const double RH1Dac1Unit = 600.0;
    const double RH1Dac2Unit = 29400.0;
    const int RH1Dac1Steps = 63;
    const int RH1Dac2Steps = 31;

    const double RH2Base = 8700.0;
    const double RH2Dac1Unit = 763.0;
    const double RH2Dac2Unit = 38400.0;
    const int RH2Dac1Steps = 63;
    const int RH2Dac2Steps = 31;

    double actualUpperBandwidth;
    double rH1Target, rH2Target;
    double rH1Actual, rH2Actual;
    int i;

    // Upper bandwidths higher than 30 kHz don't work well with the RHD2000 amplifiers
    if (upperBandwidth > 30000.0) {
        upperBandwidth = 30000.0;
    }

    rH1Target = rH1FromUpperBandwidth(upperBandwidth);

	registers.r8.rH1Dac1 = 0;
	registers.r9.rH1Dac2 = 0;
    rH1Actual = RH1Base;

    for (i = 0; i < RH1Dac2Steps; ++i) {
        if (rH1Actual < rH1Target - (RH1Dac2Unit - RH1Dac1Unit / 2)) {
            rH1Actual += RH1Dac2Unit;
			++registers.r9.rH1Dac2;
        }
    }

    for (i = 0; i < RH1Dac1Steps; ++i) {
        if (rH1Actual < rH1Target - (RH1Dac1Unit / 2)) {
            rH1Actual += RH1Dac1Unit;
			++registers.r8.rH1Dac1;
        }
    }

    rH2Target = rH2FromUpperBandwidth(upperBandwidth);

	registers.r10.rH2Dac1 = 0;
	registers.r11.rH2Dac2 = 0;
    rH2Actual = RH2Base;

    for (i = 0; i < RH2Dac2Steps; ++i) {
        if (rH2Actual < rH2Target - (RH2Dac2Unit - RH2Dac1Unit / 2)) {
            rH2Actual += RH2Dac2Unit;
			++registers.r11.rH2Dac2;
        }
    }

    for (i = 0; i < RH2Dac1Steps; ++i) {
        if (rH2Actual < rH2Target - (RH2Dac1Unit / 2)) {
            rH2Actual += RH2Dac1Unit;
			++registers.r10.rH2Dac1;
        }
    }

    double actualUpperBandwidth1, actualUpperBandwidth2;

    actualUpperBandwidth1 = upperBandwidthFromRH1(rH1Actual);
    actualUpperBandwidth2 = upperBandwidthFromRH2(rH2Actual);

    // Upper bandwidth estimates calculated from actual RH1 value and acutal RH2 value
    // should be very close; we will take their geometric mean to get a single
    // number.
    actualUpperBandwidth = sqrt(actualUpperBandwidth1 * actualUpperBandwidth2);

    /*
    LOG(true) << endl;
    LOG(true) << "Rhd2000Registers::setUpperBandwidth" << endl;
    LOG(true) << fixed << setprecision(1);

    LOG(true) << "RH1 DAC2 = " << rH1Dac2 << ", DAC1 = " << rH1Dac1 << endl;
    LOG(true) << "RH1 target: " << rH1Target << " Ohms" << endl;
    LOG(true) << "RH1 actual: " << rH1Actual << " Ohms" << endl;

    LOG(true) << "RH2 DAC2 = " << rH2Dac2 << ", DAC1 = " << rH2Dac1 << endl;
    LOG(true) << "RH2 target: " << rH2Target << " Ohms" << endl;
    LOG(true) << "RH2 actual: " << rH2Actual << " Ohms" << endl;

    LOG(true) << "Upper bandwidth target: " << upperBandwidth << " Hz" << endl;
    LOG(true) << "Upper bandwidth actual: " << actualUpperBandwidth << " Hz" << endl;

    LOG(true) << endl;
    LOG(true) << setprecision(6);
    LOG(true).unsetf(ios::floatfield);
    */

    return actualUpperBandwidth;
}

/** \brief Sets the on-chip RL DAC values appropriately to set a particular amplifier 
            lower bandwidth.
            
    The hardware will set the lower bandwidth as close to the requested value as possible.

    @param[in] lowerBandwidth   Requested lower bandwidth (in Hz).
    @returns    An estimate of the actual lower bandwidth achieved.
 */
double Rhd2000Registers::setLowerBandwidth(double lowerBandwidth)
{
    const double RLBase = 3500.0;
    const double RLDac1Unit = 175.0;
    const double RLDac2Unit = 12700.0;
    const double RLDac3Unit = 3000000.0;
    const int RLDac1Steps = 127;
    const int RLDac2Steps = 63;

    double actualLowerBandwidth;
    double rLTarget;
    double rLActual;
    int i;

    // Lower bandwidths higher than 1.5 kHz don't work well with the RHD2000 amplifiers
    if (lowerBandwidth > 1500.0) {
        lowerBandwidth = 1500.0;
    }

    rLTarget = rLFromLowerBandwidth(lowerBandwidth);

	registers.r12.rLDac1 = 0;
	registers.r13.rLDac2 = 0;
	registers.r13.rLDac3 = 0;
    rLActual = RLBase;

    if (lowerBandwidth < 0.15) {
        rLActual += RLDac3Unit;
		++registers.r13.rLDac3;
    }

    for (i = 0; i < RLDac2Steps; ++i) {
        if (rLActual < rLTarget - (RLDac2Unit - RLDac1Unit / 2)) {
            rLActual += RLDac2Unit;
			++registers.r13.rLDac2;
        }
    }

    for (i = 0; i < RLDac1Steps; ++i) {
        if (rLActual < rLTarget - (RLDac1Unit / 2)) {
            rLActual += RLDac1Unit;
			++registers.r12.rLDac1;
        }
    }

    actualLowerBandwidth = lowerBandwidthFromRL(rLActual);

    /*
    LOG(true) << endl;
    LOG(true) << fixed << setprecision(1);
    LOG(true) << "Rhd2000Registers::setLowerBandwidth" << endl;

    LOG(true) << "RL DAC3 = " << rLDac3 << ", DAC2 = " << rLDac2 << ", DAC1 = " << rLDac1 << endl;
    LOG(true) << "RL target: " << rLTarget << " Ohms" << endl;
    LOG(true) << "RL actual: " << rLActual << " Ohms" << endl;

    LOG(true) << setprecision(3);

    LOG(true) << "Lower bandwidth target: " << lowerBandwidth << " Hz" << endl;
    LOG(true) << "Lower bandwidth actual: " << actualLowerBandwidth << " Hz" << endl;

    LOG(true) << endl;
    LOG(true) << setprecision(6);
    LOG(true).unsetf(ios::floatfield);
    */

    return actualLowerBandwidth;
}

/** \brief Return a 16-bit MOSI command.

    See the appropriate chip datasheet for details of the commands.

    @param[in] commandType  Command type (should be CALIBRATE or CLEAR, as those are the only ones without arguments).
    @returns A 16-bit MOSI command.
 */
int Rhd2000Registers::createRhd2000Command(Rhd2000CommandType commandType)
{
    switch (commandType) {
        case Rhd2000CommandCalibrate:
            return 0x5500;   // 0101010100000000
            break;
        case Rhd2000CommandCalClear:
            return 0x6a00;   // 0110101000000000
            break;
        default:
        cerr << "Error in Rhd2000Registers::createRhd2000Command: " <<
                "Only 'Calibrate' or 'Clear Calibration' commands take zero arguments." << endl;
            return -1;
    }
}

/** \brief Return a 16-bit MOSI command.

    See the appropriate chip datasheet for details of the commands.

    @param[in] commandType  Command type (should be CONVERT or READ, as those are the only ones with 1 argument).
    @param[in] arg1         Argument to the command
                            \li CONVERT(channel number, 0-64)
                            \li READ(register number, 0-64)

    @returns A 16-bit MOSI command.
 */
int Rhd2000Registers::createRhd2000Command(Rhd2000CommandType commandType, int arg1)
{
    switch (commandType) {
        case Rhd2000CommandConvert:
            if (arg1 < 0 || arg1 > 63) {
                cerr << "Error in Rhd2000Registers::createRhd2000Command: " <<
                        "Channel number out of range." << endl;
                return -1;
            }
            return 0x0000 + (arg1 << 8);  // 00cccccc0000000h; if the command is 'Convert',
                                          // arg1 is the channel number
        case Rhd2000CommandRegRead:
            if (arg1 < 0 || arg1 > 63) {
                cerr << "Error in Rhd2000Registers::createRhd2000Command: " <<
                        "Register address out of range." << endl;
                return -1;
            }
            return 0xc000 + (arg1 << 8);  // 11rrrrrr00000000; if the command is 'Register Read',
                                          // arg1 is the register address
            break;
        default:
            cerr << "Error in Rhd2000Registers::createRhd2000Command: " <<
                    "Only 'Convert' and 'Register Read' commands take one argument." << endl;
            return -1;
    }
}

/** \brief Return a 16-bit MOSI command.

    See the appropriate chip datasheet for details of the commands.

    @param[in] commandType  Command type (should be WRITE, as that is the only one with 2 argument).
    @param[in] arg1         Parameter register in WRITE(register, data)
    @param[in] arg2         Parameter data in WRITE(register, data)

    @returns A 16-bit MOSI command.
 */
int Rhd2000Registers::createRhd2000Command(Rhd2000CommandType commandType, int arg1, int arg2)
{
    switch (commandType) {
        case Rhd2000CommandRegWrite:
            if (arg1 < 0 || arg1 > 63) {
                cerr << "Error in Rhd2000Registers::createRhd2000Command: " <<
                        "Register address out of range." << endl;
                return -1;
            }
            if (arg2 < 0 || arg2 > 255) {
                cerr << "Error in Rhd2000Registers::createRhd2000Command: " <<
                        "Register data out of range." << endl;
                return -1;
            }
            return 0x8000 + (arg1 << 8) + arg2; // 10rrrrrrdddddddd; if the command is 'Register Write',
                                                // arg1 is the register address and arg1 is the data
            break;
        default:
            cerr << "Error in Rhd2000Registers::createRhd2000Command: " <<
                    "Only 'Register Write' commands take two arguments." << endl;
            return -1;
    }
}

/** \brief Creates a standard command list to configure registers on an RHD2000 chip.

    The standard list consist of 60 commands to:
        \li program most RAM registers on a RHD2000 chip
        \li read those values back to confirm programming
        \li read ROM registers
        \li (optionally) run ADC calibration.  If this is false, a dummy command is added, 
			so that the command list length is the same for either value of the parameter.

    @param[out] commandList The output vector to be filled in with commands
    @param[in]  calibrate   If true, run ADC calibration
    @returns The length of the command list.
 */
int Rhd2000Registers::createCommandListRegisterConfig(vector<int> &commandList, bool calibrate)
{
    commandList.clear();    // if command list already exists, erase it and start a new one

    // Start with a few dummy commands in case chip is still powering up
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegRead, 63));
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegRead, 63));

    // Program RAM registers
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegWrite,  0, getRegisterValue( 0)));
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegWrite,  1, getRegisterValue( 1)));
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegWrite,  2, getRegisterValue( 2)));
    // Don't program Register 3 (MUX Load, Temperature Sensor, and Auxiliary Digital Output);
    // control temperature sensor in another command stream
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegWrite,  4, getRegisterValue( 4)));
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegWrite,  5, getRegisterValue( 5)));
    // Don't program Register 6 (Impedance Check DAC) here; create DAC waveform in another command stream
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegWrite,  7, getRegisterValue( 7)));
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegWrite,  8, getRegisterValue( 8)));
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegWrite,  9, getRegisterValue( 9)));
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegWrite, 10, getRegisterValue(10)));
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegWrite, 11, getRegisterValue(11)));
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegWrite, 12, getRegisterValue(12)));
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegWrite, 13, getRegisterValue(13)));
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegWrite, 14, getRegisterValue(14)));
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegWrite, 15, getRegisterValue(15)));
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegWrite, 16, getRegisterValue(16)));
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegWrite, 17, getRegisterValue(17)));

    // Read ROM registers
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegRead, 63));
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegRead, 62));
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegRead, 61));
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegRead, 60));
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegRead, 59));

    // Read chip name from ROM
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegRead, 48));
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegRead, 49));
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegRead, 50));
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegRead, 51));
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegRead, 52));
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegRead, 53));
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegRead, 54));
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegRead, 55));

    // Read Intan name from ROM
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegRead, 40));
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegRead, 41));
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegRead, 42));
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegRead, 43));
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegRead, 44));

    // Read back RAM registers to confirm programming
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegRead,  0));
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegRead,  1));
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegRead,  2));
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegRead,  3));
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegRead,  4));
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegRead,  5));
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegRead,  6));
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegRead,  7));
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegRead,  8));
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegRead,  9));
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegRead, 10));
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegRead, 11));
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegRead, 12));
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegRead, 13));
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegRead, 14));
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegRead, 15));
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegRead, 16));
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegRead, 17));

    // Optionally, run ADC calibration (should only be run once after board is plugged in)
    if (calibrate) {
        commandList.push_back(createRhd2000Command(Rhd2000CommandCalibrate));
    } else {
        commandList.push_back(createRhd2000Command(Rhd2000CommandRegRead, 63));
    }

    // Added in Version 1.2:
    // Program amplifier 31-63 power up/down registers in case a RHD2164 is connected
    // Note: We don't read these registers back, since they are only 'visible' on MISO B.
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegWrite, 18, getRegisterValue(18)));
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegWrite, 19, getRegisterValue(19)));
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegWrite, 20, getRegisterValue(20)));
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegWrite, 21, getRegisterValue(21)));

    // End with a dummy command
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegRead, 63));


    return static_cast<unsigned int>(commandList.size());
}

/** \brief Fills in this registers object with data returned when a createCommandListRegisterConfig command list was run.

	@param[in] data		A vector of integers, corresponding to some Rhd2000DataBlock's auxiliarydata[stream][auxcmdslot],
						where auxcmdslot is configured with a createCommandListRegisterConfig command list
*/
void Rhd2000Registers::readBack(const vector<int>& data) {
	const int ReadOffset = 19; // Jump over the dummy commands (2) and writing RAM (16) and 1 for ???

	// The read at position 19 is register 63, position 20 is register 62, etc.
	const int ReadIndices[36] = { 63, 62, 61, 60, 59, 48, 49, 50, 51, 52, 53, 54, 55, 40, 41, 42, 43, 44, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17 };

    register_t* pRegisters = reinterpret_cast<register_t*>(&registers);
	for (unsigned int i = 0; i < 36; i++) {
        pRegisters[ReadIndices[i]] = data[ReadOffset + i];
	}
}

/** \brief Creates a standard command list to sample temperature sensor, auxiliary ADC inputs, and supply voltage sensor on an RHD2000 chip.

    Create a list of 60 commands to sample auxiliary ADC inputs, temperature sensor, and supply
    voltage sensor.  One temperature reading (one sample of ResultA and one sample of ResultB)
    is taken during this 60-command sequence.  One supply voltage sample is taken.  Auxiliary
    ADC inputs are continuously sampled at 1/4 the amplifier sampling rate.

    Since this command list consists of writing to Register 3, it also sets the state of the
    auxiliary digital output.  If the digital output value needs to be changed dynamically,
    then variations of this command list need to be generated for each state and programmed into
    different RAM banks, and the appropriate command list selected at the right time.

    @param[out] commandList The output vector to be filled in with commands
    @returns The length of the command list.
 */
int Rhd2000Registers::createCommandListTempSensor(vector<int> &commandList)
{
    int i;

    commandList.clear();    // if command list already exists, erase it and start a new one

    registers.r3.tempEn = 1;

    commandList.push_back(createRhd2000Command(Rhd2000CommandConvert, 32));     // sample AuxIn1
    commandList.push_back(createRhd2000Command(Rhd2000CommandConvert, 33));     // sample AuxIn2
    commandList.push_back(createRhd2000Command(Rhd2000CommandConvert, 34));     // sample AuxIn3
    registers.r3.tempS1 = registers.r3.tempEn;
    registers.r3.tempS2 = 0;
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegWrite, 3, getRegisterValue(3)));

    commandList.push_back(createRhd2000Command(Rhd2000CommandConvert, 32));     // sample AuxIn1
    commandList.push_back(createRhd2000Command(Rhd2000CommandConvert, 33));     // sample AuxIn2
    commandList.push_back(createRhd2000Command(Rhd2000CommandConvert, 34));     // sample AuxIn3
    registers.r3.tempS1 = registers.r3.tempEn;
    registers.r3.tempS2 = registers.r3.tempEn;
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegWrite, 3, getRegisterValue(3)));

    commandList.push_back(createRhd2000Command(Rhd2000CommandConvert, 32));     // sample AuxIn1
    commandList.push_back(createRhd2000Command(Rhd2000CommandConvert, 33));     // sample AuxIn2
    commandList.push_back(createRhd2000Command(Rhd2000CommandConvert, 34));     // sample AuxIn3
    commandList.push_back(createRhd2000Command(Rhd2000CommandConvert, 49));     // sample Temperature Sensor

    commandList.push_back(createRhd2000Command(Rhd2000CommandConvert, 32));     // sample AuxIn1
    commandList.push_back(createRhd2000Command(Rhd2000CommandConvert, 33));     // sample AuxIn2
    commandList.push_back(createRhd2000Command(Rhd2000CommandConvert, 34));     // sample AuxIn3
    registers.r3.tempS1 = 0;
    registers.r3.tempS2 = registers.r3.tempEn;
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegWrite, 3, getRegisterValue(3)));

    commandList.push_back(createRhd2000Command(Rhd2000CommandConvert, 32));     // sample AuxIn1
    commandList.push_back(createRhd2000Command(Rhd2000CommandConvert, 33));     // sample AuxIn2
    commandList.push_back(createRhd2000Command(Rhd2000CommandConvert, 34));     // sample AuxIn3
    commandList.push_back(createRhd2000Command(Rhd2000CommandConvert, 49));     // sample Temperature Sensor

    commandList.push_back(createRhd2000Command(Rhd2000CommandConvert, 32));     // sample AuxIn1
    commandList.push_back(createRhd2000Command(Rhd2000CommandConvert, 33));     // sample AuxIn2
    commandList.push_back(createRhd2000Command(Rhd2000CommandConvert, 34));     // sample AuxIn3
    registers.r3.tempS1 = 0;
    registers.r3.tempS2 = 0;
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegWrite, 3, getRegisterValue(3)));

    commandList.push_back(createRhd2000Command(Rhd2000CommandConvert, 32));     // sample AuxIn1
    commandList.push_back(createRhd2000Command(Rhd2000CommandConvert, 33));     // sample AuxIn2
    commandList.push_back(createRhd2000Command(Rhd2000CommandConvert, 34));     // sample AuxIn3
    commandList.push_back(createRhd2000Command(Rhd2000CommandConvert, 48));     // sample Supply Voltage Sensor

    for (i = 0; i < 8; ++i) {
        commandList.push_back(createRhd2000Command(Rhd2000CommandConvert, 32));     // sample AuxIn1
        commandList.push_back(createRhd2000Command(Rhd2000CommandConvert, 33));     // sample AuxIn2
        commandList.push_back(createRhd2000Command(Rhd2000CommandConvert, 34));     // sample AuxIn3
        commandList.push_back(createRhd2000Command(Rhd2000CommandRegRead, 63));      // dummy command
    }

	return static_cast<unsigned int>(commandList.size());
}

/** \brief Create a list of 60 commands to update Register 3 (controlling the auxiliary digital ouput
            pin) every sampling period.

    Since this command list consists of writing to Register 3, it also sets the state of the
    on-chip temperature sensor.  The temperature sensor settings are therefore changed throughout
    this command list to coordinate with the 60-command list generated by createCommandListTempSensor().

    @param[out] commandList The output vector to be filled in with commands
    @returns The length of the command list.
 */
int Rhd2000Registers::createCommandListUpdateDigOut(vector<int> &commandList)
{
    int i;

    commandList.clear();    // if command list already exists, erase it and start a new one

    registers.r3.tempEn = 1;

    commandList.push_back(createRhd2000Command(Rhd2000CommandRegWrite, 3, getRegisterValue(3)));
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegWrite, 3, getRegisterValue(3)));
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegWrite, 3, getRegisterValue(3)));
    registers.r3.tempS1 = registers.r3.tempEn;
    registers.r3.tempS2 = 0;
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegWrite, 3, getRegisterValue(3)));

    commandList.push_back(createRhd2000Command(Rhd2000CommandRegWrite, 3, getRegisterValue(3)));
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegWrite, 3, getRegisterValue(3)));
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegWrite, 3, getRegisterValue(3)));
    registers.r3.tempS1 = registers.r3.tempEn;
    registers.r3.tempS2 = registers.r3.tempEn;
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegWrite, 3, getRegisterValue(3)));

    commandList.push_back(createRhd2000Command(Rhd2000CommandRegWrite, 3, getRegisterValue(3)));
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegWrite, 3, getRegisterValue(3)));
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegWrite, 3, getRegisterValue(3)));
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegWrite, 3, getRegisterValue(3)));

    commandList.push_back(createRhd2000Command(Rhd2000CommandRegWrite, 3, getRegisterValue(3)));
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegWrite, 3, getRegisterValue(3)));
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegWrite, 3, getRegisterValue(3)));
    registers.r3.tempS1 = 0;
    registers.r3.tempS2 = registers.r3.tempEn;
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegWrite, 3, getRegisterValue(3)));

    commandList.push_back(createRhd2000Command(Rhd2000CommandRegWrite, 3, getRegisterValue(3)));
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegWrite, 3, getRegisterValue(3)));
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegWrite, 3, getRegisterValue(3)));
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegWrite, 3, getRegisterValue(3)));

    commandList.push_back(createRhd2000Command(Rhd2000CommandRegWrite, 3, getRegisterValue(3)));
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegWrite, 3, getRegisterValue(3)));
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegWrite, 3, getRegisterValue(3)));
    registers.r3.tempS1 = 0;
    registers.r3.tempS2 = 0;
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegWrite, 3, getRegisterValue(3)));

    commandList.push_back(createRhd2000Command(Rhd2000CommandRegWrite, 3, getRegisterValue(3)));
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegWrite, 3, getRegisterValue(3)));
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegWrite, 3, getRegisterValue(3)));
    commandList.push_back(createRhd2000Command(Rhd2000CommandRegWrite, 3, getRegisterValue(3)));

    for (i = 0; i < 8; ++i) {
        commandList.push_back(createRhd2000Command(Rhd2000CommandRegWrite, 3, getRegisterValue(3)));
        commandList.push_back(createRhd2000Command(Rhd2000CommandRegWrite, 3, getRegisterValue(3)));
        commandList.push_back(createRhd2000Command(Rhd2000CommandRegWrite, 3, getRegisterValue(3)));
        commandList.push_back(createRhd2000Command(Rhd2000CommandRegWrite, 3, getRegisterValue(3)));
    }

	return static_cast<unsigned int>(commandList.size());
}

/** \brief Create a list of up to 1024 commands to generate a sine wave using the on-chip impedance testing voltage DAC. 

    @param[out] commandList The output vector to be filled in with commands
    @param[in]  frequency   Frequency of the sine wave to generate (in Hz).  If frequency is set to zero, a DC baseline waveform is created.
    @param[in]  amplitude   Amplitude of the sine wave to generate  (in DAC steps, 0-128).
    @returns The length of the command list.
 */
int Rhd2000Registers::createCommandListZcheckDac(vector<int> &commandList, double frequency, double amplitude)
{
    int i, period, value;
    double t;
    const double Pi = 2*acos(0.0);

    commandList.clear();    // if command list already exists, erase it and start a new one

    if (amplitude < 0.0 || amplitude > 128.0) {
        cerr << "Error in Rhd2000Registers::createCommandListZcheckDac: Amplitude out of range." << endl;
        return -1;
    }
    if (frequency < 0.0) {
        cerr << "Error in Rhd2000Registers::createCommandListZcheckDac: Negative frequency not allowed." << endl;
        return -1;
    } else if (frequency > sampleRate / 4.0) {
        cerr << "Error in Rhd2000Registers::createCommandListZcheckDac: " <<
                "Frequency too high relative to sampling rate." << endl;
        return -1;
    }
    if (frequency == 0.0) {
        for (i = 0; i < MaxCommandLength; ++i) {
            commandList.push_back(createRhd2000Command(Rhd2000CommandRegWrite, 6, 128));
        }
    } else {
        period = (int) floor(sampleRate / frequency + 0.5);
        if (period > MaxCommandLength) {
            cerr << "Error in Rhd2000Registers::createCommandListZcheckDac: Frequency too low." << endl;
            return -1;
        } else {
            t = 0.0;
            for (i = 0; i < period; ++i) {
                value = (int) floor(amplitude * sin(2 * Pi * frequency * t) + 128.0 + 0.5);
                if (value < 0) {
                    value = 0;
                } else if (value > 255) {
                    value = 255;
                }
                commandList.push_back(createRhd2000Command(Rhd2000CommandRegWrite, 6, value));
                t += 1.0 / sampleRate;
            }
        }
    }

	return static_cast<unsigned int>(commandList.size());
}
