/*
 * sd.c
 *
 *  Created on: Dec 4, 2021
 *      Author: mnznn
 */

#include <sd/sd.h>
#include <sd/csd.h>
#include <sd/cid.h>
#include <sd/ocr.h>
#include <crc/crc7.h>
#include <crc/crc16.h>
#include <stdio.h>
#include <string.h>
#include <assertion.h>

extern void Error_Handler();

#define SD_DUMMY_BYTE 0xFF

/// Max size of a SD SPI response
/// \remarks The response formats and sizes are defined in the
/// Physical Layer Simplified Specification Version 8.00 [Section 7.3.2]
#define SD_MAX_RESPONSE_SIZE 5

#define SD_DATA_BLOCK_SIZE 512
#define SD_CRC_SIZE 16

/// Voltage Supplied (VHS) parameter for CMD8
#define SD_CMD8_VHS_2p7To3p6 (0x1 << 8)

#define SD_CMD59_CRC_ON 0x1
#define SD_CMD59_CRC_OFF 0x0

/// Custom defined generic timeout for a command response
#define SD_RESPONSE_TIMEOUT 10000
/// As specified in the Physical Layer Simplified Specification Version 8.00 [Section 4.2.3, Card Initialization and Identification Process]
/// we should use a 1sec timeout when waiting the device to become ready
#define SD_ACMD41_LOOP_TIMEOUT 1000

typedef enum _SDCommand {
	/// Resets the SD memory card
	SDCMD0GoIdleState = 0,
	/// Sends SD
	SDCMD8SendIfCondition = 8,
	/// Asks the selected card to send it's Card Specific Data
	SDCMD9SendCSD = 9,
	/// Asks the selected card to send it's Card Identification Data
	SDCMD10SendCID = 10,
	/// Defines to the card that the next command is an application specific comamnd
	/// Response is R1
	SDCMD55AppCmd = 55,
	SDCMD56GenCmd = 56,
	/// Reads the OCR of a card.
	/// Response is R3
	SDCMD58ReadOrc = 58,
	/// Enables or disables the crc option
	SDCMD59CRCOnOff = 59
} SDCommand;

typedef enum _SDAppCommand {
	/// Sends host capacity support information and activates the card initialization process.
	/// Response is R1
	SDACMD41SendOpCond = 41
} SDAppCommand;

/// Performs a single byte transaction on the SPI interface by writing an reading a byte
/// \param data Data to write on the MOSI channel
/// \return Data read from the SPI from the MISO channel
/// \remarks To read a single byte to the SPI a dummy byte must be provided since we are the master
/// initiating the transaction
static BYTE PerformByteTransaction(BYTE data);
/// Reads a byte from the SPI interface by writing a dummy byte
/// \return Data read from the SPI from the MISO channel
/// \remarks To read a single byte to the SPI a dummy byte must be provided since we are the master
/// initiating the transaction
static BYTE ReadByte();
/// Writes a byte onto the SPI interface using the single byte transaction method and by ignoring the read value
/// \param data Data to write on the MOSI channel
static void WriteByte(BYTE val, BYTE *crc);
/// Base implementation of single command transaction (request send + response).
/// \param command Commadn to send
/// \param argument Argumento of the command
/// \param responseLength Length of the response to read
/// \return Bytes received or error status (negative return)
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
static SDStatus CheckVddRange();
/// Verifies the voltage level of our connected SD card by issuing a CMD58 as specified in the Physical Layer Specification
/// @return Status of the operation
static SDStatus VerifyVoltageLevel();
/// Enables the CRC check of the communication
/// @return Status of the operation0
static SDStatus EnableCRC();
/// Waits the card is ready by issuing a ACMD41 and waiting that idle state is zero
/// @return Status of the operation
static SDStatus SDWaitForReady();
static SDStatus VerifyCardCapacityStatus();

static SDStatus ReadDataBlock(UInt16 blockSize);
static SDStatus ReadRegister(SDCommand readCommand, UInt16 length);
static SDStatus ReadCSD();
static SDStatus ReadCID();
/// Apply all the necessary changes to the SD interface with the information provided in the CSD register
/// @return Status of the operation
static SDStatus FixWithCSDRegister();

// ##### Private Implementation #####

static GPIO_TypeDef *_powerGPIO = NULL;
static UInt16 _powerPin = 0;

