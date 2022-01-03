/*
 * sd.c
 *
 *  Created on: Dec 4, 2021
 *      Author: mnznn
 */

#include <sd/sd.h>
#include <sd/csd.h>
#include <sd/ocr.h>
#include <crc/crc7.h>
#include <crc/crc16.h>
#include <stdio.h>
#include <string.h>
#include <assertion.h>
#include <binary.h>
#include <console.h>

extern void Error_Handler();

#define SD_DUMMY_BYTE 0xFF
#define SD_MIN_FREQ 100000

/// Max size of a SD SPI response
/// \remarks The response formats and sizes are defined in the
/// Physical Layer Simplified Specification Version 8.00 [Section 7.3.2]
#define SD_MAX_RESPONSE_SIZE 5
/// Max size of a SD SPI register
/// \remarks The max size should be 64 bytes for SSr register [Section 3.7.1] but we are not using it
#define SD_MAX_REGISTER_SIZE 16

#define SD_DATA_BLOCK_SIZE 512
#define SD_DATA_CRC_SIZE 2

/// Voltage Supplied (VHS) parameter for CMD8
#define SD_CMD8_VHS_2p7To3p6 (0x1U << 8)

#define SD_CMD59_CRC_ON 0x1
#define SD_CMD59_CRC_OFF 0x0

#define SD_ERROR_TOKEN_ERROR 0x01
#define SD_ERROR_TOKEN_CCERROR 0x02
#define SD_ERROR_TOKEN_ECCFAILED 0x04
#define SD_ERROR_TOKEN_OUTOFRANGE 0x08

/// Custom defined generic timeout for a command response
#define SD_RESPONSE_TIMEOUT 5000
/// As specified in the Physical Layer Simplified Specification Version 8.00 [Section 4.2.3, Card Initialization and Identification Process]
/// we should use a 1sec timeout when waiting the device to become ready
#define SD_ACMD41_LOOP_TIMEOUT 1000

typedef enum _SDCommand {
    /// Resets the SD memory card
    SdCmd0GoIdleState = 0,
    /// Sends SD
    SdCmd8SendIfCondition = 8,
    /// Asks the selected card to send it's Card Specific Data
    SdCmd9SendCSD = 9,
    /// Asks the selected card to send it's Card Identification Data
    SdCmd10SendCID = 10,
    SdCmd12StopTransmission = 12,
    /// Set block length in memory access
    SdCmd16SetBlockLen = 16,
    /// Reads a block of the size selected by SET_BLOCKLEN command
    SdCmd17ReadSingleBlock = 17,
    /// Continuously transfers data blocks from card to host untill interrupted by a STOP_TRANSMISSION command
    SdCmd18ReadMultipleBlock = 18,
    /// Defines to the card that the next command is an application specific comamnd
    /// Response is R1
    SdCmd55AppCmd = 55,
    SdCmd56GenCmd = 56,
    /// Reads the OCR of a card.
    /// Response is R3
    SdCmd58ReadOrc = 58,
    /// Enables or disables the crc option
    SdCmd59CRCOnOff = 59
} SdCommand;

typedef enum _SDAppCommand {
    /// Sends host capacity support information and activates the card initialization process.
    /// Response is R1
    SdACmd41SendOpCond = 41
} SDAppCommand;

/// Completly disables the SPI interface

