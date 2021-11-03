//  ------------------------------------------------------------------------
//
//  This file is part of the Intan Technologies RHD2000 Interface
//  Version 1.3
//  Copyright (C) 2013 Intan Technologies
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

#include "signalprocessor.h"

#include <QVector>
#include <QFile>
#include <QDataStream>
#include <queue>
#include <qmath.h>
#include <iostream>

//#include "qtincludes.h"

#include "mainwindow.h"
#include "signalsources.h"
#include "signalgroup.h"
#include "signalchannel.h"
#include "rhd2000datablock.h"
#include "saveformat.h"
#include "globalconstants.h"
#include "rhd2000evalboard.h"

using std::deque;
using std::queue;
using std::unique_ptr;
using std::vector;

// The maximum number of Rhd2000DataBlock objects we will need is set by the need
// to perform electrode impedance measurements at very low frequencies.
const int maxNumBlocks = 120;

//-------------------------------------------------------------------------------

// This class stores and processes short segments of waveform data
// acquired from the USB interface board.  The primary purpose of the
// class is to read from a queue of Rhd2000DataBlock objects and scale
// this raw data appropriately to generate wavefrom vectors with units
// of volts or microvolts.
//
// The class is also capable of applying a notch filter to amplifier
// waveform data, measuring the amplitude of a particular frequency
// component (useful in the electrode impedance measurements), and
// generating synthetic neural or ECG data for demonstration purposes.

// Constructor.
SignalProcessor::SignalProcessor() 
{
    // Notch filter initial parameters.
    notchFilterEnabled = false;
    a1 = 0.0;
    a2 = 0.0;
    b0 = 0.0;
    b1 = 0.0;
    b2 = 0.0;

    // Highpass filter initial parameters.
    highpassFilterEnabled = false;
    aHpf = 0.0;
    bHpf = 0.0;
}

// Allocate memory to store waveform data.
void SignalProcessor::allocateMemory(int numStreams)
{
    numDataStreams = numStreams;

    // Allocate vector memory for waveforms from USB interface board and notch filter.
    allocateDoubleArray3D(amplifierPreFilter, numStreams, 32, SAMPLES_PER_DATA_BLOCK * maxNumBlocks);
    allocateDoubleArray3D(amplifierPostFilter, numStreams, 32, SAMPLES_PER_DATA_BLOCK * maxNumBlocks);
    allocateDoubleArray2D(highpassFilterState, numStreams, 32);
    allocateDoubleArray3D(prevAmplifierPreFilter, numStreams, 32, 2);
    allocateDoubleArray3D(prevAmplifierPostFilter, numStreams, 32, 2);
    allocateDoubleArray3D(auxChannel, numStreams, 3, (SAMPLES_PER_DATA_BLOCK / 4) * maxNumBlocks);
    allocateDoubleArray2D(supplyVoltage, numStreams, maxNumBlocks);
    allocateDoubleArray2D(boardAdc, 8, SAMPLES_PER_DATA_BLOCK * maxNumBlocks);
    allocateIntArray2D(boardDigIn, 16, SAMPLES_PER_DATA_BLOCK * maxNumBlocks);
    allocateIntArray2D(boardDigOut, 16, SAMPLES_PER_DATA_BLOCK * maxNumBlocks);

    // Initialize vector memory used in notch filter state.
    fillZerosDoubleArray3D(amplifierPostFilter);
    fillZerosDoubleArray3D(prevAmplifierPreFilter);
    fillZerosDoubleArray3D(prevAmplifierPostFilter);
    fillZerosDoubleArray2D(highpassFilterState);

    temperature.allocateMemory(numStreams);
}

// Allocates memory for a 3-D array of doubles.
void SignalProcessor::allocateDoubleArray3D(QVector<QVector<QVector<double> > > &array3D,
                                            int xSize, int ySize, int zSize)
{
    int i, j;

    if (xSize == 0) return;
    array3D.resize(xSize);
    for (i = 0; i < xSize; ++i) {
        array3D[i].resize(ySize);
        for (j = 0; j < ySize; ++j) {
            array3D[i][j].resize(zSize);
        }
    }
}

