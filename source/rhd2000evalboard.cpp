//----------------------------------------------------------------------------------
// rhd2000evalboard.cpp
//
// Intan Technoloies RHD2000 Rhythm Interface API
// Rhd2000EvalBoard Class
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

#include "rhd2000evalboard.h"

#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>
#include <cmath>
#include <QtCore>

#include "rhd2000datablock.h"
#include "common.h"

#include "okFrontPanelDLL.h"

using std::string;
using std::endl;
using std::cerr;
using std::vector;
using std::hex;
using std::uppercase;
using std::internal;
using std::nouppercase;
using std::dec;
using std::setfill;
using std::setw;
using std::deque;
using std::ostream;
using std::unique_ptr;

#define INIT_USB_BUFFER_SIZE 2400000
#define RHYTHM_BOARD_ID 500
#define FIFO_CAPACITY_WORDS 67108864

// -------------------------------------------------------------------------------------------------
int Rhd2000EvalBoard::OpalKellyLibraryHandle::refCount = 0;

Rhd2000EvalBoard::OpalKellyLibraryHandle::OpalKellyLibraryHandle()
{
}

Rhd2000EvalBoard::OpalKellyLibraryHandle::~OpalKellyLibraryHandle() {
    decRef();
}

Rhd2000EvalBoard::OpalKellyLibraryHandle* Rhd2000EvalBoard::OpalKellyLibraryHandle::create(okFP_dll_pchar dllPath) {
    if (addRef(dllPath)) {
        return new OpalKellyLibraryHandle();
    }
    return nullptr;
}

// If the DLL's already been loaded, do nothing.  If not, load from default path or (if specified) dllPath
// Return true on success; false on error
bool Rhd2000EvalBoard::OpalKellyLibraryHandle::addRef(okFP_dll_pchar dllPath) {
    if (refCount == 0) {
        okFP_dll_pchar dllLocation = dllPath;
        // If dllLocation is NULL or "", set dllLocation either to the input value or to its default
#ifdef _WIN32
        if (dllLocation == nullptr || dllLocation[0] == '\0') {
            dllLocation = _T("okFrontPanel.dll");
        }
#endif
        if (okFrontPanelDLL_LoadLib(dllLocation) == false) {
            cerr << "FrontPanel DLL could not be loaded.  " <<
                "Make sure this DLL is in the application start directory." << endl;
            return false;
        }

        char dll_date[32], dll_time[32];
        okFrontPanelDLL_GetVersion(dll_date, dll_time);
        LOG(true) << endl << "FrontPanel DLL loaded.  Built: " << dll_date << "  " << dll_time << endl;
    }
    refCount++;

    return true;
}

void Rhd2000EvalBoard::OpalKellyLibraryHandle::decRef() {
    refCount--;
    if (refCount == 0) {
        okFrontPanelDLL_FreeLib();
    }
}

// -------------------------------------------------------------------------------------------------

/** \brief Constructor.
 */
Rhd2000EvalBoard::Rhd2000EvalBoard() :
    usbBuffer(nullptr),
    usbBufferSize(0)
{
    sampleRate = SampleRate30000Hz; // Rhythm FPGA boots up with 30.0 kS/s/channel sampling rate
    numDataStreams = 0;

    for (unsigned int i = 0; i < MAX_NUM_DATA_STREAMS; ++i) {
        dataStreamEnabled[i] = false;
        dataSources[i] = BoardDataSource::PortA1;
    }

    cableDelay.resize(NUM_PORTS, -1);

    setUSBBufferSize(INIT_USB_BUFFER_SIZE);
}

void Rhd2000EvalBoard::setUSBBufferSize(unsigned int size) {
    if (size > usbBufferSize) {
        if (usbBuffer != nullptr) {
            delete[]usbBuffer;
            usbBuffer = nullptr;
        }
        usbBuffer = new unsigned char[size];
        usbBufferSize = size;
    }
}

/* This needs to be here (not in the header file) for the auto_ptr<okCFrontPanel> destructor to work correctly
 * (because the header file doesn't have access to the declarations of okCFrontPanel, and hence doesn't know
 * how to call its destructor).
 */
Rhd2000EvalBoard::~Rhd2000EvalBoard() 
{
    delete []usbBuffer;
    dev.reset();
    library.reset();
}

/** \brief Loads the Opal Kelly DLL from a non-default location.

    You don't need to call this if you just want to use the library in its default location; in that case, open() will
    load the library for you.

    @param[in] dllPath	Path of the okFrontPanel dll.

    \return true if library was successfully loaded.
*/
bool Rhd2000EvalBoard::loadLibrary(okFP_dll_pchar dllPath) {
    if (library.get() == nullptr) {
        library.reset(OpalKellyLibraryHandle::create(dllPath));
    }
    return (library.get() != nullptr);
}

vector<string> Rhd2000EvalBoard::getSerialNumbers() {
    vector<string> results;

    LOG(true) << endl << "Scanning USB for Opal Kelly devices..." << endl << endl;
    int nDevices = dev->GetDeviceCount();
    LOG(true) << "Found " << nDevices << " Opal Kelly device" << ((nDevices == 1) ? "" : "s") <<
        " connected:" << endl;
    for (int i = 0; i < nDevices; ++i) {
        LOG(true) << "  Device #" << i + 1 << ": Opal Kelly " <<
            opalKellyModelName(dev->GetDeviceListModel(i)).c_str() <<
            " with serial number " << dev->GetDeviceListSerial(i).c_str() << endl;
    }
    LOG(true) << endl;

    // Find first device in list of type XEM6010LX45.
    for (int i = 0; i < nDevices; ++i) {
        if (dev->GetDeviceListModel(i) == OK_PRODUCT_XEM6010LX45) {
            results.push_back(dev->GetDeviceListSerial(i));
        }
    }

    return results;
}

/** \brief Returns a list of serial numbers of Opal Kelly boards attached to the computer.

    It will call loadLibrary internally, which may error, or you may call it explicitly first before calling
    this function.

    @param[out] serialNumbers    Output list of serial numbers.

    @returns true if successful, false if error
*/
bool Rhd2000EvalBoard::discoverSerialNumbers(vector<string>& serialNumbers) {
    if (!loadLibrary(nullptr)) {
        return false;
    }

    if (dev.get() == nullptr) {
        dev.reset(new okCFrontPanel);
    }

    serialNumbers = getSerialNumbers();
    return true;
}

/**
 \brief Finds an Opal Kelly XEM6010 - LX45 board attached to a USB port and opens it.

 \return 
	\li 1 if successful.  
	\li -1 if Opal Kelly FrontPanel DLL cannot be loaded.
	\li -2 if an XEM6010 board is not found.
*/
int Rhd2000EvalBoard::open() {
    return openEx("");
}

/** \brief Finds an Opal Kelly XEM6010 - LX45 board attached to a USB port and opens it.

 @param[in] requestedSerialNumber   Serial number of the board to attach to.  If you pass "", the
                                    function will pick the first available board.

 \return 
	\li 1 if successful.  
	\li -1 if Opal Kelly FrontPanel DLL cannot be loaded.
	\li -2 if an XEM6010 board is not found.
*/
int Rhd2000EvalBoard::openEx(const string& requestedSerialNumber) {
    LOG(true) << "---- Intan Technologies ---- Rhythm RHD2000 Controller v1.0 ----" << endl << endl;
    if (!loadLibrary(nullptr)) {
        return -1;
    }

    dev.reset(new okCFrontPanel);

    string serialNumber = requestedSerialNumber;
    if (serialNumber == "") {
        vector<string> serialNumbers = getSerialNumbers();
        if (!serialNumbers.empty()) {
            serialNumber = serialNumbers[0];
        }
    }

    // Attempt to open device.
    if (serialNumber != "") {
        if (dev->OpenBySerial(serialNumber) == okCFrontPanel::NoError) {
            // Configure the on-board PLL appropriately.
            dev->LoadDefaultPLLConfiguration();

            // Get some general information about the XEM.
            LOG(true) << "FPGA system clock: " << getSystemClockFreq() << " MHz" << endl; // Should indicate 100 MHz
            LOG(true) << "Opal Kelly device firmware version: " << dev->GetDeviceMajorVersion() << "." <<
                dev->GetDeviceMinorVersion() << endl;
            LOG(true) << "Opal Kelly device serial number: " << dev->GetSerialNumber().c_str() << endl;
            LOG(true) << "Opal Kelly device ID string: " << dev->GetDeviceID().c_str() << endl << endl;

            return 1;
        }
    }

	dev.reset(nullptr);
    cerr << "Device could not be opened.  Is one connected?" << endl;
    return -2; 
 }

