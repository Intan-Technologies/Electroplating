//  ------------------------------------------------------------------------
//
//  This file is part of the Intan Technologies RHD2000 Interface
//  Version 1.5
//  Copyright (C) 2014 Intan Technologies
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

#ifndef SAVEFORMAT_H
#define SAVEFORMAT_H

#include <memory>
#include <string>
#include <vector>
#include <deque>
#include <exception>
#include "streams.h"

class BoardControl;
class SignalSources;
class SignalChannel;
class BinaryWriter;
class Rhd2000DataBlock;

// Save File Format Enumeration
enum SaveFormat {
    SaveFormatIntan,
    SaveFormatFilePerSignalType,
    SaveFormatFilePerChannel
};

struct Version {
    uint16_t major;
    uint16_t minor;

    Version(uint16_t ma, uint16_t mi) : major(ma), minor(mi) {}
};
bool operator<(const Version& a, const Version& b);
bool operator>=(const Version& a, const Version& b);

struct SaveFormatHeaderInfo {
    Version version;

    int notchFilterIndex;
    std::wstring note1;
    std::wstring note2;
    std::wstring note3;

    BoardControl& boardControl;

    SaveFormatHeaderInfo(BoardControl& bc);
};

//  ------------------------------------------------------------------------
struct SaveList {
    bool saveTemp;
    bool boardDigIn;
    bool boardDigOut;
    std::vector<SignalChannel*> amplifier;
    std::vector<SignalChannel*> auxInput;
    std::vector<SignalChannel*> supplyVoltage;
    std::vector<SignalChannel*> boardAdc;
    std::vector<SignalChannel*> boardDigitalIn;
    std::vector<SignalChannel*> boardDigitalOut;
    std::vector<SignalChannel*> tempSensor;

    SaveList() {
        boardDigOut = false;
        saveTemp = false;
    }

    void import(SignalSources& signalSources);
    void setDigOutFromChannels(SignalSources& signalSources);
};

//  ------------------------------------------------------------------------
class SaveFormatLogic {
public:
    virtual ~SaveFormatLogic() {}

    int bytesPerBlock(const SaveList& saveList);

    virtual int bytesPerBlockDigOut() = 0;
    virtual int bytesPerBlockDigIn(const SaveList& saveList) = 0;
    virtual int bytesPerBlockAux(const SaveList& saveList) = 0;
};

//  ------------------------------------------------------------------------
class SaveFormatWriter {
public:
    SaveFormatWriter() {
    }
    virtual ~SaveFormatWriter();

    virtual void open(const FILENAME& subdirPath, const SaveList& saveList) = 0;
    virtual void close() = 0;
    virtual void writeHeader(const SaveFormatHeaderInfo& header) = 0;

    int writeQueueOfBlocks(BoardControl& boardControl, std::deque<std::unique_ptr<Rhd2000DataBlock>> &dataQueue, int timestampOffset, const std::vector<double>& tempAvg);

    virtual int writeBlock(const SaveList& saveList, const Rhd2000DataBlock& dataBlock, int timestampOffset, const std::vector<double>& tempAvg);
    virtual void flush(const SaveList&) {}

    int bytesPerBlock(const SaveList& saveList);

protected:
    void writeHeaderInternal(BinaryWriter &outStream, const SaveFormatHeaderInfo& header);
    void checkOpen();
    static void createFileStream(const FILENAME& fullpath, std::unique_ptr<BinaryWriter>& file, unsigned int bufferSize);
    static void makeDirectory(const FILENAME& path);

    virtual bool saveTemperature(const SaveList&) { return false; }
    virtual int writeBlockInternal(const SaveList& saveList, const Rhd2000DataBlock& dataBlock, int timestampOffset, const std::vector<double>& tempAvg) = 0;
    virtual bool isOpen() const = 0;
    std::unique_ptr<SaveFormatLogic> logic;
};

//  ------------------------------------------------------------------------
class IntanSaveFormatLogic : public SaveFormatLogic {
public:
    int bytesPerBlockDigOut() override;
    int bytesPerBlockDigIn(const SaveList& saveList) override;
    int bytesPerBlockAux(const SaveList& saveList) override;
};

//  ------------------------------------------------------------------------
class IntanSaveFormat : public SaveFormatWriter {
public:
    std::unique_ptr<BinaryWriter> save;

    IntanSaveFormat();
    ~IntanSaveFormat();

    void open(const FILENAME& saveFileBaseName, const SaveList& saveList) override;
    void close() override;
    void writeHeader(const SaveFormatHeaderInfo& header) override;

protected:
    int writeBlockInternal(const SaveList& saveList, const Rhd2000DataBlock& dataBlock, int timestampOffset, const std::vector<double>& tempAvg) override;
    bool isOpen() const override;

private:
    bool saveTemperature(const SaveList& saveList) override;
};

