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

#ifndef SIGNALCHANNEL_H
#define SIGNALCHANNEL_H

#include <memory>
#include <string>

class SignalGroup;
class BinaryWriter;
class BinaryReader;

enum SignalType {
    AmplifierSignal,
    AuxInputSignal,
    SupplyVoltageSignal,
    BoardAdcSignal,
    BoardDigInSignal,
    BoardDigOutSignal
};

class SignalChannel
{
    friend BinaryWriter &operator<<(BinaryWriter &outStream, const SignalChannel &a);
    friend BinaryReader &operator>>(BinaryReader &inStream, SignalChannel &a);

public:
    SignalChannel();
    SignalChannel(SignalGroup *initSignalGroup);
    SignalChannel(const std::wstring &initCustomChannelName, const std::wstring &initNativeChannelName,
                  int initNativeChannelNumber, SignalType initSignalType, int initBoardChannel,
                  int initBoardStream, SignalGroup *initSignalGroup);
    ~SignalChannel();

    SignalGroup *signalGroup;

    std::wstring nativeChannelName;
    std::wstring customChannelName;
    int nativeChannelNumber;
    int alphaOrder;
    int userOrder;

    SignalType signalType;
    bool enabled;

    int chipChannel;
    int boardStream;

    bool voltageTriggerMode;
    int voltageThreshold;
    int digitalTriggerChannel;
    bool digitalEdgePolarity;

    double electrodeImpedanceMagnitude;
    double electrodeImpedancePhase;

    std::shared_ptr<BinaryWriter> saveFile;

private:
};

#endif // SIGNALCHANNEL_H
