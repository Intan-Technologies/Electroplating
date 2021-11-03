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

#ifndef SIGNALSOURCES_H
#define SIGNALSOURCES_H

#include "signalgroup.h"
#include <vector>
#include <string>

class SignalChannel;
class BinaryWriter;
class BinaryReader;

class SignalSources
{
    friend BinaryWriter &operator<<(BinaryWriter &outStream, const SignalSources &a);
    friend BinaryReader &operator>>(BinaryReader &inStream, SignalSources &a);

public:
    SignalSources();
    SignalChannel* findChannelFromName(const std::wstring &nativeName);
    SignalChannel* findAmplifierChannel(int boardStream, int chipChannel);

    std::vector<SignalGroup> signalPort;

private:

};

#endif // SIGNALSOURCES_H
