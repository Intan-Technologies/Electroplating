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

#include "boardcontrol.h"
#include <algorithm>
#include "saveformat.h"
#include <string.h>
#include <QtCore>

using std::vector;
using Rhd2000RegisterInternals::typed_register_t;
using namespace Rhd2000Config;

//  ------------------------------------------------------------------------
BoardControl::BoardControl() :
    leds(),
    analogOutputs(*this),
    dataStreams(),
    bandWidth(),
    fastSettle(),
    impedance(boardSampleRate, bandWidth),
    saveList(new SaveList()),
    header(*this)
{
	evalBoardMode = 0;
}

BoardControl::~BoardControl() {
    close();
}

/** \brief Writes LED configuration to the board.

    LEDs are configured via the BoardControl::leds member variable. This function sends the value of that variable to the board.

    See Rhd2000Config::LEDControl for more information.
*/
void BoardControl::updateLEDs() {
    if (okayToRunBoardCommands()) {
        evalBoard->setLedDisplay(leds.values);
    }
}

/** \brief Writes digital output configuration to the board.

    Digital outputs are configured via the BoardControl::digitalOutputs member variable. 
    This function sends the value(s) in that variable to the board.

    See Rhd2000Config::DigitalOutputControl for more information.
*/
void BoardControl::updateDigitalOutputs() {
    if (okayToRunBoardCommands()) {
        evalBoard->setTtlOut(digitalOutputs.values);

        if (digitalOutputs.comparatorsEnabled) {
            for (unsigned int dacIndex = 0; dacIndex < NUM_BOARD_ANALOG_OUTPUTS; dacIndex++) {
                ThresholdComparatorConfig &comparator = digitalOutputs.comparators[dacIndex];
                if (comparator.dirty) {
                    int threshLevel = Rhd2000DataBlock::microVoltsToAmplifierADC(comparator.threshold);
                    evalBoard->setDacThreshold(dacIndex, threshLevel, comparator.risingEdge);
                    comparator.dirty = false;
                }
            }
        }

    }
}

/** \brief Configures the board to have 16 user-controllable digital outputs.

    The board can be configured for either:
    \li 16 user-controllable digital outputs
    \li 8 user-controllable digital outputs and 8 digital outputs acting as threshold comparators

    Digital outputs are configured via the BoardControl::digitalOutputs member variable.

    See Rhd2000Config::DigitalOutputControl for more information.
*/
void BoardControl::configure16DigitalOutputs() {
    digitalOutputs.comparatorsEnabled = false;
    if (okayToRunBoardCommands()) {
        evalBoard->setTtlMode(0);
    }
}

/** \brief Configures the board to have 8 user-controllable digital outputs and 8 digital outputs acting as threshold comparators.

    The board can be configured for either:
    \li 16 user-controllable digital outputs
    \li 8 user-controllable digital outputs and 8 digital outputs acting as threshold comparators

    Digital outputs are configured via the BoardControl::digitalOutputs member variable.

    See Rhd2000Config::DigitalOutputControl for more information.
*/
void BoardControl::configure8DigitalOutputs8Comparators() {
    digitalOutputs.comparatorsEnabled = true;
    if (okayToRunBoardCommands()) {
        evalBoard->setTtlMode(1);
    }
}

/** \brief Per-dac version of updateAnalogOutputSources()
*/
void BoardControl::updateAnalogOutputSource(unsigned int dac) {
    if (okayToRunBoardCommands()) {
        const DacConfig& config = analogOutputs.dacs[dac];
        evalBoard->enableDac(dac, config.enabled);
        if (config.enabled) {
            evalBoard->selectDacDataStream(dac, config.dataStream);
            if (config.dataStream != DAC_MANUAL_INDEX) {
                evalBoard->selectDacDataChannel(dac, config.channel);
            }
        }
    }
}

/** \brief Writes the per-dac "analog output source" configuration to the board.

    Analog output sources are configured via the \p dacs member of the BoardControl::analogOutputs member variable. 
    This function sends the value(s) in that variable to the board.

    See Rhd2000Config::DacConfig for more information.
*/
void BoardControl::updateAnalogOutputSources() {
    if (okayToRunBoardCommands()) {
        for (unsigned int i = 0; i < NUM_BOARD_ANALOG_OUTPUTS; i++) {
            updateAnalogOutputSource(i);
        }
    }
}

