//----------------------------------------------------------------------------------
// rhd2000registers.h
//
// Intan Technoloies RHD2000 Rhythm Interface API
// Rhd2000Registers Class Header File
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

#ifndef RHD2000REGISTERS_H
#define RHD2000REGISTERS_H

#include <vector>

/** \file rhd2000registers.h
    \brief File containing Rhd2000Registers and Rhd2000RegisterInternals.
*/

/** \brief This namespace consists of structures containing the internal representations of the registers of an RHD2000 chip.

    Much of the information related to these registers can be found in the Intan_RHD2000_series_datasheet.pdf, in the section
    "On-Chip Registers"
 */
namespace Rhd2000RegisterInternals {
    /// An 8-bit register
    typedef unsigned char register_t;
    static_assert(sizeof(register_t) == 1, "register_t typedef is wrong"); // Registers are 1 byte

    /// Register 0: ADC Configuration and Amplifier Fast Settle
    struct register0_t {
        /// Constructor.  Sets default values.
        register0_t();

        /** \brief ADC comparator select
        
            This variable selects between four different comparators that can be used by the ADC. 

            This variable defaults to 2 and should not be changed.
         */
        register_t adcComparatorSelect : 2;

        /** \brief ADC comparator bias
        
            This variable configures the bias current of the ADC comparator.

            Values:
             - 3 (<em>default</em>) for normal operation and ADC calibration. 
             - 0 to reduce power supply current consumption by 80 &mu;A when the ADC will not be used for an extended period of time.
         */
        register_t adcComparatorBias : 2;

        /** \brief amp Vref enable
        
            Values:
             - 1 (<em>default</em>) to power up voltage references used by the biopotential amplifiers. 
             - 0 to reduce power supply current consumption by 180 &mu;A when the amplifiers will not be used for an extended period of time. 
            
            After setting this bit to one, at least 100 &mu;s must elapse before ADC samples are valid, or before ADC calibration is executed.
          */
        register_t ampVrefEnable : 1;

        /** \brief amp fast settle
        
            Setting this bit to one closes a switch in each amplifier that drives its analog output to the baseline "zero" level.
            This can be used to quickly recover from large transient events that may drive the amplifiers to their rails.
            The switch should be closed for a certain amount of time to settle the amplifiers(see "Fast Settle Function" section 
            of the data sheet for details) and then this register should be reset to zero to resume normal amplifier operation. 
         */
        register_t ampFastSettle : 1;

        /** \brief ADC reference BW
        
            This variable configures the bandwidth of an internal ADC reference generator feedback circuit. 
            
            This variable defaults to 3 and should not be changed.
         */
        register_t adcReferenceBw : 2;
    };

    /// Register 1: Supply Sensor and ADC Buffer Bias Current
    struct register1_t {
        /// Constructor.  Sets default values.
        register1_t();

        /** \brief ADC buffer bias
        
            This variable configures the bias current of an internal reference buffer in the ADC.
            This variable should be set via Rhd2000Registers::defineSampleRate, rather than directly.
        */
        register_t adcBufferBias : 6;

        /** \brief VDD sense enable
        
            Values:
                - 1 (<em>default</em>) enables the on-chip supply voltage sensor, whose output may be sampled by the 
                ADC on channel 48 (see "Supply Voltage Sensor" section in the datasheet for details). 
                - 0 Supply voltage is not sampled, reduce current consumption by 10 &mu;A.
        */
        register_t vddSenseEnable : 1;

        /// Unused
        register_t unused : 1;
    };

    /// Register 2: MUX Bias Current
    struct register2_t {
        /// Constructor.  Sets default values.
        register2_t();

        /** \brief MUX bias
        
            This variable configures the bias current of the MUX that routes the selected analog signal to the ADC input. 

            This variable should be set via Rhd2000Registers::defineSampleRate, rather than directly.
          */
        register_t muxBias : 6;

        /// Unused
        register_t unused : 2;
    };

