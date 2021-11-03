//----------------------------------------------------------------------------------
// rhd2000datablock.cpp
//
// Intan Technoloies RHD2000 Rhythm Interface API
// Rhd2000DataBlock Class
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

#include "rhd2000datablock.h"

#include <iostream>
#include <fstream>
#include <iomanip>
#include <vector>
#include <cmath>
#include <QtCore>

#include "rhd2000evalboard.h"
#include "rhd2000registers.h"

using std::vector;
using std::cerr;
using std::endl;
using std::fixed;
using std::setprecision;
using std::ios;
using std::ostream;

#define RHD2000_HEADER_MAGIC_NUMBER 0xc691199927021942

/** \brief Constructor. 

    Allocates memory for a data block supporting the specified number of data streams.

	@param[in] numDataStreams	Number of USB data streams enabled (0-7)
 */
Rhd2000DataBlock::Rhd2000DataBlock(int numDataStreams)
{
    allocateUIntArray1D(timeStamp, SAMPLES_PER_DATA_BLOCK);
    allocateIntArray3D(amplifierData, numDataStreams, 32, SAMPLES_PER_DATA_BLOCK);
    allocateIntArray3D(auxiliaryData, numDataStreams, 3, SAMPLES_PER_DATA_BLOCK);
    allocateIntArray2D(boardAdcData, 8, SAMPLES_PER_DATA_BLOCK);
    allocateIntArray1D(ttlIn, SAMPLES_PER_DATA_BLOCK);
    allocateIntArray1D(ttlOut, SAMPLES_PER_DATA_BLOCK);
}

// Allocates memory for a 1-D array of integers.
void Rhd2000DataBlock::allocateIntArray1D(vector<int> &array1D, int xSize)
{
    array1D.resize(xSize);
}

// Allocates memory for a 1-D array of unsigned integers.
void Rhd2000DataBlock::allocateUIntArray1D(vector<unsigned int> &array1D, int xSize)
{
    array1D.resize(xSize);
}

// Allocates memory for a 2-D array of integers.
void Rhd2000DataBlock::allocateIntArray2D(vector<vector<int> > & array2D, int xSize, int ySize)
{
    int i;

    array2D.resize(xSize);
    for (i = 0; i < xSize; ++i)
        array2D[i].resize(ySize);
}

// Allocates memory for a 3-D array of integers.
void Rhd2000DataBlock::allocateIntArray3D(vector<vector<vector<int> > > &array3D, int xSize, int ySize, int zSize)
{
    int i, j;

    array3D.resize(xSize);
    for (i = 0; i < xSize; ++i) {
        array3D[i].resize(ySize);

        for (j = 0; j < ySize; ++j) {
            array3D[i][j].resize(zSize);
        }
    }
}

/** \brief Returns the number of samples in a USB data block.

    \returns SAMPLES_PER_DATA_BLOCK, which is set to 60.
 */
unsigned int Rhd2000DataBlock::getSamplesPerDataBlock()
{
    return SAMPLES_PER_DATA_BLOCK;
}

/** Calculates the size of a USB data block for the given number of data streams.

    @param[in] numDataStreams Number of data streams enabled (0-7).

    @returns Size in 16-bit words.
 */
unsigned int Rhd2000DataBlock::calculateDataBlockSizeInWords(int numDataStreams)
{
    return SAMPLES_PER_DATA_BLOCK * (4 + 2 + numDataStreams * 36 + 8 + 2);
    // 4 = magic number; 2 = time stamp; 36 = (32 amp channels + 3 aux commands + 1 filler word); 8 = ADCs; 2 = TTL in/out
}