/** \brief Writes the "common to all analog outputs" configuration to the board.

    Analog outputs are configured via the BoardControl::analogOutputs member variable. This function sends the value(s) in that variable to the board.

    See Rhd2000Config::AnalogOutputControl for more information.
*/
void BoardControl::updateAnalogOutputCommon() {
    if (okayToRunBoardCommands()) {
        evalBoard->enableDacHighpassFilter(analogOutputs.highpassFilterEnabled);
        evalBoard->setDacHighpassFilter(analogOutputs.highpassFilterFrequency);
        evalBoard->setDacGain(analogOutputs.dacGain);
        evalBoard->setAudioNoiseSuppress(analogOutputs.noiseSuppress);
        evalBoard->setDspSettle(analogOutputs.dspSettle);
    }
}

/** \brief Writes the "DAC Manual" value to the board.

    DAC Manual is set via the BoardControl::analogOutputs member variable. This function sends the value in that variable to the board.

    See Rhd2000Config::AnalogOutputControl for more information.
*/
void BoardControl::updateDACManual() {
    if (okayToRunBoardCommands()) {
        evalBoard->setDacManual(analogOutputs.getDacManualRaw());
    }
}

/** \brief Disables auxiliary digital outputs on the RHD2xxx chips connected to the evaluation board.

    Specifically, this function configures the auxiliary inputs not to use an external signal.

    This function bypasses the BoardControl::auxDigOutputs member variable. So you can use it to
    disable the auxiliary digital outputs, do some operation, then call updateAuxDigOut() to restore
    the settings.

    Note that this function only configures auxiliary digital outputs; it doesn't set their values.

    See Rhd2000Config::AuxDigitalOutputControl for more information.
*/
void BoardControl::disableAuxDigOut() {
    if (okayToRunBoardCommands()) {
        for (unsigned int port = 0; port < NUM_PORTS; port++) {
            Rhd2000EvalBoard::BoardPort boardPort = static_cast<Rhd2000EvalBoard::BoardPort>(port);
            evalBoard->enableExternalDigOut(boardPort, false);
        }
    }
}

/** \brief Writes auxiliary digital output configuration to the board.

    Auxiliary digital outputs on the RHD2xxx chips connected to the evaluation board are configured via 
    the BoardControl::auxDigOutputs member variable. This function sends the value(s) in that variable to the board.

    Note that this function only configures auxiliary digital outputs; it doesn't set their values.

    See Rhd2000Config::AuxDigitalOutputControl for more information.
*/
void BoardControl::updateAuxDigOut() {
    if (okayToRunBoardCommands()) {
        for (unsigned int port = 0; port < NUM_PORTS; port++) {
            Rhd2000EvalBoard::BoardPort boardPort = static_cast<Rhd2000EvalBoard::BoardPort>(port);
            evalBoard->enableExternalDigOut(boardPort, auxDigOutputs.values[port].enabled);
            evalBoard->setExternalDigOutChannel(boardPort, auxDigOutputs.values[port].channel);
        }
    }
}

/** \brief Writes logical data stream configuration to the board.

    \ref datastreamsPage "Data streams" are configured via the BoardControl::dataStreams member variable. 
    This function sends the value(s) in that variable to the board.  It also changes the names of the
    BoardControl::signalSources members to match the streams.

    See Rhd2000Config::DataStreamControl for more information.
*/
void BoardControl::updateDataStreams() {
    if (okayToRunBoardCommands()) {
        for (unsigned int stream = 0; stream < MAX_NUM_DATA_STREAMS; ++stream) {
            const DataStreamConfig& logicalStream = dataStreams.logicalDataStreams[stream];
            bool enabled = (logicalStream.underlying != nullptr);
            evalBoard->enableDataStream(stream, enabled);
            if (enabled) {
                evalBoard->setDataSource(stream, logicalStream.getDataSource());
            }
        }
    }
    setSignalSources();
}

/** \brief Enables all the physical data streams, corresponding to MISO A for each chip.
*/
void BoardControl::enablePhysicalDataStreams() {
    if (okayToRunBoardCommands()) {
        for (unsigned int source = 0; source < MAX_NUM_BOARD_DATA_SOURCES; ++source) {
            evalBoard->enableDataStream(source, true);
            evalBoard->setDataSource(source, dataStreams.physicalDataStreams[source].dataSource);
        }
    }
}

/** \brief Reads the digital inputs of the RHD2000 evaluation board.

    Note: this function provides an immediate read of the digital inputs.  Another way to read the digital inputs is
    via the readBlocks() function, which provides the digital inputs synchronized with other inputs (e.g., amplifiers)
    as well as the board's digital and analog outputs.

    @param[out] inputValues     Values of the 16 digital inputs.
 */
