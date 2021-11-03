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

#include "impedancemeasurecontroller.h"
#include "signalsources.h"
#include "signalchannel.h"
#include "rhd2000registers.h"
#include <QtCore>
#include <iostream>

using Rhd2000RegisterInternals::typed_register_t;
using std::vector;
using std::deque;
using std::complex;
using std::unique_ptr;

#define RADIANS_TO_DEGREES  57.2957795132

//  ------------------------------------------------------------------------

ImpedanceMeasureController::ImpedanceMeasureController(BoardControl& bc, ProgressWrapper& progressWrapper_, BoardControl::CALLBACK_FUNCTION_IDLE callback_, bool continuation) :
    boardControl(bc),
    progress(progressWrapper_),
    callback(callback_)
{
    if (!continuation) {
        boardControl.leds.startProgressCounter();
    }
    rhd2164ChipPresent = boardControl.dataStreams.containsChip(typed_register_t::CHIP_ID_RHD2164);

    const unsigned int numCaps = 3;
    const unsigned int numChannels = rhd2164ChipPresent ? 64 : 32;
}

void ImpedanceMeasureController::advanceLEDs() {
    // Advance LED display
    boardControl.leds.incProgressCounter();
    boardControl.updateLEDs();
}

bool ImpedanceMeasureController::measureAmplitudesForAllCapacitances(const std::vector<unsigned int>& channels, std::vector<std::vector<std::vector<std::complex<double> > > >& measuredAmplitudes)
{
    unsigned int old_numBlocks = boardControl.read.numUsbBlocksToRead;
    boardControl.read.numUsbBlocksToRead = boardControl.impedance.numBlocks;

    vector<double> amplifierData(SAMPLES_PER_DATA_BLOCK * boardControl.impedance.numBlocks, 0);

    // We execute three complete electrode impedance measurements: one each with
    // Cseries set to 0.1 pF, 1 pF, and 10 pF.  Then we select the best measurement
    // for each channel so that we achieve a wide impedance measurement range.
    for (int capRange = 0; capRange < 3; ++capRange) {

        boardControl.auxCmds.chipRegisters.setZcheckScale(static_cast<Rhd2000Registers::ZcheckCs>(capRange));

        // Check all channels across all active data streams.
        for (unsigned int i = 0; i < channels.size(); ++i) {
            unsigned int channel = channels[i];
            unsigned int channelIndex = channel % 32;

            //progress.setValue(progress.value() + 1);
            if (progress.wasCanceled()) {
                return false;
            }

            boardControl.auxCmds.chipRegisters.setZcheckChannel(channel);
            boardControl.auxCmds.updateImpedanceRegisters();
            boardControl.updateCommandSlots();

            boardControl.runFixed(SAMPLES_PER_DATA_BLOCK * boardControl.impedance.numBlocks, callback);

            boardControl.readBlocks();

            for (unsigned int source = 0; source < MAX_NUM_BOARD_DATA_SOURCES; ++source) {
                Rhd2000Config::DataSourceControl& dsource = boardControl.dataStreams.physicalDataStreams[source];
                Rhd2000Config::DataStreamConfig* ds = dsource.getStreamForChannel(channel);
                if (ds != nullptr) {
                    int stream = ds->index;

                    getAmplifierData(boardControl.read.dataQueue, stream, channelIndex, amplifierData);

                    // Measure complex amplitude of frequency component.
                    complex<double> z = boardControl.impedance.amplitudeOfFreqComponent(amplifierData.data());
                    measuredAmplitudes[source][channel][capRange] = z;
                }
            }

            boardControl.read.emptyQueue();

            advanceLEDs();
        }
    }

    boardControl.read.numUsbBlocksToRead = old_numBlocks;

    return true;
}

void ImpedanceMeasureController::findBestImpedances(vector<vector<vector<std::complex<double> > > >& measuredAmplitudes, vector<vector<complex<double> > >& bestZ) {
    bestZ.resize(measuredAmplitudes.size());
    for (unsigned int source = 0; source < MAX_NUM_BOARD_DATA_SOURCES; ++source) {
        unsigned int numChannels = static_cast<unsigned int>(measuredAmplitudes[source].size());
        if (numChannels > 0) {
            bestZ[source].resize(numChannels, 0);

            for (unsigned int channel = 0; channel < numChannels; ++channel) {
                bestZ[source][channel] = boardControl.impedance.calculateBestImpedanceOneAmplifier(measuredAmplitudes[source][channel]);
            }
        }
    }
}

void ImpedanceMeasureController::storeBestImpedances(std::vector<std::vector<std::complex<double> > >& bestZ) {
    for (unsigned int source = 0; source < MAX_NUM_BOARD_DATA_SOURCES; ++source) {
        Rhd2000Config::DataSourceControl& dsource = boardControl.dataStreams.physicalDataStreams[source];

        for (int channelInDatasource = 0; channelInDatasource < 64; ++channelInDatasource) {
            int channel = channelInDatasource % 32;
            Rhd2000Config::DataStreamConfig* ds = dsource.getStreamForChannel(channelInDatasource);
            if (ds != nullptr) {
                int stream = ds->index;
                SignalChannel *signalChannel = boardControl.signalSources.findAmplifierChannel(stream, channel);
                if (signalChannel) {
                    signalChannel->electrodeImpedanceMagnitude = std::abs(bestZ[source][channelInDatasource]);
                    signalChannel->electrodeImpedancePhase = RADIANS_TO_DEGREES * std::arg(bestZ[source][channelInDatasource]);
                }
            }
        }
    }
}

