//----------------------------------------------------------------------------------
// rhd2000evalboard.h
//
// Intan Technoloies RHD2000 Rhythm Interface API
// Rhd2000EvalBoard Class Header File
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

#ifndef RHD2000EVALBOARD_H
#define RHD2000EVALBOARD_H

/// \brief Maximum number of data streams; corresponds to 256 channels.
const unsigned int MAX_NUM_DATA_STREAMS = 8;
/// \brief Maximum number of data sources; corresponds to Port A-D, MISO 1&2
const unsigned int MAX_NUM_BOARD_DATA_SOURCES = 8;
/// \brief Number of ports: A, B, C, D
const unsigned int NUM_PORTS = 4;
/// \brief Number of Auxiliary Command Slots
const unsigned int NUM_AUX_COMMAND_SLOTS = 3;
/// \brief Number of banks per Auxiliary Command Slot
const unsigned int NUM_BANKS = 16;
/// \brief Index of DAC Manual
const unsigned int DAC_MANUAL_INDEX = 8;
/// \brief Number of digital inputs on the evaluation board
const unsigned int NUM_DIGITAL_INPUTS = 16;
/// \brief Number of digital outputs on the evaluation board
const unsigned int NUM_DIGITAL_OUTPUTS = 16;
/// \brief The delay value can be 0..15
const unsigned int NUM_VALID_DELAYS = 16;
/// \brief Number of analog inputs on the evaluation board
const unsigned int NUM_BOARD_ANALOG_INPUTS = 8;
/// \brief Number of analog outputs (DACs) on the evaluation board.  This is also the number of digital outputs that can be run as threshold comparators off of the analog outputs.
const unsigned int NUM_BOARD_ANALOG_OUTPUTS = 8;
/// \brief Number of LEDs on the Opal Kelly board
const unsigned int NUM_LEDS = 8;

#include <vector>
#include <deque>
#include "okFrontPanelDLL.h"
#include <memory>
#include <string>

class okCFrontPanel;
class Rhd2000DataBlock;

/** \file rhd2000evalboard.h
    \brief File containing Rhd2000EvalBoard
*/

/** \brief This class provides access to and control of the Opal Kelly XEM6010 USB/FPGA interface board running the Rhythm interface Verilog code. 
	Only one instance of the Rhd2000EvalBoard object is needed to control a Rhythm-based FPGA interface.
*/
class Rhd2000EvalBoard
{

public:
    int numDataStreams; // total number of data streams currently enabled
    Rhd2000EvalBoard();
    virtual ~Rhd2000EvalBoard();

    /** \name Initialization 
		Functions for initializing the PC-side DLL, the board-side FPGA, and board-side registers.
	 */
    //@{
    virtual bool loadLibrary(okFP_dll_pchar dllPath);
    virtual bool discoverSerialNumbers(std::vector<std::string>& serialNumbers);
    virtual int open();
    virtual int openEx(const std::string& requestedSerialNumber);
    virtual bool uploadFpgaBitfile(const std::string& filename);
    virtual void initialize();
    virtual bool isOpen() const;
    //@}

	/** \brief Sampling rates for the RHD2000-series chips that are supported by the Rhythm API
	*/
	enum AmplifierSampleRate {
        SampleRate1000Hz,
        SampleRate1250Hz,
        SampleRate1500Hz,
        SampleRate2000Hz,
        SampleRate2500Hz,
        SampleRate3000Hz,
        SampleRate3333Hz,
        SampleRate4000Hz,
        SampleRate5000Hz,
        SampleRate6250Hz,
        SampleRate8000Hz,
        SampleRate10000Hz,
        SampleRate12500Hz,
        SampleRate15000Hz,
        SampleRate20000Hz,
        SampleRate25000Hz,
        SampleRate30000Hz
    };

	/** \name Sampling rates 
		Functions for getting/setting the sampling rate.  In addition to controlling the acquisition speed,
		note that some other commands (e.g., the setCableDelay() and related) depend on the sampling rate.
	 */
	//@{
    virtual bool setSampleRate(AmplifierSampleRate newSampleRate);
    virtual double getSampleRate() const;
    virtual AmplifierSampleRate getSampleRateEnum() const;
    //@}

	/** \brief Enumeration for specifying which auxiliary command slot (1-3) a given command applies to.
	*/
	enum AuxCmdSlot {
        AuxCmd1,
        AuxCmd2,
        AuxCmd3
    };

	/** \brief Enumeration for specifying which SPI port (A-D) a given command applies to.
	*/
	enum BoardPort {
        PortA,
        PortB,
        PortC,
        PortD
    };

