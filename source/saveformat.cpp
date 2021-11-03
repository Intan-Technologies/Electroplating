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

#include "saveformat.h"
#include "signalchannel.h"
#include <iostream>
#include "signalsources.h"
#include "signalgroup.h"
#include "boardcontrol.h"
#include <fstream>
#include "streams.h"
#include "common.h"
#include <sys/stat.h>


using std::cerr;
using std::endl;
using std::unique_ptr;
using std::ofstream;
using std::ios;
using std::string;
using std::vector;
using std::deque;
using std::exception;


// Saved data file constants
#define DATA_FILE_MAGIC_NUMBER  0xc6912702
#define DATA_FILE_MAIN_VERSION_NUMBER  1
#define DATA_FILE_SECONDARY_VERSION_NUMBER  4

//  ------------------------------------------------------------------------
bool operator<(const Version& a, const Version& b) {
    if (a.major < b.major) {
        return true;
    }
    if (a.major > b.major) {
        return false;
    }
    return a.minor < b.minor;
}

bool operator>=(const Version& a, const Version& b) {
    return !(a < b);
}

//  ------------------------------------------------------------------------
SaveFormatHeaderInfo::SaveFormatHeaderInfo(BoardControl& bc) : 
    boardControl(bc),
    version(DATA_FILE_MAIN_VERSION_NUMBER, DATA_FILE_SECONDARY_VERSION_NUMBER)
{
}

//  ------------------------------------------------------------------------
// Creates lists (vectors, actually) of all enabled waveforms to expedite
// save-to-disk operations.
void SaveList::import(SignalSources& signalSources)
{
    amplifier.clear();
    auxInput.clear();
    supplyVoltage.clear();
    boardAdc.clear();
    boardDigitalIn.clear();
    boardDigitalOut.clear();
    tempSensor.clear();

    boardDigIn = false;

    for (unsigned int port = 0; port < signalSources.signalPort.size(); ++port) {
        for (int index = 0; index < signalSources.signalPort[port].numChannels(); ++index) {
            SignalChannel* currentChannel = signalSources.signalPort[port].channelByNativeOrder(index);
            // Add all enabled channels to their appropriate save list.
            if (currentChannel->enabled) {
                switch (currentChannel->signalType) {
                case AmplifierSignal:
                    amplifier.push_back(currentChannel);
                    break;
                case AuxInputSignal:
                    auxInput.push_back(currentChannel);
                    break;
                case SupplyVoltageSignal:
                    supplyVoltage.push_back(currentChannel);
                    break;
                case BoardAdcSignal:
                    boardAdc.push_back(currentChannel);
                    break;
                case BoardDigInSignal:
                    boardDigIn = true;
                    boardDigitalIn.push_back(currentChannel);
                    break;
                case BoardDigOutSignal:
                    boardDigitalOut.push_back(currentChannel);
                }
            }
            // Use supply voltage signal as a proxy for the presence of a temperature sensor,
            // since these always appear together on each chip.  Add all temperature sensors
            // list, whether or not the corresponding supply voltage signals are enabled.
            if (currentChannel->signalType == SupplyVoltageSignal) {
                tempSensor.push_back(currentChannel);
            }
        }
    }
}

void SaveList::setDigOutFromChannels(SignalSources& signalSources) {
    // Set boardDigOut to true if any of the digital output channels are enabled
    boardDigOut = false;
    vector<SignalChannel>& digOutChannels = signalSources.signalPort[6].channel;
    for (unsigned int i = 0; i < digOutChannels.size(); i++) {
        if (digOutChannels[i].enabled) {
            boardDigOut = true;
        }
    }

    // And either enable or disable all of them; we don't (currently) support the granularity of individual channels
    for (unsigned int i = 0; i < digOutChannels.size(); i++) {
        digOutChannels[i].enabled = boardDigOut;
    }

}

//  ------------------------------------------------------------------------
// Returns the total number of bytes saved to disk per data block.
int SaveFormatLogic::bytesPerBlock(const SaveList& saveList)
{
    int bytes = 0;
    bytes += 4 * SAMPLES_PER_DATA_BLOCK;  // timestamps
    bytes += 2 * SAMPLES_PER_DATA_BLOCK * static_cast<unsigned int>(saveList.amplifier.size());

    bytes += bytesPerBlockAux(saveList);

    bytes += 2 * SAMPLES_PER_DATA_BLOCK * static_cast<unsigned int>(saveList.boardAdc.size());

    bytes += bytesPerBlockDigIn(saveList);

    if (saveList.boardDigOut) {
        bytes += bytesPerBlockDigOut();
    }
    return bytes;
}

//  ------------------------------------------------------------------------
SaveFormatWriter::~SaveFormatWriter() {
}

