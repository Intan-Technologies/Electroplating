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

#ifndef BOARDCONTROL_H
#define BOARDCONTROL_H

#include <memory>
#include "rhd2000config.h"
#include "signalsources.h"
#include "saveformat.h"

struct SaveList;
class SaveFormatWriter;
enum SaveFormat;

/** \brief Provides simplified control of an RHD2000 Evaluation Board system.

    This class provides a simplified interface to the RHD2000 Evaluation Board system.  
    The Rhd2000EvalBoard and Rhd2000Registers classes provide all the primitive operations on the board;
    this class combines those and organizes them by board-level functionality.

    For example, changing the bandwidth on a board requires that registers be changes; this involves
    creating a \ref commandlistsPage "command list" (an operation on the Rhd2000Registers class), then
    uploading that command list (an operation on the Rhd2000EvalBoard class); the BoardControl class
    combines all the appropriate operations to simplify the logic.

    It also provides some "business logic," e.g., how large of a queue to use for a given sampling
    rate.

    The class is organized into a series of member variables and corresponding functions.  Configuration
    is achieved by setting the member variables - this only changes values in memory, 
    then calling the updateXYZ() function to send the updated settings to the board.

    Clicking the figure above will take you to relevant documentation.
    \htmlonly
    <img src="Board.png" alt="" usemap="#Map" />
    <map name="Map" id="Map">
    <area alt="" title="analog_out" href="#analog_out" shape="rect" coords="312,238,395,301" />
    <area alt="" title="digital_out" href="#digital_out" shape="rect" coords="59,243,208,301" />
    <area alt="" title="analog_in" href="#analog_in" shape="rect" coords="300,1,382,50" />
    <area alt="" title="digital_in" href="#digital_in" shape="rect" coords="396,36,449,185" />
    <area alt="" title="ports" href="#cables" shape="rect" coords="122,2,219,41" />
    <area alt="" title="leds" href="#leds" shape="rect" coords="78,167,123,191" />
    </map>
    \endhtmlonly

    \nosubgrouping
 */
class BoardControl {
public:
    BoardControl();
    ~BoardControl();

    /** \brief Callback function to be called during long operations.

        During some of the methods below, significant board communication occurs.  A callback function may
        be passed into those methods, and it will be called while the board is busy.  This can be used to
        keep the user interface responsive, for example.
     */
    typedef void CALLBACK_FUNCTION_IDLE(void);

    /** \name Initialization
        Functionality related to initialization
    */
    //@{
    /** \brief Stores the result of Rhd2000EvalBoard::getBoardMode().

        This value is used in the save files, for instance.
     */
    int evalBoardMode;
    /** \brief Smart pointer to an Rhd2000EvalBoard object.

        Most functionality can be accessed without using this directly; it's available for those functions that can't be.
     */
    std::auto_ptr<Rhd2000EvalBoard> evalBoard;

    void create();
        //virtual int open(okFP_dll_pchar dllPath = nullptr);
        //virtual bool uploadFpgaBitfile(const std::string& filename);
        //virtual void initialize();
    void initializeInterfaceBoard(Rhd2000EvalBoard::AmplifierSampleRate sampleRate, CALLBACK_FUNCTION_IDLE callback);
    void close();
    bool okayToRunBoardCommands();
    //@}

    /** \name LEDs
        \anchor leds
        LED control
    */
    //@{
    /// Configuration object for LEDs on the Opal Kelly board
    Rhd2000Config::LEDControl leds;
    void updateLEDs();
    //@}

    /** \name Digital outputs
        \anchor digital_out
        Digital output control
    */
    //@{
    /// Configuration object for digital outputs on the RHD2000 evaluation board
    Rhd2000Config::DigitalOutputControl digitalOutputs;
    void configure16DigitalOutputs();
    void configure8DigitalOutputs8Comparators();
    void updateDigitalOutputs();
    //@}

    /** \name Analog outputs
        \anchor analog_out
        Analog output control
    */
    //@{
    /// Configuration object for analog outputs on the RHD2000 evaluation board
    Rhd2000Config::AnalogOutputControl analogOutputs;
    void updateAnalogOutputSource(unsigned int dac);
    void updateAnalogOutputSources();
    void updateAnalogOutputCommon();
    void updateDACManual();
    //@}

