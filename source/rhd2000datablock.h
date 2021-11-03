//----------------------------------------------------------------------------------
// rhd2000datablock.h
//
// Intan Technoloies RHD2000 Rhythm Interface API
// Rhd2000DataBlock Class Header File
// Version 1.4 (26 February 2014)
//
// Copyright (c) 2013-2014 Intan Technologies LLC
//
// This software is provided 'as-is', without any express or implied warranty.
// In no event will the authors be held liable for any damages arising from the
// use of this software.
//
// Permission is granted to anyone to use this software for any applications that
// use Intan Technologies integrated circuits, and to alter it and redistribute it
// freely.
//
// See http://www.intantech.com for documentation and product information.
//----------------------------------------------------------------------------------

#ifndef RHD2000DATABLOCK_H
#define RHD2000DATABLOCK_H

/** \brief Number of samples stored in a data block.
 */
const int SAMPLES_PER_DATA_BLOCK = 60; // TODO: should actually be unsigned

#include <vector>
#include <iostream>

/** \file rhd2000datablock.h
    \brief File containing Rhd2000DataBlock
*/


class Rhd2000EvalBoard;

/** \brief This class creates a data structure storing SAMPLES_PER_DATA_BLOCK data samples from a 
    Rhythm FPGA interface controlling up to eight RHD2000 chips. (A \#define statement in rhd2000datablock.h 
    currently sets SAMPLES_PER_DATA_BLOCK to 60.) 
    
    Typically, instances of Rhd2000DataBlock will be created dynamically as data becomes available over 
    the USB interface and appended to a queue that will be used to stream the data to disk or to a GUI display.
 */
class Rhd2000DataBlock
{
public:
    Rhd2000DataBlock(int numDataStreams);

	/** \brief timeStamp[sample]

		Time stamp, indexed by timeStamp[sample], where
			\li sample is the sample (0-{SAMPLES_PER_DATA_BLOCK - 1})
	*/
	std::vector<unsigned int> timeStamp;

	/** \brief amplifierData[stream][channel][sample]

		Amplifier data, indexed by amplifierData[stream][channel][sample], where 
			\li stream is the USB data stream (0-7)
			\li channel is the channel on the amplifier (0-63)
			\li sample is the sample (0-{SAMPLES_PER_DATA_BLOCK - 1})
	*/
	std::vector<std::vector<std::vector<int> > > amplifierData;

	/** \brief auxiliaryData[stream][index][sample]
	
		Auxiliary data, indexed by auxiliaryData[stream][index][sample], where
			\li stream is the USB data stream (0-7)
			\li index is the index within the auxiliary data
			\li sample is the sample (0-{SAMPLES_PER_DATA_BLOCK - 1})

		What data is returned for what index depends on how the auxiliary command are configured, 
		using Rhd2000Registers::createCommandListRegisterConfig or similar functions.
	*/
	std::vector<std::vector<std::vector<int> > > auxiliaryData;

	/** \brief boardAdcData[adc][sample]
	
		ADC input, indexed by boardAdcData[adc][sample], where
			\li adc is the index of the ADC (0-7)
			\li sample is the sample (0-{SAMPLES_PER_DATA_BLOCK - 1})
	*/
	std::vector<std::vector<int> > boardAdcData;

	/** \brief ttlIn[sample]
	
		TTL input, indexed by ttlIn[sample], where
			\li sample is the sample (0-{SAMPLES_PER_DATA_BLOCK - 1})

		16 bits of TTL input are stored as bits in a single int, so bit b is found by 
			\code{.cpp} ttlIn[sample] & (1 << b) \endcode
	*/
	std::vector<int> ttlIn;

	/** \brief ttlOut[sample]
	
		TTL output, indexed by ttlOut[sample], where
			\li sample is the sample (0-{SAMPLES_PER_DATA_BLOCK - 1})

		16 bits of TTL output are stored as bits in a single int, so bit b is found by
			\code{.cpp} ttlOut[sample] & (1 << b) \endcode
		*/
	std::vector<int> ttlOut;

    double getTemperature(int stream) const;
    double getSupplyVoltage(int stream) const;

    static unsigned int calculateDataBlockSizeInWords(int numDataStreams);
    static unsigned int getSamplesPerDataBlock();
    void fillFromUsbBuffer(unsigned char usbBuffer[], int blockIndex, unsigned int numDataStreams);
	void print(std::ostream &out, int stream) const;
	void write(std::ostream &saveOut, unsigned int numDataStreams) const;

    static double amplifierADCToMicroVolts(int value);
    static int microVoltsToAmplifierADC(double value);

    static double auxADCToVolts(int value);
    static double boardADCToVolts(int value);

private:
	void allocateIntArray3D(std::vector<std::vector<std::vector<int> > > &array3D, int xSize, int ySize, int zSize);
	void allocateIntArray2D(std::vector<std::vector<int> > &array2D, int xSize, int ySize);
	void allocateIntArray1D(std::vector<int> &array1D, int xSize);
	void allocateUIntArray1D(std::vector<unsigned int> &array1D, int xSize);

	void writeWordLittleEndian(std::ostream &outputStream, int dataWord) const;

    bool checkUsbHeader(unsigned char usbBuffer[], int index);
    unsigned int convertUsbTimeStamp(unsigned char usbBuffer[], int index);
    int convertUsbWord(unsigned char usbBuffer[], int index);
};

#endif // RHD2000DATABLOCK_H