// Check first 64 bits of USB header against the fixed Rhythm "magic number" to verify data sync.
bool Rhd2000DataBlock::checkUsbHeader(unsigned char usbBuffer[], int index)
{
    unsigned long long x1, x2, x3, x4, x5, x6, x7, x8;
    unsigned long long header;

    x1 = usbBuffer[index];
    x2 = usbBuffer[index + 1];
    x3 = usbBuffer[index + 2];
    x4 = usbBuffer[index + 3];
    x5 = usbBuffer[index + 4];
    x6 = usbBuffer[index + 5];
    x7 = usbBuffer[index + 6];
    x8 = usbBuffer[index + 7];

    header = (x8 << 56) + (x7 << 48) + (x6 << 40) + (x5 << 32) + (x4 << 24) + (x3 << 16) + (x2 << 8) + (x1 << 0);

    return (header == RHD2000_HEADER_MAGIC_NUMBER);
}

// Read 32-bit time stamp from USB data frame.
unsigned int Rhd2000DataBlock::convertUsbTimeStamp(unsigned char usbBuffer[], int index)
{
    unsigned int x1, x2, x3, x4;
    x1 = usbBuffer[index];
    x2 = usbBuffer[index + 1];
    x3 = usbBuffer[index + 2];
    x4 = usbBuffer[index + 3];

    return (x4 << 24) + (x3 << 16) + (x2 << 8) + (x1 << 0);
}

// Convert two USB bytes into 16-bit word.
int Rhd2000DataBlock::convertUsbWord(unsigned char usbBuffer[], int index)
{
    unsigned int x1, x2, result;

    x1 = (unsigned int) usbBuffer[index];
    x2 = (unsigned int) usbBuffer[index + 1];

    result = (x2 << 8) | (x1 << 0);

    return (int) result;
}

/** \brief Fill data block with raw data from USB input buffer.

    @param[in] usbBuffer        Input USB buffer
    @param[in] blockIndex       Block index.  Setting blockIndex to 0 selects the first data block in the buffer, setting blockIndex to 1 selects the second data block, etc.
    @param[in] numDataStreams   Number of data streams in the USB buffer.
 */
void Rhd2000DataBlock::fillFromUsbBuffer(unsigned char usbBuffer[], int blockIndex, unsigned int numDataStreams)
{
    int index = blockIndex * 2 * calculateDataBlockSizeInWords(numDataStreams);
    for (unsigned int t = 0; t < SAMPLES_PER_DATA_BLOCK; ++t) {
        if (!checkUsbHeader(usbBuffer, index)) {
            cerr << "Error in Rhd2000EvalBoard::readDataBlock: Incorrect header." << endl;
        }
        index += 8;
        timeStamp[t] = convertUsbTimeStamp(usbBuffer, index);
        index += 4;

        // Read auxiliary results
        for (unsigned int slot = 0; slot < NUM_AUX_COMMAND_SLOTS; ++slot) {
            for (unsigned int stream = 0; stream < numDataStreams; ++stream) {
                auxiliaryData[stream][slot][t] = convertUsbWord(usbBuffer, index);
                index += 2;
            }
        }

        // Read amplifier channels
        for (unsigned int channel = 0; channel < 32; ++channel) {
            for (unsigned int stream = 0; stream < numDataStreams; ++stream) {
                amplifierData[stream][channel][t] = convertUsbWord(usbBuffer, index);
                index += 2;
            }
        }

        // skip 36th filler word in each data stream
        index += 2 * numDataStreams;

        // Read from AD5662 ADCs
        for (unsigned int i = 0; i < NUM_BOARD_ANALOG_INPUTS; ++i) {
            boardAdcData[i][t] = convertUsbWord(usbBuffer, index);
            index += 2;
        }

        // Read TTL input and output values
        ttlIn[t] = convertUsbWord(usbBuffer, index);
        index += 2;

        ttlOut[t] = convertUsbWord(usbBuffer, index);
        index += 2;
    }
}

