//  ------------------------------------------------------------------------
//
//  This file is part of the Intan Technologies RHD2000 Interface
//  Version 1.41
//  Copyright (C) 2013-2014 Intan Technologies
//
//  ------------------------------------------------------------------------
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU Lesser General Public License as published
//  by the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU Lesser General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include "rhd2000config.h"
#include <algorithm>
#include "boardcontrol.h"
#include <string.h>
#include <QtCore>

using std::vector;
using std::complex;
using Rhd2000RegisterInternals::typed_register_t;

const double TWO_PI = 6.28318530718;
const double DEGREES_TO_RADIANS = 0.0174532925199;


/** \brief This namespace contains classes for configuring various parts of the RHD2000 evaluation board.

    In general, these classes provide an in-memory copy of the configuration.  Parameters are changed
    here, then an appropriate method of BoardControl is called to send that configuration to the board.

    See the individual classes for more information about the functionality available on the boards and
    how it's configured.
 */
namespace Rhd2000Config {
    //  ------------------------------------------------------------------------
    /** \brief Constructor

        Establishes references to data that this object is dependent on.  When
        those change, and you call dependencyChanged(), the values are used in
        recalculating.

        @param[in] boardSampleRate_     Reference to BoardControl::boardSampleRate
        @param[in] bandwidth_           Reference to BoardControl::bandWidth
        */
    ImpedanceFreq::ImpedanceFreq(double& boardSampleRate_, const BandWidth& bandwidth_) :
        boardSampleRate(boardSampleRate_),
        bandwidth(bandwidth_)
    {
        // Default electrode impedance measurement frequency
        desiredImpedanceFreq = 1000.0;

        actualImpedanceFreq = 0.0;
        impedanceFreqValid = false;
    }

    // Update electrode impedance measurement frequency, after checking that
    // requested test frequency lies within acceptable ranges based on the
    // amplifier bandwidth and the sampling rate.  See impedancefreqdialog.cpp
    // for more information.
    void ImpedanceFreq::updateImpedanceFrequency()
    {

        int impedancePeriod;
        double lowerBandwidthLimit, upperBandwidthLimit;

        upperBandwidthLimit = bandwidth.actualUpperBandwidth / 1.5;
        lowerBandwidthLimit = bandwidth.actualLowerBandwidth * 1.5;
        if (bandwidth.dspEnabled) {
            if (bandwidth.actualDspCutoffFreq > bandwidth.actualLowerBandwidth) {
                lowerBandwidthLimit = bandwidth.actualDspCutoffFreq * 1.5;
            }
        }

        if (desiredImpedanceFreq > 0.0) {
            impedancePeriod = std::lround(boardSampleRate / desiredImpedanceFreq);
            if (impedancePeriod >= 4 && impedancePeriod <= 1024 &&
                desiredImpedanceFreq >= lowerBandwidthLimit &&
                desiredImpedanceFreq <= upperBandwidthLimit) {
                actualImpedanceFreq = boardSampleRate / impedancePeriod;
                impedanceFreqValid = true;
            } else {
                actualImpedanceFreq = 0.0;
                impedanceFreqValid = false;
            }
        } else {
            actualImpedanceFreq = 0.0;
            impedanceFreqValid = false;
        }
    }

    /** \brief Update the value of ImpedanceFreq::desiredImpedanceFreq, ImpedanceFreq::actualImpedanceFreq, and
            ImpedanceFreq::impedanceFreqValid.

        @param[in] desiredImpedanceFreq_    New value for desired impedance frequency.
     */
    void ImpedanceFreq::changeImpedanceValues(double desiredImpedanceFreq_) {
        desiredImpedanceFreq = desiredImpedanceFreq_;
        updateImpedanceFrequency();
    }

    /** \brief Call this when board settings that affect impedance (sampling rate, filter bandwidth) change.
     */
    void ImpedanceFreq::dependencyChanged() {
        impedanceFreqValid = false;
        updateImpedanceFrequency();
    }