/** \brief Uploads the Rhythm configuration file (i.e., bitfile) to the Xilinx FPGA on the open Opal Kelly board.

  @param[in] filename Path of the configuration file.  Either relative to the executable file or absolute path.

  \return true if successful.
*/
bool Rhd2000EvalBoard::uploadFpgaBitfile(const string& filename)
{
    okCFrontPanel::ErrorCode errorCode = dev->ConfigureFPGA(filename);

    switch (errorCode) {
        case okCFrontPanel::NoError:
            break;
        case okCFrontPanel::DeviceNotOpen:
            cerr << "FPGA configuration failed: Device not open." << endl;
            return(false);
        case okCFrontPanel::FileError:
            cerr << "FPGA configuration failed: Cannot find configuration file." << endl;
            return(false);
        case okCFrontPanel::InvalidBitstream:
            cerr << "FPGA configuration failed: Bitstream is not properly formatted." << endl;
            return(false);
        case okCFrontPanel::DoneNotHigh:
            cerr << "FPGA configuration failed: FPGA DONE signal did not assert after configuration." << endl;
            return(false);
        case okCFrontPanel::TransferError:
            cerr << "FPGA configuration failed: USB error occurred during download." << endl;
            return(false);
        case okCFrontPanel::CommunicationError:
            cerr << "FPGA configuration failed: Communication error with firmware." << endl;
            return(false);
        case okCFrontPanel::UnsupportedFeature:
            cerr << "FPGA configuration failed: Unsupported feature." << endl;
            return(false);
        default:
            cerr << "FPGA configuration failed: Unknown error." << endl;
            return(false);
    }

    // Check for Opal Kelly FrontPanel support in the FPGA configuration.
    if (dev->IsFrontPanelEnabled() == false) {
        cerr << "Opal Kelly FrontPanel support is not enabled in this FPGA configuration." << endl;
		dev.reset(nullptr);
		return(false);
    }

    int boardId, boardVersion;
    dev->UpdateWireOuts();
    boardId = dev->GetWireOutValue(WireOutBoardId);
    boardVersion = dev->GetWireOutValue(WireOutBoardVersion);

    if (boardId != RHYTHM_BOARD_ID) {
        cerr << "FPGA configuration does not support Rhythm.  Incorrect board ID: " << boardId << endl;
        return(false);
    } else {
        LOG(true) << "Rhythm configuration file successfully loaded.  Rhythm version number: " <<
                boardVersion << endl << endl;
    }

    return(true);
}

// Reads system clock frequency from Opal Kelly board (in MHz).  Should be 100 MHz for normal
// Rhythm operation.
double Rhd2000EvalBoard::getSystemClockFreq() const
{
    // Read back the CY22393 PLL configuation
    okCPLL22393 pll;
    dev->GetEepromPLL22393Configuration(pll);

    return pll.GetOutputFrequency(0);
}

/** \brief Initializes Rhythm FPGA registers to default values.
 */
void Rhd2000EvalBoard::initialize()
{
    resetBoard();
    setSampleRate(SampleRate30000Hz);
    selectAuxCommandBankAllPorts(AuxCmd1, 0);
    selectAuxCommandBankAllPorts(AuxCmd2, 0);
    selectAuxCommandBankAllPorts(AuxCmd3, 0);
    selectAuxCommandLength(AuxCmd1, 0, 0);
    selectAuxCommandLength(AuxCmd2, 0, 0);
    selectAuxCommandLength(AuxCmd3, 0, 0);
    setContinuousRunMode(true);
    setMaxTimeStep(4294967295);  // 4294967295 == (2^32 - 1)

    setCableLengthFeet(PortA, 3.0);  // assume 3 ft cables
    setCableLengthFeet(PortB, 3.0);
    setCableLengthFeet(PortC, 3.0);
    setCableLengthFeet(PortD, 3.0);

    setDspSettle(false);

    setDataSource(0, PortA1);
    setDataSource(1, PortB1);
    setDataSource(2, PortC1);
    setDataSource(3, PortD1);
    setDataSource(4, PortA2);
    setDataSource(5, PortB2);
    setDataSource(6, PortC2);
    setDataSource(7, PortD2);

    enableDataStream(0, true);        // start with only one data stream enabled
    for (unsigned int i = 1; i < MAX_NUM_DATA_STREAMS; i++) {
        enableDataStream(i, false);
    }

    clearTtlOut();

    for (unsigned int i = 0; i < NUM_BOARD_ANALOG_OUTPUTS; i++) {
        enableDac(i, false);
        selectDacDataStream(i, 0);
        selectDacDataChannel(i, 0);
    }

    setDacManual(32768);    // midrange value = 0 V

    setDacGain(0);
    setAudioNoiseSuppress(0);

    setTtlMode(1);          // Digital outputs 0-7 are DAC comparators; 8-15 under manual control

    setDacThreshold(0, 32768, true);
    setDacThreshold(1, 32768, true);
    setDacThreshold(2, 32768, true);
    setDacThreshold(3, 32768, true);
    setDacThreshold(4, 32768, true);
    setDacThreshold(5, 32768, true);
    setDacThreshold(6, 32768, true);
    setDacThreshold(7, 32768, true);

    enableExternalFastSettle(false);
    setExternalFastSettleChannel(0);

    enableExternalDigOut(PortA, false);
    enableExternalDigOut(PortB, false);
    enableExternalDigOut(PortC, false);
    enableExternalDigOut(PortD, false);
    setExternalDigOutChannel(PortA, 0);
    setExternalDigOutChannel(PortB, 0);
    setExternalDigOutChannel(PortC, 0);
    setExternalDigOutChannel(PortD, 0);
}

/**
	\brief Sets the per-channel sampling rate of the RHD2000 chips connected to the Rhythm FPGA. 

	@param[in] newSampleRate	Requested sampling rate, specified via the AmplifierSampleRate enumeration.

	@return false if an unsupported sampling rate is requested. 
*/
bool Rhd2000EvalBoard::setSampleRate(AmplifierSampleRate newSampleRate)
{
    // Assuming a 100 MHz reference clock is provided to the FPGA, the programmable FPGA clock frequency
    // is given by:
    //
    //       FPGA internal clock frequency = 100 MHz * (M/D) / 2
    //
    // M and D are "multiply" and "divide" integers used in the FPGA's digital clock manager (DCM) phase-
    // locked loop (PLL) frequency synthesizer, and are subject to the following restrictions:
    //
    //                M must have a value in the range of 2 - 256
    //                D must have a value in the range of 1 - 256
    //                M/D must fall in the range of 0.05 - 3.33
    //
    // (See pages 85-86 of Xilinx document UG382 "Spartan-6 FPGA Clocking Resources" for more details.)
    //
    // This variable-frequency clock drives the state machine that controls all SPI communication
    // with the RHD2000 chips.  A complete SPI cycle (consisting of one CS pulse and 16 SCLK pulses)
    // takes 80 clock cycles.  The SCLK period is 4 clock cycles; the CS pulse is high for 14 clock
    // cycles between commands.
    //
    // Rhythm samples all 32 channels and then executes 3 "auxiliary" commands that can be used to read
    // and write from other registers on the chip, or to sample from the temperature sensor or auxiliary ADC
    // inputs, for example.  Therefore, a complete cycle that samples from each amplifier channel takes
    // 80 * (32 + 3) = 80 * 35 = 2800 clock cycles.
    //
    // So the per-channel sampling rate of each amplifier is 2800 times slower than the clock frequency.
    //
    // Based on these design choices, we can use the following values of M and D to generate the following
    // useful amplifier sampling rates for electrophsyiological applications:
    //
    //   M    D     clkout frequency    per-channel sample rate     per-channel sample period
    //  ---  ---    ----------------    -----------------------     -------------------------
    //    7  125          2.80 MHz               1.00 kS/s                 1000.0 usec = 1.0 msec
    //    7  100          3.50 MHz               1.25 kS/s                  800.0 usec
    //   21  250          4.20 MHz               1.50 kS/s                  666.7 usec
    //   14  125          5.60 MHz               2.00 kS/s                  500.0 usec
    //   35  250          7.00 MHz               2.50 kS/s                  400.0 usec
    //   21  125          8.40 MHz               3.00 kS/s                  333.3 usec
    //   14   75          9.33 MHz               3.33 kS/s                  300.0 usec
    //   28  125         11.20 MHz               4.00 kS/s                  250.0 usec
    //    7   25         14.00 MHz               5.00 kS/s                  200.0 usec
    //    7   20         17.50 MHz               6.25 kS/s                  160.0 usec
    //  112  250         22.40 MHz               8.00 kS/s                  125.0 usec
    //   14   25         28.00 MHz              10.00 kS/s                  100.0 usec
    //    7   10         35.00 MHz              12.50 kS/s                   80.0 usec
    //   21   25         42.00 MHz              15.00 kS/s                   66.7 usec
    //   28   25         56.00 MHz              20.00 kS/s                   50.0 usec
    //   35   25         70.00 MHz              25.00 kS/s                   40.0 usec
    //   42   25         84.00 MHz              30.00 kS/s                   33.3 usec
    //
    // To set a new clock frequency, assert new values for M and D (e.g., using okWireIn modules) and
    // pulse DCM_prog_trigger high (e.g., using an okTriggerIn module).  If this module is reset, it
    // reverts to a per-channel sampling rate of 30.0 kS/s.

    unsigned long M, D;

    switch (newSampleRate) {
    case SampleRate1000Hz:
        M = 7;
        D = 125;
        break;
    case SampleRate1250Hz:
        M = 7;
        D = 100;
        break;
    case SampleRate1500Hz:
        M = 21;
        D = 250;
        break;
    case SampleRate2000Hz:
        M = 14;
        D = 125;
        break;
    case SampleRate2500Hz:
        M = 35;
        D = 250;
        break;
    case SampleRate3000Hz:
        M = 21;
        D = 125;
        break;
    case SampleRate3333Hz:
        M = 14;
        D = 75;
        break;
    case SampleRate4000Hz:
        M = 28;
        D = 125;
        break;
    case SampleRate5000Hz:
        M = 7;
        D = 25;
        break;
    case SampleRate6250Hz:
        M = 7;
        D = 20;
        break;
    case SampleRate8000Hz:
        M = 112;
        D = 250;
        break;
    case SampleRate10000Hz:
        M = 14;
        D = 25;
        break;
    case SampleRate12500Hz:
        M = 7;
        D = 10;
        break;
    case SampleRate15000Hz:
        M = 21;
        D = 25;
        break;
    case SampleRate20000Hz:
        M = 28;
        D = 25;
        break;
    case SampleRate25000Hz:
        M = 35;
        D = 25;
        break;
    case SampleRate30000Hz:
        M = 42;
        D = 25;
        break;
    default:
        return(false);
    }

    sampleRate = newSampleRate;

    // Wait for DcmProgDone = 1 before reprogramming clock synthesizer
    while (isDcmProgDone() == false) {}

    // Reprogram clock synthesizer
    dev->SetWireInValue(WireInDataFreqPll, (256 * M + D));
    dev->UpdateWireIns();
    dev->ActivateTriggerIn(TrigInDcmProg, 0);

    // Wait for DataClkLocked = 1 before allowing data acquisition to continue
    while (isDataClockLocked() == false) {}

    return(true);
}