// Allocates memory for a 2-D array of doubles.
void SignalProcessor::allocateDoubleArray2D(QVector<QVector<double> > &array2D,
                                            int xSize, int ySize)
{
    int i;

    if (xSize == 0) return;
    array2D.resize(xSize);
    for (i = 0; i < xSize; ++i) {
        array2D[i].resize(ySize);
    }
}

// Allocates memory for a 2-D array of integers.
void SignalProcessor::allocateIntArray2D(QVector<QVector<int> > &array2D,
                                         int xSize, int ySize)
{
    int i;

    array2D.resize(xSize);
    for (i = 0; i < xSize; ++i) {
        array2D[i].resize(ySize);
    }
}

// Allocates memory for a 1-D array of doubles.
void SignalProcessor::allocateDoubleArray1D(QVector<double> &array1D,
                                            int xSize)
{
    array1D.resize(xSize);
}

// Fill a 3-D array of doubles with zero.
void SignalProcessor::fillZerosDoubleArray3D(
        QVector<QVector<QVector<double> > > &array3D)
{
    int x, y;
    int xSize = array3D.size();

    if (xSize == 0) return;

    int ySize = array3D[0].size();

    for (x = 0; x < xSize; ++x) {
        for (y = 0; y < ySize; ++y) {
            array3D[x][y].fill(0.0);
        }
    }
}

// Fill a 2-D array of doubles with zero.
void SignalProcessor::fillZerosDoubleArray2D(
        QVector<QVector<double> > &array2D)
{
    int x;
    int xSize = array2D.size();

    if (xSize == 0) return;

    for (x = 0; x < xSize; ++x) {
        array2D[x].fill(0.0);
    }
}

// Reads numBlocks blocks of raw USB data stored in a queue of Rhd2000DataBlock
// objects, loads this data into this SignalProcessor object, scaling the raw
// data to generate waveforms with units of volts or microvolts.
void SignalProcessor::loadAmplifierData(deque<unique_ptr<Rhd2000DataBlock>> &dataQueue)
{
    int numBlocks = dataQueue.size();

    int indexAmp = 0;
    int indexAux = 0;
    int indexSupply = 0;
    int indexAdc = 0;
    int indexDig = 0;

    for (int block = 0; block < numBlocks; ++block) {
        Rhd2000DataBlock& dataBlock = *dataQueue[block];

        int t, channel, stream;

        // Load and scale RHD2000 amplifier waveforms
        // (sampled at amplifier sampling rate)
        for (t = 0; t < SAMPLES_PER_DATA_BLOCK; ++t) {
            for (channel = 0; channel < 32; ++channel) {
                for (stream = 0; stream < numDataStreams; ++stream) {
                    // Amplifier waveform units = microvolts
                    amplifierPreFilter[stream][channel][indexAmp] = 
                        Rhd2000DataBlock::amplifierADCToMicroVolts(dataBlock.amplifierData[stream][channel][t]);
                }
            }
            ++indexAmp;
        }

        // Load and scale RHD2000 auxiliary input waveforms
        // (sampled at 1/4 amplifier sampling rate)
        for (t = 0; t < SAMPLES_PER_DATA_BLOCK; t += 4) {
            for (stream = 0; stream < numDataStreams; ++stream) {
                // Auxiliary input waveform units = volts
                auxChannel[stream][0][indexAux] =
                    Rhd2000DataBlock::auxADCToVolts(dataBlock.auxiliaryData[stream][Rhd2000EvalBoard::AuxCmd2][t + 1]);
                auxChannel[stream][1][indexAux] =
                    Rhd2000DataBlock::auxADCToVolts(dataBlock.auxiliaryData[stream][Rhd2000EvalBoard::AuxCmd2][t + 2]);
                auxChannel[stream][2][indexAux] =
                    Rhd2000DataBlock::auxADCToVolts(dataBlock.auxiliaryData[stream][Rhd2000EvalBoard::AuxCmd2][t + 3]);
            }
            ++indexAux;
        }

        // Load and scale RHD2000 supply voltage and temperature sensor waveforms
        // (sampled at 1/60 amplifier sampling rate)
        temperature.calculateTemps(dataBlock);
        for (stream = 0; stream < numDataStreams; ++stream) {
            // Supply voltage waveform units = volts
            supplyVoltage[stream][indexSupply] = dataBlock.getSupplyVoltage(stream);
        }
        ++indexSupply;

        // Load and scale USB interface board ADC waveforms
        // (sampled at amplifier sampling rate)
        for (t = 0; t < SAMPLES_PER_DATA_BLOCK; ++t) {
            for (channel = 0; channel < 8; ++channel) {
                // ADC waveform units = volts
                boardAdc[channel][indexAdc] =
                    Rhd2000DataBlock::boardADCToVolts(dataBlock.boardAdcData[channel][t]);
            }
            ++indexAdc;
        }

        // Load USB interface board digital input and output waveforms
        for (t = 0; t < SAMPLES_PER_DATA_BLOCK; ++t) {
            for (channel = 0; channel < 16; ++channel) {
                boardDigIn[channel][indexDig] =
                    (dataBlock.ttlIn[t] & (1 << channel)) != 0;
                boardDigOut[channel][indexDig] =
                    (dataBlock.ttlOut[t] & (1 << channel)) != 0;
            }
            ++indexDig;
        }
    }
}