    /*
        Given a measured complex impedance that is the result of an electrode impedance in parallel
        with a parasitic capacitance (i.e., due to the amplifier input capacitance and other
        capacitances associated with the chip bondpads), this function factors out the effect of the
        parasitic capacitance to return the acutal electrode impedance.

        The electrode has impedance Z_electrode, the parallel capacitor has impedance 1/j*w*C.
        (Where w is the angular frequency, w = 2*pi*f

        The combined impedance can be calculated by:
            1/Z_combined = 1/Z_electrode + 1/Z_capacitor

        Solving for this,
            1/Z_electrode = 1/Z_combined - 1/Z_capacitor
        In particular, Z_combined = z_measured, and 1/Z_capacitor = j*w*C,
        so 1/Z_electrode = 1/Z_measured - jwc
    */
    complex<double> ImpedanceFreq::factorOutParallelCapacitance(complex<double> zMeasured, double parasiticCapacitance)
    {
        const complex<double> ONE(1, 0);

        complex<double> jwc(0, TWO_PI * actualImpedanceFreq * parasiticCapacitance);
        complex<double> one_over_Zelectrode = ONE / zMeasured - jwc;
        return ONE / one_over_Zelectrode;
    }

    // This is a purely empirical function to correct observed errors in the real component
    // of measured electrode impedances at sampling rates below 15 kS/s.  At low sampling rates,
    // it is difficult to approximate a smooth sine wave with the on-chip voltage DAC and 10 kHz
    // 2-pole lowpass filter.  This function attempts to somewhat correct for this, but a better
    // solution is to always run impedance measurements at 20 kS/s, where they seem to be most
    // accurate.
    complex<double> ImpedanceFreq::empiricalResistanceCorrection(std::complex<double> zIn)
    {
        // First, convert from polar coordinates to rectangular coordinates.
        double impedanceR = zIn.real();
        double impedanceX = zIn.imag();

        // Emprically derived correction factor (i.e., no physical basis for this equation).
        impedanceR /= 10.0 * exp(-boardSampleRate / 2500.0) * cos(TWO_PI * boardSampleRate / 15000.0) + 1.0;

        return complex<double>(impedanceR, impedanceX);
    }

    /** \brief Calculates the best impedance for a given amplifier, given measured amplitudes for the three capacitor values.

        This function picks one of the capacitors (preferring voltage readings that are closest to 250uV),
        then uses the amplitude value for that reading to calculate impedance, and corrects for known board parasitics.

        @param[in] measuredAmplitudes   Measured amplitudes of the waveform for the given amplifier, for each of the three capacitor values.
        @returns the best value of impedance
     */
    complex<double> ImpedanceFreq::calculateBestImpedanceOneAmplifier(vector<complex<double> >& measuredAmplitudes) {
        const double MAX_AMPLITUDE = 3000; // Above this, we're worried about non-linearity
        int bestAmplitudeIndex;
        //double saturationVoltage = approximateSaturationVoltage(actualImpedanceFreq, bandwidth.actualUpperBandwidth);

        double amplitude[3];
        for (int i = 0; i < 3; i++) {
            amplitude[i] = std::abs(measuredAmplitudes[i]);
        }

        double currentAmplitude = 0;
        for (int i = 0; i < 3; i++) {
            // Find the largest that's smaller than MAX_AMPLITUDE
            if ((amplitude[i] < MAX_AMPLITUDE) && (amplitude[i] > currentAmplitude)) {
                bestAmplitudeIndex = i;
                currentAmplitude = amplitude[i];
            }
        }

//        // Make sure chosen capacitor is below saturation voltage
//        if (amplitude[2] < saturationVoltage) {
//            bestAmplitudeIndex = 2;
//        } else if (amplitude[1] < saturationVoltage) {
//            bestAmplitudeIndex = 1;
//        } else {
//            bestAmplitudeIndex = 0;
//        }

        // If we didn't find one (i.e., if everything is >= MAX_AMPLITUDE)
        if (bestAmplitudeIndex == -1) {
            // Find the smallest
            bestAmplitudeIndex = 0;
            for (int i = 1; i < 3; i++) {
                if (amplitude[i] < amplitude[bestAmplitudeIndex]) {
                    bestAmplitudeIndex = i;
                }
            }
        }

//        // If C2 and C3 are too close, C3 is probably saturated. Ignore C3.
//        double capRatio = amplitude[1] / amplitude[2];
//        if (capRatio > 0.2) {
//            if (bestAmplitudeIndex == 2) {
//                bestAmplitudeIndex = 1;
//            }
//        }

        double Cseries = Rhd2000Registers::getCapacitance(static_cast<Rhd2000Registers::ZcheckCs>(bestAmplitudeIndex));

        const double dacVoltageAmplitude = 128 * (1.225 / 256);  // this assumes the DAC amplitude was set to 128

        // Calculate current amplitude produced by on-chip voltage DAC
        double current = TWO_PI * actualImpedanceFreq * dacVoltageAmplitude * Cseries;

        double relativeFreq = actualImpedanceFreq / boardSampleRate;
        // Calculate impedance magnitude from calculated current and measured voltage.
        // 1.0e-6 converts uV to V
        // divide by current to convert voltage to impedance
        // 18.0 * relativeFreq * relativeFreq + 1.0 is an empirical adjustment
        double magnitudeMultiplier = (1.0e-6 / current) * (18.0 * relativeFreq * relativeFreq + 1.0);

        // Calculate impedance phase, with small correction factor accounting for the
        // 3-command SPI pipeline delay.
        double phaseAdder = DEGREES_TO_RADIANS * (360.0 * (3.0 / getPeriod()));
        complex<double> correction = std::polar(magnitudeMultiplier, phaseAdder);

        complex<double> zCorrected1 = measuredAmplitudes[bestAmplitudeIndex] * correction;

        // Factor out on-chip parasitic capacitance from impedance measurement.
        const double parasiticCapacitance = 14.0e-12;  // 15 pF: an estimate of on-chip parasitic capacitance,
        // including 10 pF of amplifier input capacitance.
        complex<double> zCorrected2 = factorOutParallelCapacitance(zCorrected1, parasiticCapacitance);

        // Perform empirical resistance correction to improve accuracy at sample rates below
        // 15 kS/s.
        // NOTE: After refining the impedance measurement algorithm, Intan has determined this empirical correction is no longer necessary for accurate measurements
        //return empiricalResistanceCorrection(zCorrected2);
        return zCorrected2;
    }


