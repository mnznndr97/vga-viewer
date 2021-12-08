/*
 * sd.c
 *
 *  Created on: Dec 4, 2021
 *      Author: mnznn
 */

#include <sd/sd.h>
#include <crc7.h>
#include <stdio.h>
#include <assertion.h>

extern void Error_Handler();

#define SD_DUMMY_BYTE 0xFF

/// Max size of a SD SPI response
/// \remarks The response formats and sizes are defined in the
/// Physical Layer Simplified Specification Version 8.00 [Section 7.3.2]
#define SD_MAX_RESPONSE_SIZE 5

typedef enum _SDCommand {
    /// Resets the SD memory card
    SDCMD0GoIdleState = 0,
    /// Sends SD
    SDCMD8SendIfCondition = 8,
    /// Reads the OCR of a card.
    /// Response is R3
    SDCMD58ReadOrc = 58
} SDCommand;

static GPIO_TypeDef* _powerGPIO = NULL;
static UInt16 _powerPin = 0;

static GPIO_TypeDef* _nssGPIO = NULL;
static UInt16 _nssPin = 0;
static SPI_HandleTypeDef* _spiHandle = NULL;

static BYTE _responseBuffer[SD_MAX_RESPONSE_SIZE];

/// OCR register definition
/// \remarks The struct is layed out as if read in little endian from an int 32
/// This is what happens when casting the result buffer to an R3 pointer
typedef struct _OCRRegister {
    /********* UInt32 LSB -> Bits from 31 to 24 ******/
    /// Only USH-I card support this bit
    UInt32 Switch1p8Accepted : 1; // Bit 24
    UInt32 Reserved : 2; // Bit 25-26
    /// Over 2TB support Status. Only SDUC card support this bit
    UInt32 CO2T : 1; // Bit 27
    UInt32 Reserved2 : 1; // Bit 28
    UInt32 UHSIICardStatus : 1; // Bit 29
    /// Card Capacity Status. This bit is valid only whe the canrd power up status bit is set
    UInt32 CCS : 1; // Bit 30
    /// Power up status. Set to low if the card has not finished the power up routine
    UInt32 PowerUpStatus : 1; // Bit 31

    /********* UInt32 byte 2 -> Bits from 23 to 16 ******/
    UInt32 v2p8tov2p9 : 1; // Bit 16
    UInt32 v2p9tov3p0 : 1; // Bit 17
    UInt32 v3p0tov3p1 : 1; // Bit 18
    UInt32 v3p1tov3p2 : 1; // Bit 19
    UInt32 v3p2tov3p3 : 1; // Bit 20
    UInt32 v3p3tov3p4 : 1; // Bit 21
    UInt32 v3p4tov3p5 : 1; // Bit 22
    UInt32 v3p5tov3p6 : 1; // Bit 23

    /********* UInt32 byte 3 -> Bits from 15 to 8 ******/
    UInt32 VddWindowReserved1 : 7; // Bits 14 - 8
    UInt32 v2p7tov2p8 : 1; // Bit 15

    /********* UInt32 MSB -> Bits from 7 to 0 ******/
    UInt32 VddWindowReserved : 8; // Bits 7 - 0

} OCRRegister;

typedef struct _ResponseR1 {
    BYTE Idle : 1; // BIT 0
    BYTE EraseReset : 1;
    BYTE IllegalCommand : 1;
    BYTE ComCrcError : 1;
    BYTE EraseSequenceError : 1;
    BYTE AddressError : 1;
    BYTE ParamError : 1;
    BYTE Reserved : 1; // BIT7
} ResponseR1;

typedef struct _ResponseR3 {
    ResponseR1 R1;
    OCRRegister OCR;
} __attribute__((aligned(1), packed)) ResponseR3;

typedef const ResponseR1* PCResponseR1;
typedef const ResponseR3* PCResponseR3;

/// Completly disables the SPI interface
static void ShutdownSDSPIInterface() {
    // We need to complety disable our SPI interface
    // Let's follow the procedure indicate in the SPI chapter of RM0090 [Section 28.3.8]

    /*
     // Let's wait to have received the last data
     while (READ_BIT(_spiHandle->Instance->SR, SPI_SR_RXNE) == 0)
     ;
     // Let' s wait for a transfer to complete (we are in full duplex bidirectional mode)
     while (READ_BIT(_spiHandle->Instance->SR, SPI_SR_TXE) == 0)
     ;
     // Let' s wait for the channel to be freen
     while (READ_BIT(_spiHandle->Instance->SR, SPI_SR_BSY) == 1)
     ;

     */
    __HAL_SPI_DISABLE(_spiHandle);

    // We don't have to do anything with the CS signal for the moment
}

