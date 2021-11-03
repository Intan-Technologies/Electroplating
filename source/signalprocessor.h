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

#ifndef SIGNALPROCESSOR_H
#define SIGNALPROCESSOR_H

#include "mainwindow.h"

#include <queue>
#include <memory>

class SignalSources;
class Rhd2000DataBlock;
struct SaveList;

class TemperatureStorage {
public:
    std::vector<double> tempAvg;

    void allocateMemory(int numStreams);
    void calculateTemps(Rhd2000DataBlock& dataBlock);
    void tempHistoryReset(unsigned int requestedLength, int numDataStreams);

private:
    std::vector<double> tempRaw;
    std::vector<std::vector<double>> tempRawHistory;

    unsigned int tempHistoryLength;
    unsigned int tempHistoryMaxLength;

    void tempHistoryPush(int numDataStreams);
    void tempHistoryCalcAvg(int numDataStreams);
};

class SignalProcessor
{
public:
    SignalProcessor();

    void allocateMemory(int numStreams);
    void setNotchFilter(double notchFreq, double bandwidth, double sampleFreq);
    void setNotchFilterEnabled(bool enable);
    void setHighpassFilter(double cutoffFreq, double sampleFreq);
    void setHighpassFilterEnabled(bool enable);
    void loadAmplifierData(std::deque<std::unique_ptr<Rhd2000DataBlock>> &dataQueue);
    int findTrigger(int triggerChannel, int triggerPolarity);
    void filterData(int numBlocks, const QVector<QVector<bool> > &channelVisible);

    TemperatureStorage temperature;

    QVector<QVector<QVector<double> > > amplifierPreFilter;
    QVector<QVector<QVector<double> > > amplifierPostFilter;
    QVector<QVector<QVector<double> > > auxChannel;
    QVector<QVector<double> > supplyVoltage;
    QVector<QVector<double> > boardAdc;
    QVector<QVector<int> > boardDigIn;
    QVector<QVector<int> > boardDigOut;

private:
    QVector<QVector<QVector<double> > > prevAmplifierPreFilter;
    QVector<QVector<QVector<double> > > prevAmplifierPostFilter;
    QVector<QVector<double> > highpassFilterState;

    int numDataStreams;
    double a1;
    double a2;
    double b0;
    double b1;
    double b2;
    bool notchFilterEnabled;
    double aHpf;
    double bHpf;
    bool highpassFilterEnabled;

    void allocateDoubleArray3D(QVector<QVector<QVector<double> > > &array3D,
                               int xSize, int ySize, int zSize);
    void allocateDoubleArray2D(QVector<QVector<double> > &array2D,
                               int xSize, int ySize);
    void allocateIntArray2D(QVector<QVector<int> > &array2D,
                            int xSize, int ySize);
    void allocateDoubleArray1D(QVector<double> &array1D, int xSize);
    void fillZerosDoubleArray3D(QVector<QVector<QVector<double> > > &array3D);
    void fillZerosDoubleArray2D(QVector<QVector<double> > &array2D);
};

#endif // SIGNALPROCESSOR_H
