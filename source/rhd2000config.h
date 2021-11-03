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

#ifndef RHD2000CONFIG_H
#define RHD2000CONFIG_H

#include "rhd2000evalboard.h"
#include "rhd2000registers.h"
#include "rhd2000datablock.h"
#include <complex>

class BoardControl;

namespace Rhd2000Config {
    struct BandWidth;
    /** \brief Frequency configuration for impedance measurements.

        See \ref impedancePage "here" for an overview of impedance measurements.

        This class controls the impedance frequency.  Note that you should
        only ever set the ImpedanceFreq::desiredImpedanceFreq member, and that
        only via the changeImpedanceValues() method; the raw members should
        be treated as read-only.

        Also, the actual impedance frequency is dependent on the board sampling
        rate and bandwidth limiting parameters, so when those change, the
        dependencyChanged() method should be called.
    */
    struct ImpedanceFreq {
        ImpedanceFreq(double& boardSampleRate_, const BandWidth& bandwidth_);

        /// Desired Impedance Frequency.  Read-only.
        double desiredImpedanceFreq;
        /// Actual Impedance Frequency (closest frequency to ImpedanceFreq::desiredImpedanceFreq that is possible, given board sampling rate and filter bandwidth).  Read-only.
        double actualImpedanceFreq;
        /// True if the ImpedanceFreq::actualImpedanceFreq is valid (i.e., achievable with given board sampling rate and filter bandwidth).  Read-only.
        bool impedanceFreqValid;

        void changeImpedanceValues(double desiredImpedanceFreq_);
        void dependencyChanged();  // When a setting that we're dependent on changes, call this

        // Used while we're actively measuring
        /// Number of data blocks to read for a measurement
        int numBlocks;
        /// Start index for calculating frequency amplitudes.  Big enough that a couple periods go by first for the signal to settle.
        int startIndex;
        /// End index for calculating frequency amplitudes.
        int endIndex;
        /// Calculates the values of \p numBlocks, \p startIndex, and \p endIndex.
        void calculateValues();

        std::complex<double> amplitudeOfFreqComponent(double* data);
        std::complex<double> calculateBestImpedanceOneAmplifier(std::vector<std::complex<double> >& measuredAmplitudes);

        double approximateSaturationVoltage(double actualZFreq, double highCutoff);

    private:
        // Dependencies from boardControl; i.e., when these change, we need to change too
        double& boardSampleRate;
        const BandWidth& bandwidth;

        double getPeriod();
        void updateImpedanceFrequency();
        std::complex<double> factorOutParallelCapacitance(std::complex<double> zIn, double parasiticCapacitance);
        std::complex<double> empiricalResistanceCorrection(std::complex<double> zIn);
    };