// Write header to data save file, containing information on
// recording settings, amplifier parameters, and signal sources.
void SaveFormatWriter::writeHeaderInternal(BinaryWriter &outfStream, const SaveFormatHeaderInfo& header)
{
    outfStream << (uint32_t)DATA_FILE_MAGIC_NUMBER;
    outfStream << header.version.major;
    outfStream << header.version.minor;

    outfStream << header.boardControl.boardSampleRate;

    outfStream << (int16_t)header.boardControl.bandWidth.dspEnabled;
    outfStream << header.boardControl.bandWidth.actualDspCutoffFreq;
    outfStream << header.boardControl.bandWidth.actualLowerBandwidth;
    outfStream << header.boardControl.bandWidth.actualUpperBandwidth;

    outfStream << header.boardControl.bandWidth.desiredDspCutoffFreq;
    outfStream << header.boardControl.bandWidth.desiredLowerBandwidth;
    outfStream << header.boardControl.bandWidth.desiredUpperBandwidth;

    outfStream << (int16_t)header.notchFilterIndex;

    outfStream << header.boardControl.impedance.desiredImpedanceFreq;
    outfStream << header.boardControl.impedance.actualImpedanceFreq;

    outfStream << header.note1;
    outfStream << header.note2;
    outfStream << header.note3;

    int16_t saveNumTempSensors = saveTemperature(*header.boardControl.saveList) ? header.boardControl.getNumTempSensors() : 0;
    outfStream << saveNumTempSensors;                        // version 1.1 addition

    outfStream << (int16_t)header.boardControl.evalBoardMode;            // version 1.3 addition

    outfStream << header.boardControl.signalSources;
}

int SaveFormatWriter::bytesPerBlock(const SaveList& saveList) {
    return logic->bytesPerBlock(saveList);
}

void SaveFormatWriter::createFileStream(const FILENAME& fullpath, std::unique_ptr<BinaryWriter>& file, unsigned int bufferSize) {
    unique_ptr<FileOutStream> fs(new FileOutStream());
    fs->open(fullpath);

    unique_ptr<BinaryWriter> bs(new BinaryWriter(std::move(fs), bufferSize));
    file.reset(bs.release());
}

void SaveFormatWriter::checkOpen() {
    if (!isOpen()) {
        throw FileNotOpenException("You must open a file before writing to it.");
    }
}

int SaveFormatWriter::writeBlock(const SaveList& saveList, const Rhd2000DataBlock& dataBlock, int timestampOffset, const std::vector<double>& tempAvg) {
    checkOpen();
    return writeBlockInternal(saveList, dataBlock, timestampOffset, tempAvg);
}

// A disk-format binary datastream is written out.
// A timestampOffset can be used to reference the trigger point to zero.
//
// Returns number of bytes written to binary datastream.
int SaveFormatWriter::writeQueueOfBlocks(BoardControl& boardControl, deque<unique_ptr<Rhd2000DataBlock>> &dataQueue, int timestampOffset, const std::vector<double>& tempAvg) {
    checkOpen();

    int numBlocks = static_cast<unsigned int>(dataQueue.size());

    int numWordsWritten = 0;

    for (int block = 0; block < numBlocks; ++block) {
        Rhd2000DataBlock& dataBlock = *dataQueue[block];
        numWordsWritten += writeBlockInternal(*boardControl.saveList, dataBlock, timestampOffset, tempAvg);
    }

    // If we are operating on the "One File Per Channel" format, we have saved all amplifier data from
    // multiple data blocks in dataStreamBufferArray.  Now we write it all at once, for each channel.
    flush(*boardControl.saveList);

    // Return total number of bytes written to binary output stream
    return (2 * numWordsWritten);
}

void SaveFormatWriter::makeDirectory(const FILENAME& path) {
#if defined(_WIN32) && defined(_UNICODE)
    _wmkdir(path.c_str());
#else
    //mkdir(path.c_str(), 0777);
#endif
}

//  ------------------------------------------------------------------------

IntanSaveFormat::IntanSaveFormat()
{
    logic.reset(new IntanSaveFormatLogic());
}

IntanSaveFormat::~IntanSaveFormat() {
    close();
}

void IntanSaveFormat::open(const FILENAME& saveFileBaseName, const SaveList&) {
    bool endsInDotRHD = false;
    if (saveFileBaseName.length() >= 4) {
        if (saveFileBaseName.substr(saveFileBaseName.length() - 4) == _T(".rhd")) {
            endsInDotRHD = true;
        }
    }
    createFileStream(endsInDotRHD ? saveFileBaseName : saveFileBaseName + _T(".rhd"), save, 256 * KILO);
}

void IntanSaveFormat::close() {
    save.reset();
}

