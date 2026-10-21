/*
  ______                              _
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
    (C)2016 Semtech

Description: Handling of the node configuration protocol

License: Revised BSD License, see LICENSE.TXT file include in the project

Maintainer: Miguel Luis, Gregory Cristian and Matthieu Verdy
*/
#ifndef __RADIO_H__
#define __RADIO_H__


#ifdef __cplusplus
extern "C" {
#endif

#include "stm32u5xx_hal.h"           // HAL (Hardware Abstraction Layer) library
#include "stm32u5xx_hal_gpio.h"      // GPIO-specific HAL functions
#include <stdbool.h>

//#include "sx126x.hpp"
/*!
 * \brief Structure describing the radio status
 */
typedef union RadioStatus_u
{
    uint8_t Value;
    struct
    {   //bit order is lsb -> msb
        uint8_t Reserved  : 1;  //!< Reserved
        uint8_t CmdStatus : 3;  //!< Command status
        uint8_t ChipMode  : 3;  //!< Chip mode
        uint8_t CpuBusy   : 1;  //!< Flag for CPU radio busy 
    }Fields;
}RadioStatus_t;

/*!
 * \brief Structure describing the error codes for callback functions
 */
typedef enum
{
    IRQ_HEADER_ERROR_CODE                   = 0x01,
    IRQ_SYNCWORD_ERROR_CODE                 = 0x02,
    IRQ_CRC_ERROR_CODE                      = 0x04,
}IrqErrorCode_t;


enum IrqPblSyncHeaderCode_t
{
    IRQ_PBL_DETECT_CODE                     = 0x01,
    IRQ_SYNCWORD_VALID_CODE                 = 0x02,
    IRQ_HEADER_VALID_CODE                   = 0x04,
};

enum IrqTimeoutCode_t
{
    IRQ_RX_TIMEOUT                          = 0x00,
    IRQ_TX_TIMEOUT                          = 0x01,
    IRQ_XYZ                                 = 0xFF,
};


/*!
 * \brief Represents the states of the radio
 */
typedef enum
{
    RF_IDLE                                 = 0x00,         //!< The radio is idle
    RF_RX_RUNNING,                                          //!< The radio is in reception state
    RF_TX_RUNNING,                                          //!< The radio is in transmission state
    RF_CAD,                                                 //!< The radio is doing channel activity detection
}RadioStates_t;

/*!
 * \brief Represents the operating mode the radio is actually running
 */
typedef enum
{
    MODE_SLEEP                              = 0x00,         //! The radio is in sleep mode
    MODE_STDBY_RC,                                          //! The radio is in standby mode with RC oscillator
    MODE_STDBY_XOSC,                                        //! The radio is in standby mode with XOSC oscillator
    MODE_FS,                                                //! The radio is in frequency synthesis mode
    MODE_TX,                                                //! The radio is in transmit mode
    MODE_RX,                                                //! The radio is in receive mode
    MODE_RX_DC,                                             //! The radio is in receive duty cycle mode
    MODE_CAD                                                //! The radio is in channel activity detection mode
}RadioOperatingModes_t;

/*!
 * \brief Declares the oscillator in use while in standby mode
 *
 * Using the STDBY_RC standby mode allow to reduce the energy consumption
 * STDBY_XOSC should be used for time critical applications
 */
typedef enum
{
    STDBY_RC                                = 0x00,
    STDBY_XOSC                              = 0x01,
}RadioStandbyModes_t;

/*!
 * \brief Declares the power regulation used to power the device
 *
 * This command allows the user to specify if DC-DC or LDO is used for power regulation.
 * Using only LDO implies that the Rx or Tx current is doubled
 */
typedef enum
{
    USE_LDO                                 = 0x00, // default
    USE_DCDC                                = 0x01,
}RadioRegulatorMode_t;

/*!
 * \brief Represents the possible packet type (i.e. modem) used
 */
typedef enum
{
    PACKET_TYPE_GFSK                        = 0x00,
    PACKET_TYPE_LORA                        = 0x01,
    PACKET_TYPE_NONE                        = 0x0F,
}RadioPacketTypes_t;

/*!
 * \brief Represents the ramping time for power amplifier
 */
typedef enum
{
    RADIO_RAMP_10_US                        = 0x00,
    RADIO_RAMP_20_US                        = 0x01,
    RADIO_RAMP_40_US                        = 0x02,
    RADIO_RAMP_80_US                        = 0x03,
    RADIO_RAMP_200_US                       = 0x04,
    RADIO_RAMP_800_US                       = 0x05,
    RADIO_RAMP_1700_US                      = 0x06,
    RADIO_RAMP_3400_US                      = 0x07,
}RadioRampTimes_t;

/*!
 * \brief Represents the number of symbols to be used for channel activity detection operation
 */
typedef enum
{
    LORA_CAD_01_SYMBOL                      = 0x00,
    LORA_CAD_02_SYMBOL                      = 0x01,
    LORA_CAD_04_SYMBOL                      = 0x02,
    LORA_CAD_08_SYMBOL                      = 0x03,
    LORA_CAD_16_SYMBOL                      = 0x04,
}RadioLoRaCadSymbols_t;

/*!
 * \brief Represents the Channel Activity Detection actions after the CAD operation is finished
 */
typedef enum
{
    LORA_CAD_ONLY                           = 0x00,
    LORA_CAD_RX                             = 0x01,
    LORA_CAD_LBT                            = 0x10,
}RadioCadExitModes_t;

/*!
 * \brief Represents the modulation shaping parameter
 */
typedef enum
{
    MOD_SHAPING_OFF                         = 0x00,
    MOD_SHAPING_G_BT_03                     = 0x08,
    MOD_SHAPING_G_BT_05                     = 0x09,
    MOD_SHAPING_G_BT_07                     = 0x0A,
    MOD_SHAPING_G_BT_1                      = 0x0B,
}RadioModShapings_t;

/*!
 * \brief Represents the modulation shaping parameter
 */
typedef enum
{
    RX_BW_4800                              = 0x1F,
    RX_BW_5800                              = 0x17,
    RX_BW_7300                              = 0x0F,
    RX_BW_9700                              = 0x1E,
    RX_BW_11700                             = 0x16,
    RX_BW_14600                             = 0x0E,
    RX_BW_19500                             = 0x1D,
    RX_BW_23400                             = 0x15,
    RX_BW_29300                             = 0x0D,
    RX_BW_39000                             = 0x1C,
    RX_BW_46900                             = 0x14,
    RX_BW_58600                             = 0x0C,
    RX_BW_78200                             = 0x1B,
    RX_BW_93800                             = 0x13,
    RX_BW_117300                            = 0x0B,
    RX_BW_156200                            = 0x1A,
    RX_BW_187200                            = 0x12,
    RX_BW_234300                            = 0x0A,
    RX_BW_312000                            = 0x19,
    RX_BW_373600                            = 0x11,
    RX_BW_467000                            = 0x09,
}RadioRxBandwidth_t;

/*!
 * \brief Represents the possible spreading factor values in LoRa packet types
 */
typedef enum
{
    LORA_SF5                                = 0x05,
    LORA_SF6                                = 0x06,
    LORA_SF7                                = 0x07,
    LORA_SF8                                = 0x08,
    LORA_SF9                                = 0x09,
    LORA_SF10                               = 0x0A,
    LORA_SF11                               = 0x0B,
    LORA_SF12                               = 0x0C,
}RadioLoRaSpreadingFactors_t;

/*!
 * \brief Represents the bandwidth values for LoRa packet type
 */
typedef enum
{
    LORA_BW_500                             = 6,
    LORA_BW_250                             = 5,
    LORA_BW_125                             = 4,
    LORA_BW_062                             = 3,
    LORA_BW_041                             = 10,
    LORA_BW_031                             = 2,
    LORA_BW_020                             = 9,
    LORA_BW_015                             = 1,
    LORA_BW_010                             = 8,
    LORA_BW_007                             = 0,
}RadioLoRaBandwidths_t;

/*!
 * \brief Represents the coding rate values for LoRa packet type
 */
typedef enum
{
    LORA_CR_4_5                             = 0x01,
    LORA_CR_4_6                             = 0x02,
    LORA_CR_4_7                             = 0x03,
    LORA_CR_4_8                             = 0x04,
}RadioLoRaCodingRates_t;

/*!
 * \brief Represents the preamble length used to detect the packet on Rx side
 */
typedef enum
{
    RADIO_PREAMBLE_DETECTOR_OFF             = 0x00,         //!< Preamble detection length off
    RADIO_PREAMBLE_DETECTOR_08_BITS         = 0x04,         //!< Preamble detection length 8 bits
    RADIO_PREAMBLE_DETECTOR_16_BITS         = 0x05,         //!< Preamble detection length 16 bits
    RADIO_PREAMBLE_DETECTOR_24_BITS         = 0x06,         //!< Preamble detection length 24 bits
    RADIO_PREAMBLE_DETECTOR_32_BITS         = 0x07,         //!< Preamble detection length 32 bit
}RadioPreambleDetection_t;

/*!
 * \brief Represents the possible combinations of SyncWord correlators activated
 */
typedef enum
{
    RADIO_ADDRESSCOMP_FILT_OFF              = 0x00,         //!< No correlator turned on, i.e. do not search for SyncWord
    RADIO_ADDRESSCOMP_FILT_NODE             = 0x01,
    RADIO_ADDRESSCOMP_FILT_NODE_BROAD       = 0x02,
}RadioAddressComp_t;

/*!
 *  \brief Radio packet length mode
 */
typedef enum
{
    RADIO_PACKET_FIXED_LENGTH               = 0x00,         //!< The packet is known on both sides, no header included in the packet
    RADIO_PACKET_VARIABLE_LENGTH            = 0x01,         //!< The packet is on variable size, header included
}RadioPacketLengthModes_t;

/*!
 * \brief Represents the CRC length
 */
typedef enum
{
    RADIO_CRC_OFF                           = 0x01,         //!< No CRC in use
    RADIO_CRC_1_BYTES                       = 0x00,
    RADIO_CRC_2_BYTES                       = 0x02,
    RADIO_CRC_1_BYTES_INV                   = 0x04,
    RADIO_CRC_2_BYTES_INV                   = 0x06,
    RADIO_CRC_2_BYTES_IBM                   = 0xF1,
    RADIO_CRC_2_BYTES_CCIT                  = 0xF2,
}RadioCrcTypes_t;

/*!
 * \brief Radio whitening mode activated or deactivated
 */
typedef enum
{
    RADIO_DC_FREE_OFF                       = 0x00,
    RADIO_DC_FREEWHITENING                  = 0x01,
}RadioDcFree_t;

/*!
 * \brief Holds the lengths mode of a LoRa packet type
 */
typedef enum
{
    LORA_PACKET_VARIABLE_LENGTH             = 0x00,         //!< The packet is on variable size, header included
    LORA_PACKET_FIXED_LENGTH                = 0x01,         //!< The packet is known on both sides, no header included in the packet
    LORA_PACKET_EXPLICIT                    = LORA_PACKET_VARIABLE_LENGTH,
    LORA_PACKET_IMPLICIT                    = LORA_PACKET_FIXED_LENGTH,
}RadioLoRaPacketLengthsMode_t;

/*!
 * \brief Represents the CRC mode for LoRa packet type
 */
typedef enum
{
    LORA_CRC_ON                             = 0x01,         //!< CRC activated
    LORA_CRC_OFF                            = 0x00,         //!< CRC not used
}RadioLoRaCrcModes_t;

/*!
 * \brief Represents the IQ mode for LoRa packet type
 */
typedef enum
{
    LORA_IQ_NORMAL                          = 0x00,
    LORA_IQ_INVERTED                        = 0x01,
}RadioLoRaIQModes_t;

/*!
 * \brief Represents the volatge used to control the TCXO on/off from DIO3
 */
typedef enum
{
    TCXO_CTRL_1_6V                          = 0x00,
    TCXO_CTRL_1_7V                          = 0x01,
    TCXO_CTRL_1_8V                          = 0x02,
    TCXO_CTRL_2_2V                          = 0x03,
    TCXO_CTRL_2_4V                          = 0x04,
    TCXO_CTRL_2_7V                          = 0x05,
    TCXO_CTRL_3_0V                          = 0x06,
    TCXO_CTRL_3_3V                          = 0x07,
}RadioTcxoCtrlVoltage_t;

/*!
 * \brief Represents the interruption masks available for the radio
 *
 * \remark Note that not all these interruptions are available for all packet types
 */
typedef enum
{
    IRQ_RADIO_NONE                          = 0x0000,
    IRQ_TX_DONE                             = 0x0001,
    IRQ_RX_DONE                             = 0x0002,
    IRQ_PREAMBLE_DETECTED                   = 0x0004,
    IRQ_SYNCWORD_VALID                      = 0x0008,
    IRQ_HEADER_VALID                        = 0x0010,
    IRQ_HEADER_ERROR                        = 0x0020,
    IRQ_CRC_ERROR                           = 0x0040,
    IRQ_CAD_DONE                            = 0x0080,
    IRQ_CAD_ACTIVITY_DETECTED               = 0x0100,
    IRQ_RX_TX_TIMEOUT                       = 0x0200,
    IRQ_RADIO_ALL                           = 0xFFFF,
}RadioIrqMasks_t;

/*!
 * \brief Represents all possible opcode understood by the radio
 */
typedef enum RadioCommands_e
{
    RADIO_GET_STATUS                        = 0xC0,
    RADIO_WRITE_REGISTER                    = 0x0D,
    RADIO_READ_REGISTER                     = 0x1D,
    RADIO_WRITE_BUFFER                      = 0x0E,
    RADIO_READ_BUFFER                       = 0x1E,
    RADIO_SET_SLEEP                         = 0x84,
    RADIO_SET_STANDBY                       = 0x80,
    RADIO_SET_FS                            = 0xC1,
    RADIO_SET_TX                            = 0x83,
    RADIO_SET_RX                            = 0x82,
    RADIO_SET_RXDUTYCYCLE                   = 0x94,
    RADIO_SET_CAD                           = 0xC5,
    RADIO_SET_TXCONTINUOUSWAVE              = 0xD1,
    RADIO_SET_TXCONTINUOUSPREAMBLE          = 0xD2,
    RADIO_SET_PACKETTYPE                    = 0x8A,
    RADIO_GET_PACKETTYPE                    = 0x11,
    RADIO_SET_RFFREQUENCY                   = 0x86,
    RADIO_SET_TXPARAMS                      = 0x8E,
    RADIO_SET_PACONFIG                      = 0x95,
    RADIO_SET_CADPARAMS                     = 0x88,
    RADIO_SET_BUFFERBASEADDRESS             = 0x8F,
    RADIO_SET_MODULATIONPARAMS              = 0x8B,
    RADIO_SET_PACKETPARAMS                  = 0x8C,
    RADIO_GET_RXBUFFERSTATUS                = 0x13,
    RADIO_GET_PACKETSTATUS                  = 0x14,
    RADIO_GET_RSSIINST                      = 0x15,
    RADIO_GET_STATS                         = 0x10,
    RADIO_RESET_STATS                       = 0x00,
    RADIO_CFG_DIOIRQ                        = 0x08,
    RADIO_GET_IRQSTATUS                     = 0x12,
    RADIO_CLR_IRQSTATUS                     = 0x02,
    RADIO_CALIBRATE                         = 0x89,
    RADIO_CALIBRATEIMAGE                    = 0x98,
    RADIO_SET_REGULATORMODE                 = 0x96,
    RADIO_GET_ERROR                         = 0x17,
    RADIO_SET_TCXOMODE                      = 0x97,
    RADIO_SET_TXFALLBACKMODE                = 0x93,
    RADIO_SET_RFSWITCHMODE                  = 0x9D,
    RADIO_SET_STOPRXTIMERONPREAMBLE         = 0x9F,
    RADIO_SET_LORASYMBTIMEOUT               = 0xA0,
}RadioCommands_t;

/*!
 * \brief Represents the possible radio system error states
 */
typedef union
{
    struct
    {
        uint8_t Rc64kCalib              : 1;                    //!< RC 64kHz oscillator calibration failed
        uint8_t Rc13mCalib              : 1;                    //!< RC 13MHz oscillator calibration failed
        uint8_t PllCalib                : 1;                    //!< PLL calibration failed
        uint8_t AdcCalib                : 1;                    //!< ADC calibration failed
        uint8_t ImgCalib                : 1;                    //!< Image calibration failed
        uint8_t XoscStart               : 1;                    //!< XOSC oscillator failed to start
        uint8_t PllLock                 : 1;                    //!< PLL lock failed
        uint8_t BuckStart               : 1;                    //!< Buck converter failed to start
        uint8_t PaRamp                  : 1;                    //!< PA ramp failed
        uint8_t                         : 7;                    //!< Reserved
    }Fields;
    uint16_t Value;
}RadioError_t;

/*!
 * \brief The radio command opcodes
 */
typedef enum RadioCommands_e RadioCommands_t;

/*!
 * \brief The radio callbacks structure
 * Holds function pointers to be called on radio interrupts
 */
typedef struct
{
    void ( *txDone )( void );                       //!< Pointer to a function run on successful transmission
    void ( *rxDone )( void );                       //!< Pointer to a function run on successful reception
    void ( *rxPreambleDetect )( void );             //!< Pointer to a function run on successful Preamble detection
    void ( *rxSyncWordDone )( void );               //!< Pointer to a function run on successful SyncWord reception
    void ( *rxHeaderDone )( void );                 //!< Pointer to a function run on successful Header reception
    void ( *txTimeout )( void );                    //!< Pointer to a function run on transmission timeout
    void ( *rxTimeout )( void );                    //!< Pointer to a function run on reception timeout
    void ( *rxError )( IrqErrorCode_t errCode );    //!< Pointer to a function run on reception error
    void ( *cadDone )( bool cadFlag );              //!< Pointer to a function run on channel activity detected
}RadioCallbacks_t;

/*!
 * \brief Class holding the basic communications with a radio
 *
 * It sets the functions to read/write registers, send commands and read/write
 * payload.
 * It also provides functions to run callback functions depending on the
 * interrupts generated from the radio.
 */
class Radio
{
protected:
    /*!
     * \brief Callback on Tx done interrupt
     */
    void ( *txDone )( void );

    /*!
     * \brief Callback on Rx done interrupt
     */
    void ( *rxDone )( void );

    /*!
     * \brief Callback on Rx preamble detection interrupt
     */
    void ( *rxPreambleDetect )( void );

    /*!
     * \brief Callback on Rx SyncWord interrupt
     */
    void ( *rxSyncWordDone )( void );

    /*!
     * \brief Callback on Rx header received interrupt
     */
    void ( *rxHeaderDone )( void );

    /*!
     * \brief Callback on Tx timeout interrupt
     */
    void ( *txTimeout )( void );

    /*!
     * \brief Callback on Rx timeout interrupt
     */
    void ( *rxTimeout )( void );

    /*!
     * \brief Callback on Rx error interrupt
     *
     * \param [out] errCode       A code indicating the type of interrupt (SyncWord error or CRC error)
     */
    void ( *rxError )( IrqErrorCode_t errCode );

    /*!
     * \brief Callback on Channel Activity Detection done interrupt
     *
     * \param [out] cadFlag       Flag for channel activity detected or not
     */
    void ( *cadDone )( bool cadFlag );

public:
    /*!
     * \brief Constructor for radio class
     * Sets the callbacks functions pointers
     *
     * \param [in]  callbacks     The structure of callbacks function pointers
     *                            to be called on radio interrupts
     *
     */
    Radio( RadioCallbacks_t *callbacks )
    {
        this->txDone = callbacks->txDone;
        this->rxDone = callbacks->rxDone;
        this->rxPreambleDetect = callbacks->rxPreambleDetect;
        this->rxSyncWordDone = callbacks->rxSyncWordDone;
        this->rxHeaderDone = callbacks->rxHeaderDone;
        this->txTimeout = callbacks->txTimeout;
        this->rxTimeout = callbacks->rxTimeout;
        this->rxError = callbacks->rxError;
        this->cadDone = callbacks->cadDone;
    }
    virtual ~Radio( void ){ };

    /*!
     * \brief Resets the radio
     */
    virtual void Reset( void ) = 0;

    /*!
     * \brief Gets the current radio status
     *
     * \retval      status        Radio status
     */
    virtual RadioStatus_t GetStatus( void ) = 0;

    /*!
     * \brief Writes the given command to the radio
     *
     * \param [in]  opcode        Command opcode
     * \param [in]  buffer        Command parameters byte array
     * \param [in]  size          Command parameters byte array size
     */
    virtual void WriteCommand( RadioCommands_t opcode, uint8_t *buffer, uint16_t size ) = 0;

    /*!
     * \brief Reads the given command from the radio
     *
     * \param [in]  opcode        Command opcode
     * \param [in]  buffer        Command parameters byte array
     * \param [in]  size          Command parameters byte array size
     */
    virtual void ReadCommand( RadioCommands_t opcode, uint8_t *buffer, uint16_t size ) = 0;

    /*!
     * \brief Writes multiple radio registers starting at address
     *
     * \param [in]  address       First Radio register address
     * \param [in]  buffer        Buffer containing the new register's values
     * \param [in]  size          Number of registers to be written
     */
    virtual void WriteRegister( uint16_t address, uint8_t *buffer, uint16_t size ) = 0;

    /*!
     * \brief Writes the radio register at the specified address
     *
     * \param [in]  address       Register address
     * \param [in]  value         New register value
     */
    virtual void WriteReg( uint16_t address, uint8_t value ) = 0;

    /*!
     * \brief Reads multiple radio registers starting at address
     *
     * \param [in]  address       First Radio register address
     * \param [out] buffer        Buffer where to copy the registers data
     * \param [in]  size          Number of registers to be read
     */
    virtual void ReadRegister( uint16_t address, uint8_t *buffer, uint16_t size ) = 0;

    /*!
     * \brief Reads the radio register at the specified address
     *
     * \param [in]  address       Register address
     *
     * \retval      value         The register value
     */
    virtual uint8_t ReadReg( uint16_t address ) = 0;

    /*!
     * \brief Writes Radio Data Buffer with buffer of size starting at offset.
     *
     * \param [in]  offset        Offset where to start writing
     * \param [in]  buffer        Buffer pointer
     * \param [in]  size          Buffer size
     */
    virtual void WriteBuffer( uint8_t offset, uint8_t *buffer, uint8_t size ) = 0;

    /*!
     * \brief Reads Radio Data Buffer at offset to buffer of size
     *
     * \param [in]  offset        Offset where to start reading
     * \param [out] buffer        Buffer pointer
     * \param [in]  size          Buffer size
     */
    virtual void ReadBuffer( uint8_t offset, uint8_t *buffer, uint8_t size ) = 0;
};

#ifdef __cplusplus
}
#endif

#endif // __RADIO_H__

