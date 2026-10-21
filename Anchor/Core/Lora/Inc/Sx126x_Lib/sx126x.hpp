/*
  ______                              _
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
    (C)2017 Semtech

Description: Generic SX126x driver implementation

License: Revised BSD License, see LICENSE.TXT file include in the project

Authors: Miguel Luis, Gregory Cristian
*/
#ifndef __SX126x_H__
#define __SX126x_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32u5xx_hal.h"           // HAL (Hardware Abstraction Layer) library
#include "stm32u5xx_hal_gpio.h"      // GPIO-specific HAL functions

#include "radio.hpp"
/*!
 * \brief Enables/disables driver debug features
 */
#define SX126x_DEBUG                                0



/*!
 * \brief Hardware IO IRQ callback function definition
 */
class SX126x;
typedef void ( SX126x::*DioIrqHandler )( void );

/*!
 * \brief IRQ triggers callback function definition
 */
class SX126xHal;
typedef void ( SX126xHal::*Trigger )( void );

/*!
 * \brief Provides the frequency of the chip running on the radio and the frequency step
 *
 * \remark These defines are used for computing the frequency divider to set the RF frequency
 */
#define XTAL_FREQ                                   32000000
#define FREQ_DIV                                    33554432
#define FREQ_STEP                                   0.95367431640625 // ( ( double )( XTAL_FREQ / ( double )FREQ_DIV ) )
#define FREQ_ERR                                    0.47683715820312


/*!
 * \brief List of devices supported by this driver
 */
#define SX1261  0
#define SX1262  1
#define SX1268  2

/*!
 * \brief List of matching supported by the sx126x
 */
#define MATCHING_FREQ_915                           0
#define MATCHING_FREQ_780                           1
#define MATCHING_FREQ_490                           2
#define MATCHING_FREQ_434                           3
#define MATCHING_FREQ_280                           4
#define MATCHING_FREQ_169                           5
#define MATCHING_FREQ_868                           6

/*!
 * \brief Compensation delay for SetAutoTx/Rx functions in 15.625 microseconds
 */
#define AUTO_RX_TX_OFFSET                           2

/*!
 * \brief LFSR initial value to compute IBM type CRC
 */
#define CRC_IBM_SEED                                0xFFFF

/*!
 * \brief LFSR initial value to compute CCIT type CRC
 */
#define CRC_CCITT_SEED                              0x1D0F

/*!
 * \brief Polynomial used to compute IBM CRC
 */
#define CRC_POLYNOMIAL_IBM                          0x8005

/*!
 * \brief Polynomial used to compute CCIT CRC
 */
#define CRC_POLYNOMIAL_CCITT                        0x1021

/*!
 * \brief The address of the register holding the first byte defining the CRC seed
 *
 */
#define REG_LR_CRCSEEDBASEADDR                      0x06BC

/*!
 * \brief The address of the register holding the first byte defining the CRC polynomial
 */
#define REG_LR_CRCPOLYBASEADDR                      0x06BE

/*!
 * \brief The address of the register holding the first byte defining the whitening seed
 */
#define REG_LR_WHITSEEDBASEADDR_MSB                 0x06B8
#define REG_LR_WHITSEEDBASEADDR_LSB                 0x06B9

/*!
 * \brief The address of the register holding the packet configuration
 */
#define REG_LR_PACKETPARAMS                         0x0704

/*!
 * \brief The address of the register holding the payload size
 */
#define REG_LR_PAYLOADLENGTH                        0x0702

/*!
 * \brief The addresses of the registers holding SyncWords values
 */
#define REG_LR_SYNCWORDBASEADDRESS                  0x06C0

/*!
 * \brief The addresses of the register holding LoRa Modem SyncWord value
 */
#define REG_LR_SYNCWORD                             0x0740

/*!
 * Syncword for Private LoRa networks
 */
#define LORA_MAC_PRIVATE_SYNCWORD                   0x1424

/*!
 * Syncword for Public LoRa networks
 */
#define LORA_MAC_PUBLIC_SYNCWORD                    0x3444

/*!
 * The address of the register giving a 4 bytes random number
 */
#define RANDOM_NUMBER_GENERATORBASEADDR             0x0819

/*!
 * The address of the register holding RX Gain value (0x94: power saving, 0x96: rx boosted)
 */