    /** \brief Controls bandwidth of the on-chip analog and digital filters on the amplifier lines.

        Note: this configures Bandwidth values in memory; you need to call
        BoardControl::updateBandwidth to send these values to the actual board.

        One of the key features of the RHD2xxx chips is the ability to do on-chip filtering.  
        This is accomplished via analog low-pass and high-pass filters and an optional digital
        high-pass filter.

        For all of these filters, you set a desired cutoff frequency, and the (computer-side)
        software calculates the actual cutoff frequency you will achieve, based on the sampling
        rate and the available on-chip resistors for bandwidth setting.

        \section bandwidthDetails Detailed information

        Each amplifier on an RHD2xxx chip has a pass band defined by analog circuitry 
        that includes a high-pass filter and a low-pass filter.  The lower end of the pass 
        band has a first-order high-pass characteristic.  The upper end of the pass 
        band is set by a third-order Butterworth low-pass filter.

        Each RHD2xxx includes an on-chip module that performs digital signal processing 
        (DSP) to implement an additional first-order high-pass filter on each digitized amplifier 
        waveform.   This feature is used to remove the residual DC offset voltages associated 
        with the analog amplifiers.

        The diagram below shows a simplified functional diagram of one channel in an 
        RHD2xxx chip.  For more information, consult the <b>RHD2000 series digital 
        physiology interface chip datasheet</b>, 
        which can be found on the <a href="http://intantech.com/downloads.html">Downloads page</a> of the Intan Technologies website.

        \image html help_diagram_chip_filters.png

        The general recommendation for best linearity is to set the DSP cutoff frequency to 
        the desired low-frequency cutoff and to set the amplifier lower bandwidth 2x to 10x 
        lower than this frequency.  Note that the DSP cutoff frequency has a limited frequency 
        resolution (stepping in powers of two), so if a precise value of low-frequency cutoff 
        is required, the amplifier lower bandwidth could be used to define this and the DSP 
        cutoff frequency set 2x to 10x below this point.  If both the DSP cutoff frequency and 
        the amplifier lower bandwidth are set to the same (or similar) frequencies, the actual 
        3-dB cutoff frequency will be higher than either frequency due to the combined effect of 
        the two filters.

        For a detailed mathematical description of all three on-chip filters, visit the 
        <a href="http://intantech.com/downloads.html">Downloads page</a> on the Intan Technologies website and consult the document <b>FAQ: 
        RHD2000 amplifier filter characteristics</b>.

        Note: the on-chip filters apply to the amplifier inputs only, not to the three auxiliary inputs.
        */
    struct BandWidth {
        /// Desired DSP Cutoff Frequency.  Should be treated as read-only, and only set via the changeValues() method.
        double desiredDspCutoffFreq;
        /// Actual (i.e., best-achievable) DSP Cutoff Frequency.  Should be treated as read-only, and only set via the changeValues() method.
        double actualDspCutoffFreq;
        /// Desired Upper Cutoff Frequency.  Should be treated as read-only, and only set via the changeValues() method.
        double desiredUpperBandwidth;
        /// Actual (i.e., best-achievable) Upper Cutoff Frequency.  Should be treated as read-only, and only set via the changeValues() method.
        double actualUpperBandwidth;
        /// Desired Lower Cutoff Frequency.  Should be treated as read-only, and only set via the changeValues() method.
        double desiredLowerBandwidth;
        /// Actual (i.e., best-achievable) Lower Cutoff Frequency.  Should be treated as read-only, and only set via the changeValues() method.
        double actualLowerBandwidth;
        /// Whether or not the DSP is enabled.  Should be treated as read-only, and only set via the changeValues() method.
        bool dspEnabled;

        BandWidth();

        void setChipRegisters(Rhd2000Registers& chipRegisters);
        void changeValues(double desiredDspCutoffFreq_, double desiredUpperBandwidth_, double desiredLowerBandwidth_, bool dspEnabled_, double boardSampleRate);
    };

    /** \brief Controls the 8 LEDs on the RHD2000 Evaluation Board.

        Note: this configures LED values in memory; you need to call
        BoardControl::updateLEDs to send these values to the actual board.

        The LEDControl::values member provides in-memory control of the 8 LEDs.  

        Additionally, this class has a 'progress counter' functionality, where a
        single LED is lit at once, and incrementing the progress counter cycles
        through which one is currently lit.
     */
    class LEDControl {
    public:
        LEDControl();

        /** \brief Values to store out to the board.

            Values should be 0 for not lit, 1 for lit.

            values[7] corresponds to the LED closest to the USB connector.
        */
        int values[8];

        void clear();
        void startProgressCounter();
        void incProgressCounter();

    private:
        int index;    // For using LEDs as a progress indicator, we keep values[index] on and the rest off
    };

    /** \brief Configuration for one threshold comparator

        Note: this configures a threshold comparator in memory; you need to call
        BoardControl::updateDigitalOutputs to send these values to the actual board.

        See DigitalOutputControl for a discussion of what the threshold comparators are
        and how to enable them.

        Each comparator can be configured independently to trigger on either
        rising or falling edges that exceed a certain threshold.
    */
    class ThresholdComparatorConfig {
    public:
        ThresholdComparatorConfig() : threshold(0.0), risingEdge(true), dirty(false) {}

        /** \brief Configures a threshold comparator.

            Note: this configures a threshold comparator in memory; you need to call
            BoardControl::updateDigitalOutputs to send these values to the actual board.

            @param[in] threshold_       Threshold in &mu;V.
            @param[in] risingEdge_      True = trigger on rising edge; false = trigger on falling edge.
        */
        void set(double threshold_, bool risingEdge_) {
            threshold = threshold_;
            risingEdge = risingEdge_;
            dirty = true;
        }