    /** \brief Register 3: MUX Load, Temperature Sensor, and Auxiliary Digital Output

        This register is not sent to an RHD2000 chip via the command list created by
        Rhd2000Registers::createCommandListRegisterConfig.  Instead, the digital outputs are controlled
        via the digital output command list (Rhd2000Registers::createCommandListUpdateDigOut), and the
        temperature sensor is controlled via the temperature sensor command list
        (Rhd2000Registers::createCommandListTempSensor).
     */
    struct register3_t {
        /// Constructor.  Sets default values.
        register3_t();

        /** \brief Digital Output Value
        
            The RHD2000 chips have an auxiliary digital output pin <b>auxout</b> that may be used to activate off-chip circuitry 
            (e.g., MOSFET switches, LEDs, stimulation circuits).

            Internally in the chip, this register bit is used to drive that auxiliary CMOS digital output pin <b>auxout</b>.

            The Rhythm API has a better method for driving <b>auxout</b> in real-time: the 
            first command slot (Rhd2000EvalBoard::AuxCmdSlot::AuxCmd1) is configured with a command list from
            Rhd2000Registers::createCommandListUpdateDigOut, and digital output is enabled via 
            Rhd2000EvalBoard::enableExternalDigOut and configured via Rhd2000EvalBoard::setExternalDigOutChannel.
         */
        register_t digOut : 1;

        /** \brief Set Digital Output to High Impedance mode.
        
            Setting this bit to one puts the digital output into high impedance (HiZ) mode.

            If you are using digital output, you should use the method described in the digOut bit, rather than setting this directly.

            If you are not using digital output, you may set this to 1 to save power.
        */
        register_t digOutHiZ : 1;

        /** \brief Enable Temperature Sensor
        
            Setting this bit to one enables the on-chip temperature sensor. 
            Current consumption may be reduced by approximately 70 &mu;A by setting this bit to zero to disable the sensor.

            Typically you should not set this manually, but rather use a command list from 
            Rhd2000Registers::createCommandListTempSensor to read the temperature sensors.
         */
        register_t tempEn : 1;

        /** \brief tempS1

            This bit is used internally on the chip as part of temperature sensing.
        
            Typically you should not set this manually, but rather use a command list from
            Rhd2000Registers::createCommandListTempSensor to read the temperature sensors.

            If you are not sensing temperatures, you can set this bit and tempS2 to zero to save power.
        */
        register_t tempS1 : 1;

        /** \brief tempS2

            See tempS1.
          */
        register_t tempS2 : 1;

        /** \brief MUX load
        
            This variable configures the total capacitance at the input of the ADC. 
            
            This variable defaults to 0 and should not be changed.
         */
        register_t muxLoad : 3;
    };

    /// Register 4: ADC Output Format and DSP Offset Removal
    struct register4_t {
        /// Constructor.  Sets default values.
        register4_t();

        /** \brief DSP cutoff frequency
        
            This variable sets the cutoff frequency of the DSP filter used to for offset removal. 
            
            You should typically use Rhd2000Registers::setDspCutoffFreq, rather than setting this directly.

            See the "DSP High-Pass Filter for Offset Removal" section of the datasheet for details.
         */
        register_t dspCutoffFreq : 4;

        /** \brief DSP Enable
        
            When this bit is set to one, the RHD2000 performs digital signal processing (DSP) offset removal
            from all 32 amplifier channels using a first-order high-pass IIR filter.

            This value is also set via Rhd2000Registers::enableDsp.
            
            See the "DSP High-Pass Filter for Offset Removal" section of the datasheet for details.
        */
        register_t dspEn : 1;

        /** \brief Absolute Value Mode

            Setting this bit to one passes all amplifier ADC conversions through an absolute value function. 
            This is equivalent to performing full-wave rectification on the signals, and may be useful for 
            implementing symmetric positive/negative thresholds or envelope estimation algorithms. 
            This bit has no effect on ADC conversions from non-amplifier channels (i.e., C > 31). 
            
            See the "Absolute Value Mode" section in the data sheet for more information.

            Note: this bit only applies to amplifier channels, not auxiliary input or other channels.
        */
        register_t absMode : 1;