static void ShutdownSPIInterface();
/// Select the SPI by asserting LOW the NSS pin
static void SelectCard();
/// Let's deselect the SPI by asserting HIGH the NSS pin
static void DeselectCard();
/// Performs a single byte transaction on the SPI interface by writing an reading a byte
/// \param data Data to write on the MOSI channel
/// \return Data read from the SPI from the MISO channel
/// \remarks To read a single byte to the SPI a dummy byte must be provided since we are the master
/// initiating the transaction
/// \par Contract
/// \parblock
///
/// \endparbloc
static BYTE PerformByteTransaction(BYTE data);
/// Reads a byte from the SPI interface by writing a dummy byte
/// \return Data read from the SPI from the MISO channel
/// \remarks To read a single byte to the SPI a dummy byte must be provided since we are the master
/// initiating the transaction
static BYTE ReadByte();
/// Writes a byte onto the SPI interface using the single byte transaction method and by ignoring the read value
/// \param data Data to write on the MOSI channel
static void WriteByte(BYTE val, BYTE* crc);
/// Base implementation of single command transaction (request send + response).
/// \param command Commadn to send
/// \param argument Argumento of the command
/// \param responseLength Length of the response to read
/// \return Bytes received or error status (negative return). Errors can be
///     SdStatusCommunicationTimeout if the SD is not connected/responding
///
/// \remarks The function does not change the NSS signal
static SBYTE PerformCommandTransaction(BYTE command, UInt32 argument, BYTE responseLength);
/// Sends a generic command to the SD card
/// @param command Command to send
/// @param argument Argument of the command
/// @param responseLength Length of the response to receive
/// @return Status of the operation (bytes read if > 0, error if < 0)
static SBYTE SendCommand(BYTE aCommand, UInt32 argument, BYTE responseLength);
/// Sends an application specific command to the SD card [defined in Table 7-4]
/// @param command Command to send
/// @param argument Argument of the command
/// @param responseLength Length of the response to receive
/// @return Status of the operation (bytes read if > 0, error if < 0)
static SBYTE SendAppCommand(BYTE aCommand, UInt32 argument, BYTE responseLength);
/// Verifies the voltage level of our connected SD card by issuing a CMD8 as specified in the Physical Layer Specification
/// This must be done immediatly after the CMD0. If the card responds with an illegal command, the card is 1.X version, >= 2.0 otherwhise
static SdStatus CheckVddRange();
/// Verifies the voltage level of our connected SD card by issuing a CMD58 as specified in the Physical Layer Specification
/// @return Status of the operation
static SdStatus VerifyVoltageLevel();
/// Enables the CRC check of the communication
/// @return Status of the operation0
static SdStatus EnableCRC();
/// Waits the card is ready by issuing a ACMD41 and waiting that idle state is zero
/// @return Status of the operation
static SdStatus SDWaitForReady();
static SdStatus VerifyCardCapacityStatus();
static BOOL IsErrorToken(BYTE data);
static SdStatus ConvertErrorToken(BYTE token);
/// Reads a data block in the specified section and checks it checksum
/// @param destination Destination buffer
/// @param blockSize Size of data to read
/// @return Status of the operation
static SdStatus ReadDataBlock(BYTE* destination, UInt16 blockSize);
static SdStatus ReadRegister(SdCommand readCommand, UInt16 length);
/// Changes the block length of memory access in the SD card
/// @return Status of the operation
static SdStatus FixReadBlockLength();
static SdStatus ReadCSD();
/// Apply all the necessary changes to the SD interface with the information provided in the CSD register
/// @return Status of the operation
static SdStatus FixWithCSDRegister();

// ##### Private Implementation #####

static GPIO_TypeDef* _powerGPIO = NULL;
static UInt16 _powerPin = 0;

static GPIO_TypeDef* _nssGPIO = NULL;
static UInt16 _nssPin = 0;
static SPI_HandleTypeDef* _spiHandle = NULL;

/// Static allocated buffer wherethe response will be read
static BYTE _responseBuffer[SD_MAX_RESPONSE_SIZE];

/// Static allocated buffer where a data block will be read togheter with the related crc
static BYTE _registersBuffer[SD_MAX_REGISTER_SIZE];
static SDDescription _attachedSdCard;

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

typedef struct _ResponseR7 {
    ResponseR1 R1;
    BYTE Raw[4];
} __attribute__((aligned(1), packed)) ResponseR7;

typedef const ResponseR1* PCResponseR1;
typedef const ResponseR3* PCResponseR3;
typedef const ResponseR7* PCResponseR7;

static void ShutdownSPIInterface() {
    // We need to complety disable our SPI interface
    // Let's follow the procedure indicate in the SPI chapter of RM0090 [Section 28.3.8]

    // We should not have anything running so we simply disable the SPI and we reset the frequency at the minimum
    __HAL_SPI_DISABLE(_spiHandle);

    // We reset the minimum baud rate
    SET_BIT(_spiHandle->Instance->CR1, SPI_CR1_BR);

    // SD SPI frequency should be at least 100KHz
    DebugAssert((((float)HAL_RCC_GetPCLK1Freq()) / 256.0f) > SD_MIN_FREQ);

    // We don't have to do anything with the CS signal for the moment
}

static void SelectCard() {
    HAL_GPIO_WritePin(_nssGPIO, _nssPin, GPIO_PIN_RESET);
}

static void DeselectCard() {
    HAL_GPIO_WritePin(_nssGPIO, _nssPin, GPIO_PIN_SET);
}

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
        // We should never get a CRC error since it should not be enabled
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

