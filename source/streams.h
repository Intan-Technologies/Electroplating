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

#ifndef STREAMS_H
#define STREAMS_H

#include <memory>
#include <string>
#include <cstdint>
#include <iosfwd>
#include <istream>

#if defined(_WIN32) && defined(_UNICODE)
    typedef std::wstring FILENAME;
#else
    typedef std::string FILENAME;
#endif

//  ------------------------------------------------------------------------
class FileOutStream  {
public:
    FileOutStream();
    ~FileOutStream();

    void open(const FILENAME& filename); // Opens with new name
    int write(const char* data, int len);

private:
    std::unique_ptr<std::ofstream> filestream;

    void close();
};

//  ------------------------------------------------------------------------
const unsigned int KILO = 1024;

class BufferedOutStream {
public:
    BufferedOutStream(std::unique_ptr<FileOutStream>&& other_, unsigned int bufferSize_);
    ~BufferedOutStream();

    int write(const char* data, int len);
    void flushIfNecessary();
    void flush();
    
private:
    std::unique_ptr<FileOutStream> other;
    char* dataStreamBuffer;
    unsigned int bufferIndex;
    unsigned int bufferSize;
};

//  ------------------------------------------------------------------------
class BinaryWriter {
public:
    BinaryWriter(std::unique_ptr<FileOutStream>&& other_, unsigned int bufferSize_);
    virtual ~BinaryWriter();

protected:
    friend BinaryWriter& operator<<(BinaryWriter& ostream, int32_t value);
    friend BinaryWriter& operator<<(BinaryWriter& ostream, uint32_t value);
    friend BinaryWriter& operator<<(BinaryWriter& ostream, int16_t value);
    friend BinaryWriter& operator<<(BinaryWriter& ostream, uint16_t value);
    friend BinaryWriter& operator<<(BinaryWriter& ostream, int8_t value);
    friend BinaryWriter& operator<<(BinaryWriter& ostream, uint8_t value);
    friend BinaryWriter& operator<<(BinaryWriter& ostream, float value);
    friend BinaryWriter& operator<<(BinaryWriter& ostream, double value);
    friend BinaryWriter& operator<<(BinaryWriter& ostream, const std::wstring& value);

private:
    BufferedOutStream other;
};

//  ------------------------------------------------------------------------
class InStream {
public:
    virtual ~InStream() {}

    virtual int read(char* data, int len) = 0;
    virtual uint64_t bytesRemaining() = 0;
};

//  ------------------------------------------------------------------------
class FileInStream : public InStream {
public:
    FileInStream();
    ~FileInStream();

    virtual bool open(const FILENAME& filename); // Opens with new name
    virtual int read(char* data, int len) override;
    uint64_t bytesRemaining() override;

private:
    std::unique_ptr<std::ifstream> filestream;
    std::istream::pos_type filesize;

    virtual void close();
};

//  ------------------------------------------------------------------------
class BinaryReader {
public:
    BinaryReader(std::unique_ptr<InStream>&& other_);
    virtual ~BinaryReader();

    uint64_t bytesRemaining() { return other->bytesRemaining();  }

protected:
    friend BinaryReader& operator>>(BinaryReader& istream, int32_t& value);
    friend BinaryReader& operator>>(BinaryReader& istream, uint32_t& value);
    friend BinaryReader& operator>>(BinaryReader& istream, int16_t& value);
    friend BinaryReader& operator>>(BinaryReader& istream, uint16_t& value);
    friend BinaryReader& operator>>(BinaryReader& istream, int8_t& value);
    friend BinaryReader& operator>>(BinaryReader& istream, uint8_t& value);
    friend BinaryReader& operator>>(BinaryReader& istream, float& value);
    friend BinaryReader& operator>>(BinaryReader& istream, double& value);
    friend BinaryReader& operator>>(BinaryReader& istream, std::wstring& value);

private:
    std::unique_ptr<InStream> other;
};

FILENAME toFileName(const std::string& s);
FILENAME toFileName(const std::wstring& ws);

#endif // STREAMS_H