    // Use a 2nd order Low Pass Filter to model the approximate voltage at which the amplifiers saturate
    // which depends on the impedance frequency and the amplifier bandwidth.
    double ImpedanceFreq::approximateSaturationVoltage(double actualZFreq, double highCutoff)
    {
        if (actualZFreq < 0.2 * highCutoff)
            return 5000.0;
        else
            return 5000.0 * sqrt(1.0 / (1.0 + pow(3.3333 * actualZFreq / highCutoff, 4)));
    }


    double ImpedanceFreq::getPeriod() {
        return boardSampleRate / actualImpedanceFreq;
    }

    void ImpedanceFreq::calculateValues() {
        // Select number of periods to measure impedance over
        int numPeriods = std::lround(0.020 * actualImpedanceFreq); // Test each channel for at least 20 msec...
        if (numPeriods < 5) numPeriods = 5; // ...but always measure across no fewer than 5 complete periods
        numBlocks = static_cast<int>(std::ceil((numPeriods + 2.0) * getPeriod() / 60.0));  // + 2 periods to give time to settle initially
        if (numBlocks < 2) numBlocks = 2;   // need first block for command to switch channels to take effect.

        int period = std::lround(boardSampleRate / actualImpedanceFreq);

        startIndex = 0;
        endIndex = startIndex + numPeriods * period - 1;

        // Move the measurement window to the end of the waveform to ignore start-up transient.
        while (endIndex < SAMPLES_PER_DATA_BLOCK * numBlocks - period) {
            startIndex += period;
            endIndex += period;
        }
    }

    /** \brief Calculates the amplitude (magnitude and phase) of the input sinusoid.
    
        @param[in] data     Input waveform.  Should range from 0..endIndex

        @returns    The real and imaginary amplitudes of a selected frequency component in the vector data, between a start index and end index.
        */
    complex<double> ImpedanceFreq::amplitudeOfFreqComponent(double* data)
    {
        int length = endIndex - startIndex + 1;
        const double k = TWO_PI * actualImpedanceFreq / boardSampleRate;  // precalculate for speed

        // Perform correlation with sine and cosine waveforms.
        double sumI = 0.0;
        double sumQ = 0.0;
        for (int t = startIndex; t <= endIndex; ++t) {
            sumI += data[t] * cos(k * t);
            sumQ += data[t] * -1.0 * sin(k * t);
        }
        complex<double> z(sumI, sumQ);
        return z * 2.0 / (double)length;
    }