static BYTE ReadByte() {
    BYTE data = PerformByteTransaction(SD_DUMMY_BYTE);
    //*crc = Crc7Add(*crc, data);
    return data;
}

static void WriteByte(BYTE val, BYTE* crc) {
    *crc = Crc7Add(*crc, val);
    PerformByteTransaction(val);
}

static SBYTE PerformCommandTransaction(BYTE command, UInt32 argument, BYTE responseLength) {
    BYTE crc = CRC7_ZERO;
    WriteByte(0x40 | command, &crc);
    WriteByte((BYTE)(argument >> 24), &crc);
    WriteByte((BYTE)(argument >> 16), &crc);
    WriteByte((BYTE)(argument >> 8), &crc);
    WriteByte((BYTE)(argument), &crc);

    BYTE dataF = (BYTE)((crc << 1) | 0x1); // End bit
    WriteByte(dataF, &crc); // We ignore this last crc

    static_assert(sizeof(ResponseR1) == sizeof(BYTE));
    DebugAssert(responseLength > 0 && responseLength <= SD_MAX_RESPONSE_SIZE);

    // We start or read loop to get the first byte of the response
    BYTE bytesRead = 0;
    BYTE readValue;
    UInt32 startTick = HAL_GetTick();
    do {
        // Response bytes may need some iterations so each time we re-initialize the
        readValue = ReadByte();
    } while (readValue == SD_DUMMY_BYTE && ((HAL_GetTick() - startTick) < SD_RESPONSE_TIMEOUT));

    // We have not received an answer (the first bit of a response is always zero)
    if (readValue == SD_DUMMY_BYTE) {
        return SdStatusCommunicationTimeout;
    }

    _responseBuffer[bytesRead++] = readValue;
    // First byte is ALWAYS (per specification, Section 7.3.2.1 and except for SEND_STATUS) a R1 response

    const ResponseR1* r1 = (const ResponseR1*)_responseBuffer;
    DebugAssert(r1->Reserved == 0); // reserved should always be zero

    // Per response specification, CRC + Illegal command seems to be the only cases where the card outputs only the
    // first byte of the response
    // Le't s handle also the others here
    if (r1->IllegalCommand)
        return SdStatusIllegalCommand;
    else if (r1->ComCrcError)
        return SdStatusCRCError;
    else if (r1->AddressError)
        return SdStatusMisalignedAddress;
    else if (r1->ParamError)
        return SdStatusParameterOutOfRange;
    else if (r1->EraseReset || r1->EraseSequenceError)
        return SdStatusErrorUnknown;
    // We cannot perform a check on the Idle flag here since the card can be in the initialization status
    // Idle checks are delegated to the caller

    // Responses don't have a CRC, so we simply read the  requested number of bytes
    while (bytesRead < responseLength) {
        readValue = ReadByte();
        _responseBuffer[bytesRead++] = readValue;
    }
    return (SBYTE)bytesRead;
}

static SBYTE SendCommand(BYTE command, UInt32 argument, BYTE responseLength) {
    SelectCard();

    SBYTE bytesRead = PerformCommandTransaction(command, argument, responseLength);

    DeselectCard();
    return bytesRead;
}

static SBYTE SendAppCommand(BYTE aCommand, UInt32 argument, BYTE responseLength) {
    // Let's select the SPI by asserting LOW the NSS pin
    HAL_GPIO_WritePin(_nssGPIO, _nssPin, GPIO_PIN_RESET);

    // Before sending an app command we must send a CMD55
    SBYTE bytesRead = PerformCommandTransaction(SdCmd55AppCmd, 0x0, sizeof(ResponseR1));
    if (bytesRead < 0) {
        goto cleanup;
        // Error. We can stop here
    }

    // Send the application specific command
    bytesRead = PerformCommandTransaction(aCommand, argument, responseLength);

cleanup:

    // Let's deselect the SPI by asserting HIGH the NSS pin
    HAL_GPIO_WritePin(_nssGPIO, _nssPin, GPIO_PIN_SET);
    return bytesRead;
}