        void get(double &threshold_, bool &risingEdge_) {
            threshold_ = threshold;
            risingEdge_ = risingEdge;
        }

    private:
        double threshold;
        bool risingEdge;
        bool dirty; // Value has only changed in memory and hence needs to be written to board

        /** \cond PRIVATE */
        friend class ::BoardControl;
        /** \endcond */
    };

    /** \brief Functionality to control digital outputs on the RHD2000 evaluation board.
    
        Note: this configures digital outputs in memory; you need to call 
        BoardControl::updateDigitalOutputs to send these values to the actual board.

        The RHD2000 Evaluation Board contains 16 digital outputs.  Additional auxiliary
        digital outputs on the individual RHD2xxx chips are controlled via
        AuxDigitalOutputControl and AuxDigOutputConfig, not by the current class.

        These 16 digital outputs can be set in one of two modes:
        \li 16 user-controllable digital outputs
        \li 8 user-controllable digital outputs and 8 digital outputs acting as threshold comparators

        Mode selection is controlled by the DigitalOutputControl::comparatorsEnabled member.  By default
        at board startup, the latter mode is selected.

        \section mode16 16 user-controllable digital outputs
        If the "16 user-controllable digital outputs" mode is set (DigitalOutputControl::comparatorsEnabled = false), 
        the 16 outputs are controlled directly by the DigitalOutputControl::values member.

        \section mode8and8 8 user-controllable digital outputs and 8 digital outputs acting as threshold comparators
        If the "8 user-controllable digital outputs and 8 digital outputs acting as threshold comparators"
        mode is set (DigitalOutputControl::comparatorsEnabled = true), then the first 8 digital outputs (0-7) 
        are threshold comparators (configured by the DigitalOutputControl::comparators member), and the
        second 8 digital outputs (8-15) are controlled directly (by the DigitalOutputControl::values member).

        \image html digital_output.png

        In this mode, digital outputs 0-7 are the result of a threshold comparison of the RHD2000 evaluation 
        board's analog outputs 0-7.  There is no remapping: digital output X always corresponds to analog output X.  
        The threshold of each comparator can be set independently, as can the polarity (i.e.,
        is it a rising-edge threshold or a falling-edge threshold).  See DigitalOutputControl::comparators
        for more information.

        See AnalogOutputControl for a more complete description of analog output configuration.  Note that the
        comparator applies after the raw analog value has been passed through a high-pass filter, but before
        any subsequent processing (digital gain or noise slicing).
    */
    class DigitalOutputControl {
    public:
        DigitalOutputControl();

        /** \brief Threshold comparator configuration of the 8 threshold comparators.

            Only active if DigitalOutputControl::comparatorsEnabled = true.

            See ThresholdComparatorConfig for details.
         */
        ThresholdComparatorConfig comparators[8];
        /** \brief Digital output values (0 or 1).
            
            If DigitalOutputControl::comparatorsEnabled = true, only values 8-15 are used
            as digital outputs; values 0-7 are controlled by the threshold comparators.
         */
        int values[16];

        /** \brief Chooses whether to use threshold comparators, and whether 8 or 16 outputs are user controllable.

            \li True = 8 user-controllable digital outputs and 8 digital outputs acting as threshold comparators.
            \li False = 16 user-controllable digital outputs
        */
        bool comparatorsEnabled;

        void clear();
        void setDacThreshold(int dacIndex, double threshold);
    };

    /** \brief Configuration for a single analog output channel, controlled by a digital-to-analog converter (DAC).
    
        Note: this configures analog outputs in memory; you need to call
        BoardControl::updateAnalogOutputSources to send these values to the actual board.

        See AnalogOutputControl for a background on the RHD2000 Evaluation Board's 8 analog outputs and how to configure them.

        The output value is not set directly here, rather the source of the value is set, and when the source changes, the value
        changes in response.  The source is set via the DacConfig::dataStream and DacConfig::channel members.

        See the member variables for more information.

    */
    struct DacConfig {
        /** \brief Enable or disable an analog output.
        
            Disabled channels output 0 V.
        */
        bool enabled;
        /** \brief Configures source for analog output (i.e., the input from this causes an analog output).

            Possible sources :
            \li DacManual value - set this value to 8; value comes from the AnalogOutputControl::dacManual value.
            \li Amplifier input - this value controls which datastream the input comes from (datastream 0-7).
        */
        int dataStream;
        /** \brief Chooses amplifier channel

            When DacConfig::datastream is set to 0-7, this value picks which amplifier channel from that datastream
            is used to control the analog output.  Note that this must be an amplifier, not one of the auxiliary inputs.
        */
        int channel;