    //  ------------------------------------------------------------------------
    BandWidth::BandWidth() {
        // Default amplifier bandwidth settings
        desiredLowerBandwidth = 0.1;
        desiredUpperBandwidth = 7500.0;
        desiredDspCutoffFreq = 1.0;
        dspEnabled = true;
    }

    /** \brief Helper function.

        @param[in, out] chipRegisters       Updated by setting the desired frequencies and enabling/disabling the DSP
    */
    void BandWidth::setChipRegisters(Rhd2000Registers& chipRegisters) {
        actualDspCutoffFreq = chipRegisters.setDspCutoffFreq(desiredDspCutoffFreq);
        actualLowerBandwidth = chipRegisters.setLowerBandwidth(desiredLowerBandwidth);
        actualUpperBandwidth = chipRegisters.setUpperBandwidth(desiredUpperBandwidth);
        chipRegisters.enableDsp(dspEnabled);
    }

    /** \brief Change the configuration parameters

        @param[in] desiredDspCutoffFreq_        Desired DSP Cutoff Frequency, in Hz.  Only valid if the DSP is enabled.
        @param[in] desiredUpperBandwidth_       Desired Upper Cutoff Frequency, in Hz.
        @param[in] desiredLowerBandwidth_       Desired Lower Cutoff Frequency, in Hz.
        @param[in] dspEnabled_                  Enable or disable the optional DSP.
        @param[in] boardSampleRate              Sampling rate of the board.
    */
    void BandWidth::changeValues(double desiredDspCutoffFreq_, double desiredUpperBandwidth_, double desiredLowerBandwidth_, bool dspEnabled_, double boardSampleRate) {
        desiredDspCutoffFreq = desiredDspCutoffFreq_;
        desiredUpperBandwidth = desiredUpperBandwidth_;
        desiredLowerBandwidth = desiredLowerBandwidth_;
        dspEnabled = dspEnabled_;

        // Call this to get the actualXYZ values set correctly; we can throw the chipRegisters object away after that
        Rhd2000Registers chipRegisters(boardSampleRate);
        setChipRegisters(chipRegisters);
    }

    //  ------------------------------------------------------------------------
    LEDControl::LEDControl() {
        clear();
    }

    /// Zeros all the in-memory LED values.
    void LEDControl::clear() {
        memset(values, 0, sizeof(values));
    }

    /** \brief Starts using the LEDs as a cycling progress counter.

        This sets LED[0]; subsequent calls to incProgressCounter() cycle.
    */
    void LEDControl::startProgressCounter() {
        clear();
        index = 0;
        values[index] = 1;
    }

    /** \brief When using the LEDs as a progress counter, turns the
                current LED off and the next one on.

        For example, if LED[0] is on, this function will clear that and
        turn LED[1] on.  This pattern continues until LED[7], after which
        the progress counter cycles around to LED[0] again.
    */
    void LEDControl::incProgressCounter() {
        values[index] = 0;
        index++;
        if (index == 8) index = 0;
        values[index] = 1;
    }

    //  ------------------------------------------------------------------------
    DigitalOutputControl::DigitalOutputControl() {
        clear();
        comparatorsEnabled = true;
    }

    /** \brief Resets all digital outputs to 0.

        This configures digital outputs in memory; you need to call
        BoardControl::updateDigitalOutputs to send these values to the actual board.

        If DigitalOutputControl::comparatorsEnabled = true, only digital outputs
        8-15 are controlled directly, so this will not affect the digital outputs.
     */
    void DigitalOutputControl::clear() {
        memset(values, 0, sizeof(values));
    }

    /** \brief Set the threshold for one of the comparators.

        Note: this is a convenience function only; you can do the same thing via the
        DigitalOutputControl::comparators member variable, with more configurability.

        @param[in] dacIndex     Which threshold comparator this applies to (0-7)
        @param[in] threshold    Threshold, in &mu;V.

        Note that if \p threshold is >= 0, a rising edge is used, if \p threshold < 0,
        a falling edge is used.
     */
    void DigitalOutputControl::setDacThreshold(int dacIndex, double threshold) {
        comparators[dacIndex].set(threshold, threshold >= 0);
    }