static SdStatus CheckVddRange() {
    // Check if 2.7-3.6v range is supported

    printf("Checking VDD range with CMD8\r\n");
    BYTE checkPattern = 0xda;
    UInt32 argument = SD_CMD8_VHS_2p7To3p6 | checkPattern;
    SBYTE bytesRcv = SendCommand(SdCmd8SendIfCondition, argument, sizeof(ResponseR7));

    // Communication error. NB: Illegal command here MUST be filtered since it indicates
    // that the card is an older version
    SdStatus status = SdStatusOk;
    if (bytesRcv < 0 && (status = (SdStatus)bytesRcv) != SdStatusIllegalCommand) {
        return (SdStatus)bytesRcv;
    }

    if (status == SdStatusIllegalCommand) {
        printf("CMD8 is not supported. SD card is 1.x version\r\n");
        _attachedSdCard.Version = SdVer1pX;
        return SdStatusOk;
    }

    printf("CMD8 supported. SD card is 2.x version or later\r\n");
    // Command is valid. Let's check if voltage is supported and pattern is valid
    _attachedSdCard.Version = SdVer2p0OrLater;

    UInt32 resultLe = ReadUInt32(((PCResponseR7)_responseBuffer)->Raw);
    UInt32 result = U32ChangeEndiannes(resultLe);
    if ((result & 0xFF) != checkPattern) {
        return SdStatusReadCorrupted;
    }
    printf("CMD8 Check pattern OK\r\n");

    if ((result & 0xF00) != SD_CMD8_VHS_2p7To3p6) {
        return SdStatusVoltageNotSupported;
    }
    printf("CMD8 Voltage OK\r\n");
    return SdStatusOk;
}

static SdStatus VerifyVoltageLevel() {
    // From Physical Layer Specification, section 7.2.1 we can issue an optional CMD58 to verify that the voltage level we are using is
    // supported

    static_assert(sizeof(ResponseR3) == 5);
    static_assert(sizeof(OCRRegister) == sizeof(Int32));
    SBYTE bytesRcv = SendCommand(SdCmd58ReadOrc, 0x0, sizeof(ResponseR3));

    printf("Checking voltage levels with CMD58. Reading OCR\r\n");

    if (bytesRcv < 0) {
        // Per initialization process specifications, if the CMD58 fails with an IllegalCommand,
        // we are not communicating with an SD card
        // Card is not supported
        SdStatus status = (SdStatus)bytesRcv;
        return status == SdStatusIllegalCommand ? SdStatusNotSDCard : status;
    }

    PCResponseR3 r3 = (PCResponseR3)_responseBuffer;
    // Checking our voltage range
    if (!(r3->OCR.v2p7tov2p8) || !(r3->OCR.v2p8tov2p9) || !(r3->OCR.v2p9tov3p0)) {
        return SdStatusVoltageNotSupported;
    }

    // No other flags should be checked in this stage since the card is still not initialized
    return SdStatusOk;
}

static SdStatus EnableCRC() {
    DebugAssert(_attachedSdCard.Version != SdVerUnknown);

    printf("Re-enabling CRC checks for commands\r\n");
    SBYTE bytesRcv = SendAppCommand(SdCmd59CRCOnOff, SD_CMD59_CRC_ON, sizeof(ResponseR1));
    if (bytesRcv < 0) {
        return (SdStatus)bytesRcv;
    }
    return SdStatusOk;
}

static SdStatus SDWaitForReady() {
    UInt32 startTick = HAL_GetTick();
    volatile PCResponseR1 pResponse = (PCResponseR1)_responseBuffer;

    printf("Waiting SD to became READY (Idle = 0)\r\n");
    DebugAssert(_attachedSdCard.Version != SdVerUnknown);
    SdStatus status = SdStatusOk;

    UInt32 hcs = _attachedSdCard.Version == SdVer2p0OrLater ? (1U << 30) : 0;
    do {
        // We don't support SDHC or SDXC, so we keep the HCS (Host Capacity Support) bit to zero
        // even if the SDversion is 2.0 or later

        SBYTE bytesRcv = SendAppCommand(SdACmd41SendOpCond, hcs, sizeof(ResponseR1));
        if (bytesRcv < 0 && (status = (SdStatus)bytesRcv) != SdStatusIllegalCommand) {
            return (SdStatus)bytesRcv;
        }
        else if (status == SdStatusIllegalCommand) {
            // Per initialization process specifications, if the AMCD41 fails with an Illegal command
            // we are not talking to a SD card
            return SdStatusNotSDCard;
        }
    } while (pResponse->Idle && ((HAL_GetTick() - startTick) < SD_ACMD41_LOOP_TIMEOUT));

    if (pResponse->Idle) {
        return SdStatusInitializationTimeout;
    }
    return status;
}