void BoardControl::readDigitalInputs(int inputValues[]) {
    if (okayToRunBoardCommands()) {
        evalBoard->getTtlIn(inputValues);
    }
}

/** \brief Writes cable delay configuration to the board.

    Cable delays for ports A, B, C, and D are configured via the BoardControl::cables member variable. 
    This function sends the value(s) in that variable to the board.

    See Rhd2000Config::Cable for more information.
*/
void BoardControl::updateCables() {
    if (okayToRunBoardCommands()) {
        for (unsigned int port = 0; port < NUM_PORTS; port++) {
            Rhd2000EvalBoard::BoardPort portEnum = static_cast<Rhd2000EvalBoard::BoardPort>(port);
            if (cables[port].manualDelayEnabled) {
                evalBoard->setCableDelay(portEnum, cables[port].manualDelay);
            } else {
                evalBoard->setCableLengthMeters(portEnum, cables[port].lengthMeters);
            }
        }
    }
}

/** \brief Writes auxiliary command list configuration to the board.

    \ref commandlistsPage "Command lists" for the three auxiliary command slots 
    are configured via the BoardControl::auxCmds member variable. 
    This function sends the value(s) in that variable to the board.

    See Rhd2000Config::AuxiliaryCommandControl for more information.
*/
void BoardControl::updateCommandSlots() {
    if (okayToRunBoardCommands()) {
        for (unsigned int slotIndex = 0; slotIndex < NUM_AUX_COMMAND_SLOTS; slotIndex++) {
            Rhd2000EvalBoard::AuxCmdSlot slot = static_cast<Rhd2000EvalBoard::AuxCmdSlot>(slotIndex);
            for (unsigned int bank = 0; bank < NUM_BANKS; bank++) {
                CommandConfig& command = auxCmds.commandSlots[slotIndex].banks[bank];
                if (command.dirty) {
                    evalBoard->uploadCommandList(command.commandList, slot, bank);
                    command.dirty = false;
                }
            }
            CommandSlotConfig& slotConfig = auxCmds.commandSlots[slotIndex];

            CommandConfig& selectedCommand = auxCmds.commandSlots[slotIndex].banks[slotConfig.selectedIndex];
            if (slotConfig.commandListLength != selectedCommand.commandList.size()) {
                slotConfig.commandListLength = static_cast<unsigned int>(selectedCommand.commandList.size());
                evalBoard->selectAuxCommandLength(slot, 0, slotConfig.commandListLength - 1);
            }

            if (slotConfig.dirty) {
                evalBoard->selectAuxCommandBankAllPorts(slot, slotConfig.selectedIndex);
                slotConfig.dirty = false;
            }
        }
    }
}

/** \brief Change RHD2000 interface board amplifier sample rate.

    This function updates both the in-memory variables (BoardControl::boardSampleRate and BoardControl::sampleRateEnum)
    and the on-board values.

    Additionally, several of the other configuration options depend on the sampling rate; those
    are updated both in memory and on the board too.

    @param[in] sampleRate   New sampling rate to set
*/
void BoardControl::changeSampleRate(Rhd2000EvalBoard::AmplifierSampleRate sampleRate)
{
    if (sampleRate < Rhd2000EvalBoard::SampleRate1000Hz || sampleRate > Rhd2000EvalBoard::SampleRate30000Hz) {
        sampleRate = Rhd2000EvalBoard::SampleRate1000Hz;
    }

    sampleRateEnum = sampleRate;
    boardSampleRate = Rhd2000EvalBoard::convertSampleRate(sampleRate);
    read.numUsbBlocksToRead = getNumUsbBlocksToRead(sampleRate);

    impedance.dependencyChanged();
    auxCmds.chipRegisters.defineSampleRate(boardSampleRate);

    if (okayToRunBoardCommands()) {
        evalBoard->setSampleRate(sampleRate);
    }

    // Now that we have set our sampling rate, we can set the MISO sampling delay
    // which is dependent on the sample rate.
    updateCables();

    updateAnalogOutputCommon();

    updateBandwidth();
}



/** \brief Writes on-chip filter bandwidth configuration to the board.

    The filters on the RHD2xxx chips connected to the evaluation board are configured via
    the BoardControl::bandWidth member variable. This function sends the value(s) in that variable to the board.

    See Rhd2000Config::BandWidth for more information.
*/
void BoardControl::updateBandwidth() {
    // Before generating register configuration command sequences, set amplifier
    // bandwidth paramters.
    bandWidth.setChipRegisters(auxCmds.chipRegisters);

    auxCmds.updateRegisterConfigCommandLists();
    updateCommandSlots();
}