    //  ------------------------------------------------------------------------
    AnalogOutputControl::AnalogOutputControl(BoardControl& boardControl_) :
        boardControl(boardControl_)
    {
        highpassFilterFrequency = 250.0;
        highpassFilterEnabled = false;
        dacGain = 0;
        setDacManualVolts(0.0);
        noiseSuppress = 0;
        dspSettle = false;
    }

    /** \brief Get manual value for analog outputs.

        @return     Raw (integer) value that should be sent to the board.
    */
    int AnalogOutputControl::getDacManualRaw() const {
        return dacManual;
    }

    /** \brief Set manual value for analog outputs.

        Individual analog outputs can be configured to read from an amplifier input channel or from the (single)
        dacManual value.  All analog outputs configured to use "Dac Manual" will output the value controlled
        by this variable.

        @param  value       Voltage value that should be sent to the board.  This is converted based on the DAC on the board to a raw value.
        @return             false if the value was not in the allowable range (e.g., if you set -1 on a DAC that only supports positive values)
    */
    bool AnalogOutputControl::setDacManualVolts(double value) {
        bool good = false;
        long rawValue = 0;

        if (boardControl.evalBoardMode == 0 || boardControl.evalBoardMode == 1) {
            if (value >= -3.3 && value <= 3.3) {
                // 0.000100708 = 6.6 / 65536
                rawValue = std::lround(value / 0.000100708) + 0x8000;
                good = true;
            }
        }
        else if (boardControl.evalBoardMode == 2) {
            if (value >= 0 && value <= 3.3) {
                // 0.000050354 = 3.3 / 65536
                rawValue = std::lround(value / 0.000050354);
                good = true;
            }
        }

        if (good) {
            // Fix rounding errors, if any
            if (rawValue < 0) {
                rawValue = 0;
            }
            if (rawValue > 0xFFFF) {
                rawValue = 0xFFFF;
            }

            dacManual = rawValue;
            return true;
        }
        else {
            return false;
        }
    }



    //  ------------------------------------------------------------------------
    AuxDigitalOutputControl::AuxDigitalOutputControl()
    {
    }

    //  ------------------------------------------------------------------------
    /** \brief Returns the port corresponding to this data stream

        For example, if the data stream is configured to be Port A, MISO 1, DDR, this will return Port A.

        @returns Port enum.
    */
    Rhd2000EvalBoard::BoardPort DataSourceControl::getPort() const {
        return Rhd2000EvalBoard::getPort(dataSource);
    }

    /** \brief Number of data streams needed for this data source.  2 for RHD2164, 1 for everything else.

        @returns    Number of streams.
    */
    unsigned int DataSourceControl::getNumStreams() const {
        switch (chipId) {
        case typed_register_t::CHIP_ID_RHD2216:
        case typed_register_t::CHIP_ID_RHD2132:
            return 1;
        case typed_register_t::CHIP_ID_RHD2164:
            return 2;
        default:
            return 0;
        }
    }

    /** \brief Number of channels for this data source.

        @returns    Number of channels.
    */
    unsigned int DataSourceControl::getNumChannels() const {
        switch (chipId) {
        case typed_register_t::CHIP_ID_RHD2216:
            return 16;
        case typed_register_t::CHIP_ID_RHD2132:
            return 32;
        case typed_register_t::CHIP_ID_RHD2164:
            return 64;
        default:
            return 0;
        }
    }

    /** \brief Returns the DataStreamConfig that corresponds to the given channel.

        Data streams are used internally in rhd2000datablock instances.  This function
        gives you easy access to the stream that corresponds to the given data source and channel.

        @param channel     Channel.  Valid range = 0-getNumChannels()
        @returns           The DataStreamConfig that corresponds to channel.
    */
    DataStreamConfig* DataSourceControl::getStreamForChannel(unsigned int channel) const
    {
        return (channel < 32) ? firstDataStream : ddrDataStream;
    }
    //  ------------------------------------------------------------------------
    /** \brief \ref datastreambinding "Ties" a datasource to the current datastream.

        @param[in] source   Datasource to tie
        @param[in] isddr    True if you want to tie to the DDR stream for the source, false for the regular stream.
    */
    void DataStreamConfig::tieTo(DataSourceControl* source, bool isddr) {
        underlying = source;
        isDDR = isddr;
        if (underlying != nullptr) {
            if (isDDR) {
                underlying->ddrDataStream = this;
            }
            else {
                underlying->firstDataStream = this;
            }
        }
    }