//  ------------------------------------------------------------------------
class MultiFileFormatWriter : public SaveFormatWriter {
public:
    MultiFileFormatWriter();
    ~MultiFileFormatWriter();

    void close();

    virtual void writeHeader(const SaveFormatHeaderInfo& header) override;

protected:
    std::unique_ptr<BinaryWriter> infoFile;
    std::unique_ptr<BinaryWriter> timestampFile;

    void createTimestampFile(const FILENAME& path);
    void createInfoFile(const FILENAME& filename);
};

//  ------------------------------------------------------------------------
class FilePerSignalLogic : public SaveFormatLogic {
public:
    int bytesPerBlockAux(const SaveList& saveList) override;
    int bytesPerBlockDigOut() override;
    int bytesPerBlockDigIn(const SaveList& saveList) override;
};

//  ------------------------------------------------------------------------
class FilePerSignalFormat : public MultiFileFormatWriter {
public:
    FilePerSignalFormat();
    ~FilePerSignalFormat();

    virtual void open(const FILENAME& subdirPath, const SaveList& saveList) override;
    virtual void close();

protected:
    virtual int writeBlockInternal(const SaveList& saveList, const Rhd2000DataBlock& dataBlock, int timestampOffset, const std::vector<double>& tempAvg);
    virtual bool isOpen() const;

private:
    std::unique_ptr<BinaryWriter> amplifierFile;
    std::unique_ptr<BinaryWriter> auxInputFile;
    std::unique_ptr<BinaryWriter> supplyFile;
    std::unique_ptr<BinaryWriter> adcInputFile;
    std::unique_ptr<BinaryWriter> digitalInputFile;
    std::unique_ptr<BinaryWriter> digitalOutputFile;

    void createSignalTypeFiles(const FILENAME& path, const SaveList& saveList);
};

//  ------------------------------------------------------------------------
class FilePerChannelFormatLogic : public SaveFormatLogic {
public:
    int bytesPerBlockAux(const SaveList& saveList) override;
    int bytesPerBlockDigOut() override;
    int bytesPerBlockDigIn(const SaveList& saveList) override;
};

//  ------------------------------------------------------------------------
class FilePerChannelFormat : public MultiFileFormatWriter {
public:
    FilePerChannelFormat(SignalSources& signalSources_);
    ~FilePerChannelFormat();

    virtual void open(const FILENAME& subdirPath, const SaveList& saveList) override;
    virtual void close();

protected:
    virtual int writeBlockInternal(const SaveList& saveList, const Rhd2000DataBlock& dataBlock, int timestampOffset, const std::vector<double>& tempAvg);
    virtual bool isOpen() const;

private:
    SignalSources& signalSources;

    void createSaveFiles(const FILENAME& path);
    void closeSaveFiles();
};

//  ------------------------------------------------------------------------
class FileNotOpenException : public std::logic_error {
public:
    FileNotOpenException(const char* message) : std::logic_error(message) {}
};

//  ------------------------------------------------------------------------
class SaveFormatReader {
public:
    SaveFormatReader() : numStreams(0) {
    }
    virtual ~SaveFormatReader() {}

    virtual void open(const FILENAME& subdirPath) = 0;
    virtual void close() = 0;
    virtual void readHeader(SaveFormatHeaderInfo& header) = 0;

    virtual void readBlock(Rhd2000DataBlock& dataBlock, std::vector<double>& tempAvg);
    virtual unsigned int numBlocksRemaining() = 0;

    unsigned int numStreams;

protected:
    void readHeaderInternal(BinaryReader &inStream, SaveFormatHeaderInfo& header);
    void checkOpen();
    static void createFileStream(const FILENAME& fullpath, std::unique_ptr<BinaryReader>& file);
    virtual void readBlockInternal(Rhd2000DataBlock& dataBlock, std::vector<double>& tempAvg) = 0;
    virtual bool isOpen() const = 0;

    std::unique_ptr<SaveFormatLogic> logic;
};

//  ------------------------------------------------------------------------
class IntanSaveFormatReader : public SaveFormatReader {
public:
    std::unique_ptr<BinaryReader> save;

    IntanSaveFormatReader();
    ~IntanSaveFormatReader();

    void open(const FILENAME& saveFileName) override;
    void close() override;
    void readHeader(SaveFormatHeaderInfo& header) override;
    unsigned int numBlocksRemaining() override;

protected:
    SaveList saveList;

    void readBlockInternal(Rhd2000DataBlock& dataBlock, std::vector<double>& tempAvg) override;
    bool isOpen() const override;
};


#endif // SAVEFORMAT_H