/** \brief Calculates optimal number of USB Blocks to read for a given sampling rate

    USB communications are more efficient with larger data transfer sizes (i.e., more USB blocks per
    read), but that increases the measurement latency.  A value of 1 will have the best latency initially,
    but the USB communications may be so inefficient that the FIFO queue fills up (i.e., that the
    latency builds up over time), and eventually overflows.

    Note that these are empirical values used in the RHD2000 Interface GUI; if you are calling this from
    code with more or less processing going on, you may want to configure the number of usb blocks to read
    manually.

    @param[in] sampleRate   Sampling rate, as an enum
    @returns                Number of datablocks to get for an approximate frame rate of 30 Hz for most sampling rates.
*/
unsigned int BoardControl::getNumUsbBlocksToRead(Rhd2000EvalBoard::AmplifierSampleRate sampleRate) {
    switch (sampleRate) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    default:
        return 1;
    case 5:
    case 6:
    case 7:
        return 2;
    case 8:
    case 9:
        return 3;
    case 10:
        return 4;
    case 11:
        return 6;
    case 12:
        return 7;
    case 13:
        return 8;
    case 14:
        return 12;
    case 15:
        return 14;
    case 16:
        return 16 * 4;
    }
}

/** \brief Writes "fast settle" configuration to the board.

    The "fast settle" functionality on the RHD2xxx chips connected to the evaluation board is configured via
    the BoardControl::fastSettle member variable. This function sends the value(s) in that variable to the board.

    Note that this function only configures fast settling; it doesn't invoke fast settling.

    See Rhd2000Config::FastSettleControl for more information.
*/
void BoardControl::updateFastSettle() {
    auxCmds.commandSlots[Rhd2000EvalBoard::AuxCmd3].selectBank(fastSettle.enabled ? 2 : 1);
    updateCommandSlots();

    if (okayToRunBoardCommands()) {
        evalBoard->enableExternalFastSettle(fastSettle.external);
        if (fastSettle.external) {
            evalBoard->setExternalFastSettleChannel(fastSettle.channel);
        }
    }
}

/** \brief Instantiates a new Rhd2000EvalBoard object in the BoardControl::evalBoard member.

    Deletes (and hence closes) any pre-existing board connection.
 */
void BoardControl::create() {
    evalBoard.reset(new Rhd2000EvalBoard());
}

/** \brief Closes out the connection to the board.

    Does a safe shutdown, including turning auxiliary digital outputs off, disabling board-level digital and analog outputs, etc.
 */
void BoardControl::close() {
    // Turn off auxiliary outputs if any are on
    disableAuxDigOut();

    bool need_to_run = auxDigOutputs.values[0].enabled || auxDigOutputs.values[1].enabled || auxDigOutputs.values[2].enabled || auxDigOutputs.values[3].enabled;

    long oldDacManualValue = analogOutputs.getDacManualRaw();
    analogOutputs.setDacManualVolts(0.0);
    updateDACManual();
    long newDacManualValue = analogOutputs.getDacManualRaw();

    if (newDacManualValue != oldDacManualValue) {
        need_to_run = true;
    }

    if (need_to_run) {
        // Run and wait for completion.
        runFixed(60, nullptr);
        flush();
        stop();
    }

    // Turn off LEDs
    leds.clear();
    updateLEDs();

    // Turn off digital outputs
    memset(digitalOutputs.values, 0, sizeof(digitalOutputs.values));
    digitalOutputs.comparatorsEnabled = false;
    updateDigitalOutputs();

    // Turn off analog outputs
    for (unsigned int i = 0; i < NUM_BOARD_ANALOG_OUTPUTS; i++) {
        analogOutputs.dacs[i].enabled = false;
    }
    updateAnalogOutputSources();

    // Delete the board object
    evalBoard.reset(nullptr);
}

// Run for 60 cycles and eat the resulting datablock
void BoardControl::run60(CALLBACK_FUNCTION_IDLE callback) {
    if (okayToRunBoardCommands() && !isRunning()) {
        // Since our longest command sequence is 60 commands, we run the SPI
        // interface for 60 samples, and wait for it to complete.
        runFixed(60, callback);

        // Read the resulting single data block from the USB interface.
        // We don't need to do anything with this data block; it was used to configure
        // the RHD2000 amplifier chips and to run ADC calibration.

        Rhd2000DataBlock dataBlock(dataStreams.getNumEnabledDataStreams());
        evalBoard->readDataBlock(&dataBlock);
    }
}