void IntanSaveFormat::writeHeader(const SaveFormatHeaderInfo& header) {
    checkOpen();
    writeHeaderInternal(*save, header);
}

bool IntanSaveFormat::isOpen() const {
    return save.get() != nullptr;
}

int IntanSaveFormat::writeBlockInternal(const SaveList& saveList, const Rhd2000DataBlock &dataBlock, int timestampOffset, const vector<double>& tempAvg)
{
    int t;
    unsigned int i;

    int numWordsWritten = 0;

    // Save timestamp data
    for (t = 0; t < SAMPLES_PER_DATA_BLOCK; ++t) {
        *save << ((int32_t)dataBlock.timeStamp[t]) - ((int32_t)timestampOffset);
    }
    numWordsWritten += 2 * SAMPLES_PER_DATA_BLOCK;

    // Save amplifier data
    for (i = 0; i < saveList.amplifier.size(); ++i) {
        for (t = 0; t < SAMPLES_PER_DATA_BLOCK; ++t) {
            *save << (uint16_t) dataBlock.amplifierData[saveList.amplifier.at(i)->boardStream][saveList.amplifier.at(i)->chipChannel][t];
        }
    }
	numWordsWritten += static_cast<unsigned int>(saveList.amplifier.size()) * SAMPLES_PER_DATA_BLOCK;

    // Save auxiliary input data
    for (i = 0; i < saveList.auxInput.size(); ++i) {
        for (t = 0; t < SAMPLES_PER_DATA_BLOCK; t += 4) {
            *save << (uint16_t) dataBlock.auxiliaryData[saveList.auxInput.at(i)->boardStream][Rhd2000EvalBoard::AuxCmd2][t + saveList.auxInput.at(i)->chipChannel + 1];
        }
    }
	numWordsWritten += static_cast<unsigned int>(saveList.auxInput.size()) * SAMPLES_PER_DATA_BLOCK;

    // Save supply voltage data
    for (i = 0; i < saveList.supplyVoltage.size(); ++i) {
        *save << (uint16_t) dataBlock.auxiliaryData[saveList.supplyVoltage.at(i)->boardStream][Rhd2000EvalBoard::AuxCmd2][28];
        ++numWordsWritten;
    }

    // Save temperature sensor data if saveTemp == true
    // Save as temperature in degrees C, multiplied by 100 and rounded to the nearest
    // signed integer.
    if (saveList.saveTemp) {
        for (i = 0; i < saveList.tempSensor.size(); ++i) {
            *save << (int16_t)(100.0 * tempAvg[saveList.tempSensor.at(i)->boardStream]);
            ++numWordsWritten;
        }
    }

    // Save board ADC data
    for (i = 0; i < saveList.boardAdc.size(); ++i) {
        for (t = 0; t < SAMPLES_PER_DATA_BLOCK; ++t) {
            *save << (uint16_t) dataBlock.boardAdcData[saveList.boardAdc.at(i)->nativeChannelNumber][t];
        }
    }
	numWordsWritten += static_cast<unsigned int>(saveList.boardAdc.size()) * SAMPLES_PER_DATA_BLOCK;

    // Save board digital input data
    if (saveList.boardDigIn) {
        // If ANY digital inputs are enabled, we save ALL 16 channels, since
        // we are writing 16-bit chunks of data.
        for (t = 0; t < SAMPLES_PER_DATA_BLOCK; ++t) {
            *save << (uint16_t)dataBlock.ttlIn[t];
            ++numWordsWritten;
        }
    }

    // Save board digital output data
    if (saveList.boardDigOut) {
        // Save all 16 channels, since we are writing 16-bit chunks of data.
        for (t = 0; t < SAMPLES_PER_DATA_BLOCK; ++t) { 
            *save << (uint16_t)dataBlock.ttlOut[t];
            ++numWordsWritten;
        }
    }

    return numWordsWritten;
}

int IntanSaveFormatLogic::bytesPerBlockDigOut() {
    return 2 * SAMPLES_PER_DATA_BLOCK; 
}

int IntanSaveFormatLogic::bytesPerBlockDigIn(const SaveList& saveList) {
    if (saveList.boardDigIn) {
        return 2 * SAMPLES_PER_DATA_BLOCK;
    }

    return 0;
}

int IntanSaveFormatLogic::bytesPerBlockAux(const SaveList& saveList) {
    int bytes = 0;
	bytes += 2 * (SAMPLES_PER_DATA_BLOCK / 4) * static_cast<unsigned int>(saveList.auxInput.size());
	bytes += 2 * (SAMPLES_PER_DATA_BLOCK / 60) * static_cast<unsigned int>(saveList.supplyVoltage.size());
    if (saveList.saveTemp) {
		bytes += 2 * (SAMPLES_PER_DATA_BLOCK / 60) * static_cast<unsigned int>(saveList.supplyVoltage.size());
    }

    return bytes;
}