        DacConfig() : enabled(false), dataStream(0), channel(0) {}
    };

    /** \brief Functionality related to analog outputs.

        Note: this configures analog outputs in memory; you need to call
        BoardControl::updateAnalogOutputCommon to send these values to the actual board, and 
        BoardControl::updateAnalogOutputSources to send the contents of the AnalogOutputControl::dacs member variable.

        The RHD2000 Evaluation Board has 8 analog outputs.  These are sometimes referred to as DAC outputs,
        because a digital signal is converted to analog via a digital-to-analog converter (DAC) before producing
        the analog output.

        \image html analog_output.png

        Each of the 8 analog outputs can be configured individually; see the AnalogOutputControl::dacs member and the 
        DacConfig class for more information.  Each may either be configured to come from an amplifier channel on one of the
        RHD2xxx chips attached to the board, or from the AnalogOutputControl::dacManual value.

        After the digital input value is converted to analog, the signal passes through an optional highpass filter,
        configured via the AnalogOutputControl::highpassFilterEnabled and AnalogOutputControl::highpassFilterFrequency members.
        If the digital input value changes rapidly, you may want to settle that filter using the AnalogOutputControl::dspSettle
        member.

        After the highpass filter, the signal is amplified via the AnalogOutputControl::dacGain value.

        <h3 class="groupheader">Noise Slicing</h3>

        The signals from analog outputs 1 and 2 are routed to the Audio Line Out as well, and noise suppression
        may occur via the AnalogOutputControl::noiseSuppress parameter.  Audio noise suppression only affects analog
        outputs 1 and 2.  Note that both the Audio Line Out and the analog output 1 and 2 outputs are post-noise suppression.

        \image html noise_slicing.png

        Any data points of the waveform that fall within the slice range are set to zero, and signals extending beyond this range are brought in
        towards zero. The result is a dramatic improvement in the audibility of action potentials. Users are encouraged to experiment with this
        feature in neural recording experiments.

        \nosubgrouping
    */
    class AnalogOutputControl {
    public:
        AnalogOutputControl(BoardControl& boardControl_);

        /** \name Digital data source selection and values.
        */
        //@{
        bool setDacManualVolts(double value);

        int getDacManualRaw() const;

        /** \brief Configuration of individual analog outputs.

        See DacConfig for details.

        Note that the C++ objects are indexed 0-7, even though the board is labeled 1-8.  So the board's
        Dac Output 1 is accessed as \p dacs[0], for example.
        */
        DacConfig dacs[8];
        //@}

        /** \name Digital high-pass filter
        */
        //@{
        /** \brief Turn on or off DSP settle function in the FPGA.

        This only executes when the RHD2000 is running and a CONVERT command is executed.
        */
        bool dspSettle;

        /** \brief Enables or disables the optional digital first-order high-pass filters implemented in the FPGA on all eight DAC output channels.

        These filters may be used to remove low-frequency local field potential (LFP) signals from neural signals to facilitate
        spike detection while still recording the complete wideband data.

        This is useful when using the low-latency FPGA thresholds to detect spikes and produce digital pulses on the TTL
        outputs, for example.

        Note that the highpass filters are either on for all 8 channels or off for all 8 channels.
        */
        bool highpassFilterEnabled;
        /** \brief Sets a cutoff frequency for optional digital first-order high-pass filters implemented in the FPGA on all eight DAC/comparator channels.
        */
        double highpassFilterFrequency;
        //@}

        /** \name Noise slicing
        */
        //@{
        /** \brief Degree of audio noise suppression (0-127).

        This improves the audibility of weak neural spikes in noisy waveforms.

        The noise slicing region of the DAC channels is set to between -16 x \p noiseSuppress x <b>step</b> and +16 x \p noiseSuppress x <b>step</b>,
        where <b>step</b> is the minimum step size of the DACs, corresponding to 0.195 &mu;V.

        */
        int noiseSuppress;
        //@}