/** \brief Set up initial settings on the interface board

    Sets the various settings in this object to their default values.  Sends command lists to the board (this is a slow
    operation) and runs the board for 60 cycles to set the chips' register values to those in memory.

    @param[in] sampleRate       The sampling rate for the board to use.
    @param[in] callback         Optional (i.e., can be NULL) callback function, to be called while the board
                                is running.  Can be used to keep the UI responsive, for example.
*/
void BoardControl::initializeInterfaceBoard(Rhd2000EvalBoard::AmplifierSampleRate sampleRate, CALLBACK_FUNCTION_IDLE callback)
{
    // Initialize interface board.
    evalBoard->initialize();
    dataStreams.logicalDataStreams[0].tieTo(&dataStreams.physicalDataStreams[0], false); // This matches the board; but we should scan the ports to update these before using them outside this function

    // Read 4-bit board mode.
    evalBoardMode = evalBoard->getBoardMode();

    // Set sample rate and upload all auxiliary SPI command sequences.
    changeSampleRate(sampleRate);

    // Select RAM Bank 0 for AuxCmd3 initially, so the ADC is calibrated.
    auxCmds.commandSlots[Rhd2000EvalBoard::AuxCmd3].selectBank(0);
    updateCommandSlots();

    // Set default configuration for all eight DACs on interface board.
    for (unsigned int i = 0; i < NUM_BOARD_ANALOG_OUTPUTS; i++) {
        analogOutputs.dacs[i].enabled = false;
        analogOutputs.dacs[i].dataStream = 8;   // Initially point DACs to DacManual1 input
        analogOutputs.dacs[i].channel = 0;
    }
    updateAnalogOutputSources();
    analogOutputs.dacGain = 0;
    analogOutputs.noiseSuppress = 0;
    updateAnalogOutputCommon();

    analogOutputs.setDacManualVolts(0.0);
    updateDACManual();

    run60(callback);

    // Now that ADC calibration has been performed, we switch to the command sequence
    // that does not execute ADC calibration.
    updateFastSettle();

    for (unsigned int port = 0; port < NUM_PORTS; port++) {
        cables[port] = Cable(false, 0, 0.0);  // Set to use length in meters, and set that length to 0.0 meters
    }
    updateCables();
}

/** \brief Reads datablocks from the board.

    Rhd2000Datablock objects are read into a queue in the BoardControl::read member.  The number of
    data blocks to be read is also configured there.

    See BoardControl::ReadControl for more information.

    @returns One of:
    \li Positive number - number of blocks read
    \li 0 - if no data is available, or less data than the required number of data blocks.  (This is not an error condition.)
    \li Negative number - error
        \li -1 This object isn't configured correctly
        \li -2 There isn't enough data, and the board is not currently running, so there won't ever be.
        \li -3 USB FIFO overflow
 */
int BoardControl::readBlocks() {
    if (okayToRunBoardCommands()) {
        bool readData = evalBoard->readDataBlocks(read.numUsbBlocksToRead, read.dataQueue);    // takes about 17 ms at 30 kS/s with 256 amplifiers
        if (readData) {
            // Check the number of words stored in the Opal Kelly USB interface FIFO.
            unsigned int wordsInFifo = evalBoard->numWordsInFifo();
            double samplePeriod = 1.0 / boardSampleRate;
            unsigned int dataBlockSize = Rhd2000DataBlock::calculateDataBlockSizeInWords(dataStreams.getNumEnabledDataStreams());
            read.latency = 1000.0 * Rhd2000DataBlock::getSamplesPerDataBlock() *
                (wordsInFifo / dataBlockSize) * samplePeriod;
            read.fifoPercentageFull = 100.0 * wordsInFifo / Rhd2000EvalBoard::fifoCapacityInWords();

            // If the USB interface FIFO (on the FPGA board) exceeds 99% full, halt
            // data acquisition and return an error
            if (read.fifoPercentageFull > 99.0) {
                stop();
                return -3; // Buffer overrun
            }

            return read.numUsbBlocksToRead;
        }

        if (!evalBoard->isRunning()) {
            return -2; // Board no longer running
        }

        return 0; // No error, but no data available
    }
    return -1; // No board
}

/** \brief Stops the board from running.

    Does not flush the board's FIFO.  This allows you to read the remaining data from the FIFO if you want.
 */
void BoardControl::stop() {
    if (okayToRunBoardCommands()) {
        read.continuous = false;
        evalBoard->setContinuousRunMode(false);
        evalBoard->setMaxTimeStep(0);
    }
}

/** \brief Starts the board running, and runs it in continuous mode.

    Care should be taken in this mode to handle the incoming data promptly (e.g., via the readBlocks() command).
    The board gets into a weird state if its FIFO fills up.
 */