        /** \brief Twos Complement Mode
        
            If this bit is set to one, amplifier conversions from the ADC are reported 
            using a "signed" two's complement representation where the amplifier baseline is 
            reported as zero and values below baseline are reported as negative numbers. 
            
            If this bit is set to zero, amplifier conversions from the ADC are reported using 
            "unsigned" offset binary notation where the baseline level is represented as 1000000000000000. 
                       
            Note: this bit only applies to amplifier channels.  ADC conversions from non-amplifier channels are always reported as unsigned binary numbers.
          */
        register_t twosComp : 1;

        /** \brief Weak MISO
        
            If this bit is set to zero, the MISO line goes to high impedance mode (HiZ) when CS is pulled high, 
            allowing multiple chips to share the same MISO line, so long as only one of their chip select lines 
            is activated at any time.
            
            If only one RHD2000 chip will be using a MISO line, this bit may be set to one (<em>default</em>), 
            and when CS is pulled high, the MISO line will be driven weakly by the chip. This can prevent the 
            line from drifting to indeterminate values between logic high and logic low.
         */
        register_t weakMiso : 1;
    };

    /** \brief Register 5: Impedance Check Control

        See the "Electrode Impedance Test" section of the datasheet for details.
     */
    struct register5_t {
        /// Constructor.  Sets default values.
        register5_t();

        /** \brief Zcheck enable
        
            Setting this bit to one activates impedance testing mode, and connects the on-chip waveform generator 
            (and pin elec_test) to the amplifier selected by the Zcheck select variable in Register 7. 
            
            This bit can be set via the Rhd2000Registers::enableZcheck method.
        */
        register_t zcheckEn : 1;

        /** \brief Zcheck select polarity 
        
            This bit is only used on the RHD2216 where the biopotential amplifiers have separate positive 
            and negative inputs (instead of a reference input common to all amplifiers). 
            
            This bit can be set via the Rhd2000Registers::setZcheckPolarity method.
        */
        register_t zcheckSelPol : 1;

        /** \brief Zcheck connect all
        
            Values:
                - 0 (<em>default</em>) Normal operation.
                - 1 Connects all electrodes together to the elec_test input pin.  This is only used for applying DC voltages to electroplate electrodes. 
            
            See the "Electrode Activation" section of the datasheet for details.
        */
        register_t zcheckConnAll : 1;

        /** \brief Zcheck scale
        
            This variable selects the series capacitor used to convert the voltage waveform 
            generated by the on-chip DAC into an AC current waveform that stimulates a selected electrode for impedance testing.
            
            These bits are typically set via the Rhd2000Registers::setZcheckScale method, rather than directly.
            
            See the "On-Chip AC Current Waveform Generator" section of the datasheet for more information.
        */
        register_t zcheckScale : 2;

        /** \brief Zcheck load
        
            This bit is used for chip testing at Intan Technologies only. 
        
            This variable defaults to 0 and should not be changed.
        */
        register_t zcheckLoad : 1;

        /** \brief Zcheck DAC power
        
            Values:
                - 1 (<em>default</em>) Activates the on-chip digital-to-analog converter (DAC) used to 
            generate waveforms for electrode impedance measurement. 
                - 0 If impedance testing is not being performed, this bit can be set to zero to reduce current consumption by 120 &mu;A. 

            This bit is typically set via the Rhd2000Registers::setZcheckDacPower method, rather than directly.
            
            See the "On-Chip AC Current Waveform Generator" section of the datasheet for more information.
          */
        register_t zcheckDacPower : 1;

        /// \brief Unused
        register_t unused : 1;
    };

    /** \brief Register 6: Impedance Check DAC

        This register is not sent to an RHD2000 chip via the command list created by
        Rhd2000Registers::createCommandListRegisterConfig.  Instead,  the separate
        Rhd2000Registers::createCommandListZcheckDac should be used for impedance checking.
     */
    struct register6_t {
        /// Constructor.  Sets default values.
        register6_t();

        /** \brief Zcheck DAC
        
            This variable sets the output voltage of an 8-bit DAC used to generate waveforms for impedance checking. 
            This variable must be updated at regular intervals to create the desired waveform. 
            
            Note that this DAC must be enabled by setting Zcheck DAC power in Register 5. 
            If impedance testing is not in progress, the value of this register should remain unchanged to minimize noise
            (although writing the same value to the register is acceptable). 
            
            See the "On-Chip AC Current Waveform Generator" section of the datasheet for more information.
          */
        register_t zcheckDac : 8;
    };