#define REG_RX_GAIN                                 0x08AC

/*!
 * The address of the register holding frequency error indication
 */
#define REG_FREQUENCY_ERRORBASEADDR                 0x076B

/*!
 * Change the value on the device internal trimming capacitor
 */
#define REG_XTA_TRIM                                0x0911

/*!
 * Set the current max value in the over current protection
 */
#define REG_OCP                                     0x08E7


/*!
 * \brief The type describing the modulation parameters for every packet types
 */
typedef struct
{
    RadioPacketTypes_t                   PacketType;        //!< Packet to which the modulation parameters are referring to.
    struct
    {
        struct
        {
            uint32_t                     BitRate;
            uint32_t                     Fdev;
            RadioModShapings_t           ModulationShaping;
            uint8_t                      Bandwidth;
        }Gfsk;
        struct
        {
            RadioLoRaSpreadingFactors_t  SpreadingFactor;   //!< Spreading Factor for the LoRa modulation
            RadioLoRaBandwidths_t        Bandwidth;         //!< Bandwidth for the LoRa modulation
            RadioLoRaCodingRates_t       CodingRate;        //!< Coding rate for the LoRa modulation
            uint8_t                      LowDatarateOptimize; //!< Indicates if the modem uses the low datarate optimization
        }LoRa;
    }Params;                                                //!< Holds the modulation parameters structure
}ModulationParams_t;

/*!
 * \brief The type describing the packet parameters for every packet types
 */
typedef struct
{
    RadioPacketTypes_t                    PacketType;        //!< Packet to which the packet parameters are referring to.
    struct
    {
        /*!
         * \brief Holds the GFSK packet parameters
         */
        struct
        {
            uint16_t                     PreambleLength;    //!< The preamble Tx length for GFSK packet type in bit
            RadioPreambleDetection_t     PreambleMinDetect; //!< The preamble Rx length minimal for GFSK packet type
            uint8_t                      SyncWordLength;    //!< The synchronization word length for GFSK packet type
            RadioAddressComp_t           AddrComp;          //!< Activated SyncWord correlators
            RadioPacketLengthModes_t     HeaderType;        //!< If the header is explicit, it will be transmitted in the GFSK packet. If the header is implicit, it will not be transmitted
            uint8_t                      PayloadLength;     //!< Size of the payload in the GFSK packet
            RadioCrcTypes_t              CrcLength;         //!< Size of the CRC block in the GFSK packet
            RadioDcFree_t                DcFree;
        }Gfsk;
        /*!
         * \brief Holds the LoRa packet parameters
         */
        struct
        {
            uint16_t                     PreambleLength;    //!< The preamble length is the number of LoRa symbols in the preamble
            RadioLoRaPacketLengthsMode_t HeaderType;        //!< If the header is explicit, it will be transmitted in the LoRa packet. If the header is implicit, it will not be transmitted
            uint8_t                      PayloadLength;     //!< Size of the payload in the LoRa packet
            RadioLoRaCrcModes_t          CrcMode;           //!< Size of CRC block in LoRa packet
            RadioLoRaIQModes_t           InvertIQ;          //!< Allows to swap IQ for LoRa packet
        }LoRa;
    }Params;                                                //!< Holds the packet parameters structure
}PacketParams_t;

/*!
 * \brief Represents the packet status for every packet type
 */
typedef struct
{
    RadioPacketTypes_t                    packetType;      //!< Packet to which the packet status are referring to.
    struct
    {
        struct
        {
            uint8_t RxStatus;
            int8_t RssiAvg;                                //!< The averaged RSSI
            int8_t RssiSync;                               //!< The RSSI measured on last packet
            uint32_t FreqError;
        }Gfsk;
        struct
        {
            int8_t RssiPkt;                                //!< The RSSI of the last packet
            int8_t SnrPkt;                                 //!< The SNR of the last packet
            int8_t SignalRssiPkt;
            uint32_t FreqError;
        }LoRa;
    }Params;
}PacketStatus_t;

/*!
 * \brief Represents the Rx internal counters values when GFSK or LoRa packet type is used
 */
typedef struct
{
    RadioPacketTypes_t                    packetType;       //!< Packet to which the packet status are referring to.
    uint16_t PacketReceived;
    uint16_t CrcOk;
    uint16_t LengthError;
}RxCounter_t;

