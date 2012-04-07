#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/*
 * Fitipower FC0013 tuner driver, taken from the kernel driver that can be found
 * on http://linux.terratec.de/tv_en.html
 */

#ifndef _WIN32
#include <stdint.h>
#endif // _WIN32

#include "rtl2832-tuner_fc0013.h"

#define FC0013_I2C_ADDR		0xc6
#define FC0013_CHECK_ADDR	0x00
#define FC0013_CHECK_VAL	0xa3

#define LOG_PREFIX			"[fc0013] "

int _FC0013_Write(RTL2832_NAMESPACE::tuner* pTuner, unsigned char RegAddr, unsigned char Byte,
	const char* function = NULL, int line_number = -1, const char* line = NULL)
{
	uint8_t data[2];

	data[0] = RegAddr;
	data[1] = Byte;

	int r = pTuner->i2c_write(FC0013_I2C_ADDR, data, 2);
	if (r <= 0)
	{
		DEBUG_TUNER_I2C(pTuner,r);
		return FC0013_I2C_ERROR;
	}

	return FC0013_I2C_SUCCESS;
}

int _FC0013_Read(RTL2832_NAMESPACE::tuner* pTuner, unsigned char RegAddr, unsigned char *pByte,
	const char* function = NULL, int line_number = -1, const char* line = NULL)
{
	uint8_t data = RegAddr;
	int r;

	r = pTuner->i2c_write(FC0013_I2C_ADDR, &data, 1);
	if (r <= 0)
	{
		DEBUG_TUNER_I2C(pTuner,r);
		return FC0013_I2C_ERROR;
	}

	r = pTuner->i2c_read(FC0013_I2C_ADDR, &data, 1);
	if (r <= 0)
	{
		DEBUG_TUNER_I2C(pTuner,r);
		return FC0013_I2C_ERROR;
	}

	*pByte = data;

	return FC0013_I2C_SUCCESS;
}

// Set FC0013 register mask bits.
int
_fc0013_SetRegMaskBits(
	RTL2832_NAMESPACE::tuner *pTuner,
	unsigned char RegAddr,
	unsigned char Msb,
	unsigned char Lsb,
	const unsigned char WritingValue,
	const char* function = NULL, int line_number = -1, const char* line = NULL
	)
{
	int i;

	unsigned char ReadingByte;
	unsigned char WritingByte;

	unsigned char Mask;
	unsigned char Shift;

	// Generate mask and shift according to MSB and LSB.
	Mask = 0;

	for(i = Lsb; i < (Msb + 1); i++)
		Mask |= 0x1 << i;

	Shift = Lsb;

	// Get tuner register byte according to register adddress.
	if(_FC0013_Read(pTuner, RegAddr, &ReadingByte, function, line_number, line) != FC0013_I2C_SUCCESS)
		goto error_status_get_tuner_registers;

	// Reserve byte unmask bit with mask and inlay writing value into it.
	WritingByte = ReadingByte & (~Mask);
	WritingByte |= (WritingValue << Shift) & Mask;

	// Write tuner register byte with writing byte.
	if(_FC0013_Write(pTuner, RegAddr, WritingByte, function, line_number, line) != FC0013_I2C_SUCCESS)
		goto error_status_set_tuner_registers;

	return FC0013_I2C_SUCCESS;
error_status_get_tuner_registers:
error_status_set_tuner_registers:
	return FC0013_I2C_ERROR;
}

// Get FC0013 register mask bits.
int
_fc0013_GetRegMaskBits(
	RTL2832_NAMESPACE::tuner *pTuner,
	unsigned char RegAddr,
	unsigned char Msb,
	unsigned char Lsb,
	unsigned char *pReadingValue,
	const char* function = NULL, int line_number = -1, const char* line = NULL
	)
{
	int i;

	unsigned char ReadingByte;

	unsigned char Mask;
	unsigned char Shift;

	// Generate mask and shift according to MSB and LSB.
	Mask = 0;

	for(i = Lsb; i < (Msb + 1); i++)
		Mask |= 0x1 << i;

	Shift = Lsb;

	// Get tuner register byte according to register adddress.
	if(_FC0013_Read(pTuner, RegAddr, &ReadingByte, function, line_number, line) != FC0013_I2C_SUCCESS)
		goto error_status_get_tuner_registers;

	// Get register bits from reading byte with mask and shift
	*pReadingValue = (ReadingByte & Mask) >> Shift;

	return FC0013_I2C_SUCCESS;
error_status_get_tuner_registers:
	return FC0013_I2C_ERROR;
}