        /** \name Digital gain
        */
        //@{
        /** \brief Scales the digital signals to all eight AD5662 DACs.

        The gain is scaled by a factor of 2<SUP>gain</SUP>.  Allowed values 0-7
        (corresponding to amplification of 1-128).
        */
        unsigned int dacGain;
        //@}

    private:
        int dacManual;
        BoardControl& boardControl;
    };

    /** \brief Configure the Auxiliary Digital Output on RHD2xxx boards connected to a single port.

        Note: this configures auxiliary digital outputs in memory; you need to call
        BoardControl::updateAuxDigOut to send these values to the actual board.

        See members for details.
    */
    struct AuxDigOutputConfig {
        /// Enable or disable realtime control of the auxiliary digital output pin.
        bool enabled;
        /// Digital input channel that should be tied to this port's auxiliary digital output pin(s).
        int channel;

        AuxDigOutputConfig() : enabled(false), channel(0) {}
    };

    /** \brief Configure the Auxiliary Digital Output on RHD2xxx boards.

        Note: this configures auxiliary digital outputs in memory; you need to call
        BoardControl::updateAuxDigOut to send these values to the actual board.

        Each RHD2xxx chip has an auxiliary digital output pin.  The RHD2000 Evaluation Board
        lets you configure this pin to follow one of the evaluation board's digital inputs.
        So by connecting a digital signal to the digital input, that signal is routed
        to the individual chips attached to ports A, B, C, and D.

        \image html aux_digital_output.png

        Note that if an RHD2000 dual headstage adapter is used and two chips are connected
        to a given port, both of those chips will be configured to use the same digital value.
    */
    class AuxDigitalOutputControl {
    public:
        AuxDigitalOutputControl();

        /// Configuration for the four ports (A, B, C, D).
        AuxDigOutputConfig values[NUM_PORTS];
    };

    class DataStreamConfig;
    /** \brief Configuration of data streams related to a single physical board \ref datasources "data source" (i.e., Port A-D, MISO 1-2).

        This works in conjunction with DataStreamConfig to configure the actual datastreams used by the board.
    */
    struct DataSourceControl {
        /// Chip ID of the chip on this data stream
        Rhd2000RegisterInternals::typed_register_t::ChipId chipId;
        /// Which of the 16 possible data sources this stream represents
        Rhd2000EvalBoard::BoardDataSource dataSource;
        /// Data stream (if any) for the first 32 channels
        DataStreamConfig* firstDataStream;
        /// Data stream (if any) for the second 32 channels (RHD2164 only)
        DataStreamConfig* ddrDataStream;

        DataSourceControl() : firstDataStream(nullptr), ddrDataStream(nullptr) {}

        Rhd2000EvalBoard::BoardPort getPort() const;
        unsigned int getNumStreams() const;
        unsigned int getNumChannels() const;
        DataStreamConfig* getStreamForChannel(unsigned int channel) const;
    };

    /** \brief Configuration of a single \ref datastreamsPage "data stream".

        Note: this configures a single data stream in memory; you need to call
        BoardControl::updateDataStreams to send these values to the actual board.

        See members for details.

        See also DataStreamControl for more information about configuring all data streams.
    */
    class DataStreamConfig {
    public:
        /// Which number data stream is this (0-7).
        unsigned int index;
        /// Is this data stream a \ref ddr "double data rate / MISO B" stream or not?
        bool isDDR;
        /// Data source that this stream is \ref datastreambinding "tied" to, if any.
        DataSourceControl* underlying;

        DataStreamConfig() : isDDR(false), underlying(nullptr) {}

        void tieTo(DataSourceControl* source, bool isddr);
        Rhd2000EvalBoard::BoardDataSource getDataSource() const;
        unsigned int getNumChannels() const;
    };