/*!
 * \brief Represents a calibration configuration
 */
typedef union
{
    struct
    {
        uint8_t RC64KEnable    : 1;                             //!< Calibrate RC64K clock
        uint8_t RC13MEnable    : 1;                             //!< Calibrate RC13M clock
        uint8_t PLLEnable      : 1;                             //!< Calibrate PLL
        uint8_t ADCPulseEnable : 1;                             //!< Calibrate ADC Pulse
        uint8_t ADCBulkNEnable : 1;                             //!< Calibrate ADC bulkN
        uint8_t ADCBulkPEnable : 1;                             //!< Calibrate ADC bulkP
        uint8_t ImgEnable      : 1;
        uint8_t                : 1;
    }Fields;
    uint8_t Value;
}CalibrationParams_t;

/*!
 * \brief Represents a sleep mode configuration
 */
typedef union
{
    struct
    {
        uint8_t WakeUpRTC               : 1;                    //!< Get out of sleep mode if wakeup signal received from RTC
        uint8_t Reset                   : 1;
        uint8_t WarmStart               : 1;
        uint8_t Reserved                : 5;
    }Fields;
    uint8_t Value;
}SleepParams_t;

/*!
 * \brief Represents the SX126x and its features
 *
 * It implements the commands the SX126x can understands
 */
#include "radio.hpp"
class SX126x : public Radio
{
public:
    /*!
     * \brief Instantiates a SX126x object and provides API functions to communicates with the radio
     * \param [in]  callbacks      Pointer to the callbacks structure defining
     *                             all callbacks function pointers
     */
    SX126x( RadioCallbacks_t *callbacks ):
        Radio( callbacks )
    {
        this->dioIrq      = &SX126x::OnDioIrq;
        this->PacketType  = PACKET_TYPE_NONE;
        this->PollingMode = false;
        this->IrqState    = false;
    }

    virtual ~SX126x( )
    {
    }

private:
    /*!
     * \brief Holds the internal operating mode of the radio
     */
    RadioOperatingModes_t OperatingMode;

    /*!
     * \brief Stores the current packet type set in the radio
     */
    RadioPacketTypes_t PacketType;

    /*!
     * \brief Holds a flag raised on radio interrupt
     */
    bool IrqState;

    /*!
     * \brief Hardware DIO IRQ functions
     */
    DioIrqHandler dioIrq;

    /*!
     * \brief Holds the polling state of the driver
     */
    bool PollingMode;

protected:

    /*!
     * \brief Sets a function to be triggered on radio interrupt
     *
     * \param [in]  irqHandler    A pointer to a function to be run on interrupt
     *                            from the radio
     */
    virtual void IoIrqInit(DioIrqHandler irqHandler ) = 0;

    /*!
     * \brief DIOs interrupt callback
     *
     * \remark Called to handle all 3 DIOs pins
     */
    void OnDioIrq( void );
public:
    /*!
     * \brief Initializes the radio driver
     */
    void Init( void );

    /*!
     * \brief Gets the current Operation Mode of the Radip
     *
     * \retval      RadioOperatingModes_t last operating mode
     */
    virtual RadioOperatingModes_t GetOperatingMode( void );

    /*!
     * \brief Wakeup the radio if it is in Sleep mode and check that Busy is low
     */
    virtual void CheckDeviceReady( void );

    /*!
     * \brief Saves the payload to be send in the radio buffer
     *
     * \param [in]  payload       A pointer to the payload
     * \param [in]  size          The size of the payload
     */
    void SetPayload( uint8_t *payload, uint8_t size );

    /*!
     * \brief Reads the payload received. If the received payload is longer
     * than maxSize, then the method returns 1 and do not set size and payload.
     *
     * \param [out] payload       A pointer to a buffer into which the payload will be copied
     * \param [out] size          A pointer to the size of the payload received
     * \param [in]  maxSize       The maximal size allowed to copy into the buffer
     */
    uint8_t GetPayload( uint8_t *payload, uint8_t *size, uint8_t maxSize );

    /*!
     * \brief Sends a payload
     *
     * \param [in]  payload       A pointer to the payload to send
     * \param [in]  size          The size of the payload to send
     * \param [in]  timeout       The timeout for Tx operation
     */
    void SendPayload( uint8_t *payload, uint8_t size, uint32_t timeout );