bool IntanSaveFormat::saveTemperature(const SaveList& saveList) { 
    return saveList.saveTemp; 
}


//  ------------------------------------------------------------------------
MultiFileFormatWriter::MultiFileFormatWriter()
{
}

MultiFileFormatWriter::~MultiFileFormatWriter() {
    close();
}

void MultiFileFormatWriter::close() {
    timestampFile.reset();
    infoFile.reset();
}

void MultiFileFormatWriter::writeHeader(const SaveFormatHeaderInfo& header) {
    checkOpen();
    writeHeaderInternal(*infoFile, header);
}

// Create filename (appended to the specified path) for timestamp data
// and open timestamp save file.
void MultiFileFormatWriter::createTimestampFile(const FILENAME& path)
{
    createFileStream(path + _T("/") + _T("time") + _T(".dat"), timestampFile, 4 * KILO);
}

void MultiFileFormatWriter::createInfoFile(const FILENAME& filename) {
    createFileStream(filename, infoFile, 4 * KILO);
}

//  ------------------------------------------------------------------------

FilePerSignalFormat::FilePerSignalFormat()
{
    logic.reset(new FilePerSignalLogic());
}

FilePerSignalFormat::~FilePerSignalFormat() {
    close();
}

void FilePerSignalFormat::open(const FILENAME& subdirPath, const SaveList& saveList) {
    // Create subdirectory for data, timestamp, and info files.
    makeDirectory(subdirPath);

    createInfoFile(subdirPath + _T("/") + _T("info.rhd"));

    createTimestampFile(subdirPath);

    createSignalTypeFiles(subdirPath, saveList);
}

// Create filename (appended to the specified path) for data files in
// "One File Per Signal Type" format.
// Open data files for "One File Per Signal Type" format.
void FilePerSignalFormat::createSignalTypeFiles(const FILENAME& path, const SaveList& saveList)
{
    if (saveList.amplifier.size() > 0) {
        createFileStream(path + _T("/") + _T("amplifier") + _T(".dat"), amplifierFile, 256 * KILO);
    }
    if (saveList.auxInput.size() > 0) {
        createFileStream(path + _T("/") + _T("auxiliary") + _T(".dat"), auxInputFile, 16 * KILO);
    }
    if (saveList.supplyVoltage.size() > 0) {
        createFileStream(path + _T("/") + _T("supply") + _T(".dat"), supplyFile, 16 * KILO);
    }
    if (saveList.boardAdc.size() > 0) {
        createFileStream(path + _T("/") + _T("analogin") + _T(".dat"), adcInputFile, 16 * KILO);
    }
    if (saveList.boardDigitalIn.size() > 0) {
        createFileStream(path + _T("/") + _T("digitalin") + _T(".dat"), digitalInputFile, 16 * KILO);
    }
    if (saveList.boardDigOut) {
        createFileStream(path + _T("/") + _T("digitalout") + _T(".dat"), digitalOutputFile, 16 * KILO);
    }
}

// Close data files for "One File Per Signal Type" format.
void FilePerSignalFormat::close()
{
    MultiFileFormatWriter::close();

    amplifierFile.reset();
    auxInputFile.reset();
    supplyFile.reset();
    adcInputFile.reset();
    digitalInputFile.reset();
    digitalOutputFile.reset();
}

bool FilePerSignalFormat::isOpen() const {
    return timestampFile.get() != nullptr; // Just checking one, as we open/close them all together.  We could be more thorough.
}