void BoardControl::runContinuously() {
    if (okayToRunBoardCommands()) {
        read.continuous = true;
        evalBoard->setContinuousRunMode(true);
        evalBoard->run();
    }
}

/** \brief Runs the board for a fixed number of time steps.

    @param[in] numTimesteps     Number of timesteps to run for
    @param[in] callback         Optional (i.e., can be NULL) callback function, to be called while the board
                                is running.  Can be used to keep the UI responsive, for example.
*/
void BoardControl::runFixed(unsigned int numTimesteps, CALLBACK_FUNCTION_IDLE callback) {
    if (okayToRunBoardCommands()) {
        read.continuous = false;
        evalBoard->setContinuousRunMode(false);
        evalBoard->setMaxTimeStep(numTimesteps);
        evalBoard->run();

        // Wait for the run to complete.
        while (evalBoard->isRunning()) {
            if (callback != nullptr) {
                callback();
            }
        }
    }
}

/** \brief Is the board currently running?

    @returns true if it is, false otherwise.
 */
bool BoardControl::isRunning() {
    if (okayToRunBoardCommands()) {
        return evalBoard->isRunning();
    }
    return false;
}

/** \brief Flush the board's FIFO.
 */
void BoardControl::flush() {
    if (okayToRunBoardCommands()) {
        evalBoard->flush();
    }
}

/** \brief Reset the board
 */
// TODO: \copydoc Rhd2000EvalBoard::resetBoard() ??
void BoardControl::resetBoard() {
    if (okayToRunBoardCommands()) {
        evalBoard->resetBoard();
    }
}

/** \brief Begins an impedance measurement

Updates the commandslots to set the impedance-measuring register values.  Calculates index values for measurements.
*/
void BoardControl::beginImpedanceMeasurement() {
    impedance.calculateValues();

    auxCmds.updateImpedanceRegisters();
    auxCmds.commandSlots[Rhd2000EvalBoard::AuxCmd1].selectBank(1); // Impedance waveform
    auxCmds.commandSlots[Rhd2000EvalBoard::AuxCmd3].selectBank(3); // ZCheck enabled
    updateCommandSlots();
}

/** \brief Begins plating

    Updates the commandslots to set the plating register values.  
    
    Note that when you're done plating, you should call endImpedanceMeasurement(), as
    there is no separate endPlating().
 */
void BoardControl::beginPlating(int channel) {
    auxCmds.chipRegisters.setZcheckChannel(channel);
    auxCmds.updateImpedanceRegisters();
    auxCmds.commandSlots[Rhd2000EvalBoard::AuxCmd1].selectBank(2); // Plating - DC
    auxCmds.commandSlots[Rhd2000EvalBoard::AuxCmd3].selectBank(3); // ZCheck enabled
    updateCommandSlots();

    // Now push the new ZCheck settings to the boards
    run60(nullptr);
}

/** \brief Ends an impedance measurement

    Updates the commandslots to set the register values to non-impedance.
 */
void BoardControl::endImpedanceMeasurement() {

    auxCmds.commandSlots[Rhd2000EvalBoard::AuxCmd1].selectBank(0); // Digital output
    auxCmds.commandSlots[Rhd2000EvalBoard::AuxCmd3].selectBank(fastSettle.enabled ? 2 : 1); // ZCheck disabled
    updateCommandSlots();

    // Now push the new ZCheck settings to the boards
    run60(nullptr);
}

/** \brief Is this object configured to run board commands?

    Checks whether create() and open() have been called.

    @returns true if so.
 */
bool BoardControl::okayToRunBoardCommands() {
    return (evalBoard.get() != nullptr) && evalBoard->isOpen();
}