static SdStatus VerifyCardCapacityStatus() {
    DebugAssert(_attachedSdCard.Version != SdVerUnknown);

    printf("Verifing card capacity with OCR\r\n");
    if (_attachedSdCard.Version == SdVer1pX) {
        // No need to read OCR [Se Figure 7-2, SPI Mode initialization flow]
        _attachedSdCard.Capacity = SdCapacityStandard;
        _attachedSdCard.AddressingMode = SDAddressingModeByte;

        printf("SD card version is 1.x. Assuming Standard capacty and byte addressing mode\r\n");
    }
    else {
        SBYTE bytesRcv = SendCommand(SdCmd58ReadOrc, 0x0, sizeof(ResponseR3));
        if (bytesRcv < 0) {
            return (SdStatus)bytesRcv;
        }

        // If we read less bytes than what we have specified, we have a problem in our program
        DebugAssert(bytesRcv == sizeof(ResponseR3));

        PCResponseR3 r3 = (PCResponseR3)_responseBuffer;
        if (r3->OCR.CCS) {
            printf("Card Capacity Status is set. Assuming Extended capacty and sector addressing mode\r\n");
            _attachedSdCard.Capacity = SdCapacityExtended;
            _attachedSdCard.AddressingMode = SDAddressingModeSector;
        }
        else {
            printf("Card Capacity Status is NOT set. Assuming Standard capacty and byte addressing mode\r\n");
            _attachedSdCard.Capacity = SdCapacityStandard;
            _attachedSdCard.AddressingMode = SDAddressingModeByte;
        }
    }

    return SdStatusOk;
}

BOOL IsErrorToken(BYTE data) {
    // Error token are in the format 0b0000XXXX
    return (((data & 0xF0) == 0x0) && ((data & 0x0F) != 0x0));
}

static SdStatus ConvertErrorToken(BYTE token) {
    DebugAssert(IsErrorToken(token));

    if (token & SD_ERROR_TOKEN_CCERROR) {
        return SdStatusReadCCError;
    }
    else if (token & SD_ERROR_TOKEN_ECCFAILED) {
        return SdStatusECCFailed;
    }
    else if (token & SD_ERROR_TOKEN_OUTOFRANGE) {
        return SdStatusParameterOutOfRange;
    }
    else {
        return SdStatusErrorUnknown;
    }
}

static SdStatus ReadDataBlock(BYTE* destination, UInt16 blockSize) {
    const BYTE startBlock = 0xFE;

    // First wait for data block
    BYTE readValue;
    UInt32 startTick = HAL_GetTick();
    do {
        // Response bytes may need some iterations so each time we re-initialize the
        readValue = ReadByte();
    } while ((readValue != startBlock && !IsErrorToken(readValue)) && ((HAL_GetTick() - startTick) < SD_RESPONSE_TIMEOUT));

    // We have not received an answer
    if (IsErrorToken(readValue)) {
        return ConvertErrorToken(readValue);
    }
    else if (readValue != startBlock) {
        return SdStatusCommunicationTimeout;
    }

    UInt16 crc = CRC16_ZERO;
    /* We read our data bloxk into the destination buffer*/
    for (int i = 0; i < blockSize; i++) {
        BYTE data = ReadByte();
        crc = Crc16Add(crc, data);
        destination[i] = data;
    }

    /* Eventually we received the last bytes of CRC */
    for (int i = 0; i < SD_DATA_CRC_SIZE; i++) {
        BYTE data = ReadByte();
        crc = Crc16Add(crc, data);
    }

    // If CRC is not zero, we have a communication problem
    if (crc != 0) {
        return SdStatusReadCorrupted;
    }
    return SdStatusOk;
}

static SdStatus ReadRegister(SdCommand readCommand, UInt16 length) {
    DebugAssert(length <= SD_MAX_REGISTER_SIZE);

    // Let's select the SPI by asserting LOW the NSS pin
    HAL_GPIO_WritePin(_nssGPIO, _nssPin, GPIO_PIN_RESET);

    SdStatus result;
    SBYTE bytesRcv = PerformCommandTransaction(readCommand, 0x0, sizeof(ResponseR1));
    // Communication error
    if (bytesRcv < 0) {
        result = (SdStatus)bytesRcv;
        goto cleanup;
    }

    // If we arrived here, no errors occured. Let's assert we are not in Idle = 1 (initialization still running)
    // No registers should be read (with this function) in the initialization process
    DebugAssert(((PCResponseR1)_responseBuffer)->Idle == 0);

    result = ReadDataBlock(_registersBuffer, length);

cleanup: HAL_GPIO_WritePin(_nssGPIO, _nssPin, GPIO_PIN_SET);
    return result;
}