    /** \brief Returns the BoardDataSource corresponding to this datastream.

        @returns BoardDataSource.  Note that this may or may not be DDR.
     */
    Rhd2000EvalBoard::BoardDataSource DataStreamConfig::getDataSource() const {
        if (underlying != nullptr) {
            Rhd2000EvalBoard::BoardDataSource datasource = underlying->dataSource;
            if (isDDR) {
                datasource = static_cast<Rhd2000EvalBoard::BoardDataSource>(datasource + Rhd2000EvalBoard::BoardDataSource::PortA1Ddr);
            }
            return datasource;
        }
        return Rhd2000EvalBoard::BoardDataSource::PortA1;
    }

    /** \brief Number of channels needed for this data source on a given stream.

        Note that, for RHD2164, MISO-A returns 32 and MISO-B returns 32.

        @returns    Number of channels.
    */
    unsigned int DataStreamConfig::getNumChannels() const {
        if (underlying == nullptr) {
            return 0;
        }
        return std::min(underlying->getNumChannels(), 32U);
    }

    //  ------------------------------------------------------------------------
    DataStreamControl::DataStreamControl()
    {
        resetPhysicalStreams();
        logicalValid = false;
        for (unsigned int stream = 0; stream < MAX_NUM_DATA_STREAMS; stream++) {
            logicalDataStreams[stream].index = stream;
        }
    }

    /** \brief Reset the physical data stream configurations.

        Used at startup and when rescanning ports.
    */
    void DataStreamControl::resetPhysicalStreams() {
        physicalValid = false;
        for (unsigned int source = 0; source < MAX_NUM_BOARD_DATA_SOURCES; source++) {
            physicalDataStreams[source].dataSource = static_cast<Rhd2000EvalBoard::BoardDataSource>(source);
            physicalDataStreams[source].chipId = typed_register_t::CHIP_ID_NONE;
        }
    }

    /** \brief Is the given chipId present?

        For example, certain logic may only be needed if RHD2164 chips are present.

        @param[in] chipId   ID of the chip to check for.
        @returns            True if one (or more) of the DataStreamControl::physicalDataStreams contains the given chip type.
    */
    bool DataStreamControl::containsChip(typed_register_t::ChipId chipId) {
        for (unsigned int source = 0; source < MAX_NUM_BOARD_DATA_SOURCES; source++) {
            if (physicalDataStreams[source].chipId == chipId) {
                return true;
            }
        }
        return false;
    }

    /** \brief Configure data streams used for reading, with certain data sources allowed.

        Based on the read-in information stored in DataStreamControl::physicalDataStreams,
        configures DataStreamControl::logicalDataStreams to be the first 8 streams
        needed.

        If more than 256 channels are present, picks the first 256 channels.

        Note that RHD2216 chips require one data stream (i.e., 32 channels), not 1/2
        a data stream.

        @param[in] allowDataSource - false at a given element means "don't use this source," true means "use this source if a chip is present."
        @returns true if it successfully configured data streams; false if more than 8 datastreams were needed.
    */
    bool DataStreamControl::configureDataStreams(bool allowDataSource[MAX_NUM_BOARD_DATA_SOURCES]) {
        bool success = true;

        // Re-initialize all the stream configs
        for (unsigned int source = 0; source < MAX_NUM_BOARD_DATA_SOURCES; source++) {
            physicalDataStreams[source].firstDataStream = nullptr;
            physicalDataStreams[source].ddrDataStream = nullptr;
        }
        for (unsigned int stream = 0; stream < MAX_NUM_DATA_STREAMS; stream++) {
            logicalDataStreams[stream].tieTo(nullptr, false);
        }

        // Configure USB data streams in consecutive order to accommodate all connected chips.
        int stream = 0;
        for (unsigned int source = 0; source < MAX_NUM_BOARD_DATA_SOURCES; ++source) {
            unsigned int numStreams = physicalDataStreams[source].getNumStreams();
            if (allowDataSource[source] && numStreams > 0) {
                unsigned int maxStreamNeeded = stream + numStreams - 1;
                if (maxStreamNeeded < MAX_NUM_DATA_STREAMS) {
                    logicalDataStreams[stream++].tieTo(&physicalDataStreams[source], false);
                    if (numStreams == 2) {
                        logicalDataStreams[stream++].tieTo(&physicalDataStreams[source], true);
                    }
                } else {
                    // Skip it
                    success = false;
                }
            }
        }
        logicalValid = true;

        return success;
    }