    /** \brief Configure data streams.

        Note: this configures data streams in memory; you need to call
        BoardControl::updateDataStreams to send these values to the actual board.

        Rhythm reads data back as a series of up to 8 32-channel \ref datastreamsPage "data streams",
        containing a maximum of 256 channels of data (8 x 32).  Data stream management is fairly
        straight-forward for RHD2216 and RHD2132 chips, which each require a single data stream.  The
        complexity comes from RHD2164 chips, which contain two data streams.

        This class provides simplified configuration of the data streams.  It allows arbitrary configurations,
        but you don't have to use that; a call to autoConfigureDataStreams() will set up a good configuration
        automatically after BoardControl::getChipIds() has been called.

        See individual members for advanced configuration options.

    */
    class DataStreamControl {
    public:
        DataStreamControl();

        /** \brief Contains information about each possible source of data, i.e., the 8 combination of port (A, B, C, D) and MISO (1 or 2).  
        
            This variable can be used for reading chip IDs, for example.  But note that each (potential) chip only has one data stream,
            so this configuration wouldn't be appropriate for reading from an RHD2164 chip.  And, since all 8 sources are enabled,
            reading from all of them will produce extra data if not all possible ports and MISOs have chips connected.

            Information is always stored in the order:
            \li Port A MISO 1
            \li Port A MISO 2
            \li Port B MISO 1
            \li Port B MISO 2
            \li Port C MISO 1
            \li Port C MISO 2
            \li Port D MISO 1
            \li Port D MISO 2
        */
        DataSourceControl physicalDataStreams[MAX_NUM_BOARD_DATA_SOURCES];
        bool containsChip(Rhd2000RegisterInternals::typed_register_t::ChipId chipId);
        void resetPhysicalStreams();

        /** \brief Contains the configuration about which datastreams are actually to be read.  
        
            This may involve using two logical streams for an RHD2164 chip, enabling or disabling certain
            chips, and removing empty ports.
            
            For example, if ports A & C have chips, a reasonable logical configuration would be:
            \li Port A
            \li Port C

            bypassing the empty Port B entirely.

            This variable is set as part of autoConfigureDataStreams(), or may be manually configured to turn
            off certain chips.
        */
        DataStreamConfig logicalDataStreams[MAX_NUM_DATA_STREAMS];
        bool configureDataStreams(bool allowDataSource[MAX_NUM_BOARD_DATA_SOURCES]);
        bool autoConfigureDataStreams();
        int getNumEnabledDataStreams();

        /// True once DataStreamControl::physicalDataStreams has been populated.
        bool physicalValid;
        /// True once DataStreamControl::logicalDataStreams has been populated.
        bool logicalValid;
    };

    /** \brief Configure the cable delay used before reading data from RHD2xxx chips.

        Note: this configures cable delays in memory; you need to call
        BoardControl::updateCables to send these values to the actual board.

        The SPI protocol, used for communication between the RHD2000 evaluation board and RHD2xxx chips, consists of :
        \li a bit being sent from the master (i.e., the RHD2000 evaluation board) to the slave (i.e., the RHD2xxx board) - this is called Master Output Slave Input (MOSI)
        \li a bit being sent from the slave (i.e., the RHD2xxx board) to the master (i.e., the RHD2000 evaluation board) - this is called Master Input Slave Output (MISO)

        Typically, both bits are read at the same time.  However, with the RHD2000 evaluation board and the RHD2xxx chip 
        physically separated by a significant distance (several feet or meters), there may be a noticeable lag before 
        the MISO input to the evaluation board stabilizes.  These functions let you to set the delay between when the 
        evaluation board writes the MOSI output and reads the MISO input to compensate for that.

        %Cable delays can be configured either:
        \li Manually - delay in integer clock steps, where each clock step is 1/2800 of a per-channel sampling period.
        \li Automatically - delay is specified in meters

        The BoardControl::getChipIds() function sets the cable delays to optimum values, based on a quick per-chip
        read test.  Those values can be read and modified via this structure.

        Note that the optimum delay changes when the board sampling rate is changed.  If the delays are configured
        in meters, the board will be reconfigured when the board sampling rate is changed.  If the delays are
        configured manually, you'll need to make any adjustments to the delays by hand.
    */
    struct Cable {
        /// True = use Cable::manualDelay to specify delay manually; False = use Cable::lengthMeters to specify delay
        bool manualDelayEnabled;
        /// Delay in integer clock steps, where each clock step is 1 / 2800 of a per - channel sampling period.
        int manualDelay;
        /// Length of the cable in meters; delay is calculated from this as sampling rate changes
        double lengthMeters;