// Looks for a trigger on digital input or boardADC triggerChannel with triggerPolarity.  
//
// If trigger is found, triggerTimeIndex returns timestamp of trigger point.  
// Otherwise, triggerTimeIndex returns -1, indicating no trigger was found.
//
// Note: you need to call LoadAmplifier data before calling this function
int SignalProcessor::findTrigger(int triggerChannel, int triggerPolarity)
{
    const double AnalogTriggerThreshold = 1.65;

    int maxIter;
    if (triggerChannel >= 16) {
        maxIter = boardAdc[triggerChannel - 16].size();
    } else {
        maxIter = boardDigIn[triggerChannel].size();
    }

    for (int t = 0; t < maxIter; ++t) {

        bool triggerFound;

        // Look for trigger from ADC
        if (triggerChannel >= 16) {
            qreal value = boardAdc[triggerChannel - 16][t];
            if (triggerPolarity) {
                // Trigger on logic low
                triggerFound = (value < AnalogTriggerThreshold);
            } else {
                // Trigger on logic high
                triggerFound = (value >= AnalogTriggerThreshold);
            }
        }

        // Look for trigger from Digital Inputs
        if (triggerChannel < 16) {
            int value = boardDigIn[triggerChannel][t];
            if (triggerPolarity) {
                // Trigger on logic low
                triggerFound = (value == 0);
            } else {
                // Trigger on logic high
                triggerFound = (value == 1);
            }
        }

        if (triggerFound) {
            return t;
        }
    }

    return -1;
}

// Set notch filter parameters.  All filter parameters are given in Hz (or
// in Samples/s).  A bandwidth of 10 Hz is recommended for 50 or 60 Hz notch
// filters.  Narrower bandwidths will produce extended ringing in the time
// domain in response to large transients.
void SignalProcessor::setNotchFilter(double notchFreq, double bandwidth,
                                     double sampleFreq)
{
    double d;

    d = exp(-PI * bandwidth / sampleFreq);

    // Calculate biquad IIR filter coefficients.
    a1 = -(1.0 + d * d) * cos(2.0 * PI * notchFreq / sampleFreq);
    a2 = d * d;
    b0 = (1 + d * d) / 2.0;
    b1 = a1;
    b2 = b0;
}

// Enables or disables amplifier waveform notch filter.
void SignalProcessor::setNotchFilterEnabled(bool enable)
{
    notchFilterEnabled = enable;
}

// Set highpass filter parameters.  All filter parameters are given in Hz (or
// in Samples/s).
void SignalProcessor::setHighpassFilter(double cutoffFreq, double sampleFreq)
{
    aHpf = exp(-1.0 * TWO_PI * cutoffFreq / sampleFreq);
    bHpf = 1.0 - aHpf;
}