    /** \name Data streams
        Control of data streams
    */
    //@{
    /// Configuration object for \ref datastreamsPage "data streams"
    Rhd2000Config::DataStreamControl dataStreams;
    void updateDataStreams();
    void enablePhysicalDataStreams();
    //@}

    /** \name Digital input
        \anchor digital_in
        Functionality related to digital inputs on RHD2000 eval board.
    */
    //@{
    void readDigitalInputs(int inputValues[]);
    //@}

    /** \name Cables
        \anchor cables
        Functionality related to cable delays
    */
    //@{
    /// Configuration object for cable delays.
    Rhd2000Config::Cable cables[NUM_PORTS];
    void updateCables();
    //@}

    /** \name Auxiliary Commands
        Simplified control of \ref commandlistsPage "command lists"
    */
    //@{
    /// Configuration object for \ref commandlistsPage "command lists"
    Rhd2000Config::AuxiliaryCommandControl auxCmds;
    void updateCommandSlots();
    //@}

    /** \name Sampling rate
        Sampling rate.
    */
    //@{
    /// Board sampling rate, in Hz.
    double boardSampleRate;
    /// Board sampling rate, as a Rhd2000EvalBoard::AmplifierSampleRate enum.
    Rhd2000EvalBoard::AmplifierSampleRate sampleRateEnum;
    void changeSampleRate(Rhd2000EvalBoard::AmplifierSampleRate sampleRate);
    static unsigned int getNumUsbBlocksToRead(Rhd2000EvalBoard::AmplifierSampleRate sampleRate);
    //@}

    /** \name Auxiliary digital outputs
    Control of auxiliary digital outputs on RHD2xxx chips
    */
    //@{
    /// Configuration object for auxiliary digital outputs on the RHD2xxx chips (i.e., boards connected to ports A, B, C, and D).
    Rhd2000Config::AuxDigitalOutputControl auxDigOutputs;
    void disableAuxDigOut();
    void updateAuxDigOut();
    //@}

    /** \name Bandwidth
        Bandwidth of the on-chip filters.
    */
    //@{
    /// Configuration object for filters on the RHD2xxx chips (i.e., boards connected to ports A, B, C, and D).
    Rhd2000Config::BandWidth bandWidth;
    void updateBandwidth();
    //@}

    /** \name Fast settle
        Controls for the "fast settle" functionality of the chip amplifiers.
    */
    //@{
    /// Configuration object for fast settle on the RHD2xxx chips (i.e., boards connected to ports A, B, C, and D).
    Rhd2000Config::FastSettleControl fastSettle;
    void updateFastSettle();
    //@}

    /** \name Read
        \anchor analog_in
        Reading data from the board
    */
    //@{
    /// Object used in reading data from the board
    Rhd2000Config::ReadControl read;
    int readBlocks();
    //@}

    /** \name Board action controls
        Start/stop board.
    */
    //@{
    void stop();
    void runContinuously();
    void runFixed(unsigned int numTimesteps, CALLBACK_FUNCTION_IDLE callback);
    bool isRunning();
    void flush();
    void resetBoard();
    //@}

    /** \name Impedance
        Controls related to impedance measurements
    */
    //@{
    /// Configuration object for impedance measurements.
    Rhd2000Config::ImpedanceFreq impedance;
    void beginImpedanceMeasurement();
    void beginPlating(int channel);
    void endImpedanceMeasurement();
    //@}

    /** \name Utility
        Utility functions
    */
    //@{
    void getChipIds(CALLBACK_FUNCTION_IDLE callback);
    //@}

    /** \name File output
    */
    //@{
    SignalSources signalSources;
    std::unique_ptr<SaveList> saveList;
    std::unique_ptr<SaveFormatWriter> writer;
    SaveFormatHeaderInfo header;
    void setSaveFormat(SaveFormat format);
    int getNumTempSensors() const;
    void setSignalSources();
    //@}

private:
    unsigned int numUsbBlocksToRead;
    void createOrUpdateAmplifierChannels(Rhd2000Config::DataStreamConfig* datastreamConfig, bool create, int port, int& channel);
    void run60(CALLBACK_FUNCTION_IDLE callback);
 };

#endif // BOARDCONTROL_H