/** \brief Converts the input enum sampling rate to Hz.

	@param sampleRate	Sampling rate to convert (as an enumerated value).

	@returns Sampling rate in Hz.
*/
double Rhd2000EvalBoard::convertSampleRate(AmplifierSampleRate sampleRate) {
	switch (sampleRate) {
		case SampleRate1000Hz:
			return 1000.0;
		case SampleRate1250Hz:
			return 1250.0;
		case SampleRate1500Hz:
			return 1500.0;
		case SampleRate2000Hz:
			return 2000.0;
		case SampleRate2500Hz:
			return 2500.0;
		case SampleRate3000Hz:
			return 3000.0;
		case SampleRate3333Hz:
			return (10000.0 / 3.0);
		case SampleRate4000Hz:
			return 4000.0;
		case SampleRate5000Hz:
			return 5000.0;
		case SampleRate6250Hz:
			return 6250.0;
		case SampleRate8000Hz:
			return 8000.0;
		case SampleRate10000Hz:
			return 10000.0;
		case SampleRate12500Hz:
			return 12500.0;
		case SampleRate15000Hz:
			return 15000.0;
		case SampleRate20000Hz:
			return 20000.0;
		case SampleRate25000Hz:
			return 25000.0;
		case SampleRate30000Hz:
			return 30000.0;
		default:
			return -1.0;
	}
}

/** \brief Returns the current per-channel sampling rate (in Hz) as a floating-point number.

	@returns Sampling rate in Hz.
*/
double Rhd2000EvalBoard::getSampleRate() const
{
	return convertSampleRate(sampleRate);
}

/** \brief Returns the current per-channel sampling rate as an AmplifierSampleRate enumeration.
 */
Rhd2000EvalBoard::AmplifierSampleRate Rhd2000EvalBoard::getSampleRateEnum() const
{
    return sampleRate;
}

/** \brief Prints a command list to the console in readable form, for diagnostic purposes.

	@param[in] out		C++ output stream to send the information to (e.g., std::cout)
    @param[in] commandList  A command list (generated by an instance of the Rhd2000Registers class)
 */
void Rhd2000EvalBoard::printCommandList(ostream &out, const vector<int> &commandList) const
{
    unsigned int i;
    int cmd, channel, reg, data;

    out << endl;
    for (i = 0; i < commandList.size(); ++i) {
        cmd = commandList[i];
        if (cmd < 0 || cmd > 0xffff) {
            out << "  command[" << i << "] = INVALID COMMAND: " << cmd << endl;
        } else if ((cmd & 0xc000) == 0x0000) {
            channel = (cmd & 0x3f00) >> 8;
            out << "  command[" << i << "] = CONVERT(" << channel << ")" << endl;
        } else if ((cmd & 0xc000) == 0xc000) {
            reg = (cmd & 0x3f00) >> 8;
            out << "  command[" << i << "] = READ(" << reg << ")" << endl;
        } else if ((cmd & 0xc000) == 0x8000) {
            reg = (cmd & 0x3f00) >> 8;
            data = (cmd & 0x00ff);
            out << "  command[" << i << "] = WRITE(" << reg << ",";
            out << hex << uppercase << internal << setfill('0') << setw(2) << data << nouppercase << dec;
            out << ")" << endl;
        } else if (cmd == 0x5500) {
            out << "  command[" << i << "] = CALIBRATE" << endl;
        } else if (cmd == 0x6a00) {
            out << "  command[" << i << "] = CLEAR" << endl;
        } else {
            out << "  command[" << i << "] = INVALID COMMAND: ";
            out << hex << uppercase << internal << setfill('0') << setw(4) << cmd << nouppercase << dec;
            out << endl;
        }
    }
    out << endl;
}

/** \brief Uploads a command list to the FPGA.

    Uploads a command list (generated by an instance of the Rhd2000Registers class) to a particular auxiliary command slot 
    and RAM bank (0-15) on the FPGA. Command slots are given using the AuxCmdSlot enumeration

    @param[in] commandList      A list of commands, each of which is created via one of the Rhd2000Registers::createRhd2000Command overloaded methods.
    @param[in] auxCommandSlot   Command slot to which to upload the command.  Given using the AuxCmdSlot enumeration.
    @param[in] bank             RAM bank.  Must be in the range 0-15.
*/
void Rhd2000EvalBoard::uploadCommandList(const vector<int> &commandList, AuxCmdSlot auxCommandSlot, int bank)
{
    unsigned int i;

    if (auxCommandSlot != AuxCmd1 && auxCommandSlot != AuxCmd2 && auxCommandSlot != AuxCmd3) {
        cerr << "Error in Rhd2000EvalBoard::uploadCommandList: auxCommandSlot out of range." << endl;
        return;
    }

    if (bank < 0 || bank > 15) {
        cerr << "Error in Rhd2000EvalBoard::uploadCommandList: bank out of range." << endl;
        return;
    }

    for (i = 0; i < commandList.size(); ++i) {
        dev->SetWireInValue(WireInCmdRamData, commandList[i]);
        dev->SetWireInValue(WireInCmdRamAddr, i);
        dev->SetWireInValue(WireInCmdRamBank, bank);
        dev->UpdateWireIns();
        switch (auxCommandSlot) {
            case AuxCmd1:
                dev->ActivateTriggerIn(TrigInRamWrite, 0);
                break;
            case AuxCmd2:
                dev->ActivateTriggerIn(TrigInRamWrite, 1);
                break;
            case AuxCmd3:
                dev->ActivateTriggerIn(TrigInRamWrite, 2);
                break;
        }
    }
}

/** \brief Selects an auxiliary command slot and bank for a particular SPI port. 

    @param[in] port             The SPI port, specified via the BoardPort enumeration
    @param[in] auxCommandSlot   The auxiliary command slot, specified via the AuxCmdSlot enumeration
    @param[in] bank             RAM bank.  Must be in the range 0-15.
 */
void Rhd2000EvalBoard::selectAuxCommandBank(BoardPort port, AuxCmdSlot auxCommandSlot, int bank)
{
    int bitShift = 0;

    if (auxCommandSlot != AuxCmd1 && auxCommandSlot != AuxCmd2 && auxCommandSlot != AuxCmd3) {
        cerr << "Error in Rhd2000EvalBoard::selectAuxCommandBank: auxCommandSlot out of range." << endl;
        return;
    }
    if (bank < 0 || bank > 15) {
        cerr << "Error in Rhd2000EvalBoard::selectAuxCommandBank: bank out of range." << endl;
        return;
    }

    switch (port) {
    case PortA:
        bitShift = 0;
        break;
    case PortB:
        bitShift = 4;
        break;
    case PortC:
        bitShift = 8;
        break;
    case PortD:
        bitShift = 12;
        break;
    }

    switch (auxCommandSlot) {
    case AuxCmd1:
        dev->SetWireInValue(WireInAuxCmdBank1, bank << bitShift, 0x000f << bitShift);
        break;
    case AuxCmd2:
        dev->SetWireInValue(WireInAuxCmdBank2, bank << bitShift, 0x000f << bitShift);
        break;
    case AuxCmd3:
        dev->SetWireInValue(WireInAuxCmdBank3, bank << bitShift, 0x000f << bitShift);
        break;
    }
    dev->UpdateWireIns();
}