// Enables or disables amplifier waveform highpass filter.
void SignalProcessor::setHighpassFilterEnabled(bool enable)
{
    highpassFilterEnabled = enable;
}

// Runs notch filter only on amplifier channels that are visible
// on the display (according to channelVisible).
void SignalProcessor::filterData(int numBlocks,
                                 const QVector<QVector<bool> > &channelVisible)
{
    int t, channel, stream;
    int length = SAMPLES_PER_DATA_BLOCK * numBlocks;

    // Note: Throughout this function, and elsewhere in this source code, we access
    // multidimensional 'QVector' containers using the at() function, so instead of
    // writing:
    //            x = my3dQVector[i][j][k];
    // we write:
    //            x = my3dQVector.at(i).at(j).at(k);
    //
    // Web research suggests that using at() instead of [] to access the contents
    // of a multidimensional QVector results in faster code, and some preliminary
    // experiments at Intan bear this out.  Web research also suggests that the
    // opposite could be true of C++ STL (non-Qt) 'vector' containers, so some
    // experimentation may be needed to optimize the runtime performance of code.

    if (notchFilterEnabled) {
        for (stream = 0; stream < numDataStreams; ++stream) {
            for (channel = 0; channel < 32; ++channel) {
                if (channelVisible.at(stream).at(channel)) {
                    // Execute biquad IIR notch filter.  The filter "looks backwards" two timesteps,
                    // so we must use the prevAmplifierPreFilter and prevAmplifierPostFilter
                    // variables to store the last two samples from the previous block of
                    // waveform data so that the filter works smoothly across the "seams".
                    amplifierPostFilter[stream][channel][0] =
                            b2 * prevAmplifierPreFilter.at(stream).at(channel).at(0) +
                            b1 * prevAmplifierPreFilter.at(stream).at(channel).at(1) +
                            b0 * amplifierPreFilter.at(stream).at(channel).at(0) -
                            a2 * prevAmplifierPostFilter.at(stream).at(channel).at(0) -
                            a1 * prevAmplifierPostFilter.at(stream).at(channel).at(1);
                    amplifierPostFilter[stream][channel][1] =
                            b2 * prevAmplifierPreFilter.at(stream).at(channel).at(1) +
                            b1 * amplifierPreFilter.at(stream).at(channel).at(0) +
                            b0 * amplifierPreFilter.at(stream).at(channel).at(1) -
                            a2 * prevAmplifierPostFilter.at(stream).at(channel).at(1) -
                            a1 * amplifierPostFilter.at(stream).at(channel).at(0);
                    for (t = 2; t < length; ++t) {
                        amplifierPostFilter[stream][channel][t] =
                                b2 * amplifierPreFilter.at(stream).at(channel).at(t - 2) +
                                b1 * amplifierPreFilter.at(stream).at(channel).at(t - 1) +
                                b0 * amplifierPreFilter.at(stream).at(channel).at(t) -
                                a2 * amplifierPostFilter.at(stream).at(channel).at(t - 2) -
                                a1 * amplifierPostFilter.at(stream).at(channel).at(t - 1);
                    }
                }
            }
        }
    } else {
        // If the notch filter is disabled, simply copy the data without filtering.
        for (stream = 0; stream < numDataStreams; ++stream) {
            for (channel = 0; channel < 32; ++channel) {
                for (t = 0; t < length; ++t) {
                    amplifierPostFilter[stream][channel][t] =
                            amplifierPreFilter.at(stream).at(channel).at(t);
                }
            }
        }
    }

    // Save the last two data points from each waveform to use in successive IIR filter
    // calculations.
    for (stream = 0; stream < numDataStreams; ++stream) {
        for (channel = 0; channel < 32; ++channel) {
            prevAmplifierPreFilter[stream][channel][0] =
                    amplifierPreFilter.at(stream).at(channel).at(length - 2);
            prevAmplifierPreFilter[stream][channel][1] =
                    amplifierPreFilter.at(stream).at(channel).at(length - 1);
            prevAmplifierPostFilter[stream][channel][0] =
                    amplifierPostFilter.at(stream).at(channel).at(length - 2);
            prevAmplifierPostFilter[stream][channel][1] =
                    amplifierPostFilter.at(stream).at(channel).at(length - 1);
        }
    }

    // Apply first-order high-pass filter, if selected
    if (highpassFilterEnabled) {
        double temp;
        for (stream = 0; stream < numDataStreams; ++stream) {
            for (channel = 0; channel < 32; ++channel) {
                if (channelVisible.at(stream).at(channel)) {
                    for (t = 0; t < length; ++t) {
                        temp = amplifierPostFilter[stream][channel][t];
                        amplifierPostFilter[stream][channel][t] -= highpassFilterState[stream][channel];
                        highpassFilterState[stream][channel] =
                                aHpf * highpassFilterState[stream][channel] + bHpf * temp;
                    }
                }
            }
        }
    }

}

