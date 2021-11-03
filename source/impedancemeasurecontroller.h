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

#ifndef IMPEDANCEMEASURECONTROLLER_H
#define IMPEDANCEMEASURECONTROLLER_H

class Rhd2000DataBlock;

namespace Rhd2000RegisterInternals {
    struct typed_register_t;
}

#include <vector>
#include <deque>
#include <complex>
#include <memory>
#include "boardcontrol.h"

class ProgressWrapper {
public:
    virtual ~ProgressWrapper() {}

    virtual void setMaximum(int maximum) = 0;
    virtual void setValue(int progress) = 0;
    virtual int value() const = 0;
    virtual int maximum() const = 0;
    virtual bool wasCanceled() const = 0;
};

class ImpedanceMeasureController {
public:
    ImpedanceMeasureController(BoardControl& bc, ProgressWrapper& progressWrapper_, BoardControl::CALLBACK_FUNCTION_IDLE callback_, bool continuation=false);

    bool runImpedanceMeasurementRealBoard();
    std::complex<double> measureOneImpedance(Rhd2000EvalBoard::BoardDataSource datasource, unsigned int channel);

private:
    BoardControl& boardControl;
    ProgressWrapper& progress;
    bool rhd2164ChipPresent;
    BoardControl::CALLBACK_FUNCTION_IDLE *callback;

    bool setupAndMeasureAmplitudes(const std::vector<unsigned int>& channels, std::vector<std::vector<std::vector<std::complex<double> > > >& measuredAmplitudes);
    bool measureAmplitudesForAllCapacitances(const std::vector<unsigned int>& channels, std::vector<std::vector<std::vector<std::complex<double> > > >& measuredAmplitudes);
    void findBestImpedances(std::vector<std::vector<std::vector<std::complex<double> > > >& measuredAmplitudes, std::vector<std::vector<std::complex<double> > >& bestZ);
    void storeBestImpedances(std::vector<std::vector<std::complex<double> > >& bestZ);
    void advanceLEDs();
    void getAmplifierData(std::deque<std::unique_ptr<Rhd2000DataBlock>> &dataQueue, int stream, int channel, std::vector<double>& amplifierData);
    std::vector<std::vector<std::vector<std::complex<double>>>> createAmplitudeMatrix();
};

#endif // IMPEDANCEMEASURECONTROLLER_H