/** \brief Selects an auxiliary command slot and bank for all SPI ports.

@param[in] auxCommandSlot   The auxiliary command slot, specified via the AuxCmdSlot enumeration
@param[in] bank             RAM bank.  Must be in the range 0-15.
*/
void Rhd2000EvalBoard::selectAuxCommandBankAllPorts(AuxCmdSlot auxCommandSlot, int bank) {
    selectAuxCommandBank(BoardPort::PortA, auxCommandSlot, bank);
    selectAuxCommandBank(BoardPort::PortB, auxCommandSlot, bank);
    selectAuxCommandBank(BoardPort::PortC, auxCommandSlot, bank);
    selectAuxCommandBank(BoardPort::PortD, auxCommandSlot, bank);
}

/** \brief Specify a command sequence length and command loop index for a particular auxiliary command slot.

	When a given command list is played, it starts at index 0 and progresses to \p endIndex.  At that point, the
	command list loops, jumping to \p loopIndex and progressing to \p endIndex.  All subsequent iterations also
	start at \p loopIndex.

    @param[in] auxCommandSlot   Auxiliary command slot, specified via the AuxCmdSlot enumeration
    @param[in] loopIndex        Command loop index (0-1023)
    @param[in] endIndex         Command sequence length (0-1023)
 */
void Rhd2000EvalBoard::selectAuxCommandLength(AuxCmdSlot auxCommandSlot, int loopIndex, int endIndex)
{
    if (auxCommandSlot != AuxCmd1 && auxCommandSlot != AuxCmd2 && auxCommandSlot != AuxCmd3) {
        cerr << "Error in Rhd2000EvalBoard::selectAuxCommandLength: auxCommandSlot out of range." << endl;
        return;
    }

    if (loopIndex < 0 || loopIndex > 1023) {
        cerr << "Error in Rhd2000EvalBoard::selectAuxCommandLength: loopIndex out of range." << endl;
        return;
    }

    if (endIndex < 0 || endIndex > 1023) {
        cerr << "Error in Rhd2000EvalBoard::selectAuxCommandLength: endIndex out of range." << endl;
        return;
    }

    switch (auxCommandSlot) {
    case AuxCmd1:
        dev->SetWireInValue(WireInAuxCmdLoop1, loopIndex);
        dev->SetWireInValue(WireInAuxCmdLength1, endIndex);
        break;
    case AuxCmd2:
        dev->SetWireInValue(WireInAuxCmdLoop2, loopIndex);
        dev->SetWireInValue(WireInAuxCmdLength2, endIndex);
        break;
    case AuxCmd3:
        dev->SetWireInValue(WireInAuxCmdLoop3, loopIndex);
        dev->SetWireInValue(WireInAuxCmdLength3, endIndex);
        break;
    }
    dev->UpdateWireIns();
}

/** \brief Reset FPGA.

  This clears all auxiliary command RAM banks, clears the USB FIFO, and resets the per-channel sampling rate to 30.0 kSample/second/channel.
*/
void Rhd2000EvalBoard::resetBoard()
{
    dev->SetWireInValue(WireInResetRun, 0x01, 0x01);
    dev->UpdateWireIns();
    dev->SetWireInValue(WireInResetRun, 0x00, 0x01);
    dev->UpdateWireIns();
}

/** \brief Configures the FPGA to either run continously or stop after a specified number of time steps.

    @param[in] continuousMode
        \li if true, set the FPGA to run continuously once started
        \li if false, run until maxTimeStep is reached.
 */
void Rhd2000EvalBoard::setContinuousRunMode(bool continuousMode)
{
    if (continuousMode) {
        dev->SetWireInValue(WireInResetRun, 0x02, 0x02);
    } else {
        dev->SetWireInValue(WireInResetRun, 0x00, 0x02);
    }
    dev->UpdateWireIns();
}

/** \brief Set maxTimeStep for cases where continuousMode == false.
 */
void Rhd2000EvalBoard::setMaxTimeStep(unsigned int maxTimeStep)
{
    unsigned int maxTimeStepLsb, maxTimeStepMsb;

    maxTimeStepLsb = maxTimeStep & 0x0000ffff;
    maxTimeStepMsb = maxTimeStep & 0xffff0000;

    dev->SetWireInValue(WireInMaxTimeStepLsb, maxTimeStepLsb);
    dev->SetWireInValue(WireInMaxTimeStepMsb, maxTimeStepMsb >> 16);
    dev->UpdateWireIns();
}

/** \brief Starts SPI data acquisition.
 */
void Rhd2000EvalBoard::run()
{
    dev->ActivateTriggerIn(TrigInSpiStart, 0);
}

/** \brief Returns true if the FPGA is currently running SPI data acquisition.
 */
bool Rhd2000EvalBoard::isRunning() const
{
    int value;

    dev->UpdateWireOuts();
    value = dev->GetWireOutValue(WireOutSpiRunning);

    if ((value & 0x01) == 0) {
        return false;
    } else {
        return true;
    }
}

/** \brief Returns the number of 16-bit words in the USB FIFO.  

    The user should never attempt to read more data than the FIFO currently contains, as it is not protected against underflow.

	@returns Number of 16-bit words in the USB FIFO.
 */
unsigned int Rhd2000EvalBoard::numWordsInFifo() const
{
    dev->UpdateWireOuts();
    return (dev->GetWireOutValue(WireOutNumWordsMsb) << 16) + dev->GetWireOutValue(WireOutNumWordsLsb);
}

/** \brief Returns the number of 16-bit words the USB SDRAM FIFO can hold.  

    The FIFO can actually hold a few
    thousand words more than the number returned by this method due to FPGA "mini-FIFOs" interfacing
    with the SDRAM, but this provides a conservative estimate of FIFO capacity.

    @returns The constant value 67,108,864
 */
unsigned int Rhd2000EvalBoard::fifoCapacityInWords()
{
    return FIFO_CAPACITY_WORDS;
}

/** \brief Set the delay for sampling the MISO line on a particular SPI port.

    @param[in] port     SPI port for which to set the sampling delay.  Specified via the BoardPort enumeration.
    @param[in] delay    Delay in integer clock steps, where each clock step is 1/2800 of a per-channel sampling period.

    Cable delay should be updated after any changes are made to the sampling rate, since cable delay calculations are based on the clock period.

    Most users will probably find it more convenient to use setCableLengthMeters() or setCableLengthFeet() instead of using setCableDelay() directly.
 */
void Rhd2000EvalBoard::setCableDelay(BoardPort port, int delay)
{
    int bitShift = 0;

    if (delay < 0 || delay > 15) {
        cerr << "Warning in Rhd2000EvalBoard::setCableDelay: delay out of range: " << delay  << endl;
    }

    if (delay < 0) delay = 0;
    if (delay > 15) delay = 15;

    switch (port) {
    case PortA:
        bitShift = 0;
        cableDelay[0] = delay;
        break;
    case PortB:
        bitShift = 4;
        cableDelay[1] = delay;
        break;
    case PortC:
        bitShift = 8;
        cableDelay[2] = delay;
        break;
    case PortD:
        bitShift = 12;
        cableDelay[3] = delay;
        break;
    default:
        cerr << "Error in RHD2000EvalBoard::setCableDelay: unknown port." << endl;
    }

    dev->SetWireInValue(WireInMisoDelay, delay << bitShift, 0x000f << bitShift);
    dev->UpdateWireIns();
}