// --------------------------------------------------------------------------------------------------
void TemperatureStorage::allocateMemory(int numStreams) {
    // Initialize vector for averaging temperature readings over time.
    vector<double> zeros(maxNumBlocks, 0);
    tempRawHistory.resize(numStreams, zeros);

    tempRaw.resize(numStreams);
    tempAvg.resize(numStreams);
    tempHistoryReset(4, numStreams);
}

void TemperatureStorage::calculateTemps(Rhd2000DataBlock &dataBlock)
{
    int numDataStreams = dataBlock.amplifierData.size();

    // Load and scale RHD2000 supply voltage and temperature sensor waveforms
    // (sampled at 1/60 amplifier sampling rate)

    for (int stream = 0; stream < numDataStreams; ++stream) {
        // Temperature sensor waveform units = degrees C
        tempRaw[stream] = dataBlock.getTemperature(stream);
    }

    // Average multiple temperature readings to improve accuracy
    tempHistoryPush(numDataStreams);
    tempHistoryCalcAvg(numDataStreams);
}

// Reset the vector and variables used to calculate running average
// of temperature sensor readings.
void TemperatureStorage::tempHistoryReset(unsigned int requestedLength, int numDataStreams)
{
    if (numDataStreams == 0) return;

    // Clear data in raw temperature sensor history vectors.
    for (int stream = 0; stream < numDataStreams; ++stream) {
        tempRawHistory[stream].assign(tempRawHistory[stream].size(), 0.0);
    }
    tempHistoryLength = 0;

    // Set number of samples used to average temperature sensor readings.
    // This number must be at least four, and must be an integer multiple of
    // four.  (See RHD2000 datasheet for details on temperature sensor operation.)

    tempHistoryMaxLength = requestedLength;

    if (tempHistoryMaxLength < 4) {
        tempHistoryMaxLength = 4;
    } else if (tempHistoryMaxLength > tempRawHistory[0].size()) {
        tempHistoryMaxLength = tempRawHistory[0].size();
    }

    int multipleOfFour = 4 * floor(((double)tempHistoryMaxLength) / 4.0);

    tempHistoryMaxLength = multipleOfFour;
}

// Push raw temperature sensor readings into the queue-like vector that
// stores the last tempHistoryLength readings.
void TemperatureStorage::tempHistoryPush(int numDataStreams)
{
    int stream, i;

    for (stream = 0; stream < numDataStreams; ++stream) {
        for (i = tempHistoryLength; i > 0; --i) {
            tempRawHistory[stream][i] = tempRawHistory[stream][i - 1];
        }
        tempRawHistory[stream][0] = tempRaw[stream];
    }
    if (tempHistoryLength < tempHistoryMaxLength) {
        ++tempHistoryLength;
    }
}

// Calculate running average of temperature from stored raw sensor readings.
// Results are stored in the tempAvg vector.
void TemperatureStorage::tempHistoryCalcAvg(int numDataStreams)
{
    for (int stream = 0; stream <  numDataStreams; ++stream) {
        tempAvg[stream] = 0.0;
        for (unsigned int i = 0; i < tempHistoryLength; ++i) {
            tempAvg[stream] += tempRawHistory[stream][i];
        }
        if (tempHistoryLength > 0) {
            tempAvg[stream] /= tempHistoryLength;
        }
    }
}