/** \brief Prints the contents of RHD2000 registers from a selected USB data stream. 

    This function assumes that the command string generated by Rhd2000Registers::createCommandListRegisterConfig has been uploaded to the AuxCmd3 slot.

	@param[in] out		C++ output stream to send the information to (e.g., std::cout)
	@param[in] stream   Selected USB data stream (0-7)
*/
void Rhd2000DataBlock::print(ostream &out, int stream) const
{
	Rhd2000Registers registersVar(1000);
	registersVar.readBack(auxiliaryData[stream][Rhd2000EvalBoard::AuxCmd3]);

    Rhd2000RegisterInternals::typed_register_t& registers = registersVar.registers;

    out << endl;
    out << "RHD 2000 Data Block contents:" << endl;
    out << "  ROM contents:" << endl;
    out << "    Chip Name: " <<
           registers.romChipName[0] <<
           registers.romChipName[1] <<
		   registers.romChipName[2] <<
		   registers.romChipName[3] <<
		   registers.romChipName[4] <<
		   registers.romChipName[5] <<
		   registers.romChipName[6] <<
		   registers.romChipName[7] << endl;
    out << "    Company Name:" <<
           registers.romCompany[0] <<
		   registers.romCompany[1] <<
		   registers.romCompany[2] <<
		   registers.romCompany[3] <<
		   registers.romCompany[4] << endl;
	out << "    Intan Chip ID: " << (int)registers.romChipId << endl;
	out << "    Number of Amps: " << (int)registers.romNumAmplifiers << endl;
    out << "    Unipolar/Bipolar Amps: ";
    switch (registers.romUnipolar) {
        case 0:
            out << "bipolar";
            break;
        case 1:
            out << "unipolar";
            break;
        default:
            out << "UNKNOWN";
    }
    out << endl;
    out << "    Die Revision: " << (int)registers.romDieRevision << endl;
    out << "    Future Expansion Register: " << (int)registers.romMisoABMarker << endl;

    out << "  RAM contents:" << endl;
	out << "    ADC reference BW:      " << (int)registers.r0.adcReferenceBw << endl;
	out << "    amp fast settle:       " << (int)registers.r0.ampFastSettle << endl;
	out << "    amp Vref enable:       " << (int)registers.r0.ampVrefEnable << endl;
	out << "    ADC comparator bias:   " << (int)registers.r0.adcComparatorBias << endl;
	out << "    ADC comparator select: " << (int)registers.r0.adcComparatorSelect << endl;
	out << "    VDD sense enable:      " << (int)registers.r1.vddSenseEnable << endl;
	out << "    ADC buffer bias:       " << (int)registers.r1.adcBufferBias << endl;
	out << "    MUX bias:              " << (int)registers.r2.muxBias << endl;
	out << "    MUX load:              " << (int)registers.r3.muxLoad << endl;
	out << "    tempS2, tempS1:        " << (int)registers.r3.tempS2 << "," << (int)registers.r3.tempS1 << endl;
	out << "    tempen:                " << (int)registers.r3.tempEn << endl;
	out << "    digout HiZ:            " << (int)registers.r3.digOutHiZ << endl;
	out << "    digout:                " << (int)registers.r3.digOut << endl;
	out << "    weak MISO:             " << (int)registers.r4.weakMiso << endl;
	out << "    twoscomp:              " << (int)registers.r4.twosComp << endl;
	out << "    absmode:               " << (int)registers.r4.absMode << endl;
	out << "    DSPen:                 " << (int)registers.r4.dspEn << endl;
	out << "    DSP cutoff freq:       " << (int)registers.r4.dspCutoffFreq << endl;
	out << "    Zcheck DAC power:      " << (int)registers.r5.zcheckDacPower << endl;
	out << "    Zcheck load:           " << (int)registers.r5.zcheckLoad << endl;
	out << "    Zcheck scale:          " << (int)registers.r5.zcheckScale << endl;
	out << "    Zcheck conn all:       " << (int)registers.r5.zcheckConnAll << endl;
	out << "    Zcheck sel pol:        " << (int)registers.r5.zcheckSelPol << endl;
	out << "    Zcheck en:             " << (int)registers.r5.zcheckEn << endl;
	out << "    Zcheck DAC:            " << (int)registers.r6.zcheckDac << endl;
	out << "    Zcheck select:         " << (int)registers.r7.zcheckSelect << endl;
	out << "    ADC aux1 en:           " << (int)registers.r9.adcAux1En << endl;
	out << "    ADC aux2 en:           " << (int)registers.r11.adcAux2En << endl;
	out << "    ADC aux3 en:           " << (int)registers.r13.adcAux3En << endl;
	out << "    offchip RH1:           " << (int)registers.r8.offChipRH1 << endl;
	out << "    offchip RH2:           " << (int)registers.r10.offChipRH2 << endl;
	out << "    offchip RL:            " << (int)registers.r12.offChipRL << endl;

	int rH1Dac1 = registers.r8.rH1Dac1;
	int rH1Dac2 = registers.r9.rH1Dac2;
	int rH2Dac1 = registers.r10.rH2Dac1;
    int rH2Dac2 = registers.r11.rH2Dac2;
	int rLDac1 = registers.r12.rLDac1;
	int rLDac2 = registers.r13.rLDac2;
	int rLDac3 = registers.r13.rLDac3;

    double rH1 = 2630.0 + rH1Dac2 * 30800.0 + rH1Dac1 * 590.0;
    double rH2 = 8200.0 + rH2Dac2 * 38400.0 + rH2Dac1 * 730.0;
    double rL = 3300.0 + rLDac3 * 3000000.0 + rLDac2 * 15400.0 + rLDac1 * 190.0;

    out << fixed << setprecision(2);

    out << "    RH1 DAC1, DAC2:        " << rH1Dac1 << " " << rH1Dac2 << " = " << (rH1 / 1000) <<
            " kOhm" << endl;
    out << "    RH2 DAC1, DAC2:        " << rH2Dac1 << " " << rH2Dac2 << " = " << (rH2 / 1000) <<
            " kOhm" << endl;
    out << "    RL DAC1, DAC2, DAC3:   " << rLDac1 << " " << rLDac2 << " " << rLDac3 << " = " <<
            (rL / 1000) << " kOhm" << endl;

	out << "    amp power[31:0]:      ";
	for (int set = 3; set >= 0; set--) {
		int value = registers.aPwr[set];
		out << " ";
		for (int bit = 7; bit >= 0; bit--) {
			out << ((value & (1 << bit)) >> bit);
		}
	}
	out << endl;

    out << endl;

    int vddSample = auxiliaryData[stream][Rhd2000EvalBoard::AuxCmd2][28];

    double tempUnitsC = getTemperature(stream);
    double tempUnitsF = (9.0 / 5.0) * tempUnitsC + 32.0;

    double vddSense = 0.0000748 * ((double) vddSample);

    out << setprecision(1);
    out << "  Temperature sensor (only one reading): " << tempUnitsC << " C (" <<
            tempUnitsF << " F)" << endl;

    out << setprecision(2);
    out << "  Supply voltage sensor                : " << vddSense << " V" << endl;

    out << setprecision(6);
    out.unsetf(ios::floatfield);
    out << endl;
}