#define FC0013_Read(t,r,b)		_FC0013_Read(t,r,b,CURRENT_FUNCTION,__LINE__,"FC0013_Read("#t", "#r", "#b")")
//#define FC0013_Read(t,r,b)		_FC0013_Read(t,r,b)

#define FC0013_Write(t,r,w)			_FC0013_Write(t,r,w,CURRENT_FUNCTION,__LINE__,"FC0013_Write("#t", "#r", "#w")")
//#define FC0013_Write(t,r,w)		_FC0013_Write(t,r,w)

#define fc0013_SetRegMaskBits(t,r,m,l,b)	_fc0013_SetRegMaskBits(t,r,m,l,b,CURRENT_FUNCTION,__LINE__,"fc0013_SetRegMaskBits("#t", "#r", "#m", "#l", "#b")")
//#define fc0013_SetRegMaskBits(t,r,m,l,b)	_fc0013_SetRegMaskBits(t,r,m,l,b)

#define fc0013_SetRegMaskBits(t,r,m,l,b)	_fc0013_SetRegMaskBits(t,r,m,l,b,CURRENT_FUNCTION,__LINE__,"fc0013_SetRegMaskBits("#t", "#r", "#m", "#l", "#b")")
//#define fc0013_SetRegMaskBits(t,r,m,l,b)	_fc0013_SetRegMaskBits(t,r,m,l,b)

///////////////////////////////////////////////////////////////////////////////

namespace RTL2832_NAMESPACE { namespace TUNERS_NAMESPACE {

static int _mapGainsFC0013[] = {	// nim_rtl2832_fc0013.c
	-63, FC0013_LNA_GAIN_LOW,
	+71, FC0013_LNA_GAIN_MIDDLE,
	191, FC0013_LNA_GAIN_HIGH_17,
	197, FC0013_LNA_GAIN_HIGH_19
};

int fc0013::TUNER_PROBE_FN_NAME(demod* d)
{
	I2C_REPEATER_SCOPE(d);

	uint8_t reg = 0;
	//CHECK_LIBUSB_RESULT_RETURN_EX(d,d->i2c_read_reg(FC0013_I2C_ADDR, FC0013_CHECK_ADDR, reg));
	int r = d->i2c_read_reg(FC0013_I2C_ADDR, FC0013_CHECK_ADDR, reg);
	if (r <= 0) return r;
	return ((reg == FC0013_CHECK_VAL) ? SUCCESS : FAILURE);
}

fc0013::fc0013(demod* p)
	: tuner_skeleton(p)
{
	for (int i = 0; i < sizeof(_mapGainsFC0013)/sizeof(_mapGainsFC0013[0]); i += 2)
		m_gain_values.push_back((double)_mapGainsFC0013[i] / 10.0);

	values_to_range(m_gain_values, m_gain_range);

	//m_frequency_range	// ?

	m_bandwidth_values.push_back(6000000);
	m_bandwidth_values.push_back(7000000);
	m_bandwidth_values.push_back(8000000);
	values_to_range(m_bandwidth_values, m_bandwidth_range);

	m_bandwidth = m_bandwidth_range.second;	// Default
}

int fc0013::initialise(tuner::PPARAMS params /*= NULL*/)
{
	if (tuner_skeleton::initialise(params) != SUCCESS)
		return FAILURE;

	THIS_I2C_REPEATER_SCOPE();

	if (FC0013_Open(this) != FC0013_FUNCTION_SUCCESS)
		return FAILURE;

	if (m_params.message_output && m_params.verbose)
		m_params.message_output->on_log_message_ex(rtl2832::log_sink::LOG_LEVEL_VERBOSE, LOG_PREFIX"Initialised (default bandwidth: %i Hz)\n", (uint32_t)bandwidth());

	return SUCCESS;
}

int fc0013::set_frequency(double freq)
{
	if ((freq <= 0) || (in_valid_range(m_frequency_range, freq) == false))
		return FAILURE;

	THIS_I2C_REPEATER_SCOPE();

	if (FC0013_SetFrequency(this, (unsigned long)(freq / 1000.0), (unsigned short)(bandwidth() / 1000000.0)) != FC0013_FUNCTION_SUCCESS)
		return FAILURE;

	m_freq = (unsigned long)(freq / 1000.0) * 1000;

	return SUCCESS;
}

int fc0013::set_bandwidth(double bw)
{
	if ((bw <= 0) || (in_valid_range(m_bandwidth_range, bw) == false))
		return FAILURE;

	THIS_I2C_REPEATER_SCOPE();

	if (FC0013_SetFrequency(this, (unsigned long)(frequency() / 1000.0), (unsigned short)(bw / 1000000.0)) != FC0013_FUNCTION_SUCCESS)
		return FAILURE;

	m_bandwidth = (unsigned long)(bw / 1000000.0) * 1000000;

	return SUCCESS;
}

int fc0013::set_gain(double gain)
{
	const int iCount = (sizeof(_mapGainsFC0013)/sizeof(_mapGainsFC0013[0])) / 2;

	int iGain = (int)(gain * 10.0);
	int i = get_map_index(iGain, _mapGainsFC0013, iCount);

	if ((i == -1) || (i == iCount))
		return FAILURE;
	
	//if (i == -1)	// Below first -> select first
	//  i = 0;
	//else if (i == iCount)	// Above last -> select last
	//  i = iCount - 1;

	unsigned char u8Write = _mapGainsFC0013[i + 1];

	THIS_I2C_REPEATER_SCOPE();

	if (fc0013_SetRegMaskBits(this, 0x14, 4, 0, u8Write) != FC0013_I2C_SUCCESS)
		return FAILURE;

// FIXME: Also really should be handling:
//	int boolVhfFlag;      // 0:false,  1:true
//	int boolEnInChgFlag;  // 0:false,  1:true
//	int intGainShift;

	m_gain = (double)_mapGainsFC0013[i] / 10.0;

	return SUCCESS;
}

} } // TUNERS_NAMESPACE, RTL2832_NAMESPACE