static GPIO_TypeDef *_nssGPIO = NULL;
static UInt16 _nssPin = 0;
static SPI_HandleTypeDef *_spiHandle = NULL;

/// Static allocated buffer wherethe response will be read
static BYTE _responseBuffer[SD_MAX_RESPONSE_SIZE];

/// Static allocated buffer where a data block will be read togheter with the related crc
static BYTE _dataBlockCrcBuffer[SD_DATA_BLOCK_SIZE + SD_CRC_SIZE];
static SDDescription _attachedSdCard;

typedef struct _ResponseR1 {
	BYTE Idle :1; // BIT 0
	BYTE EraseReset :1;
	BYTE IllegalCommand :1;
	BYTE ComCrcError :1;
	BYTE EraseSequenceError :1;
	BYTE AddressError :1;
	BYTE ParamError :1;
	BYTE Reserved :1; // BIT7
} ResponseR1;

typedef struct _ResponseR3 {
	ResponseR1 R1;
	OCRRegister OCR;
} __attribute__((aligned(1), packed)) ResponseR3;

typedef const ResponseR1 *PCResponseR1;
typedef const ResponseR3 *PCResponseR3;

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

	// We reset the minimum baud rate
	SET_BIT(_spiHandle->Instance->CR1, SPI_CR1_BR);
	DebugAssert((HAL_RCC_GetPCLK1Freq() / 256.0f) > 100000);

	// We don't have to do anything with the CS signal for the moment
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
	return (BYTE) _spiHandle->Instance->DR;
}

static BYTE ReadByte() {
	BYTE data = PerformByteTransaction(SD_DUMMY_BYTE);
	//*crc = Crc7Add(*crc, data);
	return data;
}

static void WriteByte(BYTE val, BYTE *crc) {
	*crc = Crc7Add(*crc, val);
	PerformByteTransaction(val);
}

static SBYTE PerformCommandTransaction(BYTE command, UInt32 argument, BYTE responseLength) {

	BYTE crc = CRC7_ZERO;
	WriteByte(0x40 | command, &crc);
	WriteByte((BYTE) (argument >> 24), &crc);
	WriteByte((BYTE) (argument >> 16), &crc);
	WriteByte((BYTE) (argument >> 8), &crc);
	WriteByte((BYTE) (argument), &crc);

	BYTE dataF = (BYTE) ((crc << 1) | 0x1); // End bit
	WriteByte(dataF, &crc); // We ignore this last crc

	static_assert(sizeof(ResponseR1) == sizeof(BYTE));
	DebugAssert(responseLength > 0);

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
		return SDStatusCommunicationTimeout;
	}

	_responseBuffer[bytesRead++] = readValue;
	// First byte is ALWAYS (per specification, Section 7.3.2.1 and except for SEND_STATUS) a R1 response

	const ResponseR1 *r1 = (const ResponseR1*) _responseBuffer;
	DebugAssert(r1->Reserved == 0); // reserved should always be zero

	if (r1->IllegalCommand || r1->ComCrcError) {
		// Per specification, this is the only case where the card outputs only the
		// first byte of the response
		return (SBYTE) bytesRead;
	}

	while (bytesRead < responseLength) {
		readValue = ReadByte();
		_responseBuffer[bytesRead++] = readValue;
	}
	return (SBYTE) bytesRead;
}

static SBYTE SendCommand(BYTE command, UInt32 argument, BYTE responseLength) {
	// Let's select the SPI by asserting LOW the NSS pin
	HAL_GPIO_WritePin(_nssGPIO, _nssPin, GPIO_PIN_RESET);

	SBYTE bytesRead = PerformCommandTransaction(command, argument, responseLength);

	// Let's deselect the SPI by asserting HIGH the NSS pin
	HAL_GPIO_WritePin(_nssGPIO, _nssPin, GPIO_PIN_SET);
	return bytesRead;
}

static SBYTE SendAppCommand(BYTE aCommand, UInt32 argument, BYTE responseLength) {
	// Let's select the SPI by asserting LOW the NSS pin
	HAL_GPIO_WritePin(_nssGPIO, _nssPin, GPIO_PIN_RESET);

	// Before sending an app command we must send a CMD55
	SBYTE bytesRead = PerformCommandTransaction(SDCMD55AppCmd, 0x0, sizeof(ResponseR1));
	if (bytesRead < 0) {
		goto cleanup;
		// Error
	}
	if (_responseBuffer[0] != 0x1) {
		Error_Handler();
	}

	// Send the application specific command
	bytesRead = PerformCommandTransaction(aCommand, argument, responseLength);

	cleanup:

	// Let's deselect the SPI by asserting HIGH the NSS pin
	HAL_GPIO_WritePin(_nssGPIO, _nssPin, GPIO_PIN_SET);
	return bytesRead;
}