static SdStatus FixReadBlockLength() {
    printf("Fixing read block length\r\n");

    SBYTE result = SendCommand(SdCmd16SetBlockLen, SD_DATA_BLOCK_SIZE, sizeof(ResponseR1));
    if (result < 0) return (SdStatus)result;

    return SdStatusOk;
}

static SdStatus ReadCSD() {
    printf("Reading card CSD register\r\n");

    SdStatus result = ReadRegister(SdCmd9SendCSD, SD_CSD_SIZE);
    if (result != SdStatusOk) {
        return result;
    }
    PCCsdRegister csd = (PCCsdRegister)_registersBuffer;
    _attachedSdCard.CSDValidationStatus = SdCsdValidate(csd);
    if (_attachedSdCard.CSDValidationStatus != SdCsdValidationOk) {
        printf("CSD validation failed: ");
        SdCsdDumpValidationResult(_attachedSdCard.CSDValidationStatus);
        printf("\r\n");
        return SdStatusInvalidCSD;
    }

    return SdStatusOk;
}

SdStatus FixWithCSDRegister() {
    PCCsdRegister csd = (PCCsdRegister)_registersBuffer;
    _attachedSdCard.MaxTransferSpeed = SdCsdGetMaxTransferRate(csd);
    _attachedSdCard.BlockLen = SdCsdGetMaxReadDataBlockLength(csd);

    printf("Max transfer speed is ");
    FormatFrequency((float)_attachedSdCard.MaxTransferSpeed);
    printf("\r\n");

    printf("Read block length is %" PRIu16 " bytes\r\n", _attachedSdCard.BlockLen);

    // SPI2 is on PCLK1
    UInt32 spi2Freq = HAL_RCC_GetPCLK1Freq();
    // As stated in the RM0090, the SPI baud rate control can be changed with the peripheral enabled but not when a transfer is ongoing
    DebugAssert(((UInt32)((float)spi2Freq / 2.0f)) < _attachedSdCard.MaxTransferSpeed);

    // Max baud rate
    CLEAR_BIT(_spiHandle->Instance->CR1, SPI_CR1_BR);

    return SdStatusOk;
}

// ##### Public Function definitions #####

SdStatus SdInitialize(GPIO_TypeDef* powerGPIO, UInt16 powerPin, SPI_HandleTypeDef* spiHandle) {
    if (powerGPIO == NULL || spiHandle == NULL)
        return SdStatusInvalidParameter;

    _powerGPIO = powerGPIO;
    _powerPin = powerPin;

    _nssGPIO = GPIOB;
    _nssPin = GPIO_PIN_12;

    _spiHandle = spiHandle;

    // SPI assertion (follows initialization order defined in Chapter 28.3.3 of RM0090)

    // SPI Simplified specification does not specify the correct polarity and phase
    // Let's stick to Chan description http://elm-chan.org/docs/mmc/mmc_e.html
    DebugAssert((_spiHandle->Instance->CR1 & SPI_CR1_CPOL) == SPI_POLARITY_LOW);
    DebugAssert((_spiHandle->Instance->CR1 & SPI_CR1_CPHA) == SPI_PHASE_1EDGE);
    // SPI mode for SD is 8 bit
    DebugAssert((_spiHandle->Instance->CR1 & SPI_CR1_DFF) == SPI_DATASIZE_8BIT);
    // First bit transferred is MSB
    DebugAssert((_spiHandle->Instance->CR1 & SPI_CR1_LSBFIRST) == SPI_FIRSTBIT_MSB);
    // Slave select must be managed by software
    DebugAssert((_spiHandle->Instance->CR1 & SPI_CR1_SSM) == SPI_NSS_SOFT);
    // Slave select must be managed by software
    DebugAssert((_spiHandle->Instance->CR2 & SPI_CR2_FRF) == SPI_TIMODE_DISABLE);
    // We are the master of the communication
    DebugAssert((_spiHandle->Instance->CR1 & SPI_CR1_MSTR) != 0);
    // We are in full duplex mode (2 unidirectional lines)
    DebugAssert((_spiHandle->Instance->CR1 & SPI_CR1_BIDIMODE) == 0);
    DebugAssert((_spiHandle->Instance->CR1 & SPI_CR1_RXONLY) == 0);

    return SdStatusOk;
}