int FilePerSignalFormat::writeBlockInternal(const SaveList& saveList, const Rhd2000DataBlock& dataBlock, int timestampOffset, const vector<double>&)
{
    int t;
    unsigned int i;

    int numWordsWritten = 0;

    int tAux;
    // Save timestamp data
    for (t = 0; t < SAMPLES_PER_DATA_BLOCK; ++t) {
        *timestampFile << ((int32_t)dataBlock.timeStamp[t]) - ((int32_t)timestampOffset);
    }
    numWordsWritten += 2 * SAMPLES_PER_DATA_BLOCK;

    // Save amplifier data
    for (t = 0; t < SAMPLES_PER_DATA_BLOCK; ++t) {
        for (i = 0; i < saveList.amplifier.size(); ++i) {
            *amplifierFile << (int16_t) (dataBlock.amplifierData[saveList.amplifier.at(i)->boardStream][saveList.amplifier.at(i)->chipChannel][t] - 32768);
        }
    }
	numWordsWritten += static_cast<unsigned int>(saveList.amplifier.size()) * SAMPLES_PER_DATA_BLOCK;

    // Save auxiliary input data
    for (t = 0; t < SAMPLES_PER_DATA_BLOCK; ++t) {
        tAux = 4 * (int)floor((double)t / 4.0);
        for (i = 0; i < saveList.auxInput.size(); ++i) {
            *auxInputFile << (uint16_t) dataBlock.auxiliaryData[saveList.auxInput.at(i)->boardStream][Rhd2000EvalBoard::AuxCmd2][tAux + saveList.auxInput.at(i)->chipChannel + 1];
        }
    }
	numWordsWritten += static_cast<unsigned int>(saveList.auxInput.size()) * SAMPLES_PER_DATA_BLOCK;

    // Save supply voltage data
    for (t = 0; t < SAMPLES_PER_DATA_BLOCK; ++t) {
        for (i = 0; i < saveList.supplyVoltage.size(); ++i) {
            *supplyFile << (uint16_t) dataBlock.auxiliaryData[saveList.supplyVoltage.at(i)->boardStream][Rhd2000EvalBoard::AuxCmd2][28];
        }
    }
	numWordsWritten += static_cast<unsigned int>(saveList.supplyVoltage.size()) * SAMPLES_PER_DATA_BLOCK;

    // Not saving temperature data in this save format.

    // Save board ADC data
    for (t = 0; t < SAMPLES_PER_DATA_BLOCK; ++t) {
        for (i = 0; i < saveList.boardAdc.size(); ++i) {
            *adcInputFile << (uint16_t) dataBlock.boardAdcData[saveList.boardAdc.at(i)->nativeChannelNumber][t];
        }
    }
	numWordsWritten += static_cast<unsigned int>(saveList.boardAdc.size()) * SAMPLES_PER_DATA_BLOCK;

    // Save board digital input data
    if (saveList.boardDigIn) {
        for (t = 0; t < SAMPLES_PER_DATA_BLOCK; ++t) {
            // If ANY digital inputs are enabled, we save ALL 16 channels, since
            // we are writing 16-bit chunks of data.
            *digitalInputFile << (uint16_t)dataBlock.ttlIn[t];
            ++numWordsWritten;
        }
    }

    // Save board digital output data
    if (saveList.boardDigOut) {
        for (t = 0; t < SAMPLES_PER_DATA_BLOCK; ++t) {
            // Save all 16 channels, since we are writing 16-bit chunks of data.
            *digitalOutputFile << (uint16_t)dataBlock.ttlOut[t];
            ++numWordsWritten;
        }
    }

    return numWordsWritten;
}

int FilePerSignalLogic::bytesPerBlockAux(const SaveList& saveList) {
    int bytes = 0;
    bytes += 2 * SAMPLES_PER_DATA_BLOCK * static_cast<unsigned int>(saveList.auxInput.size());
    bytes += 2 * SAMPLES_PER_DATA_BLOCK * static_cast<unsigned int>(saveList.supplyVoltage.size());

    return bytes;
}

int FilePerSignalLogic::bytesPerBlockDigOut() {
    return 2 * SAMPLES_PER_DATA_BLOCK; 
}

int FilePerSignalLogic::bytesPerBlockDigIn(const SaveList& saveList) {
    if (saveList.boardDigIn) {
        return 2 * SAMPLES_PER_DATA_BLOCK;
    }

    return 0;
}

//  ------------------------------------------------------------------------

FilePerChannelFormat::FilePerChannelFormat(SignalSources& signalSources_) :
    signalSources(signalSources_)
{
    logic.reset(new FilePerChannelFormatLogic());
}

FilePerChannelFormat::~FilePerChannelFormat() {
    close();
}

void FilePerChannelFormat::open(const FILENAME& subdirPath, const SaveList&) {
    // Create subdirectory for individual channel data files.
    makeDirectory(subdirPath);

    createTimestampFile(subdirPath);

    // Create and open file for each channel.
    createSaveFiles(subdirPath);

    createInfoFile(subdirPath + _T("/") + _T("info.rhd"));
}