/** \brief Set the delay for sampling the MISO line on a particular SPI port, based on the length of the cable.

    @param[in] port                 SPI port, specified via the BoardPort enumeration
    @param[in] lengthInMeters   length of the cable between the FPGA and the RHD2000 chip (in meters).

    Cable delay must be updated after sampleRate is changed, since cable delay calculations are
    based on the clock frequency!
*/
void Rhd2000EvalBoard::setCableLengthMeters(BoardPort port, double lengthInMeters)
{
    int delay;
    double tStep, cableVelocity, distance, timeDelay;
    const double speedOfLight = 299792458.0;  // units = meters per second
    const double xilinxLvdsOutputDelay = 1.9e-9;    // 1.9 ns Xilinx LVDS output pin delay
    const double xilinxLvdsInputDelay = 1.4e-9;     // 1.4 ns Xilinx LVDS input pin delay
    const double rhd2000Delay = 9.0e-9;             // 9.0 ns RHD2000 SCLK-to-MISO delay
    const double misoSettleTime = 6.7e-9;           // 6.7 ns delay after MISO changes, before we sample it

    tStep = 1.0 / (2800.0 * getSampleRate());  // data clock that samples MISO has a rate 35 x 80 = 2800x higher than the sampling rate
    // cableVelocity = 0.67 * speedOfLight;  // propogation velocity on cable: version 1.3 and earlier
    cableVelocity = 0.555 * speedOfLight;  // propogation velocity on cable: version 1.4 improvement based on cable measurements
    distance = 2.0 * lengthInMeters;      // round trip distance data must travel on cable
    timeDelay = (distance / cableVelocity) + xilinxLvdsOutputDelay + rhd2000Delay + xilinxLvdsInputDelay + misoSettleTime;

    delay = (int) floor(((timeDelay / tStep) + 1.0) + 0.5);

    if (delay < 1) delay = 1;   // delay of zero is too short (due to I/O delays), even for zero-length cables

    setCableDelay(port, delay);
}

/** \brief Set the delay for sampling the MISO line on a particular SPI port, based on the length of the cable.

    @param[in] port           SPI port, specified via the BoardPort enumeration
    @param[in] lengthInFeet   length of the cable between the FPGA and the RHD2000 chip (in feet).

    Cable delay must be updated after sampleRate is changed, since cable delay calculations are
    based on the clock frequency!
*/
void Rhd2000EvalBoard::setCableLengthFeet(BoardPort port, double lengthInFeet)
{
    setCableLengthMeters(port, 0.3048 * lengthInFeet);   // convert feet to meters
}

/** \brief Estimate the cable length that would correspond to a particular delay setting for use in setCableDelay(BoardPort port, int delay).

    Note: the calculation depends on sample rate, since cable delay calculations are
    based on the clock frequency!

    @param[in] delay    Delay in integer clock steps, where each clock step is 1/2800 of a per-channel sampling period.

    @returns    Estimated cable length (in meters).
*/
double Rhd2000EvalBoard::estimateCableLengthMeters(int delay) const
{
    double tStep, cableVelocity, distance;
    const double speedOfLight = 299792458.0;  // units = meters per second
    const double xilinxLvdsOutputDelay = 1.9e-9;    // 1.9 ns Xilinx LVDS output pin delay
    const double xilinxLvdsInputDelay = 1.4e-9;     // 1.4 ns Xilinx LVDS input pin delay
    const double rhd2000Delay = 9.0e-9;             // 9.0 ns RHD2000 SCLK-to-MISO delay
    const double misoSettleTime = 6.7e-9;           // 6.7 ns delay after MISO changes, before we sample it

    tStep = 1.0 / (2800.0 * getSampleRate());  // data clock that samples MISO has a rate 35 x 80 = 2800x higher than the sampling rate
    // cableVelocity = 0.67 * speedOfLight;  // propogation velocity on cable: version 1.3 and earlier
    cableVelocity = 0.555 * speedOfLight;  // propogation velocity on cable: version 1.4 improvement based on cable measurements

    // distance = cableVelocity * (delay * tStep - (xilinxLvdsOutputDelay + rhd2000Delay + xilinxLvdsInputDelay));  // version 1.3 and earlier
    distance = cableVelocity * ((((double) delay) - 1.0) * tStep - (xilinxLvdsOutputDelay + rhd2000Delay + xilinxLvdsInputDelay + misoSettleTime));  // version 1.4 improvement
    if (distance < 0.0) distance = 0.0;

    return (distance / 2.0);
}

/** \brief Estimate the cable length that would correspond to a particular delay setting for use in setCableDelay(BoardPort port, int delay).

    Note: the calculation depends on sample rate, since cable delay calculations are
    based on the clock frequency!

    @param[in] delay    Delay in integer clock steps, where each clock step is 1/2800 of a per-channel sampling period.

    @returns    Estimated cable length (in feet).
*/
double Rhd2000EvalBoard::estimateCableLengthFeet(int delay) const
{
    return 3.2808 * estimateCableLengthMeters(delay);
}

/** \brief Turn on or off DSP settle function in the FPGA.

    This only executes when CONVERT commands are executed by the RHD2000.

	@param[in] enabled	true = enable, false = disable
 */
void Rhd2000EvalBoard::setDspSettle(bool enabled)
{
    dev->SetWireInValue(WireInResetRun, (enabled ? 0x04 : 0x00), 0x04);
    dev->UpdateWireIns();
}

/** \brief Assigns a particular data source to one of the eight available USB data streams.

    @param[in] stream       One of the eight available USB data streams (0-7).
    @param[in] dataSource   Data source, specified using the BoardDataSource enumeration.
*/
void Rhd2000EvalBoard::setDataSource(int stream, BoardDataSource dataSource)
{
    int bitShift;
    OkEndPoint endPoint;

    if (stream < 0 || stream > 7) {
        cerr << "Error in Rhd2000EvalBoard::setDataSource: stream out of range." << endl;
        return;
    }

    dataSources[stream] = dataSource;

    switch (stream) {
    case 0:
        endPoint = WireInDataStreamSel1234;
        bitShift = 0;
        break;
    case 1:
        endPoint = WireInDataStreamSel1234;
        bitShift = 4;
        break;
    case 2:
        endPoint = WireInDataStreamSel1234;
        bitShift = 8;
        break;
    case 3:
        endPoint = WireInDataStreamSel1234;
        bitShift = 12;
        break;
    case 4:
        endPoint = WireInDataStreamSel5678;
        bitShift = 0;
        break;
    case 5:
        endPoint = WireInDataStreamSel5678;
        bitShift = 4;
        break;
    case 6:
        endPoint = WireInDataStreamSel5678;
        bitShift = 8;
        break;
    case 7:
        endPoint = WireInDataStreamSel5678;
        bitShift = 12;
        break;
    }

    dev->SetWireInValue(endPoint, dataSource << bitShift, 0x000f << bitShift);
    dev->UpdateWireIns();
}

/** \brief Returns the data source attached to the given stream.

    @param[in] stream       One of the eight available USB data streams (0-7).

    @return   Data source for the given stream
*/
Rhd2000EvalBoard::BoardDataSource Rhd2000EvalBoard::getDataSource(int stream) {
    if (stream < 0 || stream > 7) {
        cerr << "Error in Rhd2000EvalBoard::getDataSource: stream out of range." << endl;
        return BoardDataSource::PortA1;
    }

    return dataSources[stream];
}

/** \brief Enable or disable one of the eight available USB data streams.

    @param[in] stream       One of the eight available USB data streams (0-7).
    @param[in] enabled      True = enable, False = disable
 */
void Rhd2000EvalBoard::enableDataStream(unsigned int stream, bool enabled)
{
    if (stream > (MAX_NUM_DATA_STREAMS - 1)) {
        cerr << "Error in Rhd2000EvalBoard::setDataSource: stream out of range." << endl;
        return;
    }

    if (enabled) {
        if (!dataStreamEnabled[stream]) {
            dev->SetWireInValue(WireInDataStreamEn, 0x0001 << stream, 0x0001 << stream);
            dev->UpdateWireIns();
            dataStreamEnabled[stream] = true;
            ++numDataStreams;
        }
    } else {
        if (dataStreamEnabled[stream]) {
            dev->SetWireInValue(WireInDataStreamEn, 0x0000 << stream, 0x0001 << stream);
            dev->UpdateWireIns();
            dataStreamEnabled[stream] = false;
            numDataStreams--;
        }
    }
}

/** \brief Indicates whether one of the eight available USB data streams is enabled or disabled.

    @param[in] stream       One of the eight available USB data streams (0-7).

    @returns                  True = enabled, False = disabled
*/
bool Rhd2000EvalBoard::isDataStreamEnabled(unsigned int stream)
{
    if (stream >(MAX_NUM_DATA_STREAMS - 1)) {
        cerr << "Error in Rhd2000EvalBoard::setDataSource: stream out of range." << endl;
        return false;
    }

    return dataStreamEnabled[stream];
}

/** \brief Returns the total number of enabled USB data streams.
 */
int Rhd2000EvalBoard::getNumEnabledDataStreams() const
{
    return numDataStreams;
}

/** \brief Set all 16 bits of the digital TTL output lines on the FPGA to zero.
 */
void Rhd2000EvalBoard::clearTtlOut()
{
    dev->SetWireInValue(WireInTtlOut, 0x0000);
    dev->UpdateWireIns();
}

/** \brief Sets the 16 bits of the digital TTL output lines.

    @param[in] ttlOutArray  Values to set.  This array should be length 16 and should contain values of 0 or 1.
*/
void Rhd2000EvalBoard::setTtlOut(int ttlOutArray[])
{
    int ttlOut = 0;
    for (unsigned int i = 0; i < NUM_DIGITAL_OUTPUTS; ++i) {
        if (ttlOutArray[i] > 0)
            ttlOut += 1 << i;
    }
    dev->SetWireInValue(WireInTtlOut, ttlOut);
    dev->UpdateWireIns();
}