	/** \name Controlling auxiliary commands 

        This group of commands are used to managed the auxiliary command slots.
    
        Rhythm command lists and the auxiliary command slots are described \ref commandlistsPage "here".
	 */
	//@{
    virtual void uploadCommandList(const std::vector<int> &commandList, AuxCmdSlot auxCommandSlot, int bank);
    virtual void printCommandList(std::ostream &out, const std::vector<int> &commandList) const;
    virtual void selectAuxCommandBank(BoardPort port, AuxCmdSlot auxCommandSlot, int bank);
    virtual void selectAuxCommandBankAllPorts(AuxCmdSlot auxCommandSlot, int bank);
    virtual void selectAuxCommandLength(AuxCmdSlot auxCommandSlot, int loopIndex, int endIndex);
    //@}

    /** \name Board control */
    //@{
    virtual void resetBoard();
    virtual void setContinuousRunMode(bool continuousMode);
    virtual void setMaxTimeStep(unsigned int maxTimeStep);
    virtual void run();
    virtual bool isRunning() const;
    //@}


	/** \name Cable delay configuration 
		The SPI protocol, used by RHD2000 chips, consists of :
		\li a bit being sent from the master (i.e., the FPGA board) to the slave (i.e., the RHD2000 board) - this is called Master Output Slave Input (MOSI)
		\li a bit being sent from the slave (i.e., the RHD2000 board) to the master (i.e., the FPGA board) - this is called Master Input Slave Output (MISO)

		Typically, both bits are read at the same time.  However, with the FPGA and the RHD2000 chip physically separated by a significant distance
		(several feet or meters), there may be a noticeable lag before the MISO input to the FPGA stabilizes.  These functions let you to set the delay
		between when the FPGA writes the MOSI output and reads the MISO input to compensate for that.
		*/
    //@{
    virtual void setCableDelay(BoardPort port, int delay);
    virtual void setCableLengthMeters(BoardPort port, double lengthInMeters);
    virtual void setCableLengthFeet(BoardPort port, double lengthInFeet);
    virtual double estimateCableLengthMeters(int delay) const;
    virtual double estimateCableLengthFeet(int delay) const;
    virtual int getCableDelay(BoardPort port) const;
    virtual void getCableDelay(std::vector<int> &delays) const;
    //@}

    /** \brief Data source from the board.

        The DDR (double data rate) sources are included to support 64-channel RHD2164 chips
        that return MISO data on both the rising and falling edges of SCLK. 
        See the RHD2164 datasheet for more details.
     */
    enum BoardDataSource {
        PortA1 = 0,
        PortA2 = 1,
        PortB1 = 2,
        PortB2 = 3,
        PortC1 = 4,
        PortC2 = 5,
        PortD1 = 6,
        PortD2 = 7,
        PortA1Ddr = 8,
        PortA2Ddr = 9,
        PortB1Ddr = 10,
        PortB2Ddr = 11,
        PortC1Ddr = 12,
        PortC2Ddr = 13,
        PortD1Ddr = 14,
        PortD2Ddr = 15
    };

	/** \name Data stream configuration 

        Data streams are described \ref datastreamsPage "here".

        These commands let you configure data streams.
	 */
	//@{
    virtual void setDataSource(int stream, BoardDataSource dataSource);
    virtual BoardDataSource getDataSource(int stream);
    virtual void enableDataStream(unsigned int stream, bool enabled);
    virtual bool isDataStreamEnabled(unsigned int stream);
    virtual int getNumEnabledDataStreams() const;
    //@}

    /** \name Digital I/Os */
    //@{
    virtual void clearTtlOut();
    virtual void setTtlOut(int ttlOutArray[]);
    virtual void getTtlIn(int ttlInArray[]);
    //@}

    /** \name LEDs */
    //@{
    virtual void setLedDisplay(int ledArray[]);
    //@}

	/** \name Reading/writing data
		These commands are used to read data blocks from the eval board's FIFO and write that data to disk files.
	 */
	//@{
    virtual unsigned int numWordsInFifo() const;
    virtual void flush();
    virtual bool readDataBlock(Rhd2000DataBlock *dataBlock);
    virtual bool readDataBlocks(unsigned int numBlocks, std::deque<std::unique_ptr<Rhd2000DataBlock>> &dataQueue);
    virtual int queueToFile(std::deque<std::unique_ptr<Rhd2000DataBlock>> &dataQueue, std::ostream &saveOut);
	//@}

	/** \name DACs
		These commands control the 8 on-board Digital-to-Analog Converters.  The DACs may be enabled or disabled,
		and may be configured to mimic a certain amplifier input (data stream and channel) or a manual value.
	*/
	//@{
    virtual void setDacManual(int value);
    virtual void enableDac(int dacChannel, bool enabled);
    virtual void setDacGain(int gain);
    virtual void selectDacDataStream(int dacChannel, int stream);
    virtual void selectDacDataChannel(int dacChannel, int dataChannel);
	//@}