// All the board-related work for impedance measurement.
// Sets up the board, runs the measurement (which results in amplitudes only) for all specified channels, and restores the board to pre-measurement state
// Doesn't convert the measurements to impedances
bool ImpedanceMeasureController::setupAndMeasureAmplitudes(const std::vector<unsigned int>& channels, std::vector<std::vector<std::vector<std::complex<double> > > >& measuredAmplitudes) {
    // Disable external fast settling, since this interferes with DAC commands in AuxCmd1.
    bool externalFastSettle = boardControl.fastSettle.external;
    boardControl.fastSettle.external = false;
    boardControl.updateFastSettle();

    // Disable auxiliary digital output control during impedance measurements.
    boardControl.disableAuxDigOut();

    boardControl.digitalOutputs.values[15] = 1;
    boardControl.updateDigitalOutputs();

    // Turn LEDs on to indicate that data acquisition is running.
    //boardControl.leds.values[4] = 1;
    //boardControl.updateLEDs();

    // Now do the actual measurements
    boardControl.auxCmds.createImpedanceDACsCommand(boardControl.boardSampleRate, boardControl.impedance.actualImpedanceFreq);

    boardControl.impedance.changeImpedanceValues(1000);

    boardControl.updateCommandSlots();

    boardControl.beginImpedanceMeasurement();

    bool good = measureAmplitudesForAllCapacitances(channels, measuredAmplitudes);

    // Switch back to flatline
    boardControl.endImpedanceMeasurement();

    boardControl.stop();

    boardControl.flush();

    boardControl.digitalOutputs.values[15] = 0;
    boardControl.updateDigitalOutputs();

    // Re-enable external fast settling, if selected.
    boardControl.fastSettle.external = externalFastSettle;
    boardControl.updateFastSettle();

    // Re-enable auxiliary digital output control, if selected.
    boardControl.updateAuxDigOut();

    return good;
}

// Create matrix of complex of size (MAX_NUM_BOARD_DATA_SOURCES x number of channels x 3) to store complex amplitudes
// of all amplifier channels at three different Cseries values.
vector<vector<vector<complex<double>>>> ImpedanceMeasureController::createAmplitudeMatrix() {
    vector<vector<vector<complex<double> > > > measuredAmplitudes; // [datasource][channel][capacitance]
    measuredAmplitudes.resize(MAX_NUM_BOARD_DATA_SOURCES);
    for (unsigned int i = 0; i < MAX_NUM_BOARD_DATA_SOURCES; ++i) {
        int numChannels = boardControl.dataStreams.physicalDataStreams[i].getNumChannels();
        if (numChannels > 0) {
            measuredAmplitudes[i].resize(numChannels);
            for (int j = 0; j < numChannels; ++j) {
                measuredAmplitudes[i][j].resize(3);
            }
        }
    }
    return measuredAmplitudes;
}

// Execute an electrode impedance measurement procedure for one channels.
complex<double> ImpedanceMeasureController::measureOneImpedance(Rhd2000EvalBoard::BoardDataSource datasource, unsigned int channel) {
    vector<vector<vector<complex<double> > > > measuredAmplitudes = createAmplitudeMatrix(); // [datasource][channel][capacitance]

    // Create an array of channels to measure
    vector<unsigned int> channels;
    channels.push_back(channel);

    bool good = setupAndMeasureAmplitudes(channels, measuredAmplitudes);

    // And now, store the acquired data
    if (good) {
        return boardControl.impedance.calculateBestImpedanceOneAmplifier(measuredAmplitudes[datasource][channel]);
    }
    else {
        return complex<double>(0, 0);
    }
}

// Execute an electrode impedance measurement procedure for all channels.
bool ImpedanceMeasureController::runImpedanceMeasurementRealBoard() {
    vector<vector<vector<complex<double> > > > measuredAmplitudes = createAmplitudeMatrix(); // [datasource][channel][capacitance]

    // Create an array of channels to measure
    const unsigned int maxChannel = rhd2164ChipPresent ? 64 : 32;
    vector<unsigned int> channels(maxChannel, 0);
    for (unsigned int i = 0; i < maxChannel; ++i) {
        channels[i] = i;
    }

    bool good = setupAndMeasureAmplitudes(channels, measuredAmplitudes);

    // Turn off LEDs.
    boardControl.leds.clear();
    boardControl.updateLEDs();

    // And now, store the acquired data
    if (good) {
        vector<vector<complex<double> > > bestZ;
        findBestImpedances(measuredAmplitudes, bestZ);
        storeBestImpedances(bestZ);
    }

    return good;
}

// Reads numBlocks blocks of raw USB data stored in a queue of Rhd2000DataBlock
// objects, extracts the amplifier data in units of microvolts.
void ImpedanceMeasureController::getAmplifierData(deque<unique_ptr<Rhd2000DataBlock>> &dataQueue, int stream, int channel, vector<double>& amplifierData)
{

    for (unsigned int block = 0; block < dataQueue.size(); ++block) {
        Rhd2000DataBlock& dataBlock = *dataQueue[block];

        // Load and scale RHD2000 amplifier waveforms
        // (sampled at amplifier sampling rate)
        for (unsigned int t = 0; t < SAMPLES_PER_DATA_BLOCK; ++t) {
            // Amplifier waveform units = microvolts

            //dataBlock.print(std::cout, 0);
            amplifierData[block * SAMPLES_PER_DATA_BLOCK + t] = Rhd2000DataBlock::amplifierADCToMicroVolts(dataBlock.amplifierData[stream][channel][t]);
        }
    }
}