/// Performs a single byte transaction on the SPI interface by writing an reading a byte
/// \param data Data to write on the MOSI channel
/// \return Data read from the SPI from the MISO channel
/// \remarks To read a single byte to the SPI a dummy byte must be provided since we are the master
/// initiating the transaction
static BYTE PerformByteTransaction(BYTE data) {
    // We write the byte into the data register. This will clear the TXE flag [RM0090 - Section 28.3.7]
    _spiHandle->Instance->DR = data;

    UInt32 srRegister = 0;
    UInt32 spiStatusMsk = SPI_SR_TXE_Msk | SPI_SR_RXNE_Msk;

    UInt32 supportedErrorMsk = SPI_SR_MODF_Msk | SPI_SR_OVR_Msk;
    do {
        srRegister = _spiHandle->Instance->SR;

        // We should never get a FrameFormat error since is not used in standard SPI mode
        if ((srRegister & SPI_SR_FRE_Msk) != 0)
            Error_Handler();
        // We should never get a CRC error since is should not be enabled
        if ((srRegister & SPI_SR_CRCERR_Msk) != 0)
            Error_Handler();
        // We should never get an underrun error since is should not be used in SPI mode
        if ((srRegister & SPI_SR_UDR_Msk) != 0)
            Error_Handler();

        if ((srRegister & supportedErrorMsk) != 0) {
            // How to handle this situation?
            Error_Handler();
        }
    } while ((srRegister & spiStatusMsk) != spiStatusMsk);

    // We read the byte from the data register. This will clear the RXNE flag [RM0090 - Section 28.3.7]
    return (BYTE)_spiHandle->Instance->DR;
}

/// Reads a byte from the SPI interface by writing a dummy byte
/// \return Data read from the SPI from the MISO channel
/// \remarks To read a single byte to the SPI a dummy byte must be provided since we are the master
/// initiating the transaction
static BYTE ReadByte() {
    BYTE data = PerformByteTransaction(SD_DUMMY_BYTE);
    //*crc = Crc7Add(*crc, data);
    return data;
}

/// Writes a byte onto the SPI interface using the single byte transaction method and by ignoring the read value
/// \param data Data to write on the MOSI channel
static void WriteByte(BYTE val, BYTE* crc) {
    *crc = Crc7Add(*crc, val);
    PerformByteTransaction(val);
}

static BYTE SendCommand(BYTE command, UInt32 argument, BYTE responseLength) {
    BYTE crc = CRC7_ZERO;
    WriteByte(0x40 | command, &crc);
    WriteByte((BYTE)(argument >> 24), &crc);
    WriteByte((BYTE)(argument >> 16), &crc);
    WriteByte((BYTE)(argument >> 8), &crc);
    WriteByte((BYTE)(argument), &crc);

    BYTE dataF = (BYTE)((crc << 1) | 0x1); // End bit
    WriteByte(dataF, &crc); // We ignore this last crc

    static_assert(sizeof(ResponseR1) == sizeof(BYTE));

    // We start or read loop to get the first byte of the response
    BYTE bytesRead = 0;
    BYTE readValue;
    do {
        // Response bytes may need some iterations so each time we re-initialize the
        readValue = ReadByte();
    } while (readValue == SD_DUMMY_BYTE);

    _responseBuffer[bytesRead++] = readValue;
    // First byte is ALWAYS (per specification, Section 7.3.2.1 and except for SEND_STATUS) a R1 response

    const ResponseR1* r1 = (const ResponseR1*)_responseBuffer;
    DebugAssert(r1->Reserved == 0); // reserved should always be zero

    if (r1->IllegalCommand || r1->ComCrcError) {
        // Per specification, this is the only case where the card outputs only the
        // first byte of the response
        return bytesRead;
    }

    while (bytesRead < responseLength) {
        readValue = ReadByte();
        _responseBuffer[bytesRead++] = readValue;
    }
    return bytesRead;
}

static void SendCommand2(BYTE command, UInt32 argument) {
    BYTE data1 = PerformByteTransaction(0x40 | command);
    BYTE data2 = PerformByteTransaction((BYTE)(argument >> 24));
    BYTE data3 = PerformByteTransaction((BYTE)(argument >> 16));
    BYTE data4 = PerformByteTransaction((BYTE)(argument >> 8));
    BYTE data5 = PerformByteTransaction((BYTE)(argument));
    BYTE data6 = PerformByteTransaction((BYTE)(0x87));

    static_assert(sizeof(ResponseR1) == sizeof(BYTE));

    BYTE rcv1 = PerformByteTransaction(0xFF);
    BYTE rcv2 = PerformByteTransaction(0xFF);
    BYTE rcv3 = PerformByteTransaction(0xFF);
    BYTE rcv4 = PerformByteTransaction(0xFF);
    BYTE rcv5 = PerformByteTransaction(0xFF);
    BYTE rcv6 = PerformByteTransaction(0xFF);
    BYTE rcv7 = PerformByteTransaction(0xFF);
    BYTE rcv8 = PerformByteTransaction(0xFF);

    ResponseR1* r1 = (ResponseR1*)&rcv1;
    ResponseR1* r2 = (ResponseR1*)&rcv2;
    ResponseR1* r3 = (ResponseR1*)&rcv3;
    ResponseR1* r4 = (ResponseR1*)&rcv4;
    ResponseR1* r5 = (ResponseR1*)&rcv5;
    ResponseR1* r6 = (ResponseR1*)&rcv6;
    ResponseR1* r7 = (ResponseR1*)&rcv7;
    ResponseR1* r8 = (ResponseR1*)&rcv8;
}