/** \brief Gets the IDs of all chips attached to the board.

    Scan SPI Ports A-D to find all connected RHD2000 amplifier chips.
    Read the chip ID from on-chip ROM register 63 to determine the number
    of amplifier channels on each port.  This process is repeated at all
    possible MISO delays in the FPGA, and the cable length on each port
    is inferred from this.

    The resulting chip IDs are stored in the BoardControl::dataStreams member's 
    Rhd2000Config::DataStreamControl::physicalDataStreams member, and the optimal
    cable delays are stored in the BoardControl::cables member.

    Note that the scans are done at 30 kHz.

    @param[in] callback         Optional (i.e., can be NULL) callback function, to be called while the board
                                is running.  Can be used to keep the UI responsive, for example.
*/
void BoardControl::getChipIds(CALLBACK_FUNCTION_IDLE callback)
{
    Rhd2000EvalBoard::AmplifierSampleRate currentSampleRate = evalBoard->getSampleRateEnum();

	// Set sampling rate to highest value for maximum temporal resolution.
	// Only need to do this for the underlying board, not the UI, because we change it back at the end of this function
	changeSampleRate(Rhd2000EvalBoard::SampleRate30000Hz);

	// Enable all physical data streams, with sources to cover one or two chips
	// on Ports A-D.
    dataStreams.resetPhysicalStreams();
    enablePhysicalDataStreams();

    auxCmds.commandSlots[Rhd2000EvalBoard::AuxCmd3].selectBank(0);
    updateCommandSlots();

    Rhd2000DataBlock dataBlock(MAX_NUM_DATA_STREAMS);
	vector<int> sumGoodDelays(MAX_NUM_DATA_STREAMS, 0);
	vector<int> indexFirstGoodDelay(MAX_NUM_DATA_STREAMS, -1);
	vector<int> indexSecondGoodDelay(MAX_NUM_DATA_STREAMS, -1);

    // Reset cables to their default setting
    for (unsigned int port = 0; port < NUM_PORTS; port++) {
        cables[port] = Cable();
    }

	// Run SPI command sequence at all 16 possible FPGA MISO delay settings
	// to find optimum delay for each SPI interface cable.
    for (unsigned int delay = 0; delay < NUM_VALID_DELAYS; ++delay) {
        for (unsigned int port = 0; port < NUM_PORTS; port++) {
            cables[port].manualDelayEnabled = true;
            cables[port].manualDelay = delay;
		}
        updateCables();

        // Since our longest command sequence is 60 commands, we run the SPI
        // interface for 60 samples, and wait for it to complete.
        runFixed(60, callback);

		// Read the resulting single data block from the USB interface.
		evalBoard->readDataBlock(&dataBlock);

		// Read the Intan chip ID number from each RHD2000 chip found.
		// Record delay settings that yield good communication with the chip.
        for (unsigned int source = 0; source < MAX_NUM_BOARD_DATA_SOURCES; ++source) {
            Rhd2000Registers registersVar(1000);
            registersVar.readBack(dataBlock.auxiliaryData[source][Rhd2000EvalBoard::AuxCmd3]);

			int register59Value;
            typed_register_t::ChipId id = registersVar.registers.deviceId(register59Value);

            if (id == typed_register_t::CHIP_ID_RHD2132 || id == typed_register_t::CHIP_ID_RHD2216 ||
                (id == typed_register_t::CHIP_ID_RHD2164 && register59Value == typed_register_t::REGISTER_59_MISO_A)) {
				// cout << "Delay: " << delay << " on source " << source << " is good." << endl;
				sumGoodDelays[source] = sumGoodDelays[source] + 1;
				if (indexFirstGoodDelay[source] == -1) {
					indexFirstGoodDelay[source] = delay;
                    dataStreams.physicalDataStreams[source].chipId = id;
				}
				else if (indexSecondGoodDelay[source] == -1) {
					indexSecondGoodDelay[source] = delay;
                    dataStreams.physicalDataStreams[source].chipId = id;
				}
			}
		}
	}

    dataStreams.physicalValid = true;

    // Set cable delay settings that yield good communication with each
	// RHD2000 chip.
	vector<int> optimumDelay(MAX_NUM_DATA_STREAMS, 0);
    for (unsigned int stream = 0; stream < MAX_NUM_DATA_STREAMS; ++stream) {
		if (sumGoodDelays[stream] == 1 || sumGoodDelays[stream] == 2) {
			optimumDelay[stream] = indexFirstGoodDelay[stream];
		}
		else if (sumGoodDelays[stream] > 2) {
			optimumDelay[stream] = indexSecondGoodDelay[stream];
		}
	}

    for (unsigned int port = 0; port < NUM_PORTS; port++) {
		//        cout << "Port " << static_cast<char>('A' + port) << " cable delay: " << qMax(optimumDelay[2*port], optimumDelay[2*port + 1]) << endl;
        cables[port].manualDelay = std::max(optimumDelay[2 * port], optimumDelay[2 * port + 1]);
	}
    updateCables(); // Set the cable delays based on the calculated manual delays (manualDelayEnabled is still true from above)
    for (unsigned int port = 0; port < NUM_PORTS; port++) {
        cables[port].lengthMeters = evalBoard->estimateCableLengthMeters(cables[port].manualDelay);
        cables[port].manualDelayEnabled = false;
    }

	// Return sample rate to original user-selected value.
	changeSampleRate(currentSampleRate);
    updateFastSettle();

    // And restore original (logical) streams
    updateDataStreams();
}