	/** \name Threshold comparators
		8 low-latency FPGA threshold comparators can be configured (these are output on digital
		TTL output lines 8-15).
	*/
	//@{
    virtual void enableDacHighpassFilter(bool enable);
    virtual void setDacHighpassFilter(double cutoff);
    virtual void setDacThreshold(int dacChannel, int threshold, bool trigPolarity);
    virtual void setTtlMode(int mode);
	//@}

	/** \name Fast settle
		The fast settle functionality can be triggered with an external digital input
	 */
	//@{
    virtual void enableExternalFastSettle(bool enable);
    virtual void setExternalFastSettleChannel(int channel);
	//@}

	/** \name Auxiliary output
		The auxiliary digital output pin (<b>auxout</b>) on RHD2000 chips can be controlled
		via an external digital input.
	 */
    //@{
    virtual void enableExternalDigOut(BoardPort port, bool enable);
    virtual void setExternalDigOutChannel(BoardPort port, int channel);
    //@}

    /** \name Misc */
    //@{
    virtual void setAudioNoiseSuppress(int noiseSuppress);
    virtual void setDspSettle(bool enabled);
    virtual int getBoardMode() const;
    virtual std::string getSerialNumber() const;
    //@}


    // Static functions

    static unsigned int fifoCapacityInWords();
    static double convertSampleRate(AmplifierSampleRate sampleRate);
    static BoardPort getPort(BoardDataSource source);

private:
    std::auto_ptr<okCFrontPanel> dev;
    AmplifierSampleRate sampleRate;
    //int numDataStreams; // total number of data streams currently enabled
    bool dataStreamEnabled[MAX_NUM_DATA_STREAMS];
    BoardDataSource dataSources[MAX_NUM_DATA_STREAMS];
    std::vector<int> cableDelay;

    // Buffer for reading bytes from USB interface
    unsigned char* usbBuffer;
    unsigned int usbBufferSize;
    void setUSBBufferSize(unsigned int size);

    // Opal Kelly module USB interface endpoint addresses
    enum OkEndPoint {
        WireInResetRun = 0x00,
        WireInMaxTimeStepLsb = 0x01,
        WireInMaxTimeStepMsb = 0x02,
        WireInDataFreqPll = 0x03,
        WireInMisoDelay = 0x04,
        WireInCmdRamAddr = 0x05,
        WireInCmdRamBank = 0x06,
        WireInCmdRamData = 0x07,
        WireInAuxCmdBank1 = 0x08,
        WireInAuxCmdBank2 = 0x09,
        WireInAuxCmdBank3 = 0x0a,
        WireInAuxCmdLength1 = 0x0b,
        WireInAuxCmdLength2 = 0x0c,
        WireInAuxCmdLength3 = 0x0d,
        WireInAuxCmdLoop1 = 0x0e,
        WireInAuxCmdLoop2 = 0x0f,
        WireInAuxCmdLoop3 = 0x10,
        WireInLedDisplay = 0x11,
        WireInDataStreamSel1234 = 0x12,
        WireInDataStreamSel5678 = 0x13,
        WireInDataStreamEn = 0x14,
        WireInTtlOut = 0x15,
        WireInDacSource1 = 0x16,
        WireInDacSource2 = 0x17,
        WireInDacSource3 = 0x18,
        WireInDacSource4 = 0x19,
        WireInDacSource5 = 0x1a,
        WireInDacSource6 = 0x1b,
        WireInDacSource7 = 0x1c,
        WireInDacSource8 = 0x1d,
        WireInDacManual = 0x1e,
        WireInMultiUse = 0x1f,

        TrigInDcmProg = 0x40,
        TrigInSpiStart = 0x41,
        TrigInRamWrite = 0x42,
        TrigInDacThresh = 0x43,
        TrigInDacHpf = 0x44,
        TrigInExtFastSettle = 0x45,
        TrigInExtDigOut = 0x46,

        WireOutNumWordsLsb = 0x20,
        WireOutNumWordsMsb = 0x21,
        WireOutSpiRunning = 0x22,
        WireOutTtlIn = 0x23,
        WireOutDataClkLocked = 0x24,
        WireOutBoardMode = 0x25,
        WireOutBoardId = 0x3e,
        WireOutBoardVersion = 0x3f,

        PipeOutData = 0xa0
    };

	std::string opalKellyModelName(int model) const;
    double getSystemClockFreq() const;

    bool isDcmProgDone() const;
    bool isDataClockLocked() const;

    std::vector<std::string> getSerialNumbers();

    // Helper class to load/unload the library only as needed
    class OpalKellyLibraryHandle {
    public:
        ~OpalKellyLibraryHandle();

        static OpalKellyLibraryHandle* create(okFP_dll_pchar dllPath);

    private:
        OpalKellyLibraryHandle();

        static bool addRef(okFP_dll_pchar dllPath);
        static void decRef();
        static int refCount;
    };

    std::unique_ptr<OpalKellyLibraryHandle> library;
};

#endif // RHD2000EVALBOARD_H