    /*!
     * \brief Sets the Sync Word given by index used in GFSK
     *
     * \param [in]  syncWord      SyncWord bytes ( 8 bytes )
     *
     * \retval      status        [0: OK, 1: NOK]
     */
    uint8_t SetSyncWord( uint8_t *syncWord );

    /*!
     * \brief Sets the Initial value for the LFSR used for the CRC calculation
     *
     * \param [in]  seed          Initial LFSR value ( 2 bytes )
     *
     */
    void SetCrcSeed( uint16_t seed );

    /*!
     * \brief Sets the seed used for the CRC calculation
     *
     * \param [in]  seed          The seed value
     *
     */
    void SetCrcPolynomial( uint16_t seed );

    /*!
     * \brief Sets the Initial value of the LFSR used for the whitening in GFSK protocols
     *
     * \param [in]  seed          Initial LFSR value
     */
    void SetWhiteningSeed( uint16_t seed );
    
    /*!
     * \brief Gets a 32 bits random value generated by the radio
     *
     * \remark The radio must be in reception mode before executing this function
     *
     * \retval randomValue    32 bits random value
     */
    uint32_t GetRandom( void );

    /*!
     * \brief Sets the radio in sleep mode
     *
     * \param [in]  sleepConfig   The sleep configuration describing data
     *                            retention and RTC wake-up
     */
    void SetSleep( SleepParams_t sleepConfig );

    /*!
     * \brief Sets the radio in configuration mode
     *
     * \param [in]  mode          The standby mode to put the radio into
     */
    void SetStandby( RadioStandbyModes_t mode );

    /*!
     * \brief Sets the radio in FS mode
     */
    void SetFs( void );

    /*!
     * \brief Sets the radio in transmission mode
     *
     * \param [in]  timeout       Structure describing the transmission timeout value
     */
    void SetTx( uint32_t timeout );

    /*!
     * \brief Sets the radio in reception Boosted mode
     *
     * \param [in]  timeout       Structure describing the transmission timeout value
     */
    void SetRxBoosted( uint32_t timeout );

    /*!
     * \brief Sets the radio in reception mode
     *
     * \param [in]  timeout       Structure describing the reception timeout value
     */
    void SetRx( uint32_t timeout );

    /*!
     * \brief Sets the Rx duty cycle management parameters
     *
     * \param [in]  rxTime        Structure describing reception timeout value
     * \param [in]  sleepTime     Structure describing sleep timeout value
     */
    void SetRxDutyCycle( uint32_t rxTime, uint32_t sleepTime );

    /*!
     * \brief Sets the radio in CAD mode
     */
    void SetCad( void );

    /*!
     * \brief Sets the radio in continuous wave transmission mode
     */
    void SetTxContinuousWave( void );

    /*!
     * \brief Sets the radio in continuous preamble transmission mode
     */
    void SetTxInfinitePreamble( void );

    /*!
     * \brief Decide which interrupt will stop the internal radio rx timer.
     *
     * \param [in]  enable          [0: Timer stop after header/syncword detection
     *                               1: Timer stop after preamble detection]
     */
    void SetStopRxTimerOnPreambleDetect( bool enable );

    /*!
     * \brief Set the number of symbol the radio will wait to validate a reception
     *
     * \param [in]  SymbNum          number of LoRa symbols
     */
    void SetLoRaSymbNumTimeout( uint8_t SymbNum );
    
    /*!
     * \brief Sets the power regulators operating mode
     *
     * \param [in]  mode          [0: LDO, 1:DC_DC]
     */
    void SetRegulatorMode( RadioRegulatorMode_t mode );
    
    /*!
     * \brief Calibrates the given radio block
     *
     * \param [in]  calibParam    The description of blocks to be calibrated
     */
    void Calibrate( CalibrationParams_t calibParam );

    /*!
     * \brief Calibrates the Image rejection depending of the frequency
     *
     * \param [in]  freq    The operating frequency
     */
    void CalibrateImage( uint32_t freq );

    /*!
     * \brief Sets the transmission parameters
     *
     * \param [in]  paDutyCycle     Duty Cycle for the PA
     * \param [in]  HpMax           0 for sx1261, 7 for sx1262
     * \param [in]  deviceSel       1 for sx1261, 0 for sx1262
     * \param [in]  paLUT           0 for 14dBm LUT, 1 for 22dBm LUT
     */
    void SetPaConfig( uint8_t paDutyCycle, uint8_t HpMax, uint8_t deviceSel, uint8_t paLUT );