    /// Register 7: Impedance Check Amplifier Select
    struct register7_t {
        /// Constructor.  Sets default values.
        register7_t();

        /** \brief Zcheck select
        
            This variable selects the amplifier whose electrode will be connected to the on-chip impedance
            testing circuitry if Zcheck en is set to one. 
            
            In 16- and 32-amplifier chips, the MSB of this six-bit register is ignored. 
            
            This is typically set via the Rhd2000Registers::setZcheckChannel method, rather than directly.

            See the "Electrode Impedance Test" section of the datasheet for more information.
            */
        register_t zcheckSelect : 6;

        /// Unused
        register_t unused : 2;
    };

    /// Register 8: On-Chip Amplifier Bandwidth Select
    struct register8_t {
        /// Constructor.  Sets default values.
        register8_t();

        /** \brief RH1 DAC1
        
            This variable is one of four used to set the upper cutoff frequency of the biopotential amplifiers.

            This variable should typically be set via the Rhd2000Registers::setUpperBandwidth method, not directly.
        */
        register_t rH1Dac1 : 6;

        /// Unused
        register_t unused : 1;

        /** \brief offchip RH1
        
            Setting this bit to one switches from using an on-chip programmable resistor for setting 
            amplifier upper bandwidth to using external resistor RH1 (connected to pin auxin1) 
            to set amplifier bandwidth.
            
            Tables in the datasheet provide appropriate values for bandwidth-setting resistors.
        */
        register_t offChipRH1 : 1;
    };

    /// Register 9: On-Chip Amplifier Bandwidth Select
    struct register9_t {
        /// Constructor.  Sets default values.
        register9_t();

        /** \brief RH1 DAC2

        This variable is one of four used to set the upper cutoff frequency of the biopotential amplifiers.

        This variable should typically be set via the Rhd2000Registers::setUpperBandwidth method, not directly.
        */
        register_t rH1Dac2 : 5;

        /// Unused
        register_t unused : 2;

        /** \brief ADC aux1 enable

        Values:
            - 1 (<em>default</em>) Activates a buffer that allows pin auxin1 to be used as an auxiliary ADC input (range of 0.10V to 2.45V), corresponding to channel 32.
            - 0 Either
                - register8_t::offchipRH1 is enabled, so the auxin1 pin is being used for that resistor or
                - Don't sample the auxiliary ADC input.

        The auxiliary ADC inputs are sampled in the Rhd2000Registers::createCommandListTempSensor command list.

        See the "Auxiliary ADC Inputs" section of the data sheet for more information.
        */
        register_t adcAux1En : 1;
    };

    /// Register 10: On-Chip Amplifier Bandwidth Select
    struct register10_t {
        /// Constructor.  Sets default values.
        register10_t();

        /** \brief RH2 DAC1

        This variable is one of four used to set the upper cutoff frequency of the biopotential amplifiers.

        This variable should typically be set via the Rhd2000Registers::setUpperBandwidth method, not directly.
        */
        register_t rH2Dac1 : 6;

        /// Unused
        register_t unused : 1;

        /** \brief offchipRH2

        Setting this bit to one switches from using an on-chip programmable resistor for setting
        amplifier upper bandwidth to using external resistor RH2 (connected to pin auxin2)
        to set amplifier bandwidth.

        Tables in the datasheet provide appropriate values for bandwidth-setting resistors.
        */
        register_t offChipRH2 : 1;
    };

    /// Register 11: On-Chip Amplifier Bandwidth Select
    struct register11_t {
        /// Constructor.  Sets default values.
        register11_t();

        /** \brief RH2 DAC2

        This variable is one of four used to set the upper cutoff frequency of the biopotential amplifiers.

        This variable should typically be set via the Rhd2000Registers::setUpperBandwidth method, not directly.
        */
        register_t rH2Dac2 : 5;

        /// Unused
        register_t unused : 2;