/** \brief Reads the 16 bits of the digital TTL input lines on the FPGA into a length-16 integer array.

    @param[out] ttlInArray  Array of length 16 to receive the values.
 */
void Rhd2000EvalBoard::getTtlIn(int ttlInArray[])
{
    dev->UpdateWireOuts();
    int ttlIn = dev->GetWireOutValue(WireOutTtlIn);

    for (unsigned int i = 0; i < NUM_DIGITAL_INPUTS; ++i) {
        ttlInArray[i] = 0;
        if ((ttlIn & (1 << i)) > 0)
            ttlInArray[i] = 1;
    }
}

/** \brief Set manual value for DACs.

    Sets the manual AD5662 DAC control (DacManual) WireIn to value.
    
    @param[in] value Value to set the ADC to (0-65536).
*/
void Rhd2000EvalBoard::setDacManual(int value)
{
    if (value < 0 || value > 65535) {
        cerr << "Error in Rhd2000EvalBoard::setDacManual: value out of range." << endl;
        return;
    }

    dev->SetWireInValue(WireInDacManual, value);
    dev->UpdateWireIns();
}

/** \brief Sets the eight red LEDs on the Opal Kelly XEM6010 board.

    @param[in] ledArray     A length-8 integer array.  0 = LED off, 1 = LED on. 
    \li ledArray[0] is the LED closest to the power LED
    \li ledArray[7] is the one closest to the edge of the board.
 */
void Rhd2000EvalBoard::setLedDisplay(int ledArray[])
{
    int ledOut = 0;
    for (unsigned int i = 0; i < NUM_LEDS; ++i) {
        if (ledArray[i] > 0)
            ledOut += 1 << i;
    }
    dev->SetWireInValue(WireInLedDisplay, ledOut);
    dev->UpdateWireIns();
}

/** \brief Enables or disables AD5662 DACs connected to the FPGA.

    @param[in] dacChannel   DAC channel (0-7)
    @param[in] enabled      true = enable, false = disable
*/
void Rhd2000EvalBoard::enableDac(int dacChannel, bool enabled)
{
    if (dacChannel < 0 || dacChannel > 7) {
        cerr << "Error in Rhd2000EvalBoard::enableDac: dacChannel out of range." << endl;
        return;
    }

    switch (dacChannel) {
    case 0:
        dev->SetWireInValue(WireInDacSource1, (enabled ? 0x0200 : 0x0000), 0x0200);
        break;
    case 1:
        dev->SetWireInValue(WireInDacSource2, (enabled ? 0x0200 : 0x0000), 0x0200);
        break;
    case 2:
        dev->SetWireInValue(WireInDacSource3, (enabled ? 0x0200 : 0x0000), 0x0200);
        break;
    case 3:
        dev->SetWireInValue(WireInDacSource4, (enabled ? 0x0200 : 0x0000), 0x0200);
        break;
    case 4:
        dev->SetWireInValue(WireInDacSource5, (enabled ? 0x0200 : 0x0000), 0x0200);
        break;
    case 5:
        dev->SetWireInValue(WireInDacSource6, (enabled ? 0x0200 : 0x0000), 0x0200);
        break;
    case 6:
        dev->SetWireInValue(WireInDacSource7, (enabled ? 0x0200 : 0x0000), 0x0200);
        break;
    case 7:
        dev->SetWireInValue(WireInDacSource8, (enabled ? 0x0200 : 0x0000), 0x0200);
        break;
    }
    dev->UpdateWireIns();
}

/** \brief Scales the digital signals to all eight AD5662 DACs.

    The gain is scaled by a factor of 2<SUP>gain</SUP>.

    @param[in] gain Factor for gain scaling (0-7).  See above for explanation.    
 */
void Rhd2000EvalBoard::setDacGain(int gain)
{
    if (gain < 0 || gain > 7) {
        cerr << "Error in Rhd2000EvalBoard::setDacGain: gain out of range." << endl;
        return;
    }

    dev->SetWireInValue(WireInResetRun, gain << 13, 0xe000);
    dev->UpdateWireIns();
}

/** \brief Sets the noise slicing region for audio left and right.

    This improves the audibility of weak neural spikes in noisy waveforms.

    @param[in] noiseSuppress    Degree of noise suppression (0-127).


    The noise slicing region of the DAC channels is set to between -16 x \p noiseSuppress x <b>step</b> and +16 x \p noiseSuppress x <b>step</b>, 
    where <b>step</b> is the minimum step size of the DACs, corresponding to 0.195 &mu;V.
 */
void Rhd2000EvalBoard::setAudioNoiseSuppress(int noiseSuppress)
{
    if (noiseSuppress < 0 || noiseSuppress > 127) {
        cerr << "Error in Rhd2000EvalBoard::setAudioNoiseSuppress: noiseSuppress out of range." << endl;
        return;
    }

    dev->SetWireInValue(WireInResetRun, noiseSuppress << 6, 0x1fc0);
    dev->UpdateWireIns();
}

/** \brief Assigns a particular data stream to an AD5662 DAC channel.

    @param[in] dacChannel       AD5662 DAC channel (0-7)
    @param[in] stream           Data stream to use.  One of:
                                    \li Data stream (0-7) or 
                                    \li DacManual value (8)
*/
void Rhd2000EvalBoard::selectDacDataStream(int dacChannel, int stream)
{
    if (dacChannel < 0 || dacChannel > 7) {
        cerr << "Error in Rhd2000EvalBoard::selectDacDataStream: dacChannel out of range." << endl;
        return;
    }

    if (stream < 0 || stream > 9) {
        cerr << "Error in Rhd2000EvalBoard::selectDacDataStream: stream out of range." << endl;
        return;
    }

    switch (dacChannel) {
    case 0:
        dev->SetWireInValue(WireInDacSource1, stream << 5, 0x01e0);
        break;
    case 1:
        dev->SetWireInValue(WireInDacSource2, stream << 5, 0x01e0);
        break;
    case 2:
        dev->SetWireInValue(WireInDacSource3, stream << 5, 0x01e0);
        break;
    case 3:
        dev->SetWireInValue(WireInDacSource4, stream << 5, 0x01e0);
        break;
    case 4:
        dev->SetWireInValue(WireInDacSource5, stream << 5, 0x01e0);
        break;
    case 5:
        dev->SetWireInValue(WireInDacSource6, stream << 5, 0x01e0);
        break;
    case 6:
        dev->SetWireInValue(WireInDacSource7, stream << 5, 0x01e0);
        break;
    case 7:
        dev->SetWireInValue(WireInDacSource8, stream << 5, 0x01e0);
        break;
    }
    dev->UpdateWireIns();
}

/** \brief Assigns a particular data stream to an AD5662 DAC channel.

    @param[in] dacChannel       AD5662 DAC channel (0-7)
    @param[in] dataChannel      Amplifier channel (0-31)
*/
void Rhd2000EvalBoard::selectDacDataChannel(int dacChannel, int dataChannel)
{
    if (dacChannel < 0 || dacChannel > 7) {
        cerr << "Error in Rhd2000EvalBoard::selectDacDataChannel: dacChannel out of range." << endl;
        return;
    }

    if (dataChannel < 0 || dataChannel > 31) {
        cerr << "Error in Rhd2000EvalBoard::selectDacDataChannel: dataChannel out of range." << endl;
        return;
    }

    switch (dacChannel) {
    case 0:
        dev->SetWireInValue(WireInDacSource1, dataChannel << 0, 0x001f);
        break;
    case 1:
        dev->SetWireInValue(WireInDacSource2, dataChannel << 0, 0x001f);
        break;
    case 2:
        dev->SetWireInValue(WireInDacSource3, dataChannel << 0, 0x001f);
        break;
    case 3:
        dev->SetWireInValue(WireInDacSource4, dataChannel << 0, 0x001f);
        break;
    case 4:
        dev->SetWireInValue(WireInDacSource5, dataChannel << 0, 0x001f);
        break;
    case 5:
        dev->SetWireInValue(WireInDacSource6, dataChannel << 0, 0x001f);
        break;
    case 6:
        dev->SetWireInValue(WireInDacSource7, dataChannel << 0, 0x001f);
        break;
    case 7:
        dev->SetWireInValue(WireInDacSource8, dataChannel << 0, 0x001f);
        break;
    }
    dev->UpdateWireIns();
}