    /** \brief Automatically configure data streams used for reading, using all datasources with chips.

        @returns true if it successfully autoconfigured data streams; false if more than 8 datastreams were needed.
    */
    bool DataStreamControl::autoConfigureDataStreams()
    {
        bool enabled[MAX_NUM_BOARD_DATA_SOURCES] = { true, true, true, true, true, true, true, true };
        return configureDataStreams(enabled);
    }

    /** \brief Number of enabled data streams.

        @returns The number of enabled (logical) data streams.
    */
    int DataStreamControl::getNumEnabledDataStreams() {
        int sum = 0;
        for (unsigned int stream = 0; stream < MAX_NUM_DATA_STREAMS; stream++) {
            if (logicalDataStreams[stream].underlying != nullptr) {
                sum++;
            }
        }
        return sum;
    }

    //  ------------------------------------------------------------------------
    void CommandConfig::set(std::vector<int> commandList_) {
        if (!dirty) {
            // If we're dirty, stay dirty.  If not, only become dirty if the command list is different.
            dirty = (commandList != commandList_);
        }
        commandList = commandList_;
    }
    //  ------------------------------------------------------------------------
    AuxiliaryCommandControl::AuxiliaryCommandControl() :
        chipRegisters(Rhd2000EvalBoard::SampleRate1000Hz)  // Rate is changed before it's needed
    {
        // Create command lists to be uploaded to auxiliary command slots Aux1 & 2
        chipRegisters.setDigOutLow();   // Take auxiliary output out of HiZ mode.
        createDigitalOutAndSensorsCommands();
        createDCZCheckCommand();
    }

    /** \brief Updates the in-memory command lists related to digital output and reading sensors.

        Changes in Register 3 of AuxiliaryCommandControl::chipRegisters are converted 
        to command lists by calling this function.  
        
        The affected command lists are:
        - Digital Output(Auxiliary Command Slot 1, Bank 0)
        - Read Temperature Sensor, AuxIn1, AuxIn2, AuxIn3, Voltage Supply(Auxiliary Command Slot 2, Bank 0)
    */
    void AuxiliaryCommandControl::createDigitalOutAndSensorsCommands() {
        vector<int> commandList;

        // Create a command list for the AuxCmd1 slot.  This command sequence will continuously
        // update Register 3, which controls the auxiliary digital output pin on each RHD2000 chip.
        // In concert with the v1.4 Rhythm FPGA code, this permits real-time control of the digital
        // output pin on chips on each SPI port.
        chipRegisters.createCommandListUpdateDigOut(commandList);
        commandSlots[Rhd2000EvalBoard::AuxCmd1].banks[0].set(commandList);
        commandSlots[Rhd2000EvalBoard::AuxCmd1].selectBank(0);

        // Next, we'll create a command list for the AuxCmd2 slot.  This command sequence
        // will sample the temperature sensor and other auxiliary ADC inputs.
        chipRegisters.createCommandListTempSensor(commandList);
        commandSlots[Rhd2000EvalBoard::AuxCmd2].banks[0].set(commandList);
        commandSlots[Rhd2000EvalBoard::AuxCmd2].selectBank(0);
    }