// Write a 16-bit dataWord to an outputStream in "little endian" format (i.e., least significant
// byte first).  We must do this explicitly for cross-platform consistency.  For example, Windows
// is a little-endian OS, while Mac OS X and Linux can be little-endian or big-endian depending on
// the processor running the operating system.
//
// (See "Endianness" article in Wikipedia for more information.)
void Rhd2000DataBlock::writeWordLittleEndian(ostream &outputStream, int dataWord) const
{
    unsigned short msb, lsb;

    lsb = ((unsigned short) dataWord) & 0x00ff;
    msb = (((unsigned short) dataWord) & 0xff00) >> 8;

    outputStream << (unsigned char) lsb;
    outputStream << (unsigned char) msb;
}

/** \brief Writes the contents of a data block object to a binary output stream.

    Data is written in little endian format (i.e., least significant byte first).

    @param[in] saveOut          Output stream to write to
    @param[in] numDataStreams   Number of data streams in the blocks
*/
void Rhd2000DataBlock::write(ostream &saveOut, unsigned int numDataStreams) const
{

    for (unsigned int t = 0; t < SAMPLES_PER_DATA_BLOCK; ++t) {
        writeWordLittleEndian(saveOut, timeStamp[t]);
        for (unsigned int channel = 0; channel < 32; ++channel) {
            for (unsigned int stream = 0; stream < numDataStreams; ++stream) {
                writeWordLittleEndian(saveOut, amplifierData[stream][channel][t]);
            }
        }
        for (unsigned int slot = 0; slot < NUM_AUX_COMMAND_SLOTS; ++slot) {
            for (unsigned int stream = 0; stream < numDataStreams; ++stream) {
                writeWordLittleEndian(saveOut, auxiliaryData[stream][slot][t]);
            }
        }
        for (unsigned int i = 0; i < NUM_BOARD_ANALOG_INPUTS; ++i) {
            writeWordLittleEndian(saveOut, boardAdcData[i][t]);
        }
        writeWordLittleEndian(saveOut, ttlIn[t]);
        writeWordLittleEndian(saveOut, ttlOut[t]);
    }
}