// Create filenames (appended to the specified path) for each waveform
// and open individual save data files for all enabled waveforms.
void FilePerChannelFormat::createSaveFiles(const FILENAME& path)
{
    for (unsigned int port = 0; port < signalSources.signalPort.size(); ++port) {
        for (int index = 0; index < signalSources.signalPort[port].numChannels(); ++index) {
            SignalChannel* currentChannel = signalSources.signalPort[port].channelByNativeOrder(index);
            // Only create filenames for enabled channels.
            if (currentChannel->enabled) {
                FILENAME name;
                FILENAME nativeChannelName = toFileName(currentChannel->nativeChannelName);
                switch (currentChannel->signalType) {
                case AmplifierSignal:
                    name = path + _T("/") + _T("amp-") + nativeChannelName + _T(".dat");
                    break;
                case AuxInputSignal:
                    name = path + _T("/") + _T("aux-") + nativeChannelName + _T(".dat");
                    break;
                case SupplyVoltageSignal:
                    name = path + _T("/") + _T("vdd-") + nativeChannelName + _T(".dat");
                    break;
                case BoardAdcSignal:
                    name = path + _T("/") + _T("board-") + nativeChannelName + _T(".dat");
                    break;
                case BoardDigInSignal:
                    name = path + _T("/") + _T("board-") + nativeChannelName + _T(".dat");
                    break;
                case BoardDigOutSignal:
                    name = path + _T("/") + _T("board-") + nativeChannelName + _T(".dat");
                    break;
                }

                unique_ptr<BinaryWriter> tmp;
                createFileStream(name, tmp, 4 * KILO);
                currentChannel->saveFile.reset(tmp.release());
            }
        }
    }
}

void FilePerChannelFormat::close() {
    MultiFileFormatWriter::close();
    closeSaveFiles();
}

// Close individual save data files for all enabled waveforms.
void FilePerChannelFormat::closeSaveFiles()
{
    for (unsigned int port = 0; port < signalSources.signalPort.size(); ++port) {
        for (int index = 0; index < signalSources.signalPort[port].numChannels(); ++index) {
            SignalChannel* currentChannel = signalSources.signalPort[port].channelByNativeOrder(index);
            // Only close files for enabled channels.
            if (currentChannel->enabled) {
                currentChannel->saveFile.reset();
            }
        }
    }
}

bool FilePerChannelFormat::isOpen() const {
    return timestampFile.get() != nullptr; // Just checking one, as we open/close them all together.  We could be more thorough.
}

int FilePerChannelFormat::writeBlockInternal(const SaveList& saveList, const Rhd2000DataBlock& dataBlock, int timestampOffset, const vector<double>&)
{
    int t, j;
    unsigned int i;

    int numWordsWritten = 0;

    // Save timestamp data
    for (t = 0; t < SAMPLES_PER_DATA_BLOCK; ++t) {
        *timestampFile << ((int32_t)dataBlock.timeStamp[t]) - ((int32_t)timestampOffset);
    }
    numWordsWritten += 2 * SAMPLES_PER_DATA_BLOCK;

    // Save amplifier data to dataStreamBufferArray; In in effort to increase write speed we will
    // collect amplifier data from all data blocks and then write all data at the end of this function.
    for (i = 0; i < saveList.amplifier.size(); ++i) {
        for (t = 0; t < SAMPLES_PER_DATA_BLOCK; ++t) {
            *saveList.amplifier.at(i)->saveFile << (int16_t)
                (dataBlock.amplifierData[saveList.amplifier.at(i)->boardStream][saveList.amplifier.at(i)->chipChannel][t] - 32768);
        }
        numWordsWritten += SAMPLES_PER_DATA_BLOCK;
    }

    // Save auxiliary input data
    for (i = 0; i < saveList.auxInput.size(); ++i) {
        for (t = 0; t < SAMPLES_PER_DATA_BLOCK; t += 4) {
            for (j = 0; j < 4; ++j) {   // Aux data is sampled at 1/4 amplifier sampling rate; write each sample 4 times
                *(saveList.auxInput.at(i)->saveFile) << (uint16_t)
                    dataBlock.auxiliaryData[saveList.auxInput.at(i)->boardStream][Rhd2000EvalBoard::AuxCmd2][t + saveList.auxInput.at(i)->chipChannel + 1];
            }
        }
        numWordsWritten += SAMPLES_PER_DATA_BLOCK;
    }

    // Save supply voltage data
    for (i = 0; i < saveList.supplyVoltage.size(); ++i) {
        for (j = 0; j < SAMPLES_PER_DATA_BLOCK; ++j) {   // Vdd data is sampled at 1/60 amplifier sampling rate; write each sample 60 times
            *saveList.supplyVoltage.at(i)->saveFile << (uint16_t)
                dataBlock.auxiliaryData[saveList.supplyVoltage.at(i)->boardStream][Rhd2000EvalBoard::AuxCmd2][28];
        }
        numWordsWritten += SAMPLES_PER_DATA_BLOCK;
    }

    // Not saving temperature data in this save format.

    // Save board ADC data
    for (i = 0; i < saveList.boardAdc.size(); ++i) {
        for (t = 0; t < SAMPLES_PER_DATA_BLOCK; ++t) {
            *saveList.boardAdc.at(i)->saveFile << (uint16_t)
                dataBlock.boardAdcData[saveList.boardAdc.at(i)->nativeChannelNumber][t];
        }
        numWordsWritten += SAMPLES_PER_DATA_BLOCK;
    }

    // Save board digital input data
    for (i = 0; i < saveList.boardDigitalIn.size(); ++i) {
        for (t = 0; t < SAMPLES_PER_DATA_BLOCK; ++t) {
            *saveList.boardDigitalIn.at(i)->saveFile << (uint16_t)
                ((dataBlock.ttlIn[t] & (1 << saveList.boardDigitalIn.at(i)->nativeChannelNumber)) != 0);
        }
        numWordsWritten += SAMPLES_PER_DATA_BLOCK;
    }

    // Save board digital output data
    if (saveList.boardDigOut) {
        for (i = 0; i < NUM_DIGITAL_OUTPUTS; ++i) {
            for (t = 0; t < SAMPLES_PER_DATA_BLOCK; ++t) {
                *saveList.boardDigitalOut.at(i)->saveFile << (uint16_t)
                    ((dataBlock.ttlOut[t] & (1 << i)) != 0);
            }
            numWordsWritten += SAMPLES_PER_DATA_BLOCK;
        }
    }
    return numWordsWritten;
}