    /** \brief Updates the in-memory command lists related to registers (normal usage)

        Changes in registers other than 3 or 6 of AuxiliaryCommandControl::chipRegisters are converted 
        to command lists by calling this function.  The register configuration command lists
        for normal usage (not impedance measurement) are configured by this function.

        The affected command lists are:
        - Configure registers with ADC Calibration enabled (Auxiliary Command Slot 3, Bank 0)
        - Configure registers (Auxiliary Command Slot 3, Bank 1)
        - Configure registers with fast settle enabled (Auxiliary Command Slot 3, Bank 2)
    */
    void AuxiliaryCommandControl::updateRegisterConfigCommandLists() {
        // For the AuxCmd3 slot, we will create three command sequences.  All sequences
        // will configure and read back the RHD2000 chip registers, but one sequence will
        // also run ADC calibration.  Another sequence will enable amplifier 'fast settle'.

        vector<int> commandList;

        // Upload version with ADC calibration to AuxCmd3 RAM Bank 0.
        chipRegisters.createCommandListRegisterConfig(commandList, true);
        commandSlots[Rhd2000EvalBoard::AuxCmd3].banks[0].set(commandList);

        // Upload version with no ADC calibration to AuxCmd3 RAM Bank 1.
        chipRegisters.createCommandListRegisterConfig(commandList, false);
        commandSlots[Rhd2000EvalBoard::AuxCmd3].banks[1].set(commandList);

        // Upload version with fast settle enabled to AuxCmd3 RAM Bank 2.
        chipRegisters.setFastSettle(true);
        chipRegisters.createCommandListRegisterConfig(commandList, false);
        commandSlots[Rhd2000EvalBoard::AuxCmd3].banks[2].set(commandList);
        chipRegisters.setFastSettle(false);

    }

    /** \brief Updates the in-memory command lists related to the impedance waveform

        Creates a sine wave for use in measuring impedance.  Internally, this waveform
        uses Register 6, but that's internal only; note that this function does <b>not</b>
        use Register 6 of the AuxiliaryCommandControl::chipRegisters member variable.

        The affected command list is:
        - Waveform for impedance check (Auxiliary Command Slot 1, Bank 1)

        @param[in] sampleRate       Board sampling rate
        @param[in] impedanceFreq    Frequency of impedance measurement sine wave.
    */
    void AuxiliaryCommandControl::createImpedanceDACsCommand(double sampleRate, double impedanceFreq) {
        Rhd2000Registers chipRegisters(sampleRate);
        vector<int> commandList;

        // Create a command list for the AuxCmd1 slot.
        chipRegisters.createCommandListZcheckDac(commandList, impedanceFreq, 128.0);
        commandSlots[Rhd2000EvalBoard::AuxCmd1].banks[1].set(commandList);
    }

    /** \brief Updates the in-memory command list related to the DC plating waveform

    Creates a DC (i.e., constant voltage) waveform for use in plating.  Internally, this waveform
    uses Register 6, but that's internal only; note that this function does <b>not</b>
    use Register 6 of the AuxiliaryCommandControl::chipRegisters member variable.

    The affected command list is:
    - DC waveform for plating (Auxiliary Command Slot 1, Bank 2)

    */
    void AuxiliaryCommandControl::createDCZCheckCommand() {
        Rhd2000Registers chipRegisters(Rhd2000EvalBoard::SampleRate1000Hz);  // Rate doesn't matter
        vector<int> commandList;

        // Create a command list for the AuxCmd1 slot.
        chipRegisters.createCommandListZcheckDac(commandList, 0, 128.0);
        commandSlots[Rhd2000EvalBoard::AuxCmd1].banks[2].set(commandList);
    }

    /** \brief Updates the in-memory command lists related to registers (impedance measurement)

        Changes in registers other than 3 or 6 of AuxiliaryCommandControl::chipRegisters are converted 
        to command lists by calling this function.  The register configuration command lists
        for impedance measurement (not normal usage) are configured by this function.

        The affected command lists are:
        - Configure registers with impedance check enabled (Auxiliary Command Slot 3, Bank 3)

        See \ref impedancePage "here" for an overview of impedance measurements.
    */
    void AuxiliaryCommandControl::updateImpedanceRegisters() {
        // Upload version with no ADC calibration to AuxCmd3 RAM Bank 3.
        vector<int> commandList;

        chipRegisters.enableZcheck(true);
        chipRegisters.createCommandListRegisterConfig(commandList, false);
        commandSlots[Rhd2000EvalBoard::AuxCmd3].banks[3].set(commandList);
        chipRegisters.enableZcheck(false);
    }

    //  ------------------------------------------------------------------------
    FastSettleControl::FastSettleControl() {
        enabled = false;
        external = false;
        channel = 0;
    }


    //  ------------------------------------------------------------------------
    ReadControl::ReadControl() : currentBlockNum(0), continuous(false)
    {

    }

    /// Empty the in-memory data queue.
    void ReadControl::emptyQueue() {
        // dataQueue.clear does extra work (freeing more memory) that causes a significant slow-down
        dataQueue.erase(dataQueue.begin(), dataQueue.end());
    }
}