///////////////////////////////////////////////////////////////////////////////

int FC0013_SetVhfTrack(RTL2832_NAMESPACE::tuner* pTuner, unsigned long FrequencyKHz)
{
	unsigned char read_byte;

    if (FrequencyKHz <= 177500)	// VHF Track: 7
    {
		if(FC0013_Read(pTuner, 0x1D, &read_byte) != FC0013_I2C_SUCCESS) goto error_status;
		if(FC0013_Write(pTuner, 0x1D, (read_byte & 0xE3) | 0x1C) != FC0013_I2C_SUCCESS) goto error_status;

    }
    else if (FrequencyKHz <= 184500)	// VHF Track: 6
    {
		if(FC0013_Read(pTuner, 0x1D, &read_byte) != FC0013_I2C_SUCCESS) goto error_status;
		if(FC0013_Write(pTuner, 0x1D, (read_byte & 0xE3) | 0x18) != FC0013_I2C_SUCCESS) goto error_status;

    }
    else if (FrequencyKHz <= 191500)	// VHF Track: 5
    {
		if(FC0013_Read(pTuner, 0x1D, &read_byte) != FC0013_I2C_SUCCESS) goto error_status;
		if(FC0013_Write(pTuner, 0x1D, (read_byte & 0xE3) | 0x14) != FC0013_I2C_SUCCESS) goto error_status;

    }
    else if (FrequencyKHz <= 198500)	// VHF Track: 4
    {
		if(FC0013_Read(pTuner, 0x1D, &read_byte) != FC0013_I2C_SUCCESS) goto error_status;
		if(FC0013_Write(pTuner, 0x1D, (read_byte & 0xE3) | 0x10) != FC0013_I2C_SUCCESS) goto error_status;

    }
    else if (FrequencyKHz <= 205500)	// VHF Track: 3
    {
		if(FC0013_Read(pTuner, 0x1D, &read_byte) != FC0013_I2C_SUCCESS) goto error_status;
		if(FC0013_Write(pTuner, 0x1D, (read_byte & 0xE3) | 0x0C) != FC0013_I2C_SUCCESS) goto error_status;

    }
    else if (FrequencyKHz <= 212500)	// VHF Track: 2
    {
		if(FC0013_Read(pTuner, 0x1D, &read_byte) != FC0013_I2C_SUCCESS) goto error_status;
		if(FC0013_Write(pTuner, 0x1D, (read_byte & 0xE3) | 0x08) != FC0013_I2C_SUCCESS) goto error_status;

    }
    else if (FrequencyKHz <= 219500)	// VHF Track: 2
    {
		if(FC0013_Read(pTuner, 0x1D, &read_byte) != FC0013_I2C_SUCCESS) goto error_status;
		if(FC0013_Write(pTuner, 0x1D, (read_byte & 0xE3) | 0x08) != FC0013_I2C_SUCCESS) goto error_status;

    }
    else if (FrequencyKHz <= 226500)	// VHF Track: 1
    {
		if(FC0013_Read(pTuner, 0x1D, &read_byte) != FC0013_I2C_SUCCESS) goto error_status;
		if(FC0013_Write(pTuner, 0x1D, (read_byte & 0xE3) | 0x04) != FC0013_I2C_SUCCESS) goto error_status;
    }
    else	// VHF Track: 1
    {
		if(FC0013_Read(pTuner, 0x1D, &read_byte) != FC0013_I2C_SUCCESS) goto error_status;
		if(FC0013_Write(pTuner, 0x1D, (read_byte & 0xE3) | 0x04) != FC0013_I2C_SUCCESS) goto error_status;

    }

	//------------------------------------------------ arios modify 2010-12-24
	// " | 0x10" ==> " | 0x30"   (make sure reg[0x07] bit5 = 1)

	// Enable VHF filter.
	if(FC0013_Read(pTuner, 0x07, &read_byte) != FC0013_I2C_SUCCESS) goto error_status;
	if(FC0013_Write(pTuner, 0x07, read_byte | 0x10) != FC0013_I2C_SUCCESS) goto error_status;

	// Disable UHF & GPS.
	if(FC0013_Read(pTuner, 0x14, &read_byte) != FC0013_I2C_SUCCESS) goto error_status;
	if(FC0013_Write(pTuner, 0x14, read_byte & 0x1F) != FC0013_I2C_SUCCESS) goto error_status;


	return FC0013_FUNCTION_SUCCESS;

error_status:
	return FC0013_FUNCTION_ERROR;
}