        /** \brief ADC aux2 enable

        Values:
            - 1 (<em>default</em>) Activates a buffer that allows pin auxin2 to be used as an auxiliary ADC input (range of 0.10V to 2.45V), corresponding to channel 33.
            - 0 Either
                - register10_t::offchipRH2 is enabled, so the auxin2 pin is being used for that resistor or
                - Don't sample the auxiliary ADC input.

        The auxiliary ADC inputs are sampled in the Rhd2000Registers::createCommandListTempSensor command list.

        See the "Auxiliary ADC Inputs" section of the data sheet for more information.
        */
        register_t adcAux2En : 1;
    };

    /// Register 12: On-Chip Amplifier Bandwidth Select
    struct register12_t {
        /// Constructor.  Sets default values.
        register12_t();

        /** \brief RL DAC1

        This variable is one of three used to set the lower cutoff frequency of the biopotential amplifiers.

        This variable should typically be set via the Rhd2000Registers::setLowerBandwidth method, not directly.
        */
        register_t rLDac1 : 7;

        /** \brief offchip RL

        Setting this bit to one switches from using an on-chip programmable resistor for setting
        amplifier lower bandwidth to using external resistor RL (connected to pin auxin3)
        to set amplifier bandwidth.
        Tables in the datasheet provide appropriate values for bandwidth-setting resistors.
        */
        register_t offChipRL : 1;
    };

    /// Register 13: On-Chip Amplifier Bandwidth Select
    struct register13_t {
        /// Constructor.  Sets default values.
        register13_t();

        /** \brief RL DAC2

        This variable is one of three used to set the lower cutoff frequency of the biopotential amplifiers.

        This variable should typically be set via the Rhd2000Registers::setLowerBandwidth method, not directly.
        */
        register_t rLDac2 : 6;

        /** \brief RL DAC3

        This variable is one of three used to set the lower cutoff frequency of the biopotential amplifiers.

        This variable should typically be set via the Rhd2000Registers::setLowerBandwidth method, not directly.
        */
        register_t rLDac3 : 1;

        /** \brief ADC aux3 enable

            Values:
                - 1 (<em>default</em>) Activates a buffer that allows pin auxin3 to be used as an auxiliary ADC input (range of 0.10V to 2.45V), corresponding to channel 34.
                - 0 Either
                    - register12_t::offchipRL is enabled, so the auxin3 pin is being used for that resistor or
                    - Don't sample the auxiliary ADC input.
        
            The auxiliary ADC inputs are sampled in the Rhd2000Registers::createCommandListTempSensor command list.
        
            See the "Auxiliary ADC Inputs" section of the data sheet for more information.
        */
        register_t adcAux3En : 1;
    };

    /** \brief A set of register values

        This structure contains the entire set of register values on the RHD2000 series chips.  Individual registers are documented separately.
    */
    struct typed_register_t {
        typed_register_t();

        /// Register 0: ADC Configuration and Amplifier Fast Settle
        register0_t r0;
        /// Register 1: Supply Sensor and ADC Buffer Bias Current
        register1_t r1;
        /// Register 2: MUX Bias Current
        register2_t r2;
        /// Register 3: MUX Load, Temperature Sensor, and Auxiliary Digital Output
        register3_t r3;
        /// Register 4: ADC Output Format and DSP Offset Removal
        register4_t r4;
        /// Register 5: Impedance Check Control
        register5_t r5;
        /// Register 6: Impedance Check DAC
        register6_t r6;
        /// Register 7: Impedance Check Amplifier Select
        register7_t r7;

        /** \name Registers 8-13: On-Chip Amplifier Bandwidth Select */
        //@{
        /// Register 8: On-Chip Amplifier Bandwidth Select, highpass filter
        register8_t r8;
        /// Register 9: On-Chip Amplifier Bandwidth Select, highpass filter
        register9_t r9;
        /// Register 10: On-Chip Amplifier Bandwidth Select, highpass filter
        register10_t r10;
        /// Register 11: On-Chip Amplifier Bandwidth Select, highpass filter
        register11_t r11;
        /// Register 12: On-Chip Amplifier Bandwidth Select, lowpass filter
        register12_t r12;
        /// Register 13: On-Chip Amplifier Bandwidth Select, lowpass filter
        register13_t r13;
        //@}