static SDStatus CheckVddRange() {
	// Check if 2.7-3.6v range is supported

	BYTE checkPattern = 0xda;
	UInt32 argument = SD_CMD8_VHS_2p7To3p6 || checkPattern;
	SBYTE bytesRcv = SendCommand(SDCMD8SendIfCondition, SD_CMD8_VHS_2p7To3p6, sizeof(ResponseR1));

	// Communication error
	if (bytesRcv < 0) {
		return (SDStatus) bytesRcv;
	}

	PCResponseR1 r1 = (PCResponseR1) _responseBuffer;
	if (r1->IllegalCommand) {
		_attachedSdCard.Version = SDVer1pX;
	} else if (r1->IllegalCommand) {
		_attachedSdCard.Version = SDVer2p0OrLater;
		// TODO: check with a new card
		Error_Handler();
	}
	return SDStatusOk;
}

static SDStatus VerifyVoltageLevel() {
	//  From Physical Layer Specification, section 7.2.1 we can issue an optional CMD58 to verify that the voltage level we are using is
	// supported

	static_assert(sizeof(ResponseR3) == 5);
	static_assert(sizeof(OCRRegister) == sizeof(Int32));
	SBYTE bytesRcv = SendCommand(SDCMD58ReadOrc, 0x0, sizeof(ResponseR3));

	// Card is not supported
	if (bytesRcv == 1) {
		PCResponseR1 r1 = (PCResponseR1) _responseBuffer;
		if (r1->IllegalCommand)
			return SDStatusNotSDCard;
		else
			Error_Handler();
	}

	PCResponseR3 r3 = (PCResponseR3) _responseBuffer;
	// Checking our voltage range
	if (!(r3->OCR.v2p7tov2p8) || !(r3->OCR.v2p8tov2p9) || !(r3->OCR.v2p9tov3p0)) {
		return SDStatusVoltageNotSupported;
	}

	// No other flags should be checked in this stage since the card is still not initialized
	return SDStatusOk;
}

static SDStatus EnableCRC() {
	DebugAssert(_attachedSdCard.Version != SDVerUnknown);

	SBYTE bytesRcv = SendAppCommand(SDCMD59CRCOnOff, SD_CMD59_CRC_ON, sizeof(ResponseR1));
	if (bytesRcv < 0) {
		return (SDStatus) bytesRcv;
	} else if (!((PCResponseR1) _responseBuffer)->Idle) {
		Error_Handler();
	}
	return SDStatusOk;
}

static SDStatus SDWaitForReady() {
	UInt32 startTick = HAL_GetTick();
	volatile PCResponseR1 pResponse = (PCResponseR1) _responseBuffer;

	DebugAssert(_attachedSdCard.Version != SDVerUnknown);
	do {
		// We don't support SDHC or SDXC, so we keep the HCS (Host Capacity Support) bit to zero
		// even if the SDversion is 2.0 or later

		SBYTE bytesRcv = SendAppCommand(SDACMD41SendOpCond, 0x0, sizeof(ResponseR1));
		if (bytesRcv < 0) {
			return (SDStatus) bytesRcv;
		} else if (pResponse->IllegalCommand) {
			return SDStatusNotSDCard;
		}
	} while (pResponse->Idle && ((HAL_GetTick() - startTick) < SD_ACMD41_LOOP_TIMEOUT));

	if (pResponse->Idle) {
		return SDStatusInitializationTimeout;
	}
	return SDStatusOk;
}

static SDStatus VerifyCardCapacityStatus() {
	DebugAssert(_attachedSdCard.Version != SDVerUnknown);

	if (_attachedSdCard.Version == SDVer1pX) {
		// No need to read OCR [Se Figure 7-2, SPI Mode initialization flow]
		_attachedSdCard.Capacity = SDCapacityStandard;
	} else {
		SBYTE bytesRcv = SendCommand(SDCMD58ReadOrc, 0x0, sizeof(ResponseR3));

		if (bytesRcv < 0) {
			return (SDStatus) bytesRcv;
		} else if (bytesRcv == 1) {
			Error_Handler();
		}

		PCResponseR3 r3 = (PCResponseR3) _responseBuffer;
		if (r3->OCR.CCS) {
			_attachedSdCard.Capacity = SDCapacityExtended;
            _attachedSdCard.AddressingMode = SDAddressingModeSector;
		} else {
			_attachedSdCard.Capacity = SDCapacityStandard;
            _attachedSdCard.AddressingMode = SDAddressingModeByte;
		}
	}
	return SDStatusOk;
}