        /// Constructor
        Cable(bool manualDelayEnabled_ = false, int manualDelay_ = 0, double lengthMeters_ = 0.0) :
            manualDelayEnabled(manualDelayEnabled_), manualDelay(manualDelay_), lengthMeters(lengthMeters_) {}
    };

    /** \brief A single command list.

        Stores a single \ref commandlists "command list", i.e., the content of a single \ref banks "bank".

        Internally, this stores additional information as to whether the in-memory command list matches the on-board command list.
    */
    class CommandConfig {
    public:
        CommandConfig() : dirty(false) {}

        /// Sets the command list to store.
        void set(std::vector<int> commandList_);

    private:
        std::vector<int> commandList;
        bool dirty;

        /** \cond PRIVATE */
        friend class ::BoardControl;
        /** \endcond */
    };

    /** \brief A single auxiliary command slot.

        Stores \ref commandlists "command lists " for all \ref banks "banks" in an auxiliary command slot
        (i.e. one of auxcmd1, auxcmd2, or auxcmd3).

        Also stores which of the banks is selected currently.
     */
    class CommandSlotConfig {
    public:
        CommandSlotConfig() : commandListLength(0), selectedIndex(0), dirty(false) {}

        /// The 16 \ref banks "banks" corresponding to this auxiliary command slot.
        CommandConfig banks[NUM_BANKS];

        /** \brief Choose which bank is selected.

            @param[in] index    Which bank is selected
        */
        void selectBank(int index) {
            selectedIndex = index;
            dirty = true;
        }

    private:
        unsigned int commandListLength;
        int selectedIndex;
        bool dirty;

        /** \cond PRIVATE */
        friend class ::BoardControl;
        /** \endcond */
    };

    /** \brief Configure auxiliary command slots.

        Note: this configures auxiliary command slots in memory; you need to call
        BoardControl::updateCommandSlots to send these values to the actual board.

        The RHD2000 board contains 3 command slots for auxiliary commands, called
        AuxCmd1, AuxCmd2, AuxCmd3.  Each of those contains 16 banks.  A command list
        can be uploaded to each of these 3x16 banks (this is a relatively slow operation)
        and each slot can be switched between banks very quickly.

        Full details of command slots, banks, and command lists can be found
        \ref commandlistsPage "here".

        The low-level Rhythm API lets you configure abitrary auxiliary commands.  The
        current class simplifies that, by using the following configuration:

        - Auxiliary Command Slot 1 (AuxCmd1)
          Bank | Content
          ---- | -------
          0    |  \ref commandlist_digitaloutput "Digital Output"
          1    |  \ref commandlist_impedance "Waveform for impedance check"
          2    |  \ref commandlist_impedance "DC waveform for plating"

        - Auxiliary Command Slot 2 (AuxCmd2)
          Bank | Content
          ---- | -------
          0    |  \ref commandlist_temperature "Read Temperature Sensor, AuxIn1, AuxIn2, AuxIn3, Voltage Supply"

        - Auxiliary Command Slot 3 (AuxCmd3)
          Bank | Content
          ---- | -------
          0    |  \ref commandlist_configureregisters "Configure registers" with ADC Calibration enabled
          1    |  \ref commandlist_configureregisters "Configure registers"
          2    |  \ref commandlist_configureregisters "Configure registers" with fast settle enabled
          3    |  \ref commandlist_configureregisters "Configure registers" with impedance check enabled


        At any given time, exactly one bank is selected for each slot.

        These command lists are manipulated via the various members below.  Note that in some cases, you need
        to change the AuxiliaryCommandControl::chipRegisters member, then call one of the member functions
        to update the appropriate command list(s) in memory.
     */
    class AuxiliaryCommandControl {
    public:
        AuxiliaryCommandControl();