/** \brief Converts an amplifier value to &mu;V.

    @param[in] value    Amplifier value (0-65535)
    @returns   The corresponding voltage, in &mu;V.
*/
double Rhd2000DataBlock::amplifierADCToMicroVolts(int value) {
    return 0.195 * (value - 0x8000); // Convert to microvolts
}

/** \brief Converts &mu;V to amplifier units.

    Useful in setting DAC thresholds, for example.

    @param[in] value    Voltage, in &mu;V.
    @returns Corresponding amplifier value (0-65535).
*/
int Rhd2000DataBlock::microVoltsToAmplifierADC(double value) {
    return std::lround(value / 0.195) + 0x8000;
}

/** \brief Converts an auxiliary ADC value to volts.

    Note: The auxiliary ADCs (which read auxin1, auxin2, and auxin3) have a range up to 2.45 V, 
    with 65,536 steps in between, so each increment in value corresponds to 0.0000374 V.

    @param[in] value    Aux ADC value (0-65535)
    @returns   The corresponding voltage, in volts.
*/
double Rhd2000DataBlock::auxADCToVolts(int value) {
    return 0.0000374 * value;
}

/** \brief Converts a board ADC value to volts.

    Note: The 8 board ADCs have a range up to 3.3 V,
    with 65,536 steps in between, so each increment in value corresponds to 0.000050354 V.


    @param[in] value    Board ADC value (0-65535)
    @returns   The corresponding voltage, in volts.
*/
double Rhd2000DataBlock::boardADCToVolts(int value) {
    return 0.000050354 * value;
}

/** \brief Returns the temperature reading of the given stream.

    Assumes that the auxiliary data is set up per standard configuration, 
    with AuxCmd2 containing a command list created with createCommandListTempSensor.

    @param[in] stream   Selected USB data stream (0-7)
    @returns    Temperature reading, in C.
 */
double Rhd2000DataBlock::getTemperature(int stream) const {
    int tempA = auxiliaryData[stream][Rhd2000EvalBoard::AuxCmd2][12];
    int tempB = auxiliaryData[stream][Rhd2000EvalBoard::AuxCmd2][20];
    double tempUnitsC = ((double)(tempB - tempA)) / 98.9 - 273.15;
    return tempUnitsC;
}

/** \brief Returns the supply voltage reading of the given stream.

    Assumes that the auxiliary data is set up per standard configuration,
    with AuxCmd2 containing a command list created with createCommandListTempSensor.

    Note: the supply voltage is halved via a voltage divider, and that 1/2 supply is measured
    using the same ADCs as in auxADCToVolts.

    @param[in] stream   Selected USB data stream (0-7)
    @returns    Supply Voltage reading, in Volts.
*/
double Rhd2000DataBlock::getSupplyVoltage(int stream) const {
    return 2 * auxADCToVolts(auxiliaryData[stream][Rhd2000EvalBoard::AuxCmd2][28]);
}