static SDStatus ReadDataBlock(UInt16 blockSize) {
	blockSize += 3; // 2 bytes of crc16 + Start block

	const BYTE startBlock = 0xFE;

	// First wait for data block
	BYTE readValue;
	UInt32 startTick = HAL_GetTick();
	do {
		// Response bytes may need some iterations so each time we re-initialize the
		readValue = ReadByte();
	} while (readValue != startBlock && ((HAL_GetTick() - startTick) < SD_RESPONSE_TIMEOUT));

	// We have not received an answer
	if (readValue != startBlock) {
		return SDStatusCommunicationTimeout;
	}

	UInt16 crc = CRC16_ZERO;
	for (int i = 0; i < blockSize - 1; i++) {
		BYTE data = ReadByte();
		crc = Crc16Add(crc, data);
		_dataBlockCrcBuffer[i] = data;
	}

	if (crc != 0) {
		return SDStatusReadCorrupted;
	}
	return SDStatusOk;
}

static SDStatus ReadRegister(SDCommand readCommand, UInt16 length) {
	// Let's select the SPI by asserting LOW the NSS pin
	HAL_GPIO_WritePin(_nssGPIO, _nssPin, GPIO_PIN_RESET);

	SDStatus result;
	SBYTE bytesRcv = PerformCommandTransaction(readCommand, 0x0, sizeof(ResponseR1));
	// Communication error
	if (bytesRcv < 0) {
		result = (SDStatus) bytesRcv;
		goto cleanup;
	}

	if (_responseBuffer[0] != 0) {
		Error_Handler();
	}

	result = ReadDataBlock(length);
	if (result != SDStatusOk) {
		goto cleanup;
	}

	result = SDStatusOk;
	cleanup: HAL_GPIO_WritePin(_nssGPIO, _nssPin, GPIO_PIN_SET);
	return result;
}

static SDStatus ReadCSD() {
	SDStatus result = ReadRegister(SDCMD9SendCSD, SD_CSD_SIZE);
	if (result != SDStatusOk) {
		return result;
	}

	PCCSDRegister csd = (PCCSDRegister) _dataBlockCrcBuffer;
	_attachedSdCard.CSDValidationStatus = SDCSDValidate(csd);
	if (_attachedSdCard.CSDValidationStatus != SDCSDValidationOk) {
		return SDStatusInvalidCSD;
	}

	return SDStatusOk;
}

static SDStatus ReadCID() {
	SDStatus result = ReadRegister(SDCMD10SendCID, SD_CID_SIZE);
	if (result != SDStatusOk) {
		return result;
	}

    PCCIDRegister cid = (PCCIDRegister)_dataBlockCrcBuffer;
    _attachedSdCard.CIDValidationStatus = SDCIDValidate(cid);
    if (_attachedSdCard.CIDValidationStatus != SDCIDValidationOk) {
        return SDStatusInvalidCID;
    }

	return SDStatusOk;
}

SDStatus FixWithCSDRegister() {
	PCCSDRegister csd = (PCCSDRegister) _dataBlockCrcBuffer;
	_attachedSdCard.MaxTransferSpeed = SDCSDGetMaxTransferRate(csd);

	// SPI2 is on PCLK1
	UInt32 spi2Freq = HAL_RCC_GetPCLK1Freq();
	// As stated in the RM0090, the SPI baud rate control can be changed with the peripheral enabled but not when a transfer is ongoing
	DebugAssert((spi2Freq / 2.0f) < _attachedSdCard.MaxTransferSpeed);

	// Max baud rate
	CLEAR_BIT(_spiHandle->Instance->CR1, SPI_CR1_BR);

	return SDStatusOk;
}

// ##### Public Function definitions #####