// FC0013 Open Function, includes enable/reset pin control and registers initialization.
//void FC0013_Open()
int FC0013_Open(RTL2832_NAMESPACE::tuner* pTuner)
{
	// Enable FC0013 Power
	// (...)
	// FC0013 Enable = High
	// (...)
	// FC0013 Reset = High -> Low
	// (...)

    //================================ update base on new FC0013 register bank
	if(FC0013_Write(pTuner, 0x01, 0x09) != FC0013_I2C_SUCCESS) goto error_status;
	if(FC0013_Write(pTuner, 0x02, 0x16) != FC0013_I2C_SUCCESS) goto error_status;
	if(FC0013_Write(pTuner, 0x03, 0x00) != FC0013_I2C_SUCCESS) goto error_status;
	if(FC0013_Write(pTuner, 0x04, 0x00) != FC0013_I2C_SUCCESS) goto error_status;
	if(FC0013_Write(pTuner, 0x05, 0x17) != FC0013_I2C_SUCCESS) goto error_status;
	if(FC0013_Write(pTuner, 0x06, 0x02) != FC0013_I2C_SUCCESS) goto error_status;
//	if(FC0013_Write(pTuner, 0x07, 0x27) != FC0013_I2C_SUCCESS) goto error_status;		// 28.8MHz, GainShift: 15
	if(FC0013_Write(pTuner, 0x07, 0x2A) != FC0013_I2C_SUCCESS) goto error_status;		// 28.8MHz, modified by Realtek
	if(FC0013_Write(pTuner, 0x08, 0xFF) != FC0013_I2C_SUCCESS) goto error_status;
	if(FC0013_Write(pTuner, 0x09, 0x6F) != FC0013_I2C_SUCCESS) goto error_status;		// Enable Loop Through
	if(FC0013_Write(pTuner, 0x0A, 0xB8) != FC0013_I2C_SUCCESS) goto error_status;
	if(FC0013_Write(pTuner, 0x0B, 0x82) != FC0013_I2C_SUCCESS) goto error_status;

	if(FC0013_Write(pTuner, 0x0C, 0xFE) != FC0013_I2C_SUCCESS) goto error_status;   // Modified for up-dowm AGC by Realtek(for master, and for 2836BU dongle).
//	if(FC0013_Write(pTuner, 0x0C, 0xFC) != FC0013_I2C_SUCCESS) goto error_status;   // Modified for up-dowm AGC by Realtek(for slave, and for 2832 mini dongle).

//	if(FC0013_Write(pTuner, 0x0D, 0x09) != FC0013_I2C_SUCCESS) goto error_status;
	if(FC0013_Write(pTuner, 0x0D, 0x01) != FC0013_I2C_SUCCESS) goto error_status;   // Modified for AGC non-forcing by Realtek.

	if(FC0013_Write(pTuner, 0x0E, 0x00) != FC0013_I2C_SUCCESS) goto error_status;
	if(FC0013_Write(pTuner, 0x0F, 0x00) != FC0013_I2C_SUCCESS) goto error_status;
	if(FC0013_Write(pTuner, 0x10, 0x00) != FC0013_I2C_SUCCESS) goto error_status;
	if(FC0013_Write(pTuner, 0x11, 0x00) != FC0013_I2C_SUCCESS) goto error_status;
	if(FC0013_Write(pTuner, 0x12, 0x00) != FC0013_I2C_SUCCESS) goto error_status;
	if(FC0013_Write(pTuner, 0x13, 0x00) != FC0013_I2C_SUCCESS) goto error_status;

	if(FC0013_Write(pTuner, 0x14, 0x50) != FC0013_I2C_SUCCESS) goto error_status;		// DVB-T, High Gain
//	if(FC0013_Write(pTuner, 0x14, 0x48) != FC0013_I2C_SUCCESS) goto error_status;		// DVB-T, Middle Gain
//	if(FC0013_Write(pTuner, 0x14, 0x40) != FC0013_I2C_SUCCESS) goto error_status;		// DVB-T, Low Gain

	if(FC0013_Write(pTuner, 0x15, 0x01) != FC0013_I2C_SUCCESS) goto error_status;


	return FC0013_FUNCTION_SUCCESS;

error_status:
	return FC0013_FUNCTION_ERROR;
}