// --------------------------------------------------------------------------------------------
// Save format functions
// --------------------------------------------------------------------------------------------

// Returns the total number of temperature sensors connected to the interface board.
// Only returns valid value after createSaveList() has been called.
int BoardControl::getNumTempSensors() const
{
    return static_cast<unsigned int>(saveList->tempSensor.size());
}

void BoardControl::createOrUpdateAmplifierChannels(Rhd2000Config::DataStreamConfig* datastreamConfig, bool create, int port, int& channel) {
    if (datastreamConfig != nullptr) {
        int numChannels = datastreamConfig->getNumChannels();
        for (int i = 0; i < numChannels; ++i) {
            if (create) {  // Create new ones.
                signalSources.signalPort[port].addAmplifierChannel(channel, i, datastreamConfig->index);
            } else {
                // Update source indices for amplifier channels.
                signalSources.signalPort[port].channel[channel].boardStream = datastreamConfig->index;
            }
            channel++;
        }
    }
}

void BoardControl::setSignalSources() {
    int numChannelsOnPort[4] = { 0, 0, 0, 0 };

    for (unsigned int source = 0; source < MAX_NUM_BOARD_DATA_SOURCES; ++source) {
        const DataSourceControl& datasource = dataStreams.physicalDataStreams[source];
        if (datasource.firstDataStream != nullptr) { // Data source is being used
            numChannelsOnPort[datasource.getPort()] += datasource.getNumChannels();
        }
    }

    // Add channel descriptions to the SignalSources object to create a list of all waveforms.
    for (unsigned int port = 0; port < NUM_PORTS; ++port) {
        if (numChannelsOnPort[port] == 0) {
            signalSources.signalPort[port].channel.clear();
            signalSources.signalPort[port].enabled = false;
        } else {
            bool numChannelsChanged = signalSources.signalPort[port].numAmplifierChannels() != numChannelsOnPort[port];

            if (numChannelsChanged) {  // if number of channels on port has changed...
                signalSources.signalPort[port].channel.clear();  // ...clear existing channels...
            }

            // Create or update channels for amplifier channels
            int channel = 0;
            for (int miso = 0; miso < 1; ++miso) {
                const DataSourceControl& datasource = dataStreams.physicalDataStreams[2 * port + miso];
                createOrUpdateAmplifierChannels(datasource.firstDataStream, numChannelsChanged, port, channel);
                createOrUpdateAmplifierChannels(datasource.ddrDataStream, numChannelsChanged, port, channel);
            }

            int auxName = 1;
            int vddName = 1;
            for (int miso = 0; miso < 1; ++miso) {
                const DataSourceControl& datasource = dataStreams.physicalDataStreams[2 * port + miso];
                if (datasource.firstDataStream != nullptr) {
                    int stream = datasource.firstDataStream->index;
                    if (numChannelsChanged) {  // if number of channels on port has changed...
                        // Now create auxiliary input channels and supply voltage channels for each chip.
                        signalSources.signalPort[port].addAuxInputChannel(channel++, 0, auxName++, stream);
                        signalSources.signalPort[port].addAuxInputChannel(channel++, 1, auxName++, stream);
                        signalSources.signalPort[port].addAuxInputChannel(channel++, 2, auxName++, stream);
                        signalSources.signalPort[port].addSupplyVoltageChannel(channel++, 0, vddName++, stream);
                    } else {
                        // Update stream indices for auxiliary channels and supply voltage channels.
                        signalSources.signalPort[port].channel[channel++].boardStream = stream;
                        signalSources.signalPort[port].channel[channel++].boardStream = stream;
                        signalSources.signalPort[port].channel[channel++].boardStream = stream;
                        signalSources.signalPort[port].channel[channel++].boardStream = stream;
                    }
                }
            }
        }
    }

    for (unsigned int port = 0; port < NUM_PORTS; port++) {
        SignalGroup& signalGroup = signalSources.signalPort[port];
        signalGroup.enabled = (signalGroup.numAmplifierChannels() != 0);
    }
}

void BoardControl::setSaveFormat(SaveFormat format)
{
    switch (format) {
    case SaveFormatIntan:
        writer.reset(new IntanSaveFormat());
        break;
    case SaveFormatFilePerSignalType:
        writer.reset(new FilePerSignalFormat());
        break;
    case SaveFormatFilePerChannel:
        writer.reset(new FilePerChannelFormat(signalSources));
        break;
    default:
        writer.reset();
        break;
    }
}