        /** \brief Register values to send to the device

            Contains values of all the registers.  
            Changes in register 3 are converted to command lists by calling createDigitalOutAndSensorsCommands() for
            - Digital Output (Auxiliary Command Slot 1, Bank 0)
            - Read Temperature Sensor, AuxIn1, AuxIn2, AuxIn3, Voltage Supply (Auxiliary Command Slot 2, Bank 0)

            Changes in registers other than 3 or 6 are converted to command lists by calling:
            - updateRegisterConfigCommandLists() for
             - Configure registers with ADC Calibration enabled (Auxiliary Command Slot 3, Bank 0)
             - Configure registers (Auxiliary Command Slot 3, Bank 1)
             - Configure registers with fast settle enabled (Auxiliary Command Slot 3, Bank 2)
            - updateImpedanceRegisters() for
             - Configure registers with impedance check enabled (Auxiliary Command Slot 3, Bank 3)

            Register 6 is controlled via createImpedanceDACsCommand(), not by the current variable.
        */
        Rhd2000Registers chipRegisters;

        /// In-memory storage of command slot configurations.  Also used to select the active bank within each command slot.
        CommandSlotConfig commandSlots[NUM_AUX_COMMAND_SLOTS];

        void updateImpedanceRegisters();
        void createDigitalOutAndSensorsCommands();
        void createImpedanceDACsCommand(double sampleRate, double impedanceFreq);
        void createDCZCheckCommand();
        void updateRegisterConfigCommandLists();
    };

    /** \brief Configure the "fast settle" functionality on RHD2xxx chips.

        Note: this configures fast settle in memory; you need to call
        BoardControl::updateFastSettle to send these values to the actual board.

        All RHD2xxx chips have a hardware 'fast settle' function that rapidly
        resets the analog signal path of each amplifier channel to zero to prevent
        (or recover from) saturation caused by large transient input signals such as those
        due to nearby stimulation.  Recovery from amplifier saturation can be slow when
        the lower bandwidth is set to a low frequency (e.g., 1 Hz).

        Fast settle can be configured in one of three ways:
        \li Disabled
        \li Manual - fast settling occurs until it is disabled
        \li External - fast setting is configured via one of the digital input lines

        To select <b>Manual</b> mode, set \p enabled to true.
        To select <b>External</b> mode, set \p enabled to true, set \p external to true, and set a \p channel.

        Note: If <b>Manual</b> mode is selected, fast settling will begin immediately.  
        If <b>External</b> mode is selected, fast settling will occur when the selected digital input goes high.
    */
    struct FastSettleControl {
        /// Enable or disable manual fast settling
        bool enabled;
        /// Enable or disable external fast settling
        bool external;
        /** \brief Digital input line (0-15) to control fast settling (external only).

            A logic high signal on the selected digital input will enable amplifier fast settling.
         */
        int channel;

        FastSettleControl();
    };

    /** \brief Data structures used in reading from a board.

        Maintains a queue of data that has been read from the board.

        The number of blocks to read at a time is configured via ReadControl::numUsbBlocksToRead; that typically 
        occurs as part of BoardControl::changeSampleRate(), or it can be done manually here.

        When BoardControl::readBlocks() is called, it reads ReadControl::numUsbBlocksToRead blocks (if available)
        into ReadControl::dataQueue, and calculates ReadControl::latency and ReadControl::fifoPercentageFull.

        Subsequent data processing operations may occur on the data in ReadControl::dataQueue, including the
        usual deque operations, such as pop_front.  As such operations are in-memory, they will typically be
        faster than the BoardControl::readBlocks() operation.
    */
    struct ReadControl {
        ReadControl();

        /** \brief The number of blocks to read at a time.

            Note: setting this value too low will result in very inefficient usage of the USB, and may cause
            the FPGA's FIFO to fill up faster than the data can be read out.  Setting the value too high
            will cause an unnecessary lag between events and when the data is read into the computer.
        */
        unsigned int numUsbBlocksToRead;
        /// Data that has been read from the board
        std::deque<std::unique_ptr<Rhd2000DataBlock>> dataQueue;

        /** \brief Read latency, in ms.

            This can be interpreted as amount of data (in milliseconds worth) that is in the board's FIFO
            and has not yet been transferred to the computer.
         */
        double latency;
        /** \brief FPGA's FIFO percentage full.  
        
            You should take care to ensure the FIFO doesn't fill up and overflow; it may require a board reset or even power cycling
            the evaluation board, and the software on the computer may lock up.
        */
        double fifoPercentageFull;

        void emptyQueue();

        unsigned int currentBlockNum;

        /** \brief True if the board is running continuously.
        */
        bool continuous;
    };

}

#endif // RHD2000CONFIG_H
