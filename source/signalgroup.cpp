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

#include "signalgroup.h"
#include <iostream>
#include "signalchannel.h"
#include "streams.h"
#include <stdio.h>
#include <wchar.h>
#include <algorithm>

using std::cerr;
using std::endl;
using std::cout;
using std::vector;
using std::wstring;

#ifdef _WIN32
#pragma warning(disable: 4996) // On Windows, disable the warning about sprintf being bad.
#endif

// Data structure containing a description of all signal channels on a particular
// signal port (e.g., SPI Port A, or Interface Board Digital Inputs).

// Must have a default constructor to create vectors of this object.
SignalGroup::SignalGroup()
{
}

// Constructor.
SignalGroup::SignalGroup(const wstring &initialName, const wstring &initialPrefix)
{
    name = initialName;
    prefix = initialPrefix;
    enabled = true;
}

// Add new amplifier channel to this signal group.
void SignalGroup::addAmplifierChannel()
{
    SignalChannel newChannel(this);
    channel.push_back(newChannel);
}

// Add a channel for one of the RHD2000-chip channels
void SignalGroup::addChipChannel(const std::wstring& name, int nativeChannelNumber, int chipChannel, int boardStream, SignalType type) {
    SignalChannel newChannel(name, name,
                             nativeChannelNumber, type,
                             chipChannel, boardStream, this);
    channel.push_back(newChannel);
    updateAlphabeticalOrder();
}

// Add a channel for one of the USB interface board channels
void SignalGroup::addBoardChannel(const std::wstring& name, int nativeChannelNumber, SignalType type) {
    SignalChannel newChannel(name, name,
                            nativeChannelNumber, type,
                            nativeChannelNumber, 0, this);
    channel.push_back(newChannel);
    updateAlphabeticalOrder();
}

// Add new amplifier channel (with specified properties) to this signal group.
void SignalGroup::addAmplifierChannel(int nativeChannelNumber, int chipChannel,
                                      int boardStream)
{
    wchar_t tmp[4];
    swprintf(tmp, sizeof(tmp)/sizeof(tmp[0]), L"%03d", nativeChannelNumber);

    wstring nativeChannelName = prefix + L"-" + tmp;

    addChipChannel(nativeChannelName, nativeChannelNumber, chipChannel, boardStream, AmplifierSignal);
}

// Add new auxiliary input channel to this signal group.
void SignalGroup::addAuxInputChannel(int nativeChannelNumber, int chipChannel, int nameNumber, int boardStream)
{
    wchar_t tmp[4];
    swprintf(tmp, sizeof(tmp)/sizeof(tmp[0]), L"%d", nameNumber);
    wstring nativeChannelName = prefix + L"-" + L"AUX" + tmp;

    addChipChannel(nativeChannelName, nativeChannelNumber, chipChannel, boardStream, AuxInputSignal);
}

// Add new supply voltage channel to this signal group.
void SignalGroup::addSupplyVoltageChannel(int nativeChannelNumber, int chipChannel, int nameNumber, int boardStream)
{
    wchar_t tmp[4];
    swprintf(tmp, sizeof(tmp)/sizeof(tmp[0]), L"%d", nameNumber);
    wstring nativeChannelName = prefix + L"-" + L"VDD" + tmp;

    addChipChannel(nativeChannelName, nativeChannelNumber, chipChannel, boardStream, SupplyVoltageSignal);
}

// Add new USB interface board ADC channel to this signal group.
void SignalGroup::addBoardAdcChannel(int nativeChannelNumber)
{
    wchar_t tmp[4];
    swprintf(tmp, sizeof(tmp)/sizeof(tmp[0]), L"%02d", nativeChannelNumber);

    wstring nativeChannelName = prefix + L"-" + tmp;

    addBoardChannel(nativeChannelName, nativeChannelNumber, BoardAdcSignal);
}

// Add new USB interface board digital input channel to this signal group.
void SignalGroup::addBoardDigInChannel(int nativeChannelNumber)
{
    wchar_t tmp[4];
    swprintf(tmp, sizeof(tmp)/sizeof(tmp[0]), L"%02d", nativeChannelNumber);

    wstring nativeChannelName = prefix + L"-" + tmp;

    addBoardChannel(nativeChannelName, nativeChannelNumber, BoardDigInSignal);
}

// Add new USB interface board digital output channel to this signal group.
void SignalGroup::addBoardDigOutChannel(int nativeChannelNumber)
{
    wchar_t tmp[4];
    swprintf(tmp, sizeof(tmp)/sizeof(tmp[0]), L"%02d", nativeChannelNumber);

    wstring nativeChannelName = prefix + L"-" + tmp;

    addBoardChannel(nativeChannelName, nativeChannelNumber, BoardDigOutSignal);
}

// Returns a pointer to a signal channel with a particular native order index.
SignalChannel* SignalGroup::channelByNativeOrder(int index)
{
    for (unsigned int i = 0; i < channel.size(); ++i) {
        if (channel[i].nativeChannelNumber == index) {
            return &channel[i];
        }
    }
    return 0;
}