/** \brief Enables or disables real-time control of amplifier fast settle from an external digital input.

    If external triggering is enabled, the fast settling of amplifiers on all connected
    chips will be controlled in real time via one of the 16 TTL inputs.  
    Use setExternalFastSettleChannel(int channel) to select which TTL input.

    The 'fast settle' functionality is also called 'blanking.'

    @param[in] enable   true = enable, false = disable
*/
void Rhd2000EvalBoard::enableExternalFastSettle(bool enable)
{
    dev->SetWireInValue(WireInMultiUse, enable ? 1 : 0);
    dev->UpdateWireIns();
    dev->ActivateTriggerIn(TrigInExtFastSettle, 0);
}

/** \brief Selects which external TTL digital input channel is used to control amplifier fast settle function.

    The fast settle functionality must be enabled with enableExternalFastSettle() to use this functionality.

    The 'fast settle' functionality is also called 'blanking.'

    @param[in] channel  Index of the channel (0-15)
*/
void Rhd2000EvalBoard::setExternalFastSettleChannel(int channel)
{
    if (channel < 0 || channel > 15) {
        cerr << "Error in Rhd2000EvalBoard::setExternalFastSettleChannel: channel out of range." << endl;
        return;
    }

    dev->SetWireInValue(WireInMultiUse, channel);
    dev->UpdateWireIns();
    dev->ActivateTriggerIn(TrigInExtFastSettle, 1);
}

/** \brief Enables or disables real-time control of auxiliary digital output pin (auxout) on RHD2000 chips.

    If external control is enabled, the digital output of all chips connected to a
    selected SPI port will be controlled in real time via one of the 16 TTL inputs.

    Which digital output channel is used to control the auxiliary output is controlled via the setExternalDigOutChannel() method.

    @param[in] port     SPI port to configure
    @param[in] enable   true = enable, false = disable
*/
void Rhd2000EvalBoard::enableExternalDigOut(BoardPort port, bool enable)
{
    dev->SetWireInValue(WireInMultiUse, enable ? 1 : 0);
    dev->UpdateWireIns();

    switch (port) {
    case PortA:
        dev->ActivateTriggerIn(TrigInExtDigOut, 0);
        break;
    case PortB:
        dev->ActivateTriggerIn(TrigInExtDigOut, 1);
        break;
    case PortC:
        dev->ActivateTriggerIn(TrigInExtDigOut, 2);
        break;
    case PortD:
        dev->ActivateTriggerIn(TrigInExtDigOut, 3);
        break;
    default:
        cerr << "Error in Rhd2000EvalBoard::enableExternalDigOut: port out of range." << endl;
    }
}

/** \brief Selects which external TTL digital input channel is used to control the auxiliary digital output pin.

    @param[in] port     SPI port.  This setting will apply to all chips attached to that SPI port.
    @param[in] channel  Digital input channel to use (0-15)
*/
void Rhd2000EvalBoard::setExternalDigOutChannel(BoardPort port, int channel)
{
    if (channel < 0 || channel > 15) {
        cerr << "Error in Rhd2000EvalBoard::setExternalDigOutChannel: channel out of range." << endl;
        return;
    }

    dev->SetWireInValue(WireInMultiUse, channel);
    dev->UpdateWireIns();

    switch (port) {
    case PortA:
        dev->ActivateTriggerIn(TrigInExtDigOut, 4);
        break;
    case PortB:
        dev->ActivateTriggerIn(TrigInExtDigOut, 5);
        break;
    case PortC:
        dev->ActivateTriggerIn(TrigInExtDigOut, 6);
        break;
    case PortD:
        dev->ActivateTriggerIn(TrigInExtDigOut, 7);
        break;
    default:
        cerr << "Error in Rhd2000EvalBoard::setExternalDigOutChannel: port out of range." << endl;
    }
}

/** \brief Enables or disables the optional digital first-order high-pass filters implemented in the FPGA on all eight DAC/comparator output channels. 

    These filters may be used to remove low-frequency local field potential (LFP) signals from neural signals to facilitate
    spike detection while still recording the complete wideband data.

    This is useful when using the low-latency FPGA thresholds to detect spikes and produce digital pulses on the TTL
    outputs, for example.

    @param[in] enable   true = enable, false = disable
 */
void Rhd2000EvalBoard::enableDacHighpassFilter(bool enable)
{
    dev->SetWireInValue(WireInMultiUse, enable ? 1 : 0);
    dev->UpdateWireIns();
    dev->ActivateTriggerIn(TrigInDacHpf, 0);
}

/** \brief Sets a cutoff frequency for optional digital first-order high-pass filters implemented in the FPGA on all eight DAC/comparator channels.

    See enableDacHighpassFilter(bool enable) for uses of these filters.

    @param[in] cutoff   Cutoff frequency (in Hz)
 */
void Rhd2000EvalBoard::setDacHighpassFilter(double cutoff)
{
    double b;
    int filterCoefficient;
    const double pi = 3.1415926535897;

    // Note that the filter coefficient is a function of the amplifier sample rate, so this
    // function should be called after the sample rate is changed.
    b = 1.0 - exp(-2.0 * pi * cutoff / getSampleRate());

    // In hardware, the filter coefficient is represented as a 16-bit number.
    filterCoefficient = (int) floor(65536.0 * b + 0.5);

    if (filterCoefficient < 1) {
        filterCoefficient = 1;
    } else if (filterCoefficient > 65535) {
        filterCoefficient = 65535;
    }

    dev->SetWireInValue(WireInMultiUse, filterCoefficient);
    dev->UpdateWireIns();
    dev->ActivateTriggerIn(TrigInDacHpf, 1);
}

/** \brief Sets a threshold level and trigger polarity for a low-latency FPGA threshold comparator on a DAC channel. 

    Threshold output signals appear on TTL outputs 0-7.  
    To enable the comparator output signals on TTL outputs 0-7, the setTtlMode(int mode) function must be used.

    If the corresponding DAC is disabled, the digital output will always be low.

    @param[in] dacChannel   DAC channel (0 - 7)
    @param[in] threshold    Threshold level (0-65535)
                            Corresponds to the RHD2000 chip ADC output value, where the 'zero' level is 32768 and the step size is 0.195 &mu;V. 
    @param[in] trigPolarity Trigger polarity
                            \li true => RHD2000 signals equaling or rising above the threshold produce a high digital output. 
                            \li false => RHD2000 signals equaling for falling below the threshold produce a high digital output. 
*/
void Rhd2000EvalBoard::setDacThreshold(int dacChannel, int threshold, bool trigPolarity)
{
    if (dacChannel < 0 || dacChannel > 7) {
        cerr << "Error in Rhd2000EvalBoard::setDacThreshold: dacChannel out of range." << endl;
        return;
    }

    if (threshold < 0 || threshold > 65535) {
        cerr << "Error in Rhd2000EvalBoard::setDacThreshold: threshold out of range." << endl;
        return;
    }

    // Set threshold level.
    dev->SetWireInValue(WireInMultiUse, threshold);
    dev->UpdateWireIns();
    dev->ActivateTriggerIn(TrigInDacThresh, dacChannel);

    // Set threshold polarity.
    dev->SetWireInValue(WireInMultiUse, (trigPolarity ? 1 : 0));
    dev->UpdateWireIns();
    dev->ActivateTriggerIn(TrigInDacThresh, dacChannel + 8);
}

/** \brief Sets the TTL digital output mode of the FPGA. 

    @param[in] mode     The mode to use (0 or 1)
                        \li 0 => All 16 TTL digital output pins are controlled manually using setTtlOut()
                        \li 1 => the upper eight TTL outputs are controlled manually; the lower eight TTL outputs are controlled by low-latency threshold comparators connected to the eight waveforms routed to the DACs.
*/
void Rhd2000EvalBoard::setTtlMode(int mode)
{
    if (mode < 0 || mode > 1) {
        cerr << "Error in Rhd2000EvalBoard::setTtlMode: mode out of range." << endl;
        return;
    }

    dev->SetWireInValue(WireInResetRun, mode << 3, 0x0008);
    dev->UpdateWireIns();
}

// Is variable-frequency clock DCM programming done?
bool Rhd2000EvalBoard::isDcmProgDone() const
{
    int value;

    dev->UpdateWireOuts();
    value = dev->GetWireOutValue(WireOutDataClkLocked);

    return ((value & 0x0002) > 1);
}

// Is variable-frequency clock PLL locked?
bool Rhd2000EvalBoard::isDataClockLocked() const
{
    int value;

    dev->UpdateWireOuts();
    value = dev->GetWireOutValue(WireOutDataClkLocked);

    return ((value & 0x0001) > 0);
}

/** \brief Flush all remaining data out of the FIFO.  

    This function should only be called when SPI data acquisition has been stopped.
 */
void Rhd2000EvalBoard::flush()
{
    while (numWordsInFifo() >= usbBufferSize / 2) {
        dev->ReadFromPipeOut(PipeOutData, usbBufferSize, usbBuffer);
    }
    while (numWordsInFifo() > 0) {
        dev->ReadFromPipeOut(PipeOutData, 2 * numWordsInFifo(), usbBuffer);
    }
}