SdStatus SdPerformPowerCycle() {
    SdStatus shutdownStatus = SdShutdown();
    if (shutdownStatus != SdStatusOk) {
        return shutdownStatus;
    }

    printf("Performing SD power on cycle ...\r\n");
    // We reset the GPIO to an output push pull state
    CLEAR_BIT(_nssGPIO->OTYPER, _nssPin);
    // CS shall be kept high during initialization as stated in section 6.4.1.1
    HAL_GPIO_WritePin(_nssGPIO, _nssPin, GPIO_PIN_SET);

    // We can now enable the GPIO, and wait another 1 ms (at least) to let the VDD stabilize
    // There are some constraints also on the slope on the VDD change but we can do little about this
    HAL_GPIO_WritePin(_powerGPIO, _powerPin, GPIO_PIN_SET);
    HAL_Delay(10);

    // Let's clear the attacched card informations
    memset(&_attachedSdCard, 0, sizeof(SDDescription));

    // After these step, the SD card (if alredy inserted) should be ready to receive the initialization sequence
    return SdStatusOk;
}

SdStatus SdTryConnect() {
    // We assume here that a power cycle was already performed and VDD is stable
    // We can start the initialization sequence by putting the card in the idle state

    // From Section 6.4.1.4, we supply at least 74 clock cycles to the SD card
    // To let the SCK run, as we can see in the SPI chapter of RM0090 [Figure 258]
    // we need to issue a byte transfer

    // So, firstly we enable the SPI
    __HAL_SPI_ENABLE(_spiHandle);

    printf("Providing initialization clock\r\n");

    // Then whe sent 10 dummy bytes which correspond to 80 SPI clock cycles
    BYTE dummyCrc = CRC7_ZERO;
    for (size_t i = 0; i < 10; i++) {
        WriteByte(SD_DUMMY_BYTE, &dummyCrc);
    }

    printf("Sending CMD_0\r\n");
    // After the Power On cycle, we are ready to issue a CMD0 with the NSS pin asserted to LOW
    HAL_GPIO_WritePin(_nssGPIO, _nssPin, GPIO_PIN_RESET);
    SBYTE responseLength = SendCommand(SdCmd0GoIdleState, 0, 1);
    if (responseLength < 0) {
        // Error for CMD0 -> We cannot go further or SPI is not supported
        return (SdStatus)responseLength;
    }
    else if (((ResponseR1*)_responseBuffer)->Idle == 0) {
        // If everything is ok we should get an idle state
        return SdStatusErrorUnknown;
    }

    // Initialization step: ask the card if the specified VHS is supported
    SdStatus status;
    if ((status = CheckVddRange()) != SdStatusOk) {
        return status;
    }

    // Initialization step (optional): query the card for the supported voltage ranges through OCR
    if ((status = VerifyVoltageLevel()) != SdStatusOk) {
        return status;
    }

    // As stated in the "Bus Transfer Protection" [Section 7.2.2] of the SD Specification,
    // CRC should be enabled before issuing ACMD41
    // On some of my SD card this command returned an error so let's just ignore it for the moment
    // (The command should be mandatory though)
    // We just keep computing the CRC7 (commands are not too big so we can affor this waste)
    if ((status = EnableCRC()) != SdStatusOk) {
        printf("Cannot enable CRC: ");
        SdDumpStatusCode(status);
        printf("\r\n");
    }

    // Initialization step: wait for initialization
    if ((status = SDWaitForReady()) != SdStatusOk) {
        return status;
    }

    // Initialization step (last): verify Card Capacity Status
    if ((status = VerifyCardCapacityStatus()) != SdStatusOk) {
        return status;
    }

    if (_attachedSdCard.Capacity == SdCapacityStandard) {
        // As we can see in the CSD version 1.0, Standard Capacity card might have a sector size != 512 so we need to correct it
        FixReadBlockLength();
    }

    if ((status = ReadCSD()) != SdStatusOk) {
        return status;
    }

    if ((status = FixWithCSDRegister()) != SdStatusOk) {
        return status;
    }

    return SdStatusOk;
}

SdStatus SdDisconnect() {
    // Preamble: we shutdown the SPI interface to later re-enabled it
    ShutdownSPIInterface();
    // We leave the NSS high
    HAL_GPIO_WritePin(_nssGPIO, _nssPin, GPIO_PIN_SET);

    return SdStatusOk;
}