int FC0013_SetFrequency(RTL2832_NAMESPACE::tuner* pTuner, unsigned long Frequency, unsigned short Bandwidth)
{
//    bool VCO1 = false;
//    unsigned int doubleVCO;
//    unsigned short xin, xdiv;
//	unsigned char reg[21], am, pm, multi;
    int VCO1 = FC0013_FALSE;
    unsigned long doubleVCO;
    unsigned short xin, xdiv;
	unsigned char reg[21], am, pm, multi;

	unsigned char read_byte;

	unsigned long CrystalFreqKhz;

	int CrystalFreqHz = /*CRYSTAL_FREQ*/pTuner->parent()->crystal_frequency();

	// Get tuner crystal frequency in KHz.
	// Note: CrystalFreqKhz = round(CrystalFreqHz / 1000)
	CrystalFreqKhz = (CrystalFreqHz + 500) / 1000;

	// modified 2011-02-09: for D-Book test
	// set VHF_Track = 7
	if(FC0013_Read(pTuner, 0x1D, &read_byte) != FC0013_I2C_SUCCESS) goto error_status;

	// VHF Track: 7
    if(FC0013_Write(pTuner, 0x1D, (read_byte & 0xE3) | 0x1C) != FC0013_I2C_SUCCESS) goto error_status;


	if( Frequency < 300000 )
	{
		// Set VHF Track.
		if(FC0013_SetVhfTrack(pTuner, Frequency) != FC0013_I2C_SUCCESS) goto error_status;

		// Enable VHF filter.
		if(FC0013_Read(pTuner, 0x07, &read_byte) != FC0013_I2C_SUCCESS) goto error_status;
		if(FC0013_Write(pTuner, 0x07, read_byte | 0x10) != FC0013_I2C_SUCCESS) goto error_status;

		// Disable UHF & disable GPS.
		if(FC0013_Read(pTuner, 0x14, &read_byte) != FC0013_I2C_SUCCESS) goto error_status;
		if(FC0013_Write(pTuner, 0x14, read_byte & 0x1F) != FC0013_I2C_SUCCESS) goto error_status;
	}
	else if ( (Frequency >= 300000) && (Frequency <= 862000) )
	{
		// Disable VHF filter.
		if(FC0013_Read(pTuner, 0x07, &read_byte) != FC0013_I2C_SUCCESS) goto error_status;
		if(FC0013_Write(pTuner, 0x07, read_byte & 0xEF) != FC0013_I2C_SUCCESS) goto error_status;

		// enable UHF & disable GPS.
		if(FC0013_Read(pTuner, 0x14, &read_byte) != FC0013_I2C_SUCCESS) goto error_status;
		if(FC0013_Write(pTuner, 0x14, (read_byte & 0x1F) | 0x40) != FC0013_I2C_SUCCESS) goto error_status;
	}
	else if (Frequency > 862000)
	{
		// Disable VHF filter
		if(FC0013_Read(pTuner, 0x07, &read_byte) != FC0013_I2C_SUCCESS) goto error_status;
		if(FC0013_Write(pTuner, 0x07, read_byte & 0xEF) != FC0013_I2C_SUCCESS) goto error_status;

		// Disable UHF & enable GPS
		if(FC0013_Read(pTuner, 0x14, &read_byte) != FC0013_I2C_SUCCESS) goto error_status;
		if(FC0013_Write(pTuner, 0x14, (read_byte & 0x1F) | 0x20) != FC0013_I2C_SUCCESS) goto error_status;
	}

	if (Frequency * 96 < 3560000)
    {
        multi = 96;
        reg[5] = 0x82;
        reg[6] = 0x00;
    }
    else if (Frequency * 64 < 3560000)
    {
        multi = 64;
        reg[5] = 0x02;
        reg[6] = 0x02;
    }
    else if (Frequency * 48 < 3560000)
    {
        multi = 48;
        reg[5] = 0x42;
        reg[6] = 0x00;
    }
    else if (Frequency * 32 < 3560000)
    {
        multi = 32;
        reg[5] = 0x82;
        reg[6] = 0x02;
    }
    else if (Frequency * 24 < 3560000)
    {
        multi = 24;
        reg[5] = 0x22;
        reg[6] = 0x00;
    }
    else if (Frequency * 16 < 3560000)
    {
        multi = 16;
        reg[5] = 0x42;
        reg[6] = 0x02;
    }
    else if (Frequency * 12 < 3560000)
    {
        multi = 12;
        reg[5] = 0x12;
        reg[6] = 0x00;
    }
    else if (Frequency * 8 < 3560000)
    {
        multi = 8;
        reg[5] = 0x22;
        reg[6] = 0x02;
    }
    else if (Frequency * 6 < 3560000)
    {
        multi = 6;
        reg[5] = 0x0A;
        reg[6] = 0x00;
    }
    else if (Frequency * 4 < 3800000)
    {
        multi = 4;
        reg[5] = 0x12;
        reg[6] = 0x02;
    }
	else
	{
        Frequency = Frequency / 2;
		multi = 4;
        reg[5] = 0x0A;
        reg[6] = 0x02;
	}

    doubleVCO = Frequency * multi;

    reg[6] = reg[6] | 0x08;
//	VCO1 = true;
	VCO1 = FC0013_TRUE;

	// Calculate VCO parameters: ap & pm & xin.
//	xdiv = (unsigned short)(doubleVCO / (Crystal_Frequency/2));
	xdiv = (unsigned short)(doubleVCO / (CrystalFreqKhz/2));
//	if( (doubleVCO - xdiv * (Crystal_Frequency/2)) >= (Crystal_Frequency/4) )
	if( (doubleVCO - xdiv * (CrystalFreqKhz/2)) >= (CrystalFreqKhz/4) )
	{
		xdiv = xdiv + 1;
	}

	pm = (unsigned char)( xdiv / 8 );
    am = (unsigned char)( xdiv - (8 * pm));

    if (am < 2)
    {
        reg[1] = am + 8;
        reg[2] = pm - 1;
    }
    else
    {
        reg[1] = am;
        reg[2] = pm;
    }

//	xin = (unsigned short)(doubleVCO - ((unsigned short)(doubleVCO / (Crystal_Frequency/2))) * (Crystal_Frequency/2));
	xin = (unsigned short)(doubleVCO - ((unsigned short)(doubleVCO / (CrystalFreqKhz/2))) * (CrystalFreqKhz/2));
//	xin = ((xin << 15)/(Crystal_Frequency/2));
	xin = (unsigned short)((xin << 15)/(CrystalFreqKhz/2));

//	if( xin >= (unsigned short) pow( (double)2, (double)14) )
//	{
//		xin = xin + (unsigned short) pow( (double)2, (double)15);
//	}
	if( xin >= (unsigned short) 16384 )
		xin = xin + (unsigned short) 32768;

	reg[3] = (unsigned char)(xin >> 8);
	reg[4] = (unsigned char)(xin & 0x00FF);


	//===================================== Only for testing
//	printf("Frequency: %d, Fa: %d, Fp: %d, Xin:%d \n", Frequency, am, pm, xin);


	// Set Low-Pass Filter Bandwidth.
    switch(Bandwidth)
    {
        case 6:
			reg[6] = 0x80 | reg[6];
            break;
        case 7:
			reg[6] = ~0x80 & reg[6];
            reg[6] = 0x40 | reg[6];
            break;
        case 8:
        default:
			reg[6] = ~0xC0 & reg[6];
            break;
    }

	reg[5] = reg[5] | 0x07;

	if(FC0013_Write(pTuner, 0x01, reg[1]) != FC0013_I2C_SUCCESS) goto error_status;
	if(FC0013_Write(pTuner, 0x02, reg[2]) != FC0013_I2C_SUCCESS) goto error_status;
	if(FC0013_Write(pTuner, 0x03, reg[3]) != FC0013_I2C_SUCCESS) goto error_status;
	if(FC0013_Write(pTuner, 0x04, reg[4]) != FC0013_I2C_SUCCESS) goto error_status;
	if(FC0013_Write(pTuner, 0x05, reg[5]) != FC0013_I2C_SUCCESS) goto error_status;
	if(FC0013_Write(pTuner, 0x06, reg[6]) != FC0013_I2C_SUCCESS) goto error_status;

	if (multi == 64)
	{
//		FC0013_Write(0x11, FC0013_Read(0x11) | 0x04);
		if(FC0013_Read(pTuner, 0x11, &read_byte) != FC0013_I2C_SUCCESS) goto error_status;
		if(FC0013_Write(pTuner, 0x11, read_byte | 0x04) != FC0013_I2C_SUCCESS) goto error_status;
	}
	else
	{
//		FC0013_Write(0x11, FC0013_Read(0x11) & 0xFB);
		if(FC0013_Read(pTuner, 0x11, &read_byte) != FC0013_I2C_SUCCESS) goto error_status;
		if(FC0013_Write(pTuner, 0x11, read_byte & 0xFB) != FC0013_I2C_SUCCESS) goto error_status;
	}

	if(FC0013_Write(pTuner, 0x0E, 0x80) != FC0013_I2C_SUCCESS) goto error_status;
	if(FC0013_Write(pTuner, 0x0E, 0x00) != FC0013_I2C_SUCCESS) goto error_status;

	if(FC0013_Write(pTuner, 0x0E, 0x00) != FC0013_I2C_SUCCESS) goto error_status;
//	reg[14] = 0x3F & FC0013_Read(0x0E);
	if(FC0013_Read(pTuner, 0x0E, &read_byte) != FC0013_I2C_SUCCESS) goto error_status;
	reg[14] = 0x3F & read_byte;

	if (VCO1)
    {
        if (reg[14] > 0x3C)
        {
            reg[6] = ~0x08 & reg[6];

			if(FC0013_Write(pTuner, 0x06, reg[6]) != FC0013_I2C_SUCCESS) goto error_status;

			if(FC0013_Write(pTuner, 0x0E, 0x80) != FC0013_I2C_SUCCESS) goto error_status;
			if(FC0013_Write(pTuner, 0x0E, 0x00) != FC0013_I2C_SUCCESS) goto error_status;
        }
    }
	else
    {
        if (reg[14] < 0x02)
        {
            reg[6] = 0x08 | reg[6];

			if(FC0013_Write(pTuner, 0x06, reg[6]) != FC0013_I2C_SUCCESS) goto error_status;

			if(FC0013_Write(pTuner, 0x0E, 0x80) != FC0013_I2C_SUCCESS) goto error_status;
			if(FC0013_Write(pTuner, 0x0E, 0x00) != FC0013_I2C_SUCCESS) goto error_status;
        }
    }


	return FC0013_FUNCTION_SUCCESS;

error_status:
	return FC0013_FUNCTION_ERROR;
}