/// \brief Verifies the voltage level of our connected SD card by issuing a CMD58 as specified in the Physical Layer Specification
/// @return Status of the operation
static SDStatus VerifyVoltageLevel() {
    //  From Physical Layer Specification, section 7.2.1 we can issue an optional CMD58 to verify that the voltage level we are using is
    // supported

    static_assert(sizeof(ResponseR3) == 5);
    static_assert(sizeof(OCRRegister) == sizeof(Int32));
    BYTE bytesRcv = SendCommand(SDCMD58ReadOrc, 0x0, sizeof(ResponseR3));

    // Card is not supported
    if (bytesRcv == 1) {
        PCResponseR1 r1 = (PCResponseR1)_responseBuffer;
        if (r1->IllegalCommand)
            return SDStatusNotSDCard;
        else
            Error_Handler();
    }

    PCResponseR3 r3 = (PCResponseR3)_responseBuffer;
    // Checking our voltage range
    if (!(r3->OCR.v2p7tov2p8) || !(r3->OCR.v2p8tov2p9) || !(r3->OCR.v2p9tov3p0)) {
        return SDStatusVoltageNotSupported;
    }

    if (r3->OCR.PowerUpStatus == 0) {
        return SDStatusPowerUpNotCompleted;
    }

    // Ignoring CSS for the moment
    return SDStatusOk;
}

// ##### Public Function definitions #####

SDStatus SDInitialize(GPIO_TypeDef* powerGPIO, UInt16 powerPin, SPI_HandleTypeDef* spiHandle) {
    _powerGPIO = powerGPIO;
    _powerPin = powerPin;

    _nssGPIO = GPIOB;
    _nssPin = GPIO_PIN_12;

    _spiHandle = spiHandle;

    // To initialize the SD layer, let's perform a power cycle in case our system where resetted but the SD card didn' t have time to perform
    // the power cycle correctly
    return SDPerformPowerCycle();
}

SDStatus SDPerformPowerCycle() {
    // Preamble: we shutdown the SPI interface to later re-enabled it
    ShutdownSDSPIInterface();

    /* Reference specification : Physical Layer Simplified Specification Version 8.00[Section 6.4.1.2] */

    // We need to lower the VDD level under the 0.5V threshold for at least 1 ms
    // The other lines should also be low in this phase
    // Let's do 10ms to be sure
    HAL_GPIO_WritePin(_powerGPIO, _powerPin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(_nssGPIO, _nssPin, GPIO_PIN_RESET);
    HAL_Delay(10);

    // We reset the GPIO to an output push pull state
    CLEAR_BIT(_nssGPIO->OTYPER, _nssPin);
    // CS shall be kept high during initialization as stated in section 6.4.1.1
    HAL_GPIO_WritePin(_nssGPIO, _nssPin, GPIO_PIN_SET);

    // We can now enable the GPIO, and wait another 1 ms (at least) to let the VDD stabilize
    // There are some constraints also on the slope on the VDD change but we can do little about this
    HAL_GPIO_WritePin(_powerGPIO, _powerPin, GPIO_PIN_SET);
    HAL_Delay(10);

    // After these step, the SD card (if alredy inserted) should be ready to receive the initialization sequence
    return SDStatusOk;
}

SDStatus SDTryConnect() {
    // We assume here that a power cycle was already performed and VDD is stable
    // We can start the initialization sequence by putting the card in the idle state

    // From Section 6.4.1.4, we supply at leas 74 clock cycles to the SD cart
    // We do this by simply enabling the SPI
    __HAL_SPI_ENABLE(_spiHandle);

    // We cannot measure exactly the 74 clock cycles so let' s just wait a little more
    HAL_Delay(10);

    HAL_GPIO_WritePin(_nssGPIO, _nssPin, GPIO_PIN_RESET);
    BYTE reponseLength = SendCommand(SDCMD0GoIdleState, 0, 1);

    // If everything is ok we should get an idle state
    if (((ResponseR1*)_responseBuffer)->Idle == 0) {
        Error_Handler();
    }

    //SendCommand2(SD_CMD_8, 0x1aa);
    SDStatus status = SDStatusOk;
    if ((status = VerifyVoltageLevel()) != SDStatusOk) {
        Error_Handler();
    }

    /************************************************************/

    // Preamble: we shutdown the SPI interface to later re-enabled it
    ShutdownSDSPIInterface();

    /* Reference specification : Physical Layer Simplified Specification Version 8.00[Section 6.4.1.2] */

    // We need to lower the VDD level under the 0.5V threshold for at least 1 ms
    // The other lines should also be low in this phase
    // Let's do 10ms to be sure
    HAL_GPIO_WritePin(_powerGPIO, _powerPin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(_nssGPIO, _nssPin, GPIO_PIN_RESET);
    HAL_Delay(10);
}