SdStatus SdReadSector(BYTE* destination, UInt32 sector) {
    UInt32 address = sector;
    if (_attachedSdCard.AddressingMode == SDAddressingModeByte) {
        address *= _attachedSdCard.BlockLen;
    }

    // Let's select the SPI by asserting LOW the NSS pin
    HAL_GPIO_WritePin(_nssGPIO, _nssPin, GPIO_PIN_RESET);

    SdStatus result;
    SBYTE commandResult = PerformCommandTransaction(SdCmd17ReadSingleBlock, address, sizeof(ResponseR1));
    if (commandResult < 0) {
        result = (SdStatus)commandResult;
        goto cleanup;
    }

    result = ReadDataBlock(destination, _attachedSdCard.BlockLen);
    if (result != SdStatusOk) {
        goto cleanup;
    }

    result = SdStatusOk;
cleanup: HAL_GPIO_WritePin(_nssGPIO, _nssPin, GPIO_PIN_SET);
    return result;
}

SdStatus SdReadSectors(BYTE* destination, UInt32 sector, UInt32 count) {
    UInt32 address = sector;

    UInt16 blockSize = _attachedSdCard.BlockLen;
    if (_attachedSdCard.AddressingMode == SDAddressingModeByte) {
        address *= blockSize;
    }

    // Let's select the SPI by asserting LOW the NSS pin
    HAL_GPIO_WritePin(_nssGPIO, _nssPin, GPIO_PIN_RESET);

    SdStatus result;
    SBYTE commandResult = PerformCommandTransaction(SdCmd18ReadMultipleBlock, address, sizeof(ResponseR1));
    if (commandResult < 0) {
        result = (SdStatus)commandResult;
        goto cleanup;
    }

    do {
        result = ReadDataBlock(destination, blockSize);
        if (result != SdStatusOk) {
            // Do we need to send a stop command here?
            goto cleanup;
        }

        // Move destination ptr forward
        destination += blockSize;
    } while (--count);

    commandResult = PerformCommandTransaction(SdCmd12StopTransmission, 0, sizeof(ResponseR1));
    if (commandResult < 0) {
        result = (SdStatus)commandResult;
        // In our test SD the CMD 12 returns a R1 with all the bits set to 1
        // but everything works fines, so let's forget about the error
        // goto cleanup;
    }

    BYTE lineBusy;
    do {
        lineBusy = ReadByte();
    } while (lineBusy == 0);

    result = SdStatusOk;
cleanup: HAL_GPIO_WritePin(_nssGPIO, _nssPin, GPIO_PIN_SET);
    return result;
}

void SdDumpStatusCode(SdStatus status) {
    switch (status) {
    case SdStatusOk:
        printf("Ok");
        break;
    case SdStatusInvalidParameter:
        printf("Invalid parameter");
        break;
    case SdStatusCommunicationTimeout:
        printf("Comm Timeout");
        break;
    case SdStatusNotSDCard:
        printf("Device is not an SD card");
        break;
    case SdStatusVoltageNotSupported:
        printf("SD card does not support the supplied voltage");
        break;
    case SdStatusInitializationTimeout:
        printf("SD card initialization timeout");
        break;
    case SdStatusCRCError:
        printf("Command CRC is not valid");
        break;
    case SdStatusIllegalCommand:
        printf("Illegal command");
        break;
    case SdStatusMisalignedAddress:
        printf("Misaligned address provided");
        break;
    case SdStatusParameterOutOfRange:
        printf("Parameters out of range");
        break;
    case SdStatusInvalidCSD:
        printf("Invalid CSD received");
        break;
    case SdStatusInvalidCID:
        printf("Invalid CID received");
        break;
    case SdStatusReadCorrupted:
        printf("Invalid data CRC");
        break;
    case SdStatusReadCCError:
        printf("CC read error");
        break;
    case SdStatusECCFailed:
        printf("ECC failed");
        break;
    default:
        printf("%" PRId32 ", unknown status", (Int32)status);
        break;
    }
}

SdStatus SdShutdown() {
    printf("Performing SD power off cycle ...\r\n");
    // Preamble: we shutdown the SPI interface to later re-enabled it
    ShutdownSPIInterface();

    /* Reference specification : Physical Layer Simplified Specification Version 8.00[Section 6.4.1.2] */

    // We need to lower the VDD level under the 0.5V threshold for at least 1 ms
    // The other lines should also be low in this phase
    // Let's do 10ms to be sure
    HAL_GPIO_WritePin(_powerGPIO, _powerPin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(_nssGPIO, _nssPin, GPIO_PIN_RESET);
    HAL_Delay(10);

    return SdStatusOk;
}