int FilePerChannelFormatLogic::bytesPerBlockAux(const SaveList& saveList) {
    int bytes = 0;
    bytes += 2 * SAMPLES_PER_DATA_BLOCK * static_cast<unsigned int>(saveList.auxInput.size());
    bytes += 2 * SAMPLES_PER_DATA_BLOCK * static_cast<unsigned int>(saveList.supplyVoltage.size());

    return bytes;
}

int FilePerChannelFormatLogic::bytesPerBlockDigOut() {
    return 2 * SAMPLES_PER_DATA_BLOCK * 16; 
}

int FilePerChannelFormatLogic::bytesPerBlockDigIn(const SaveList& saveList) {
	return 2 * SAMPLES_PER_DATA_BLOCK * static_cast<unsigned int>(saveList.boardDigitalIn.size());
}

//  ------------------------------------------------------------------------
// Write header to data save file, containing information on
// recording settings, amplifier parameters, and signal sources.
void SaveFormatReader::readHeaderInternal(BinaryReader &inStream, SaveFormatHeaderInfo& header)
{
    uint32_t tmp;
    inStream >> tmp;
    if (tmp != DATA_FILE_MAGIC_NUMBER) {
        throw std::invalid_argument("Invalid file type");
    }
    header.version.major = header.version.minor = 0;
    inStream >> header.version.major;
    inStream >> header.version.minor;

    inStream >> header.boardControl.boardSampleRate;

    int16_t itmp;
    inStream >> itmp;
    header.boardControl.bandWidth.dspEnabled = (itmp != 0);
    inStream >> header.boardControl.bandWidth.actualDspCutoffFreq;
    inStream >> header.boardControl.bandWidth.actualLowerBandwidth;
    inStream >> header.boardControl.bandWidth.actualUpperBandwidth;

    inStream >> header.boardControl.bandWidth.desiredDspCutoffFreq;
    inStream >> header.boardControl.bandWidth.desiredLowerBandwidth;
    inStream >> header.boardControl.bandWidth.desiredUpperBandwidth;

    inStream >> itmp;
    header.notchFilterIndex = itmp;

    inStream >> header.boardControl.impedance.desiredImpedanceFreq;
    inStream >> header.boardControl.impedance.actualImpedanceFreq;

    inStream >> header.note1;
    inStream >> header.note2;
    inStream >> header.note3;

    if (header.version >= Version(1, 1)) { // version 1.1 addition
        inStream >> itmp;
        // Ignore for now
        //    int16_t saveNumTempSensors = saveTemperature(*header.boardControl.saveList) ? header.boardControl.getNumTempSensors() : 0;
    }

    if (header.version >= Version(1, 3)) { // version 1.3 addition
        inStream >> itmp;
        header.boardControl.evalBoardMode = itmp;
    }

    inStream >> header.boardControl.signalSources;

    // Set the numStreams value
    bool datastreamsEnabled[8] = { false, false, false, false, false, false, false, false };
    for (unsigned int i = 0; i < 4; i++) {
        for (SignalChannel& channel : header.boardControl.signalSources.signalPort[i].channel) {
            if (channel.enabled) {
                datastreamsEnabled[channel.boardStream] = true;
            }
        }
    }
    for (bool stream : datastreamsEnabled) {
        if (stream) {
            numStreams++;
        }
    }
}

void SaveFormatReader::createFileStream(const FILENAME& fullpath, std::unique_ptr<BinaryReader>& file) {
    unique_ptr<FileInStream> fs(new FileInStream());
    fs->open(fullpath);

    unique_ptr<BinaryReader> bs(new BinaryReader(std::move(fs)));
    file.reset(bs.release());
}