    /*!
     * \brief Defines into which mode the chip goes after a TX / RX done
     *
     * \param [in]  fallbackMode    The mode in which the radio goes
     */
    void SetRxTxFallbackMode( uint8_t fallbackMode );

    /*!
     * \brief   Sets the IRQ mask and DIO masks
     *
     * \param [in]  irqMask       General IRQ mask
     * \param [in]  dio1Mask      DIO1 mask
     * \param [in]  dio2Mask      DIO2 mask
     * \param [in]  dio3Mask      DIO3 mask
     */
    void SetDioIrqParams( uint16_t irqMask, uint16_t dio1Mask, uint16_t dio2Mask, uint16_t dio3Mask );

    /*!
     * \brief Returns the current IRQ status
     *
     * \retval      irqStatus     IRQ status
     */
    uint16_t GetIrqStatus( void );

    /*
     * \brief Indicates if DIO2 is used to control an RF Switch
     *
     * \param [in] enable     true of false
     */
    void SetDio2AsRfSwitchCtrl( uint8_t enable );
    
    /*
     * \brief Indicates if the Radio main clock is supplied from a tcxo
     *
     * \param [in] tcxoVoltage     voltage used to control the TCXO
     * \param [in] timeout         time given to the TCXO to go to 32MHz
     */
    void SetDio3AsTcxoCtrl( RadioTcxoCtrlVoltage_t tcxoVoltage, uint32_t timeout );
    
    /*!
     * \brief Sets the RF frequency
     *
     * \param [in]  frequency     RF frequency [Hz]
     */
    void SetRfFrequency( uint32_t frequency );

    /*!
     * \brief Sets the radio for the given protocol
     *
     * \param [in]  packetType    [PACKET_TYPE_GFSK, PACKET_TYPE_LORA]
     *
     * \remark This method has to be called before SetRfFrequency,
     *         SetModulationParams and SetPacketParams
     */
    void SetPacketType( RadioPacketTypes_t packetType );

    /*!
     * \brief Gets the current radio protocol
     *
     * \retval      packetType    [PACKET_TYPE_GFSK, PACKET_TYPE_LORA]
     */
    RadioPacketTypes_t GetPacketType( void );

    /*!
     * \brief Sets the transmission parameters
     *
     * \param [in]  power         RF output power [-18..13] dBm
     * \param [in]  rampTime      Transmission ramp up time
     */
    void SetTxParams( int8_t power, RadioRampTimes_t rampTime );
    
    /*!
     * \brief Set the modulation parameters
     *
     * \param [in]  modParams     A structure describing the modulation parameters
     */
    void SetModulationParams( ModulationParams_t *modParams );

    /*!
     * \brief Sets the packet parameters
     *
     * \param [in]  packetParams  A structure describing the packet parameters
     */
    void SetPacketParams( PacketParams_t *packetParams );

    /*!
     * \brief Sets the Channel Activity Detection (CAD) parameters
     *
     * \param [in]  cadSymbolNum   The number of symbol to use for CAD operations
     *                             [LORA_CAD_01_SYMBOL, LORA_CAD_02_SYMBOL,
     *                              LORA_CAD_04_SYMBOL, LORA_CAD_08_SYMBOL,
     *                              LORA_CAD_16_SYMBOL]
     * \param [in]  cadDetPeak     Limite for detection of SNR peak used in the CAD
     * \param [in]  cadDetMin      Set the minimum symbol recognition for CAD
     * \param [in]  cadExitMode    Operation to be done at the end of CAD action
     *                             [LORA_CAD_ONLY, LORA_CAD_RX, LORA_CAD_LBT]
     * \param [in]  cadTimeout     Defines the timeout value to abort the CAD activity
     */
    void SetCadParams( RadioLoRaCadSymbols_t cadSymbolNum, uint8_t cadDetPeak, uint8_t cadDetMin, RadioCadExitModes_t cadExitMode, uint32_t cadTimeout );

    /*!
     * \brief Sets the data buffer base address for transmission and reception
     *
     * \param [in]  txBaseAddress Transmission base address
     * \param [in]  rxBaseAddress Reception base address
     */
    void SetBufferBaseAddresses( uint8_t txBaseAddress, uint8_t rxBaseAddress );