        /** \brief Registers 14-21: Individual Amplifier Power

            Note: RHD2164 uses registers 14-21.  All other chips use registers 14-17 only.

            Setting these bits to zero powers down individual biopotential amplifiers, 
            saving power if there are channels that don’t need to be observed. 
            Each amplifier consumes power in proportion to its upper cutoff frequency. 
            Current consumption is approximately 7.6 &mu;A/kHz per amplifier.
            
            Under normal operation, these bits should be set to one.

            Amplifiers should typically be powered up or down using the Rhd2000Registers::setAmpPowered,
            Rhd2000Registers::powerUpAllAmps, and Rhd2000Registers::powerDownAllAmps methods, rather than
            setting these directly
        */
        register_t aPwr[8];

        /// Registers 22-39: Not used
        register_t blank1[18];

        /** \name Registers 40-63: ROM

        Each RHD2000 chip contains the following ROM registers that provide information on the identity and capabilities of the particular chip.
        */
        //@{
        /** \brief Registers 40-44: Company Designation

        The read-only registers 40-44 contain the characters INTAN in ASCII. 
        The contents of these registers can be read to verify the fidelity of the SPI interface.
        */
        char romCompany[5];

        /// Registers 45-47: Unused
        register_t blank3[3];

        /** \brief Registers 48-56: Chip Name

        The read-only registers 48-56 contain the null-terminated chip name (e.g. "RHD2132\0") in ASCII.
        */
        char romChipName[8];

        /// Registers 56-58: Unused
        register_t blank4[3];

        /** \brief RHD2164 MISO ID numbers from ROM register 59

            See romMisoABMarker for details.
         */
        enum MisoMarker {
            REGISTER_59_MISO_A = 53,
            REGISTER_59_MISO_B = 58
        };


        /** \brief Register 59: MISO A/B Marker

            This read-only variable returns 00110101 (decimal 53) on MISO A and 00111010 (decimal 58) on MISO B. 
            These distinct bytes can be checked by the SPI master device to confirm signal integrity on the SPI bus 
            (e.g., to adjust internal sampling times to compensate for cable propagation delay).
        */
        register_t romMisoABMarker;

        /** \brief Register 60: Die Revision

        This read-only variable encodes a die revision number which is set by Intan Technologies to encode various versions of a chip.
        */
        register_t romDieRevision;

        /** \brief Register 61: Unipolar/Bipolar Amplifiers

            This read-only variable is set to zero if the on-chip biopotential amplifiers have independent differential (bipolar)
            inputs like the RHD2216 chip. It is set to one if the amplifiers have unipolar inputs and a common reference, 
            like the RHD2132 chip or RHD2164 chip.
        */
        register_t romUnipolar;

        /** \brief Register 62: Number of Amplifiers

        This read-only variable encodes the total number of biopotential amplifiers on the chip (e.g., 64).
        */
        register_t romNumAmplifiers;

        /// RHD2000 chip ID numbers from ROM register 63, plus software-only values
        enum ChipId {
            /// No chip.  Either detection hasn't been run yet, or no chip existed on the port.
            CHIP_ID_NONE = 0,

            /// RHD2132, 32-channel unipolar chip
            CHIP_ID_RHD2132 = 1,
            /// RHD2216, 16-channel bipolar chip
            CHIP_ID_RHD2216 = 2,
            /// RHD2164, 64-channel unipolar chip
            CHIP_ID_RHD2164 = 4,
        };

        /** \brief Register 63: Intan Technologies Chip ID

        This read-only variable encodes a unique Intan Technologies ID number indicating the type of chip.
        The chip IDs are encoded by the ChipId enum.
        */
        register_t romChipId;
        //@}

        ChipId deviceId(int &register59Value) const;
    };
    static_assert(sizeof(typed_register_t) == 64, "typed_register_t is missing registers");
}