// Returns a pointer to a signal channel with a particular alphabetical order index.
SignalChannel* SignalGroup::channelByAlphaOrder(int index)
{
    for (unsigned int i = 0; i < channel.size(); ++i) {
        if (channel[i].alphaOrder == index) {
            return &channel[i];
        }
    }
    return 0;
}

SignalChannel* SignalGroup::channelByCustomName(const wstring& name) {
    for (unsigned int i = 0; i < channel.size(); ++i) {
        if (channel[i].customChannelName == name) {
            return &channel[i];
        }
    }
    return nullptr;
}

// Returns a pointer to a signal channel with a particular user-selected order index
SignalChannel* SignalGroup::channelByIndex(int index)
{
    for (unsigned int i = 0; i < channel.size(); ++i) {
        if (channel[i].userOrder == index) {
            return &channel[i];
        }
    }
    cerr << "SignalGroup::channelByUserOrder: index " << index << " not found." << endl;
    return 0;
}

// Returns the total number of channels in this signal group.
int SignalGroup::numChannels() const
{
    return static_cast<unsigned int>(channel.size());
}

// Returns the total number of AMPLIFIER channels in this signal group.
int SignalGroup::numAmplifierChannels() const
{
    int count = 0;
    for (unsigned int i = 0; i < channel.size(); ++i) {
        if (channel[i].signalType == AmplifierSignal) {
            ++count;
        }
    }
    return count;
}

wstring SignalGroup::toLower(const wstring& s) {
    wstring out = s;
    std::transform(s.begin(), s.end(), out.begin(), towlower);
    return out;
}

// Updates the alphabetical order indices of all signal channels in this signal group.
void SignalGroup::updateAlphabeticalOrder()
{
    wstring currentFirstNameLower;
    int currentFirstIndex;
    int i, j;
    int size = static_cast<unsigned int>(channel.size());

    // Mark all alphaetical order indices with -1 to indicate they have not yet
    // been assigned an order.
    for (i = 0; i < size; ++i) {
        channel[i].alphaOrder = -1;
    }

    for (i = 0; i < size; ++i) {
        // Find the first remaining non-alphabetized item.
        j = 0;
        while (channel[j].alphaOrder != -1) {
            ++j;
        }
        currentFirstNameLower = toLower(channel[j].customChannelName);
        currentFirstIndex = j;

        // Now compare all other remaining items.
        while (++j < size) {
            if (channel[j].alphaOrder == -1) {
                if (toLower(channel[j].customChannelName) < currentFirstNameLower) {
                    currentFirstNameLower = toLower(channel[j].customChannelName);
                    currentFirstIndex = j;
                }
            }
        }

        // Now assign correct alphabetical order to this item.
        channel[currentFirstIndex].alphaOrder = i;
    }
}

// Restores channels to their original order.
void SignalGroup::setOriginalChannelOrder()
{
    for (unsigned int i = 0; i < channel.size(); ++i) {
        channel[i].userOrder = channel[i].nativeChannelNumber;
    }
}

// Orders signal channels alphabetically.
void SignalGroup::setAlphabeticalChannelOrder()
{
    updateAlphabeticalOrder();
    for (unsigned int i = 0; i < channel.size(); ++i) {
        channel[i].userOrder = channel[i].alphaOrder;
    }
}

// Diagnostic routine to display all channels in this group (to cout).
void SignalGroup::print() const
{
    cout << "SignalGroup " << name.c_str() << " (" << prefix.c_str() <<
            ") enabled:" << enabled << endl;
    for (unsigned int i = 0; i < channel.size(); ++i) {
        cout << "  SignalChannel " << channel[i].nativeChannelNumber << " " << channel[i].customChannelName.c_str() << " (" <<
                channel[i].nativeChannelName.c_str() << ") stream:" << channel[i].boardStream <<
                " channel:" << channel[i].chipChannel << endl;
    }
    cout << endl;
}

// Streams all signal channels in this group out to binary data stream.
BinaryWriter& operator<<(BinaryWriter& outStream, const SignalGroup &a)
{
    outStream << a.name;
    outStream << a.prefix;
    outStream << (int16_t) a.enabled;
    outStream << (int16_t) a.numChannels();
    outStream << (int16_t) a.numAmplifierChannels();
    for (int i = 0; i < a.numChannels(); ++i) {
        outStream << a.channel[i];
    }
    return outStream;
}

// Streams all signal channels in this group in from binary data stream.
BinaryReader& operator>>(BinaryReader &inStream, SignalGroup &a)
{
    int16_t tempQint16;

    inStream >> a.name;
    inStream >> a.prefix;
    inStream >> tempQint16;
    a.enabled = (tempQint16 != 0);
    inStream >> tempQint16;
    int nTotal = (int) tempQint16;
    inStream >> tempQint16; // numAmplifierChannels

    // Delete all existing SignalChannel objects in this SignalGroup
    a.channel.clear();

    for (int i = 0; i < nTotal; ++i) {
        a.addAmplifierChannel();
        inStream >> a.channel[i];
    }
    a.updateAlphabeticalOrder();

    return inStream;
}