    /*!
     * \brief Gets the current radio status
     *
     * \retval      status        Radio status
     */
    virtual RadioStatus_t GetStatus( void );

    /*!
     * \brief Returns the instantaneous RSSI value for the last packet received
     *
     * \retval      rssiInst      Instantaneous RSSI
     */
    int8_t GetRssiInst( void );

    /*!
     * \brief Gets the last received packet buffer status
     *
     * \param [out] payloadLength Last received packet payload length
     * \param [out] rxStartBuffer Last received packet buffer address pointer
     */
    void GetRxBufferStatus( uint8_t *payloadLength, uint8_t *rxStartBuffer );

    /*!
     * \brief Gets the last received packet payload length
     *
     * \param [out] pktStatus     A structure of packet status
     */
    void GetPacketStatus( PacketStatus_t *pktStatus );

    /*!
     * \brief Returns the possible system erros
     *
     * \retval sysErrors Value representing the possible sys failures
     */
    RadioError_t GetDeviceErrors( void );

    /*!
     * \brief Clears the IRQs
     *
     * \param [in]  irq           IRQ(s) to be cleared
     */
    void ClearIrqStatus( uint16_t irq );

    /*!
     * \brief Set the driver in polling mode.
     *
     * In polling mode the application is responsible to call ProcessIrqs( ) to
     * execute callbacks functions.
     * The default mode is Interrupt Mode.
     * @code
     * // Initializations and callbacks declaration/definition
     * radio = SX126x( mosi, miso, sclk, nss, busy, int1, int2, int3, rst, &callbacks );
     * radio.Init( );
     * radio.SetPollingMode( );
     *
     * while( true )
     * {
     *                            //     IRQ processing is automatically done
     *     radio.ProcessIrqs( );  // <-- here, as well as callback functions
     *                            //     calls
     *     // Do some applicative work
     * }
     * @endcode
     *
     * \see SX126x::SetInterruptMode
     */
     void SetPollingMode( void );

    /*!
     * \brief Set the driver in interrupt mode.
     *
     * In interrupt mode, the driver communicate with the radio during the
     * interruption by direct calls to ProcessIrqs( ). The main advantage is
     * the possibility to have low power application architecture.
     * This is the default mode.
     * @code
     * // Initializations and callbacks declaration/definition
     * radio = SX126x( mosi, miso, sclk, nss, busy, int1, int2, int3, rst, &callbacks );
     * radio.Init( );
     * radio.SetInterruptMode( );   // Optionnal. Driver default behavior
     *
     * while( true )
     * {
     *     // Do some applicative work
     * }
     * @endcode
     *
     * \see SX126x::SetPollingMode
     */
     void SetInterruptMode( void );

     /*
      * Initial Operational mode
      * Author: Aviad Cohen + Genadi 10^9
      */
     void InitOperationalMode( void );
    /*!
     * \brief Resets the radio
     */
    virtual void Reset( void ) = 0;

    /*!
     * \brief Wake-ups the radio from Sleep mode
     */
    virtual void Wakeup( void ) = 0;

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
     * \retval      data          Register value
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

    /*!
     * \brief Gets the current status of the radio DIOs
     *
     * \retval      status        [Bit #3: DIO3, Bit #2: DIO2,
     *                             Bit #1: DIO1, Bit #0: BUSY]
     */
    virtual uint8_t GetDioStatus( void ) = 0;

    /*!
     * \brief Returns the device type
     *
     * \retval      0: SX1261, 1: SX1262, 2: SX1268
     */
    virtual uint8_t GetDeviceType( void ) = 0;

    /*!
     * \brief Returns the matching frequency
     *
     * \retval      1: 868 MHz
     *              0: 915 MHz
     */
    virtual uint8_t GetFreqSelect( void ) = 0;
    
    /*!
     * \brief RF Switch power on
     */
    virtual void AntSwOn( void ) = 0;

    /*!
     * \brief RF Switch power off
     */
    virtual void AntSwOff( void ) = 0;

    /*!
     * \brief Process the analysis of radio IRQs and calls callback functions
     *        depending on radio state
     */
    void ProcessIrqs( void );

    void InvokeHandler ( void );
};

#ifdef __cplusplus
}
#endif

#endif // __SX126x_H__