/** \brief This class creates and manages a data structure representing the internal RAM registers on a RHD2000 chip, 
    and generates command lists to configure the chip and perform other functions. 
	
	Changing the value of variables 
    within an instance of this class does not directly affect a RHD2000 chip connected to the FPGA; rather, 
    a command list must be generated from this class and then downloaded to the FPGA board using 
    Rhd2000EvalBoard::uploadCommandList. 
    
    Typically, one instance of Rhd2000Registers will be created for each RHD2000 chip attached to the 
    Rhythm interface. However, if all chips will receive the same MOSI commands, then only one instance 
    of Rhd2000Registers is required.
 */
class Rhd2000Registers
{
public:
    /** \brief Values of registers

        This variable contains the values of the various registers.  Note that this is an in-memory copy only,
        and that the values are not transferred to the chip(s) until appropriate command lists (created via
        createCommandListRegisterConfig, createCommandListTempSensor, createCommandListUpdateDigOut, and 
        createCommandListZcheckDac) are created, uploaded to the chip via Rhd2000EvalBoard::uploadCommandList,
        selected, and executed.

        Typically, these should be manipulated via the functions in the current class.  The individual register
        types allow more complete manipulation of the registers for functionality not exposed through the current
        class.
    */
    Rhd2000RegisterInternals::typed_register_t registers;


    Rhd2000Registers(double sampleRate);

    void defineSampleRate(double newSampleRate);

    void setFastSettle(bool enabled);

    void setDigOutLow();
    void setDigOutHigh();
    void setDigOutHiZ();

    void enableAux1(bool enabled);
    void enableAux2(bool enabled);
    void enableAux3(bool enabled);

    void enableDsp(bool enabled);
    double setDspCutoffFreq(double newDspCutoffFreq);
    double getDspCutoffFreq() const;

    void enableZcheck(bool enabled);
    void setZcheckDacPower(bool enabled);

    /** Values of the on-chip series capacitor used during impedance testing.
     */
    enum ZcheckCs {
        /** 0.1 pF */ ZcheckCs100fF,
        /**   1 pF */ ZcheckCs1pF,    
        /**  10 pF */ ZcheckCs10pF    
    };

    /** Values used for specifying polarity of inputs to be impedance tested. */
    enum ZcheckPolarity {
        ZcheckPositiveInput,
        ZcheckNegativeInput
    };

    void setZcheckScale(ZcheckCs scale);
    void setZcheckPolarity(ZcheckPolarity polarity);
    int setZcheckChannel(int channel);
    static double getCapacitance(Rhd2000Registers::ZcheckCs capacitanceEnum);

    bool getAmpPowered(int channel);
    void setAmpPowered(int channel, bool powered);
    void powerUpAllAmps();
    void powerDownAllAmps();

    int getRegisterValue(int reg) const;

    double setUpperBandwidth(double upperBandwidth);
    double setLowerBandwidth(double lowerBandwidth);

    int createCommandListRegisterConfig(std::vector<int> &commandList, bool calibrate);
	int createCommandListTempSensor(std::vector<int> &commandList);
	int createCommandListUpdateDigOut(std::vector<int> &commandList);
	int createCommandListZcheckDac(std::vector<int> &commandList, double frequency, double amplitude);

    void readBack(const std::vector<int>& data);

    /** Commands to send over MOSI to the RHD2000 chip.
     */
    enum Rhd2000CommandType {
        Rhd2000CommandConvert,
        Rhd2000CommandCalibrate,
        Rhd2000CommandCalClear,
        Rhd2000CommandRegWrite,
        Rhd2000CommandRegRead
    };

    int createRhd2000Command(Rhd2000CommandType commandType);
    int createRhd2000Command(Rhd2000CommandType commandType, int arg1);
    int createRhd2000Command(Rhd2000CommandType commandType, int arg1, int arg2);

private:
    double sampleRate;

    double rH1FromUpperBandwidth(double upperBandwidth) const;
    double rH2FromUpperBandwidth(double upperBandwidth) const;
    double rLFromLowerBandwidth(double lowerBandwidth) const;
    double upperBandwidthFromRH1(double rH1) const;
    double upperBandwidthFromRH2(double rH2) const;
    double lowerBandwidthFromRL(double rL) const;

    static const int MaxCommandLength = 1024; // size of on-FPGA auxiliary command RAM banks
};

#endif // RHD2000REGISTERS_H