void SaveFormatReader::checkOpen() {
    if (!isOpen()) {
        throw FileNotOpenException("You must open a file before writing to it.");
    }
}

void SaveFormatReader::readBlock(Rhd2000DataBlock& dataBlock, std::vector<double>& tempAvg) {
    checkOpen();
    readBlockInternal(dataBlock, tempAvg);
}

//  ------------------------------------------------------------------------

IntanSaveFormatReader::IntanSaveFormatReader()
{
    logic.reset(new IntanSaveFormatLogic());
}

IntanSaveFormatReader::~IntanSaveFormatReader() {
    close();
}

void IntanSaveFormatReader::open(const FILENAME& saveFileName) {
    createFileStream(saveFileName, save);
}

void IntanSaveFormatReader::close() {
    save.reset();
}

void IntanSaveFormatReader::readHeader(SaveFormatHeaderInfo& header) {
    checkOpen();
    readHeaderInternal(*save, header);

    saveList.import(header.boardControl.signalSources);
    saveList.setDigOutFromChannels(header.boardControl.signalSources);
}

unsigned int IntanSaveFormatReader::numBlocksRemaining() {
    uint64_t bytesRemaining = save->bytesRemaining();
    unsigned int blockSize = logic->bytesPerBlock(saveList);
    if (bytesRemaining % blockSize == 0) {
        return static_cast<unsigned int>(bytesRemaining / blockSize);
    }
    else {
        throw std::invalid_argument("Error in parser - invalid number of blocks detected.");
    }
}

bool IntanSaveFormatReader::isOpen() const {
    return save.get() != nullptr;
}

void IntanSaveFormatReader::readBlockInternal(Rhd2000DataBlock &dataBlock, vector<double>& tempAvg)
{
    unsigned int t, i;
    int32_t tmpI32;
    uint16_t tmpU16;
    int16_t tmpI16;

    // Save timestamp data
    for (t = 0; t < SAMPLES_PER_DATA_BLOCK; ++t) {
        *save >> tmpI32;
        dataBlock.timeStamp[t] = tmpI32;
    }

    // Save amplifier data
    for (i = 0; i < saveList.amplifier.size(); ++i) {
        for (t = 0; t < SAMPLES_PER_DATA_BLOCK; ++t) {
            *save >> tmpU16;
            dataBlock.amplifierData[saveList.amplifier.at(i)->boardStream][saveList.amplifier.at(i)->chipChannel][t] = tmpU16;
        }
    }

    // Save auxiliary input data
    for (i = 0; i < saveList.auxInput.size(); ++i) {
        for (t = 0; t < SAMPLES_PER_DATA_BLOCK; t += 4) {
            *save >> tmpU16;
            dataBlock.auxiliaryData[saveList.auxInput.at(i)->boardStream][Rhd2000EvalBoard::AuxCmd2][t + saveList.auxInput.at(i)->chipChannel + 1] = tmpU16;
        }
    }

    // Save supply voltage data
    for (i = 0; i < saveList.supplyVoltage.size(); ++i) {
        *save >> tmpU16;
        dataBlock.auxiliaryData[saveList.supplyVoltage.at(i)->boardStream][Rhd2000EvalBoard::AuxCmd2][28] = tmpU16;
    }

    // Save temperature sensor data if saveTemp == true
    // Save as temperature in degrees C, multiplied by 100 and rounded to the nearest
    // signed integer.
    if (saveList.saveTemp) {
        for (i = 0; i < saveList.tempSensor.size(); ++i) {
            *save >> tmpI16;
            tempAvg[saveList.tempSensor.at(i)->boardStream] = tmpI16 / 100.0;
        }
    }

    // Save board ADC data
    for (i = 0; i < saveList.boardAdc.size(); ++i) {
        for (t = 0; t < SAMPLES_PER_DATA_BLOCK; ++t) {
            *save >> tmpU16;
            dataBlock.boardAdcData[saveList.boardAdc.at(i)->nativeChannelNumber][t] = tmpU16;
        }
    }

    // Save board digital input data
    if (saveList.boardDigIn) {
        // If ANY digital inputs are enabled, we save ALL 16 channels, since
        // we are writing 16-bit chunks of data.
        for (t = 0; t < SAMPLES_PER_DATA_BLOCK; ++t) {
            *save >> tmpU16;
            dataBlock.ttlIn[t] = tmpU16;
        }
    }

    // Save board digital output data
    if (saveList.boardDigOut) {
        // Save all 16 channels, since we are writing 16-bit chunks of data.
        for (t = 0; t < SAMPLES_PER_DATA_BLOCK; ++t) {
            *save >> tmpU16;
            dataBlock.ttlOut[t] = tmpU16;
        }
    }
}