SDStatus SDInitialize(GPIO_TypeDef *powerGPIO, UInt16 powerPin, SPI_HandleTypeDef *spiHandle) {
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
	SDStatus shutdownStatus = SDShutdown();
	if (shutdownStatus != SDStatusOk) {
		return shutdownStatus;
	}

	// We reset the GPIO to an output push pull state
	CLEAR_BIT(_nssGPIO->OTYPER, _nssPin);
	// CS shall be kept high during initialization as stated in section 6.4.1.1
	HAL_GPIO_WritePin(_nssGPIO, _nssPin, GPIO_PIN_SET);

	// We can now enable the GPIO, and wait another 1 ms (at least) to let the VDD stabilize
	// There are some constraints also on the slope on the VDD change but we can do little about this
	HAL_GPIO_WritePin(_powerGPIO, _powerPin, GPIO_PIN_SET);
	HAL_Delay(10);

	memset(&_attachedSdCard, 0, sizeof(SDDescription));

	// After these step, the SD card (if alredy inserted) should be ready to receive the initialization sequence
	return SDStatusOk;
}

SDStatus SDTryConnect() {
	// We assume here that a power cycle was already performed and VDD is stable
	// We can start the initialization sequence by putting the card in the idle state

	// From Section 6.4.1.4, we supply at least 74 clock cycles to the SD card
	// To let the SCK run, as we can see in the SPI chapter of RM0090 [Figure 258]
	// we need to issue a byte transfer

	// So, firstly we enable the SPI
	__HAL_SPI_ENABLE(_spiHandle);

	// Then whe sent 10 dummy bytes which correspond to 80 SPI clock cycles
	BYTE dummyCrc = CRC7_ZERO;
	for (size_t i = 0; i < 10; i++) {
		WriteByte(SD_DUMMY_BYTE, &dummyCrc);
	}

	// After the Power On cycle, we are ready to issue a CMD0 with the NSS pin asserted to LOW
	HAL_GPIO_WritePin(_nssGPIO, _nssPin, GPIO_PIN_RESET);
	SBYTE responseLength = SendCommand(SDCMD0GoIdleState, 0, 1);
	if (responseLength < 0) {
		// Error fomr CMD0 -> We cannot go further or SPI is not supported
		return (SDStatus) responseLength;
	}

	// If everything is ok we should get an idle state
	if (((ResponseR1*) _responseBuffer)->Idle == 0) {
		Error_Handler();
	}

	// Initialization step: ask the card if the specified VHS is supported
	SDStatus status;
	if ((status = CheckVddRange()) != SDStatusOk) {
		return status;
	}

	// Initialization step (optional): query the card for the supported voltage ranges through OCR
	if ((status = VerifyVoltageLevel()) != SDStatusOk) {
		return status;
	}

	// As stated in the "Bus Transfer Protection" [Section 7.2.2] of the SD Specification,
	// CRC should be enabled before issuing ACMD41
	if ((status = EnableCRC()) != SDStatusOk) {
		Error_Handler();
	}

	// Initialization step: wait for initialization
	if ((status = SDWaitForReady()) != SDStatusOk) {
		Error_Handler();
	}

	// Initialization step (last): verify Card Capacity Status
	if ((status = VerifyCardCapacityStatus()) != SDStatusOk) {
		Error_Handler();
	}

	if ((status = ReadCSD()) != SDStatusOk) {
		Error_Handler();
	}

	if ((status = FixWithCSDRegister()) != SDStatusOk) {
		Error_Handler();
	}

	if ((status = ReadCID()) != SDStatusOk) {
		Error_Handler();
	}

	return SDStatusOk;
}

SDStatus SDDisconnect() {
	// Preamble: we shutdown the SPI interface to later re-enabled it
	ShutdownSDSPIInterface();
	// We leave the NSS high
	HAL_GPIO_WritePin(_nssGPIO, _nssPin, GPIO_PIN_SET);

	return SDStatusOk;
}

SDStatus SDShutdown() {
	// Preamble: we shutdown the SPI interface to later re-enabled it
	ShutdownSDSPIInterface();

	/* Reference specification : Physical Layer Simplified Specification Version 8.00[Section 6.4.1.2] */

	// We need to lower the VDD level under the 0.5V threshold for at least 1 ms
	// The other lines should also be low in this phase
	// Let's do 10ms to be sure
	HAL_GPIO_WritePin(_powerGPIO, _powerPin, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(_nssGPIO, _nssPin, GPIO_PIN_RESET);
	HAL_Delay(10);

	return SDStatusOk;
}