/** \brief Reads a data block from the USB interface, if one is available.

    @param[out] dataBlock   Output Rhd2000DataBlock object.

    @returns true if data block was available.
*/
// TODO: BY:MG - description in PDF show this returning an int
bool Rhd2000EvalBoard::readDataBlock(Rhd2000DataBlock *dataBlock)
{
    unsigned int numBytesToRead;

    numBytesToRead = 2 * dataBlock->calculateDataBlockSizeInWords(numDataStreams);

    if (numBytesToRead > usbBufferSize) {
        setUSBBufferSize(numBytesToRead);
    }

    dev->ReadFromPipeOut(PipeOutData, numBytesToRead, usbBuffer);

    dataBlock->fillFromUsbBuffer(usbBuffer, 0, numDataStreams);

    return true;
}

/** \brief Reads a specified number of data blocks from the USB interface.

    @param[in] numBlocks    Number of blocks requested.
    @param[out] dataQueue   std::deque data structure for storing output.

    @returns true if the requested number of data blocks were available.
*/
bool Rhd2000EvalBoard::readDataBlocks(unsigned int numBlocks, deque<unique_ptr<Rhd2000DataBlock>> &dataQueue)
{
    unsigned int numWordsToRead, numBytesToRead;

    numWordsToRead = numBlocks * Rhd2000DataBlock::calculateDataBlockSizeInWords(numDataStreams);

    if (numWordsInFifo() < numWordsToRead)
        return false;

    numBytesToRead = 2 * numWordsToRead;

    if (numBytesToRead > usbBufferSize) {
        setUSBBufferSize(numBytesToRead);
    }

    dev->ReadFromPipeOut(PipeOutData, numBytesToRead, usbBuffer);

    for (unsigned int i = 0; i < numBlocks; ++i) {
        unique_ptr<Rhd2000DataBlock> dataBlock(new Rhd2000DataBlock(numDataStreams));
        dataBlock->fillFromUsbBuffer(usbBuffer, i, numDataStreams);
        dataQueue.push_back(std::move(dataBlock));
    }

    return true;
}

/** \brief Writes the contents of a data block queue to a binary output stream. 

    @param[in,out] dataQueue    std::deque containing data blocks to be written.  Note that
                                the deque is emptied as a side effect of this operation.
    @param[in] saveOut      The output stream to write to.

    @returns number of data blocks written.
*/
int Rhd2000EvalBoard::queueToFile(deque<unique_ptr<Rhd2000DataBlock>> &dataQueue, ostream &saveOut)
{
    int count = 0;

    while (!dataQueue.empty()) {
        dataQueue.front()->write(saveOut, getNumEnabledDataStreams());
        dataQueue.pop_front();
        ++count;
    }

    return count;
}

// Return name of Opal Kelly board based on model code.
string Rhd2000EvalBoard::opalKellyModelName(int model) const
{
    switch (model) {
    case OK_PRODUCT_XEM3001V1:
        return("XEM3001V1");
    case OK_PRODUCT_XEM3001V2:
        return("XEM3001V2");
    case OK_PRODUCT_XEM3010:
        return("XEM3010");
    case OK_PRODUCT_XEM3005:
        return("XEM3005");
    case OK_PRODUCT_XEM3001CL:
        return("XEM3001CL");
    case OK_PRODUCT_XEM3020:
        return("XEM3020");
    case OK_PRODUCT_XEM3050:
        return("XEM3050");
    case OK_PRODUCT_XEM9002:
        return("XEM9002");
    case OK_PRODUCT_XEM3001RB:
        return("XEM3001RB");
    case OK_PRODUCT_XEM5010:
        return("XEM5010");
    case OK_PRODUCT_XEM6110LX45:
        return("XEM6110LX45");
    case OK_PRODUCT_XEM6001:
        return("XEM6001");
    case OK_PRODUCT_XEM6010LX45:
        return("XEM6010LX45");
    case OK_PRODUCT_XEM6010LX150:
        return("XEM6010LX150");
    case OK_PRODUCT_XEM6110LX150:
        return("XEM6110LX150");
    case OK_PRODUCT_XEM6006LX9:
        return("XEM6006LX9");
    case OK_PRODUCT_XEM6006LX16:
        return("XEM6006LX16");
    case OK_PRODUCT_XEM6006LX25:
        return("XEM6006LX25");
    case OK_PRODUCT_XEM5010LX110:
        return("XEM5010LX110");
    case OK_PRODUCT_ZEM4310:
        return("ZEM4310");
    case OK_PRODUCT_XEM6310LX45:
        return("XEM6310LX45");
    case OK_PRODUCT_XEM6310LX150:
        return("XEM6310LX150");
    case OK_PRODUCT_XEM6110V2LX45:
        return("XEM6110V2LX45");
    case OK_PRODUCT_XEM6110V2LX150:
        return("XEM6110V2LX150");
    case OK_PRODUCT_XEM6002LX9:
        return("XEM6002LX9");
    case OK_PRODUCT_XEM6310MTLX45:
        return("XEM6310MTLX45");
    case OK_PRODUCT_XEM6320LX130T:
        return("XEM6320LX130T");
    default:
        return("UNKNOWN");
    }
}

/** \brief Return 4-bit "board mode" input.

    Reads four digital inputs pins on the FPGA (see JP3 I/O Connections section) a returns this as an integer. 
    These pins will typically be hard-wired either high or low to encode a 4-bit number that identifies particular properties of the interface board.

	@returns 4-bit "board mode" as described above.
*/
int Rhd2000EvalBoard::getBoardMode() const
{
    int mode;

    dev->UpdateWireOuts();
    mode = dev->GetWireOutValue(WireOutBoardMode);

    LOG(true) << "Board mode: " << mode << endl << endl;

    return mode;
}

/** \brief Returns the serial number of the FPGA.

    This can be used to confirm that communications with the FPGA is working, for example.

    @return serial number of the FPGA.
*/
string Rhd2000EvalBoard::getSerialNumber() const {
    return dev->GetSerialNumber();
}

/** \brief Return FPGA cable delay for selected SPI port.

 @param[in] port SPI port for which to return cable delay

 @returns Cable delay in integer clock steps, where each clock step is 1/2800 of a per-channel sampling period.
 */
int Rhd2000EvalBoard::getCableDelay(BoardPort port) const
{
    switch (port) {
    case PortA:
        return cableDelay[0];
    case PortB:
        return cableDelay[1];
    case PortC:
        return cableDelay[2];
    case PortD:
        return cableDelay[3];
    default:
        cerr << "Error in RHD2000EvalBoard::getCableDelay: unknown port." << endl;
        return -1;
    }
}

/** \brief Return FPGA cable delay for all SPI ports.

 @param[out] delays     Cable delays in integer clock steps, where each clock step is 1/2800 of a per-channel sampling period.
 */
// Note: removing std:: here confuses Doxygen
void Rhd2000EvalBoard::getCableDelay(std::vector<int> &delays) const
{
    if (delays.size() != NUM_PORTS) {
        delays.resize(NUM_PORTS);
    }
    for (unsigned int port = 0; port < NUM_PORTS; ++port) {
        delays[port] = cableDelay[port];
    }
}

/** \brief True if this object has been opened.

    @return true if open() has been called
*/
bool Rhd2000EvalBoard::isOpen() const {
    return dev.get() != nullptr;
}

/** \brief Returns the port corresponding to a given BoardDataSource.

    For example, BoardDataSource 'PortA1Ddr' -> PortA.

    @param[in] source   The BoardDataSource.
    @return Corresponding port
*/
Rhd2000EvalBoard::BoardPort Rhd2000EvalBoard::getPort(BoardDataSource source)
{
    switch (source)
    {
    case Rhd2000EvalBoard::PortA1:
    case Rhd2000EvalBoard::PortA2:
    case Rhd2000EvalBoard::PortA1Ddr:
    case Rhd2000EvalBoard::PortA2Ddr:
        return Rhd2000EvalBoard::PortA;
    case Rhd2000EvalBoard::PortB1:
    case Rhd2000EvalBoard::PortB2:
    case Rhd2000EvalBoard::PortB1Ddr:
    case Rhd2000EvalBoard::PortB2Ddr:
        return Rhd2000EvalBoard::PortB;
    case Rhd2000EvalBoard::PortC1:
    case Rhd2000EvalBoard::PortC2:
    case Rhd2000EvalBoard::PortC1Ddr:
    case Rhd2000EvalBoard::PortC2Ddr:
        return Rhd2000EvalBoard::PortC;
    case Rhd2000EvalBoard::PortD1:
    case Rhd2000EvalBoard::PortD2:
    case Rhd2000EvalBoard::PortD1Ddr:
    case Rhd2000EvalBoard::PortD2Ddr:
        return Rhd2000EvalBoard::PortD;
    default: // Should never happen; cases above cover it
        return Rhd2000EvalBoard::PortA;
    }
}
