#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/*
 * Elonics E4000 tuner driver, taken from the kernel driver that can be found
 * on http://linux.terratec.de/tv_en.html
 */

#ifndef _WIN32
#include <stdint.h>
#endif // _WIN32

#include "rtl2832-tuner_e4000.h"

using namespace std;

#define FUNCTION_ERROR		1
#define FUNCTION_SUCCESS	0
#define NO_USE				0
#define I2C_BUFFER_LEN		128
#define LOG_PREFIX			"[e4000] "

///////////////////////////////////////////////////////////////////////////////

#define E4K_I2C_ADDR		0xc8
#define E4K_CHECK_ADDR		0x02
#define E4K_CHECK_VAL		0x40

static int
_I2CReadByte(
	RTL2832_NAMESPACE::tuner* pTuner,
	unsigned char NoUse,
	unsigned char RegAddr,
	unsigned char *pReadingByte,
	const char* function = NULL, int line_number = -1, const char* line = NULL
	)
{
	uint8_t data = RegAddr;
	int r;

	r = pTuner->i2c_write(E4K_I2C_ADDR, &data, 1);
	if (r <= 0)
	{
	  DEBUG_TUNER_I2C(pTuner,r);
	  return E4000_I2C_FAIL;
	}

	r = pTuner->i2c_read(E4K_I2C_ADDR, &data, 1);
	if (r <= 0)
	{
	  DEBUG_TUNER_I2C(pTuner,r);
	  return E4000_I2C_FAIL;
	}

	*pReadingByte = data;

	return E4000_I2C_SUCCESS;
}

static int
_I2CWriteByte(
	RTL2832_NAMESPACE::tuner* pTuner,
	unsigned char NoUse,
	unsigned char RegAddr,
	unsigned char WritingByte,
	const char* function = NULL, int line_number = -1, const char* line = NULL
	)
{
	uint8_t data[2];

	data[0] = RegAddr;
	data[1] = WritingByte;

	int r = pTuner->i2c_write(E4K_I2C_ADDR, data, 2);
	if (r <= 0)
	{
	  DEBUG_TUNER_I2C(pTuner,r);
	  return E4000_I2C_FAIL;
	}

	return E4000_I2C_SUCCESS;
}

static int
_I2CWriteArray(
	RTL2832_NAMESPACE::tuner* pTuner,
	unsigned char NoUse,
	unsigned char RegStartAddr,
	unsigned char ByteNum,
	unsigned char *pWritingBytes,
	const char* function = NULL, int line_number = -1, const char* line = NULL
	)
{
	unsigned int i;
	uint8_t WritingBuffer[I2C_BUFFER_LEN];

	WritingBuffer[0] = RegStartAddr;

	for(i = 0; i < ByteNum; i++)
		WritingBuffer[1 + i] = pWritingBytes[i];

	int r = pTuner->i2c_write(E4K_I2C_ADDR, WritingBuffer, ByteNum + 1);
	if (r <= 0)
	{
	  DEBUG_TUNER_I2C(pTuner,r);
	  return E4000_I2C_FAIL;
	}

	return E4000_I2C_SUCCESS;
}

// Swap these to disable debug string generation and verbose reporting

#define I2CReadByte(t,n,r,b)		_I2CReadByte(t,n,r,b,CURRENT_FUNCTION,__LINE__,"I2CReadByte("#t", "#n", "#r", "#b")")
//#define I2CReadByte(t,n,r,b)		_I2CReadByte(t,n,r,b)

#define I2CWriteByte(t,n,r,w)		_I2CWriteByte(t,n,r,w,CURRENT_FUNCTION,__LINE__,"I2CReadByte("#t", "#n", "#r", "#w")")
//#define I2CWriteByte(t,n,r,w)		_I2CWriteByte(t,n,r,w)

#define I2CWriteArray(t,n,r,b,w)	_I2CWriteArray(t,n,r,b,w,CURRENT_FUNCTION,__LINE__,"I2CReadByte("#t", "#n", "#r", "#b", "#w")")
//#define I2CWriteArray(t,n,r,b,w)	_I2CWriteArray(t,n,r,b,w)

///////////////////////////////////////////////////////////////////////////////

namespace RTL2832_NAMESPACE { namespace TUNERS_NAMESPACE {

static const int _mapGainsE4000[] = {	// nim_rtl2832_e4000.c
	-50,	2,
	-25,	3,
	0,		4,
	25,		5,
	50,		6,
	75,		7,
	100,	8,
	125,	9,
	150,	10,
	175,	11,
	200,	12,
	225,	13,
	250,	14,
	300,	15,	// Apparently still 250
};

int e4000::TUNER_PROBE_FN_NAME(demod* d)
{
	I2C_REPEATER_SCOPE(d);

	uint8_t reg = 0;
	//CHECK_LIBUSB_RESULT_RETURN_EX(d,d->i2c_read_reg(E4K_I2C_ADDR, E4K_CHECK_ADDR, reg));
	int r = d->i2c_read_reg(E4K_I2C_ADDR, E4K_CHECK_ADDR, reg);
	if (r <= 0) return r;
	return ((reg == E4K_CHECK_VAL) ? SUCCESS : FAILURE);
}

e4000::e4000(demod* p)
	: tuner_skeleton(p)
{
	for (int i = 0; i < sizeof(_mapGainsE4000)/sizeof(_mapGainsE4000[0]); i += 2)
		m_gain_values.push_back((double)_mapGainsE4000[i] / 10.0);

	values_to_range(m_gain_values, m_gain_range);

	//m_frequency_range	// 64-1700 MHz, but can do more so leave it undefined

	//m_bandwidth_values
	//values_to_range(m_bandwidth_values, m_bandwidth_range);

	m_bandwidth = 8000000;	// Default

	m_gain_modes.insert(make_pair(RTL2832_E4000_TUNER_GAIN_NORMAL, "nominal"));
	m_gain_modes.insert(make_pair(RTL2832_E4000_TUNER_GAIN_LINEAR, "linear"));
	m_gain_modes.insert(make_pair(RTL2832_E4000_TUNER_GAIN_SENSITIVE, "sensitive"));
}

int e4000::initialise(PPARAMS params /*= NULL*/)
{
	if (tuner_skeleton::initialise(params) != SUCCESS)
		return FAILURE;

	THIS_I2C_REPEATER_SCOPE();

	bool enable_dc_offset_loop = false;	// FIXME: Parameterise this (e.g. flag field in tuner::PARAMS, or another explicit property?)
	bool set_manual_gain = true;

	if (e4000_Initialize(this, enable_dc_offset_loop, set_manual_gain) != FUNCTION_SUCCESS)
		return FAILURE;

	if (set_bandwidth(bandwidth()) != SUCCESS)
		return FAILURE;

	if (m_params.message_output && m_params.verbose)
		m_params.message_output->on_log_message_ex(rtl2832::log_sink::LOG_LEVEL_VERBOSE, LOG_PREFIX"Initialised (default bandwidth: %i Hz)\n", (uint32_t)bandwidth());

	return SUCCESS;
}

int e4000::set_frequency(double freq)
{
	if ((freq <= 0) || (in_valid_range(m_frequency_range, freq) == false))
		return FAILURE;

	THIS_I2C_REPEATER_SCOPE();

	bool update_gain_control = false;	// FIXME: This should be based on 'set_manual_gain' in 'initialise'
	bool enable_dc_offset_lut = true;

	if (e4000_SetRfFreqHz(this, (unsigned long)freq, update_gain_control, enable_dc_offset_lut) != FUNCTION_SUCCESS)
		return FAILURE;

	m_freq = (int)((freq + 500.0) / 1000.0) * 1000;

	// FIXME: Technically should call 'update_gain_mode' if 'm_auto_gain_mode' is enabled, but I2C will become overloaded

	return SUCCESS;
}

int e4000::set_bandwidth(double bw)
{
	if ((bw <= 0) || (in_valid_range(m_bandwidth_range, bw) == false))
		return FAILURE;

	THIS_I2C_REPEATER_SCOPE();

	if (in_valid_range(m_bandwidth_range, bw) == false)
		return FAILURE;

	if (e4000_SetBandwidthHz(this, bw) != FUNCTION_SUCCESS)
		return FAILURE;
 
	m_bandwidth = (int)((bw + 500.0) / 1000.0) * 1000;

	return SUCCESS;
}

int e4000::set_gain(double gain)
{
	const int iCount = (sizeof(_mapGainsE4000)/sizeof(_mapGainsE4000[0])) / 2;

	int iGain = (int)(gain * 10.0);
	int i = get_map_index(iGain, _mapGainsE4000, iCount);

	if ((i == -1) || (i == iCount))
		return FAILURE;

	//if (i == -1)	// Below first -> select first
	//  i = 0;
	//else if (i == iCount)	// Above last -> select last
	//  i = iCount - 1;

	unsigned char u8Write = _mapGainsE4000[i + 1];

	THIS_I2C_REPEATER_SCOPE();

	unsigned char u8Read = 0;
	if (I2CReadByte(this, 0, RTL2832_E4000_LNA_GAIN_ADDR, &u8Read) != E4000_I2C_SUCCESS)
		return FAILURE;

	u8Write |= (u8Read & ~RTL2832_E4000_LNA_GAIN_MASK);
  
	if (I2CWriteByte(this, 0, RTL2832_E4000_LNA_GAIN_ADDR, u8Write) != E4000_I2C_SUCCESS)
		return FAILURE;

	m_gain = (double)_mapGainsE4000[i] / 10.0;

	if (m_auto_gain_mode)
	{
		if (update_gain_mode() != SUCCESS)
			return FAILURE;
	}

	return SUCCESS;
}

int e4000::update_gain_mode()
{
	int i;
	if (calc_appropriate_gain_mode(i))
	{
		if (set_gain_mode(i) != SUCCESS)
			return FAILURE;

		if (m_params.verbose)
		{
			num_name_map_t::iterator it = m_gain_modes.find(i);
			if (it != m_gain_modes.end())	// Double check
			{
				if (m_params.message_output)	// Not checking 'm_params.verbose' here
					m_params.message_output->on_log_message_ex(rtl2832::log_sink::LOG_LEVEL_VERBOSE, LOG_PREFIX"Gain mode: %s\n", it->second.c_str());
			}
		}
	}

	return SUCCESS;
}

int e4000::set_auto_gain_mode(bool on /*= true*/)
{
	if (on)
	{
		if (update_gain_mode() != SUCCESS)
			return FAILURE;
	}

	return tuner_skeleton::set_auto_gain_mode(on);
}

int e4000::set_gain_mode(int mode)
{
	int freq= (int)(frequency() / 1000.0);
	int bw	= (int)(bandwidth() / 1000.0);

	THIS_I2C_REPEATER_SCOPE();

	switch (mode)
	{
		default:
		case RTL2832_E4000_TUNER_GAIN_SENSITIVE:
			if (E4000_sensitivity(this, freq, bw) != E4000_I2C_SUCCESS)
				return FAILURE;
			break;
		case RTL2832_E4000_TUNER_GAIN_LINEAR:
			if (E4000_linearity(this, freq, bw) != E4000_I2C_SUCCESS)
				return FAILURE;
		  break;
		case RTL2832_E4000_TUNER_GAIN_NORMAL:
			if (E4000_nominal(this, freq, bw) != E4000_I2C_SUCCESS)
				return FAILURE;
		  break;
	}

	m_gain_mode = mode;

	return SUCCESS;
}

bool e4000::calc_appropriate_gain_mode(int& mode)/* const*/
{
	static const long LnaGainTable[RTL2832_E4000_LNA_GAIN_TABLE_LEN][RTL2832_E4000_LNA_GAIN_BAND_NUM] =
	{
		// VHF Gain,	UHF Gain,		ReadingByte
		{-50,			-50	},		//	0x0
		{-25,			-25	},		//	0x1
		{-50,			-50	},		//	0x2
		{-25,			-25	},		//	0x3
		{0,				0	},		//	0x4
		{25,			25	},		//	0x5
		{50,			50	},		//	0x6
		{75,			75	},		//	0x7
		{100,			100	},		//	0x8
		{125,			125	},		//	0x9
		{150,			150	},		//	0xa
		{175,			175	},		//	0xb
		{200,			200	},		//	0xc
		{225,			250	},		//	0xd
		{250,			280	},		//	0xe
		{250,			280	},		//	0xf

		// Note: The gain unit is 0.1 dB.
	};

	static const long LnaGainAddTable[RTL2832_E4000_LNA_GAIN_ADD_TABLE_LEN] =
	{
		// Gain,		ReadingByte
		NO_USE,		//	0x0
		NO_USE,		//	0x1
		NO_USE,		//	0x2
		0,			//	0x3
		NO_USE,		//	0x4
		20,			//	0x5
		NO_USE,		//	0x6
		70,			//	0x7

		// Note: The gain unit is 0.1 dB.
	};

	static const long MixerGainTable[RTL2832_E4000_MIXER_GAIN_TABLE_LEN][RTL2832_E4000_MIXER_GAIN_BAND_NUM] =
	{
		// VHF Gain,	UHF Gain,		ReadingByte
		{90,			40	},		//	0x0
		{170,			120	},		//	0x1

		// Note: The gain unit is 0.1 dB.
	};

	static const long IfStage1GainTable[RTL2832_E4000_IF_STAGE_1_GAIN_TABLE_LEN] =
	{
		// Gain,		ReadingByte
		-30,		//	0x0
		60,			//	0x1

		// Note: The gain unit is 0.1 dB.
	};

	static const long IfStage2GainTable[RTL2832_E4000_IF_STAGE_2_GAIN_TABLE_LEN] =
	{
		// Gain,		ReadingByte
		0,			//	0x0
		30,			//	0x1
		60,			//	0x2
		90,			//	0x3

		// Note: The gain unit is 0.1 dB.
	};

	static const long IfStage3GainTable[RTL2832_E4000_IF_STAGE_3_GAIN_TABLE_LEN] =
	{
		// Gain,		ReadingByte
		0,			//	0x0
		30,			//	0x1
		60,			//	0x2
		90,			//	0x3

		// Note: The gain unit is 0.1 dB.
	};

	static const long IfStage4GainTable[RTL2832_E4000_IF_STAGE_4_GAIN_TABLE_LEN] =
	{
		// Gain,		ReadingByte
		0,			//	0x0
		10,			//	0x1
		20,			//	0x2
		20,			//	0x3

		// Note: The gain unit is 0.1 dB.
	};

	static const long IfStage5GainTable[RTL2832_E4000_IF_STAGE_5_GAIN_TABLE_LEN] =
	{
		// Gain,		ReadingByte
		0,			//	0x0
		30,			//	0x1
		60,			//	0x2
		90,			//	0x3
		120,		//	0x4
		120,		//	0x5
		120,		//	0x6
		120,		//	0x7

		// Note: The gain unit is 0.1 dB.
	};

	static const long IfStage6GainTable[RTL2832_E4000_IF_STAGE_6_GAIN_TABLE_LEN] =
	{
		// Gain,		ReadingByte
		0,			//	0x0
		30,			//	0x1
		60,			//	0x2
		90,			//	0x3
		120,		//	0x4
		120,		//	0x5
		120,		//	0x6
		120,		//	0x7

		// Note: The gain unit is 0.1 dB.
	};

	unsigned long RfFreqHz;
	int RfFreqKhz;
	unsigned long BandwidthHz;
	int BandwidthKhz;

	unsigned char ReadingByte;
	int BandIndex;

	unsigned char TunerBitsLna, TunerBitsLnaAdd, TunerBitsMixer;
	unsigned char TunerBitsIfStage1, TunerBitsIfStage2, TunerBitsIfStage3, TunerBitsIfStage4;
	unsigned char TunerBitsIfStage5, TunerBitsIfStage6;

	long TunerGainLna, TunerGainLnaAdd, TunerGainMixer;
	long TunerGainIfStage1, TunerGainIfStage2, TunerGainIfStage3, TunerGainIfStage4;
	long TunerGainIfStage5, TunerGainIfStage6;

	long TunerGainTotal;
	long TunerInputPower;

	/////////////////////////
	int TunerGainMode = -1;
	THIS_I2C_REPEATER_SCOPE();
	/////////////////////////

	// Get tuner RF frequency in KHz.
	// Note: RfFreqKhz = round(RfFreqHz / 1000)
	//if(this->GetRfFreqHz(this, &RfFreqHz) != E4000_I2C_SUCCESS)
	//	goto error_status_get_tuner_registers;
	RfFreqHz = (unsigned long)frequency();

	RfFreqKhz = (int)((RfFreqHz + 500) / 1000);

	// Get tuner bandwidth in KHz.
	// Note: BandwidthKhz = round(BandwidthHz / 1000)
	//if(pTunerExtra->GetBandwidthHz(this, &BandwidthHz) != E4000_I2C_SUCCESS)
	//	goto error_status_get_tuner_registers;
	BandwidthHz = (unsigned long)bandwidth();

	BandwidthKhz = (int)((BandwidthHz + 500) / 1000);

	// Determine band index.
	BandIndex = (RfFreqHz < RTL2832_E4000_RF_BAND_BOUNDARY_HZ) ? 0 : 1;

	// Get tuner LNA gain according to reading byte and table.
	if(I2CReadByte(this, NO_USE, RTL2832_E4000_LNA_GAIN_ADDR, &ReadingByte) != E4000_I2C_SUCCESS)
		goto error_status_get_tuner_registers;

	TunerBitsLna = (ReadingByte & RTL2832_E4000_LNA_GAIN_MASK) >> RTL2832_E4000_LNA_GAIN_SHIFT;
	TunerGainLna = LnaGainTable[TunerBitsLna][BandIndex];

	// Get tuner LNA additional gain according to reading byte and table.
	if(I2CReadByte(this, NO_USE, RTL2832_E4000_LNA_GAIN_ADD_ADDR, &ReadingByte) != E4000_I2C_SUCCESS)
		goto error_status_get_tuner_registers;

	TunerBitsLnaAdd = (ReadingByte & RTL2832_E4000_LNA_GAIN_ADD_MASK) >> RTL2832_E4000_LNA_GAIN_ADD_SHIFT;
	TunerGainLnaAdd = LnaGainAddTable[TunerBitsLnaAdd];

	// Get tuner mixer gain according to reading byte and table.
	if(I2CReadByte(this, NO_USE, RTL2832_E4000_MIXER_GAIN_ADDR, &ReadingByte) != E4000_I2C_SUCCESS)
		goto error_status_get_tuner_registers;

	TunerBitsMixer = (ReadingByte & RTL2832_E4000_MIXER_GAIN_MASK) >> RTL2832_E4000_LNA_GAIN_ADD_SHIFT;
	TunerGainMixer = MixerGainTable[TunerBitsMixer][BandIndex];

	// Get tuner IF stage 1 gain according to reading byte and table.
	if(I2CReadByte(this, NO_USE, RTL2832_E4000_IF_STAGE_1_GAIN_ADDR, &ReadingByte) != E4000_I2C_SUCCESS)
		goto error_status_get_tuner_registers;

	TunerBitsIfStage1 = (ReadingByte & RTL2832_E4000_IF_STAGE_1_GAIN_MASK) >> RTL2832_E4000_IF_STAGE_1_GAIN_SHIFT;
	TunerGainIfStage1 = IfStage1GainTable[TunerBitsIfStage1];

	// Get tuner IF stage 2 gain according to reading byte and table.
	if(I2CReadByte(this, NO_USE, RTL2832_E4000_IF_STAGE_2_GAIN_ADDR, &ReadingByte) != E4000_I2C_SUCCESS)
		goto error_status_get_tuner_registers;

	TunerBitsIfStage2 = (ReadingByte & RTL2832_E4000_IF_STAGE_2_GAIN_MASK) >> RTL2832_E4000_IF_STAGE_2_GAIN_SHIFT;
	TunerGainIfStage2 = IfStage2GainTable[TunerBitsIfStage2];

	// Get tuner IF stage 3 gain according to reading byte and table.
	if(I2CReadByte(this, NO_USE, RTL2832_E4000_IF_STAGE_3_GAIN_ADDR, &ReadingByte) != E4000_I2C_SUCCESS)
		goto error_status_get_tuner_registers;

	TunerBitsIfStage3 = (ReadingByte & RTL2832_E4000_IF_STAGE_3_GAIN_MASK) >> RTL2832_E4000_IF_STAGE_3_GAIN_SHIFT;
	TunerGainIfStage3 = IfStage3GainTable[TunerBitsIfStage3];

	// Get tuner IF stage 4 gain according to reading byte and table.
	if(I2CReadByte(this, NO_USE, RTL2832_E4000_IF_STAGE_4_GAIN_ADDR, &ReadingByte) != E4000_I2C_SUCCESS)
		goto error_status_get_tuner_registers;

	TunerBitsIfStage4 = (ReadingByte & RTL2832_E4000_IF_STAGE_4_GAIN_MASK) >> RTL2832_E4000_IF_STAGE_4_GAIN_SHIFT;
	TunerGainIfStage4 = IfStage4GainTable[TunerBitsIfStage4];

	// Get tuner IF stage 5 gain according to reading byte and table.
	if(I2CReadByte(this, NO_USE, RTL2832_E4000_IF_STAGE_5_GAIN_ADDR, &ReadingByte) != E4000_I2C_SUCCESS)
		goto error_status_get_tuner_registers;

	TunerBitsIfStage5 = (ReadingByte & RTL2832_E4000_IF_STAGE_5_GAIN_MASK) >> RTL2832_E4000_IF_STAGE_5_GAIN_SHIFT;
	TunerGainIfStage5 = IfStage5GainTable[TunerBitsIfStage5];

	// Get tuner IF stage 6 gain according to reading byte and table.
	if(I2CReadByte(this, NO_USE, RTL2832_E4000_IF_STAGE_6_GAIN_ADDR, &ReadingByte) != E4000_I2C_SUCCESS)
		goto error_status_get_tuner_registers;

	TunerBitsIfStage6 = (ReadingByte & RTL2832_E4000_IF_STAGE_6_GAIN_MASK) >> RTL2832_E4000_IF_STAGE_6_GAIN_SHIFT;
	TunerGainIfStage6 = IfStage6GainTable[TunerBitsIfStage6];

	// Calculate tuner total gain.
	// Note: The unit of tuner total gain is 0.1 dB.
	TunerGainTotal = TunerGainLna + TunerGainLnaAdd + TunerGainMixer + 
	                 TunerGainIfStage1 + TunerGainIfStage2 + TunerGainIfStage3 + TunerGainIfStage4 +
	                 TunerGainIfStage5 + TunerGainIfStage6;

	// Calculate tuner input power.
	// Note: The unit of tuner input power is 0.1 dBm
	TunerInputPower = RTL2832_E4000_TUNER_OUTPUT_POWER_UNIT_0P1_DBM - TunerGainTotal;

/*	fprintf(stderr, _T("Current gain state:\n"
		"\tFreq:\t%i kHz\n"
		"\tBW:\t%i kHz\n"
		"\tLNA:\t%.1f dB\n"
		"\tLNA+:\t%.1f dB\n"
		"\tMixer:\t%.1f dB\n"
		"\tIF 1:\t%.1f dB\n"
		"\tIF 2:\t%.1f dB\n"
		"\tIF 3:\t%.1f dB\n"
		"\tIF 4:\t%.1f dB\n"
		"\tIF 5:\t%.1f dB\n"
		"\tIF 6:\t%.1f dB\n"
		"\tTotal:\t%.1f dB\n"
		"\tPower:\t%.1f dBm\n"),
		RfFreqKhz,
		BandwidthKhz,
		(float)TunerGainLna/10.0f,
		(float)TunerGainLnaAdd/10.0f,
		(float)TunerGainMixer/10.0f,
		(float)TunerGainIfStage1/10.0f,
		(float)TunerGainIfStage2/10.0f,
		(float)TunerGainIfStage3/10.0f,
		(float)TunerGainIfStage4/10.0f,
		(float)TunerGainIfStage5/10.0f,
		(float)TunerGainIfStage6/10.0f,
		(float)TunerGainTotal/10.0f,
		(float)TunerInputPower/10.0f);
*/
	// Determine tuner gain mode according to tuner input power.
	// Note: The unit of tuner input power is 0.1 dBm
	switch (m_gain_mode)
	{
		default:
		case RTL2832_E4000_TUNER_GAIN_SENSITIVE:
			if(TunerInputPower > -650)
				TunerGainMode = RTL2832_E4000_TUNER_GAIN_NORMAL;
			break;

		case RTL2832_E4000_TUNER_GAIN_NORMAL:
			if(TunerInputPower < -750)
				TunerGainMode = RTL2832_E4000_TUNER_GAIN_SENSITIVE;
			if(TunerInputPower > -400)
				TunerGainMode = RTL2832_E4000_TUNER_GAIN_LINEAR;
			break;

		case RTL2832_E4000_TUNER_GAIN_LINEAR:
			if(TunerInputPower < -500)
				TunerGainMode = RTL2832_E4000_TUNER_GAIN_NORMAL;
			break;
	}
	
	if (TunerGainMode == -1)	// No change
	{
		mode = m_gain_mode;
		return false;
	}

	mode = TunerGainMode;

	return true;
error_status_get_tuner_registers:
	mode = NOT_SUPPORTED;
	return false;
}

} } // TUNERS_NAMESPACE, RTL2832_NAMESPACE

///////////////////////////////////////////////////////////////////////////////

/**

@see   TUNER_FP_INITIALIZE

*/
int
e4000_Initialize(
	RTL2832_NAMESPACE::tuner* pTuner,
	bool enable_dc_offset_loop /*= true*/,
	bool set_manual_gain /*= false*/
	)
{
	// Initialize tuner.
	// Note: Call E4000 source code functions.
	if(tunerreset(pTuner) != E4000_1_SUCCESS)
		goto error_status_execute_function;

	if(Tunerclock(pTuner) != E4000_1_SUCCESS)
		goto error_status_execute_function;

	if(Qpeak(pTuner) != E4000_1_SUCCESS)
		goto error_status_execute_function;

	if (enable_dc_offset_loop)
	{
	  if(DCoffloop(pTuner) != E4000_1_SUCCESS)	// MY CHANGE: This introduces the periodic crap on the right side of the LO
		goto error_status_execute_function;
	}

	if(GainControlinit(pTuner) != E4000_1_SUCCESS)
		goto error_status_execute_function;

	///////////////////////////////////
	if (set_manual_gain)
	{
	  if(Gainmanual(pTuner) != E4000_1_SUCCESS)
		goto error_status_execute_function;
	}
	///////////////////////////////////

	return FUNCTION_SUCCESS;


error_status_execute_function:
	return FUNCTION_ERROR;
}

/**

@see   TUNER_FP_SET_RF_FREQ_HZ

*/
int
e4000_SetRfFreqHz(
	RTL2832_NAMESPACE::tuner* pTuner,
	unsigned long RfFreqHz,
	bool update_gain_control /*= true*/,
	bool enable_dc_offset_lut /*= true*/
	)
{
//	E4000_EXTRA_MODULE *pExtra;

	int RfFreqKhz;
	int CrystalFreqKhz;

	int CrystalFreqHz = /*CRYSTAL_FREQ*/pTuner->parent()->crystal_frequency();

	// Set tuner RF frequency in KHz.
	// Note: 1. RfFreqKhz = round(RfFreqHz / 1000)
	//          CrystalFreqKhz = round(CrystalFreqHz / 1000)
	//       2. Call E4000 source code functions.
	RfFreqKhz      = (int)((RfFreqHz + 500) / 1000);
	CrystalFreqKhz = (int)((CrystalFreqHz + 500) / 1000);

	if (update_gain_control)
	{
	  if(Gainmanual(pTuner) != E4000_1_SUCCESS)
		goto error_status_execute_function;
	}

	if(E4000_gain_freq(pTuner, RfFreqKhz) != E4000_1_SUCCESS)
		goto error_status_execute_function;

	if(PLL(pTuner, CrystalFreqKhz, RfFreqKhz) != E4000_1_SUCCESS)
		goto error_status_execute_function;

	if(LNAfilter(pTuner, RfFreqKhz) != E4000_1_SUCCESS)
		goto error_status_execute_function;

	if(freqband(pTuner, RfFreqKhz) != E4000_1_SUCCESS)
		goto error_status_execute_function;

	if (enable_dc_offset_lut)
	{
	  if (DCoffLUT(pTuner) != E4000_1_SUCCESS)	// Enabling this results in big increase in noise floor
		goto error_status_execute_function;
	}

	if (update_gain_control)
	{
	  if(GainControlauto(pTuner) != E4000_1_SUCCESS)	// CHANGED: Leaving it manual
		goto error_status_execute_function;
	}

	return FUNCTION_SUCCESS;


error_status_execute_function:
	return FUNCTION_ERROR;
}

/**

@brief   Set E4000 tuner bandwidth.

*/
int
e4000_SetBandwidthHz(
	RTL2832_NAMESPACE::tuner* pTuner,
	unsigned long BandwidthHz
	)
{
//	E4000_EXTRA_MODULE *pExtra;

	int BandwidthKhz;
	int CrystalFreqKhz;

	int CrystalFreqHz = /*CRYSTAL_FREQ*/pTuner->parent()->crystal_frequency();


	// Get tuner extra module.
//	pExtra = &(pTuner->Extra.E4000);


	// Set tuner bandwidth Hz.
	// Note: 1. BandwidthKhz = round(BandwidthHz / 1000)
	//          CrystalFreqKhz = round(CrystalFreqHz / 1000)
	//       2. Call E4000 source code functions.
	BandwidthKhz   = (int)((BandwidthHz + 500) / 1000);
	CrystalFreqKhz = (int)((CrystalFreqHz + 500) / 1000);

	if(IFfilter(pTuner, BandwidthKhz, CrystalFreqKhz) != E4000_1_SUCCESS)
		goto error_status_execute_function;


	return FUNCTION_SUCCESS;

error_status_execute_function:
	return FUNCTION_ERROR;
}


// The following context is source code provided by Elonics.

// Elonics source code - E4000_API_rev2_04_realtek.cpp


//****************************************************************************/
//
//  Filename    E4000_initialisation.c
//  Revision    2.04
//
// Description:
//  Initialisation script for the Elonics E4000 revC tuner
//
//  Copyright (c)  Elonics Ltd
//
//    Any software supplied free of charge for use with elonics
//    evaluation kits is supplied without warranty and for
//    evaluation purposes only. Incorporation of any of this
//    code into products for open sale is permitted but only at
//    the user's own risk. Elonics accepts no liability for the
//    integrity of this software whatsoever.
//
//
//****************************************************************************/
//#include <stdio.h>
//#include <stdlib.h>
//
// User defined variable definitions
//
/*
int Ref_clk = 26000;   // Reference clock frequency(kHz).
int Freq = 590000; // RF Frequency (kHz)
int bandwidth = 8000;  //RF channel bandwith (kHz)
*/
//
// API defined variable definitions
//int VCO_freq;
//unsigned char writearray[5];
//unsigned char read1[1];
//int status;
//
//
// function definitions
//
/*
int tunerreset ();
int Tunerclock();
int filtercal();
int Qpeak();
int PLL(int Ref_clk, int Freq);
int LNAfilter(int Freq);
int IFfilter(int bandwidth, int Ref_clk);
int freqband(int Freq);
int DCoffLUT();
int DCoffloop();
int commonmode();
int GainControlinit();
*/
//
//****************************************************************************
// --- Public functions ------------------------------------------------------
/****************************************************************************\
*  Function: tunerreset
*
*  Detailed Description:
*  The function resets the E4000 tuner. (Register 0x00).
*
\****************************************************************************/

int tunerreset(RTL2832_NAMESPACE::tuner* pTuner)
{
	unsigned char writearray[5];
	int status;

	writearray[0] = 64;
	// For dummy I2C command, don't check executing status.
	status=I2CWriteByte (pTuner, 200,2,writearray[0]);
	status=I2CWriteByte (pTuner, 200,2,writearray[0]);
	//printf("\nRegister 0=%d", writearray[0]);
	if(status != E4000_I2C_SUCCESS)
	{
		return E4000_1_FAIL;
	}

	writearray[0] = 0;
	status=I2CWriteByte (pTuner, 200,9,writearray[0]);
	if(status != E4000_I2C_SUCCESS)
	{
		return E4000_1_FAIL;
	}

	writearray[0] = 0;
	status=I2CWriteByte (pTuner, 200,5,writearray[0]);
	if(status != E4000_I2C_SUCCESS)
	{
		return E4000_1_FAIL;
	}

	writearray[0] = 7;
	status=I2CWriteByte (pTuner, 200,0,writearray[0]);
	//printf("\nRegister 0=%d", writearray[0]);
	if(status != E4000_I2C_SUCCESS)
	{
		return E4000_1_FAIL;
	}

	return E4000_1_SUCCESS;
}
/****************************************************************************\
*  Function: Tunerclock
*
*  Detailed Description:
*  The function configures the E4000 clock. (Register 0x06, 0x7a).
*  Function disables the clock - values can be modified to enable if required.
\****************************************************************************/

int Tunerclock(RTL2832_NAMESPACE::tuner* pTuner)
{
	unsigned char writearray[5];
	int status;

	writearray[0] = 0;
	status=I2CWriteByte(pTuner, 200,6,writearray[0]);
	//printf("\nRegister 6=%d", writearray[0]);
	if(status != E4000_I2C_SUCCESS)
	{
		return E4000_1_FAIL;
	}

	writearray[0] = 150;
	status=I2CWriteByte(pTuner, 200,122,writearray[0]);
	//printf("\nRegister 7a=%d", writearray[0]);
	//**Modify commands above with value required if output clock is required,
	if(status != E4000_I2C_SUCCESS)
	{
		return E4000_1_FAIL;
	}

	return E4000_1_SUCCESS;
}
/****************************************************************************\
*  Function: filtercal
*
*  Detailed Description:
*  Instructs RC filter calibration. (Register 0x7b).
*
\****************************************************************************/
/*
int filtercal(RTL2832_NAMESPACE::tuner* pTuner)
{
  //writearray[0] = 1;
 //I2CWriteByte (pTuner, 200,123,writearray[0]);
 //printf("\nRegister 7b=%d", writearray[0]);
  //return;
   return E4000_1_SUCCESS;
}
*/
/****************************************************************************\
*  Function: Qpeak()
*
*  Detailed Description:
*  The function configures the E4000 gains.
*  Also sigma delta controller. (Register 0x82).
*
\****************************************************************************/

int Qpeak(RTL2832_NAMESPACE::tuner* pTuner)
{
	unsigned char writearray[5];
	int status;

	writearray[0] = 1;
	writearray[1] = 254;
	status=I2CWriteArray(pTuner, 200,126,2,writearray);
	//printf("\nRegister 7e=%d", writearray[0]);
	//printf("\nRegister 7f=%d", writearray[1]);
	if(status != E4000_I2C_SUCCESS)
	{
		return E4000_1_FAIL;
	}

	status=I2CWriteByte (pTuner, 200,130,0);
	//printf("\nRegister 82=0");
	if(status != E4000_I2C_SUCCESS)
	{
		return E4000_1_FAIL;
	}

	status=I2CWriteByte (pTuner, 200,36,5);
	//printf("\nRegister 24=5");
	if(status != E4000_I2C_SUCCESS)
	{
		return E4000_1_FAIL;
	}

	writearray[0] = 32;
	writearray[1] = 1;
	status=I2CWriteArray(pTuner, 200,135,2,writearray);
	if(status != E4000_I2C_SUCCESS)
	{
		return E4000_1_FAIL;
	}

	//printf("\nRegister 87=%d", writearray[0]);
	//printf("\nRegister 88=%d", writearray[1]);
	return E4000_1_SUCCESS;
}
/****************************************************************************\
*  Function: E4000_gain_freq()
*
*  Detailed Description:
*  The function configures the E4000 gains vs. freq
*  0xa3 to 0xa7. Also 0x24.
*
\****************************************************************************/
int E4000_gain_freq(RTL2832_NAMESPACE::tuner* pTuner, int Freq)
{
	unsigned char writearray[5];
	int status;

/*	if (Freq<=350000)
	{
		writearray[0] = 0x10;
		writearray[1] = 0x42;
		writearray[2] = 0x09;
		writearray[3] = 0x21;
		writearray[4] = 0x94;
	}
	else if(Freq>=1000000)
	{
		writearray[0] = 0x10;
		writearray[1] = 0x42;
		writearray[2] = 0x09;
		writearray[3] = 0x21;
		writearray[4] = 0x94;
	}
	else
*/	{
		writearray[0] = 0x10;
		writearray[1] = 0x42;
		writearray[2] = 0x09;
		writearray[3] = 0x21;
		writearray[4] = 0x94;
	}
	status=I2CWriteArray(pTuner, 200,163,5,writearray);
	if(status != E4000_I2C_SUCCESS)
	{
		return E4000_1_FAIL;
	}

	if (Freq<=350000)
	{
		writearray[0] = 94;
		writearray[1] = 6;
		status=I2CWriteArray(pTuner, 200,159,2,writearray);
		if(status != E4000_I2C_SUCCESS)
		{
			return E4000_1_FAIL;
		}

		writearray[0] = 0;
		status=I2CWriteArray(pTuner, 200,136,1,writearray);
		if(status != E4000_I2C_SUCCESS)
		{
			return E4000_1_FAIL;
		}
	}
	else
	{
		writearray[0] = 127;
		writearray[1] = 7;
		status=I2CWriteArray(pTuner, 200,159,2,writearray);
		if(status != E4000_I2C_SUCCESS)
		{
			return E4000_1_FAIL;
		}

		writearray[0] = 1;
		status=I2CWriteArray(pTuner, 200,136,1,writearray);
		if(status != E4000_I2C_SUCCESS)
		{
			return E4000_1_FAIL;
		}
	}

	//printf("\nRegister 9f=%d", writearray[0]);
	//printf("\nRegister a0=%d", writearray[1]);
	return E4000_1_SUCCESS;
}
/****************************************************************************\
*  Function: DCoffloop
*
*  Detailed Description:
*  Populates DC offset LUT. (Registers 0x2d, 0x70, 0x71).
*  Turns on DC offset LUT and time varying DC offset.
\****************************************************************************/
int DCoffloop(RTL2832_NAMESPACE::tuner* pTuner)
{
	unsigned char writearray[5];
	int status;

	//writearray[0]=0;
	//I2CWriteByte(pTuner, 200,115,writearray[0]);
	//printf("\nRegister 73=%d", writearray[0]);
	writearray[0] = 31;
	status=I2CWriteByte(pTuner, 200,45,writearray[0]);
	//printf("\nRegister 2d=%d", writearray[0]);
	if(status != E4000_I2C_SUCCESS)
	{
		return E4000_1_FAIL;
	}

	writearray[0] = 1;
	writearray[1] = 1;
	status=I2CWriteArray(pTuner, 200,112,2,writearray);
	//printf("\nRegister 70=%d", writearray[0]);
	//printf("\nRegister 71=%d", writearray[0]);
	if(status != E4000_I2C_SUCCESS)
	{
		return E4000_1_FAIL;
	}

	return E4000_1_SUCCESS;
}
/****************************************************************************\
*  Function: commonmode
*
*  Detailed Description:
*  Configures common mode voltage. (Registers 0x2f).
*
\****************************************************************************/
/*
int commonmode(RTL2832_NAMESPACE::tuner* pTuner)
{
     //writearray[0] = 0;
     //I2CWriteByte(Device_address,47,writearray[0]);
     //printf("\nRegister 0x2fh = %d", writearray[0]);
     // Sets 550mV. Modify if alternative is desired.
     return E4000_1_SUCCESS;
}
*/
/****************************************************************************\
*  Function: GainControlinit
*
*  Detailed Description:
*  Configures gain control mode. (Registers 0x1d, 0x1e, 0x1f, 0x20, 0x21,
*  0x1a, 0x74h, 0x75h).
*  User may wish to modify values depending on usage scenario.
*  Routine configures LNA: autonomous gain control
*  IF PWM gain control.
*  PWM thresholds = default
*  Mixer: switches when LNA gain =7.5dB
*  Sensitivity / Linearity mode: manual switch
*
\****************************************************************************/
int GainControlinit(RTL2832_NAMESPACE::tuner* pTuner)
{
	unsigned char writearray[5];
	unsigned char read1[1];
	int status;

	unsigned char sum=255;

	writearray[0] = 23;
	status=I2CWriteByte(pTuner, 200,26,writearray[0]);
	if(status != E4000_I2C_SUCCESS)
	{
		return E4000_1_FAIL;
	}
	//printf("\nRegister 1a=%d", writearray[0]);

	status=I2CReadByte(pTuner, 201,27,read1);
	if(status != E4000_I2C_SUCCESS)
	{
		return E4000_1_FAIL;
	}

	writearray[0] = 16;
	writearray[1] = 4;
	writearray[2] = 26;
	writearray[3] = 15;
	writearray[4] = 167;
	status=I2CWriteArray(pTuner, 200,29,5,writearray);
	//printf("\nRegister 1d=%d", writearray[0]);
	if(status != E4000_I2C_SUCCESS)
	{
		return E4000_1_FAIL;
	}

	writearray[0] = 81;
	status=I2CWriteByte(pTuner, 200,134,writearray[0]);
	if(status != E4000_I2C_SUCCESS)
	{
		return E4000_1_FAIL;
	}
	//printf("\nRegister 86=%d", writearray[0]);
///////////////////////////////////////////////////////////////////////////////
	//For Realtek - gain control logic
	status=I2CReadByte(pTuner, 201,27,read1);
	if(status != E4000_I2C_SUCCESS)
	{
		return E4000_1_FAIL;
	}

	if(read1[0]<=sum)
	{
		sum=read1[0];
	}

	status=I2CWriteByte(pTuner, 200,31,writearray[2]);
	if(status != E4000_I2C_SUCCESS)
	{
		return E4000_1_FAIL;
	}
///////////////////////////////////////
	status=I2CReadByte(pTuner, 201,27,read1);
	if(status != E4000_I2C_SUCCESS)
	{
		return E4000_1_FAIL;
	}

	if(read1[0] <= sum)
	{
		sum=read1[0];
	}

	status=I2CWriteByte(pTuner, 200,31,writearray[2]);
	if(status != E4000_I2C_SUCCESS)
	{
		return E4000_1_FAIL;
	}
///////////////////////////////////////
	status=I2CReadByte(pTuner, 201,27,read1);
	if(status != E4000_I2C_SUCCESS)
	{
		return E4000_1_FAIL;
	}

	if(read1[0] <= sum)
	{
		sum=read1[0];
	}

	status=I2CWriteByte(pTuner, 200,31,writearray[2]);
	if(status != E4000_I2C_SUCCESS)
	{
		return E4000_1_FAIL;
	}
///////////////////////////////////////
	status=I2CReadByte(pTuner, 201,27,read1);
	if(status != E4000_I2C_SUCCESS)
	{
		return E4000_1_FAIL;
	}

	if(read1[0] <= sum)
	{
		sum=read1[0];
	}

	status=I2CWriteByte(pTuner, 200,31,writearray[2]);
	if(status != E4000_I2C_SUCCESS)
	{
		return E4000_1_FAIL;
	}
///////////////////////////////////////
	status=I2CReadByte(pTuner, 201,27,read1);
	if(status != E4000_I2C_SUCCESS)
	{
		return E4000_1_FAIL;
	}

	if (read1[0]<=sum)
	{
		sum=read1[0];
	}

	writearray[0]=sum;
	status=I2CWriteByte(pTuner, 200,27,writearray[0]);
	if(status != E4000_I2C_SUCCESS)
	{
		return E4000_1_FAIL;
	}
///////////////////////////////////////////////////////////////////////////////
	//printf("\nRegister 1b=%d", writearray[0]);
	//printf("\nRegister 1e=%d", writearray[1]);
	//printf("\nRegister 1f=%d", writearray[2]);
	//printf("\nRegister 20=%d", writearray[3]);
	//printf("\nRegister 21=%d", writearray[4]);
	//writearray[0] = 3;
	//writearray[1] = 252;
	//writearray[2] = 3;
	//writearray[3] = 252;
	//I2CWriteArray(pTuner, 200,116,4,writearray);
	//printf("\nRegister 74=%d", writearray[0]);
	//printf("\nRegister 75=%d", writearray[1]);
	//printf("\nRegister 76=%d", writearray[2]);
	//printf("\nRegister 77=%d", writearray[3]);

	return E4000_1_SUCCESS;
}

/****************************************************************************\
*  Main program
*
*
*
\****************************************************************************/
/*
int main()
{
     tunerreset ();
     Tunerclock();
     //filtercal();
     Qpeak();
     //PLL(Ref_clk, Freq);
     //LNAfilter(Freq);
     //IFfilter(bandwidth, Ref_clk);
     //freqband(Freq);
     //DCoffLUT();
     DCoffloop();
     //commonmode();
     GainControlinit();
//     system("PAUSE");
  return(0);
}
*/


// Elonics source code - frequency_change_rev2.04_realtek.c


//****************************************************************************/
//
//  Filename    E4000_freqchangerev2.04.c
//  Revision    2.04
//
// Description:
//  Frequency change script for the Elonics E4000 revB tuner
//
//  Copyright (c)  Elonics Ltd
//
//    Any software supplied free of charge for use with elonics
//    evaluation kits is supplied without warranty and for
//    evaluation purposes only. Incorporation of any of this
//    code into products for open sale is permitted but only at
//    the user's own risk. Elonics accepts no liability for the
//    integrity of this software whatsoever.
//
//
//****************************************************************************/
//#include <stdio.h>
//#include <stdlib.h>
//
// User defined variable definitions
//
/*
int Ref_clk = 20000;   // Reference clock frequency(kHz).
int Freq = 590000; // RF Frequency (kHz)
int bandwidth = 8;  //RF channel bandwith (MHz)
*/
//
// API defined variable definitions
//int VCO_freq;
//unsigned char writearray[5];
//unsigned char read1[1];
//int E4000_1_SUCCESS;
//int E4000_1_FAIL;
//int E4000_I2C_SUCCESS;
//int status;
//
//
// function definitions
//
/*
int Gainmanual();
int PLL(int Ref_clk, int Freq);
int LNAfilter(int Freq);
int IFfilter(int bandwidth, int Ref_clk);
int freqband(int Freq);
int DCoffLUT();
int GainControlauto();
*/
//
//****************************************************************************
// --- Public functions ------------------------------------------------------
/****************************************************************************\
//****************************************************************************\
*  Function: Gainmanual
*
*  Detailed Description:
*  Sets Gain control to serial interface control.
*
\****************************************************************************/
int Gainmanual(RTL2832_NAMESPACE::tuner* pTuner)
{
	unsigned char writearray[5];
	int status;

	writearray[0]=0;
	status=I2CWriteByte(pTuner, 200,26,writearray[0]);
	//printf("\nRegister 1a=%d", writearray[0]);
	if(status != E4000_I2C_SUCCESS)
	{
		return E4000_1_FAIL;
	}

	writearray[0] = 0;
	status=I2CWriteByte (pTuner, 200,9,writearray[0]);
	if(status != E4000_I2C_SUCCESS)
	{
		return E4000_1_FAIL;
	}

	writearray[0] = 0;
	status=I2CWriteByte (pTuner, 200,5,writearray[0]);
	if(status != E4000_I2C_SUCCESS)
	{
		return E4000_1_FAIL;
	}

	return E4000_1_SUCCESS;
}

/****************************************************************************\
*  Function: PLL
*
*  Detailed Description:
*  Configures E4000 PLL divider & sigma delta. 0x0d,0x09, 0x0a, 0x0b).
*
\****************************************************************************/
int PLL(RTL2832_NAMESPACE::tuner* pTuner, int Ref_clk, int Freq)
{
	int VCO_freq;
	unsigned char writearray[5];
	int status;

	unsigned char divider;
	int intVCOfreq;
	int SigDel;
	int SigDel2;
	int SigDel3;
//	int harmonic_freq;
//	int offset;

	if (Freq<=72400)
	{
		writearray[4] = 15;
		VCO_freq=Freq*48;
	}
	else if (Freq<=81200)
    {
		writearray[4] = 14;
		VCO_freq=Freq*40;
	}
	else if (Freq<=108300)
	{
		writearray[4]=13;
		VCO_freq=Freq*32;
	}
	else if (Freq<=162500)
	{
		writearray[4]=12;
		VCO_freq=Freq*24;
	}
	else if (Freq<=216600)
	{
		writearray[4]=11;
		VCO_freq=Freq*16;
	}
	else if (Freq<=325000)
	{
		writearray[4]=10;
		VCO_freq=Freq*12;
	}
	else if (Freq<=350000)
	{
		writearray[4]=9;
		VCO_freq=Freq*8;
	}
	else if (Freq<=432000)
	{
		writearray[4]=3;
		VCO_freq=Freq*8;
	}
	else if (Freq<=667000)
	{
		writearray[4]=2;
		VCO_freq=Freq*6;
	}
	else if (Freq<=1200000)
	{
		writearray[4]=1;
		VCO_freq=Freq*4;
	}
	else
	{
		writearray[4]=0;
		VCO_freq=Freq*2;
	}

	//printf("\nVCOfreq=%d", VCO_freq);
//	divider =  VCO_freq * 1000 / Ref_clk;
	divider =  VCO_freq / Ref_clk;
	//printf("\ndivider=%d", divider);
	writearray[0]= divider;
//	intVCOfreq = divider * Ref_clk /1000;
	intVCOfreq = divider * Ref_clk;
	//printf("\ninteger VCO freq=%d", intVCOfreq);
//	SigDel=65536 * 1000 * (VCO_freq - intVCOfreq) / Ref_clk;
	SigDel=65536 * (VCO_freq - intVCOfreq) / Ref_clk;
	//printf("\nSigma delta=%d", SigDel);
	if (SigDel<=1024)
	{
		SigDel = 1024;
	}
	else if (SigDel>=64512)
	{
		SigDel=64512;
	}
	SigDel2 = SigDel / 256;
	//printf("\nSigdel2=%d", SigDel2);
	writearray[2] = (unsigned char)SigDel2;
	SigDel3 = SigDel - (256 * SigDel2);
	//printf("\nSig del3=%d", SigDel3);
	writearray[1]= (unsigned char)SigDel3;
	writearray[3]=(unsigned char)0;
	status=I2CWriteArray(pTuner, 200,9,5,writearray);
	//printf("\nRegister 9=%d", writearray[0]);
	//printf("\nRegister a=%d", writearray[1]);
	//printf("\nRegister b=%d", writearray[2]);
	//printf("\nRegister d=%d", writearray[4]);
	if(status != E4000_I2C_SUCCESS)
	{
		return E4000_1_FAIL;
	}

	if (Freq<=82900)
	{
		writearray[0]=0;
		writearray[2]=1;
	}
	else if (Freq<=89900)
	{
		writearray[0]=3;
		writearray[2]=9;
	}
	else if (Freq<=111700)
	{
		writearray[0]=0;
		writearray[2]=1;
	}
	else if (Freq<=118700)
	{
		writearray[0]=3;
		writearray[2]=1;
	}
	else if (Freq<=140500)
	{
		writearray[0]=0;
		writearray[2]=3;
	}
	else if (Freq<=147500)
	{
		writearray[0]=3;
		writearray[2]=11;
	}
	else if (Freq<=169300)
	{
		writearray[0]=0;
		writearray[2]=3;
	}
	else if (Freq<=176300)
	{
		writearray[0]=3;
		writearray[2]=11;
	}
	else if (Freq<=198100)
	{
		writearray[0]=0;
		writearray[2]=3;
	}
	else if (Freq<=205100)
	{
		writearray[0]=3;
		writearray[2]=19;
	}
	else if (Freq<=226900)
	{
		writearray[0]=0;
		writearray[2]=3;
	}
	else if (Freq<=233900)
	{
		writearray[0]=3;
		writearray[2]=3;
	}
	else if (Freq<=350000)
	{
		writearray[0]=0;
		writearray[2]=3;
	}
	else if (Freq<=485600)
	{
		writearray[0]=0;
		writearray[2]=5;
	}
	else if (Freq<=493600)
	{
		writearray[0]=3;
		writearray[2]=5;
	}
	else if (Freq<=514400)
	{
		writearray[0]=0;
		writearray[2]=5;
	}
	else if (Freq<=522400)
	{
		writearray[0]=3;
		writearray[2]=5;
	}
	else if (Freq<=543200)
	{
		writearray[0]=0;
		writearray[2]=5;
	}
	else if (Freq<=551200)
	{
		writearray[0]=3;
		writearray[2]=13;
	}
	else if (Freq<=572000)
	{
		writearray[0]=0;
		writearray[2]=5;
	}
	else if (Freq<=580000)
	{
		writearray[0]=3;
		writearray[2]=13;
	}
	else if (Freq<=600800)
	{
		writearray[0]=0;
		writearray[2]=5;
	}
	else if (Freq<=608800)
	{
		writearray[0]=3;
		writearray[2]=13;
	}
	else if (Freq<=629600)
	{
		writearray[0]=0;
		writearray[2]=5;
	}
	else if (Freq<=637600)
	{
		writearray[0]=3;
		writearray[2]=13;
	}
	else if (Freq<=658400)
	{
		writearray[0]=0;
		writearray[2]=5;
	}
	else if (Freq<=666400)
	{
		writearray[0]=3;
		writearray[2]=13;
	}
	else if (Freq<=687200)
	{
		writearray[0]=0;
		writearray[2]=5;
	}
	else if (Freq<=695200)
	{
		writearray[0]=3;
		writearray[2]=13;
	}
	else if (Freq<=716000)
	{
		writearray[0]=0;
		writearray[2]=5;
	}
	else if (Freq<=724000)
	{
		writearray[0]=3;
		writearray[2]=13;
	}
	else if (Freq<=744800)
	{
		writearray[0]=0;
		writearray[2]=5;
	}
	else if (Freq<=752800)
	{
		writearray[0]=3;
		writearray[2]=21;
	}
	else if (Freq<=773600)
	{
		writearray[0]=0;
		writearray[2]=5;
	}
	else if (Freq<=781600)
	{
		writearray[0]=3;
		writearray[2]=21;
	}
	else if (Freq<=802400)
	{
		writearray[0]=0;
		writearray[2]=5;
	}
	else if (Freq<=810400)
	{
		writearray[0]=3;
		writearray[2]=21;
	}
	else if (Freq<=831200)
	{
		writearray[0]=0;
		writearray[2]=5;
	}
	else if (Freq<=839200)
	{
		writearray[0]=3;
		writearray[2]=21;
	}
	else if (Freq<=860000)
	{
		writearray[0]=0;
		writearray[2]=5;
	}
	else if (Freq<=868000)
	{
		writearray[0]=3;
		writearray[2]=21;
	}
	else
	{
		writearray[0]=0;
		writearray[2]=7;
	}

	status=I2CWriteByte (pTuner, 200,7,writearray[2]);
	if(status != E4000_I2C_SUCCESS)
	{
		return E4000_1_FAIL;
	}

	status=I2CWriteByte (pTuner, 200,5,writearray[0]);
	if(status != E4000_I2C_SUCCESS)
	{
		return E4000_1_FAIL;
	}

	return E4000_1_SUCCESS;
}

/****************************************************************************\
*  Function: LNAfilter
*
*  Detailed Description:
*  The function configures the E4000 LNA filter. (Register 0x10).
*
\****************************************************************************/

int LNAfilter(RTL2832_NAMESPACE::tuner* pTuner, int Freq)
{
	unsigned char writearray[5];
	int status;

	if(Freq<=370000)
	{
		writearray[0]=0;
	}
	else if(Freq<=392500)
	{
		writearray[0]=1;
	}
	else if(Freq<=415000)
	{
		writearray[0] =2;
	}
	else if(Freq<=437500)
	{
		writearray[0]=3;
	}
	else if(Freq<=462500)
	{
		writearray[0]=4;
	}
	else if(Freq<=490000)
	{
		writearray[0]=5;
	}
	else if(Freq<=522500)
	{
		writearray[0]=6;
	}
	else if(Freq<=557500)
	{
		writearray[0]=7;
	}
	else if(Freq<=595000)
	{
		writearray[0]=8;
	}
	else if(Freq<=642500)
	{
		writearray[0]=9;
	}
	else if(Freq<=695000)
	{
		writearray[0]=10;
	}
	else if(Freq<=740000)
	{
		writearray[0]=11;
	}
	else if(Freq<=800000)
	{
		writearray[0]=12;
	}
	else if(Freq<=865000)
	{
		writearray[0] =13;
	}
	else if(Freq<=930000)
	{
		writearray[0]=14;
	}
	else if(Freq<=1000000)
	{
		writearray[0]=15;
	}
	else if(Freq<=1310000)
	{
		writearray[0]=0;
	}
	else if(Freq<=1340000)
	{
		writearray[0]=1;
	}
	else if(Freq<=1385000)
	{
		writearray[0]=2;
	}
	else if(Freq<=1427500)
	{
		writearray[0]=3;
	}
	else if(Freq<=1452500)
	{
		writearray[0]=4;
	}
	else if(Freq<=1475000)
	{
		writearray[0]=5;
	}
	else if(Freq<=1510000)
	{
		writearray[0]=6;
	}
	else if(Freq<=1545000)
	{
		writearray[0]=7;
	}
	else if(Freq<=1575000)
	{
		writearray[0] =8;
	}
	else if(Freq<=1615000)
	{
		writearray[0]=9;
	}
	else if(Freq<=1650000)
	{
		writearray[0] =10;
	}
	else if(Freq<=1670000)
	{
		writearray[0]=11;
	}
	else if(Freq<=1690000)
	{
		writearray[0]=12;
	}
	else if(Freq<=1710000)
	{
		writearray[0]=13;
	}
	else if(Freq<=1735000)
	{
		writearray[0]=14;
	}
	else
	{
		writearray[0]=15;
	}
	status=I2CWriteByte (pTuner, 200,16,writearray[0]);
	//printf("\nRegister 10=%d", writearray[0]);
	if(status != E4000_I2C_SUCCESS)
	{
		return E4000_1_FAIL;
	}

	return E4000_1_SUCCESS;
}
/****************************************************************************\
*  Function: IFfilter
*
*  Detailed Description:
*  The function configures the E4000 IF filter. (Register 0x11,0x12).
*
\****************************************************************************/
int IFfilter(RTL2832_NAMESPACE::tuner* pTuner, int bandwidth, int Ref_clk)
{
	unsigned char writearray[5];
	int status;

	int IF_BW;

	IF_BW = bandwidth / 2;
	if(IF_BW<=2150)
	{
		writearray[0]=253;
		writearray[1]=31;
	}
	else if(IF_BW<=2200)
	{
		writearray[0]=253;
		writearray[1]=30;
	}
	else if(IF_BW<=2240)
	{
		writearray[0]=252;
		writearray[1]=29;
	}
	else if(IF_BW<=2280)
	{
		writearray[0]=252;
		writearray[1]=28;
	}
	else if(IF_BW<=2300)
	{
		writearray[0]=252;
		writearray[1]=27;
	}
	else if(IF_BW<=2400)
	{
		writearray[0]=252;
		writearray[1]=26;
	}
	else if(IF_BW<=2450)
	{
		writearray[0]=252;
		writearray[1]=25;
	}
	else if(IF_BW<=2500)
	{
		writearray[0]=252;
		writearray[1]=24;
	}
	else if(IF_BW<=2550)
	{
		writearray[0]=252;
		writearray[1]=23;
	}
	else if(IF_BW<=2600)
	{
		writearray[0]=252;
		writearray[1]=22;
	}
	else if(IF_BW<=2700)
	{
		writearray[0]=252;
		writearray[1]=21;
	}
	else if(IF_BW<=2750)
	{
		writearray[0]=252;
		writearray[1]=20;
	}
	else if(IF_BW<=2800)
	{
		writearray[0]=252;
		writearray[1]=19;
	}
	else if(IF_BW<=2900)
	{
		writearray[0]=251;
		writearray[1]=18;
	}
	else if(IF_BW<=2950)
	{
		writearray[0]=251;
		writearray[1]=17;
	}
	else if(IF_BW<=3000)
	{
		writearray[0]=251;
		writearray[1]=16;
	}
	else if(IF_BW<=3100)
	{
		writearray[0]=251;
		writearray[1]=15;
	}
	else if(IF_BW<=3200)
	{
		writearray[0]=250;
		writearray[1]=14;
	}
	else if(IF_BW<=3300)
	{
		writearray[0]=250;
		writearray[1]=13;
	}
	else if(IF_BW<=3400)
	{
		writearray[0]=249;
		writearray[1]=12;
	}
	else if(IF_BW<=3600)
	{
		writearray[0]=249;
		writearray[1]=11;
	}
	else if(IF_BW<=3700)
	{
		writearray[0]=249;
		writearray[1]=10;
	}
	else if(IF_BW<=3800)
	{
		writearray[0]=248;
		writearray[1]=9;
	}
	else if(IF_BW<=3900)
	{
		writearray[0]=248;
		writearray[1]=8;
	}
	else if(IF_BW<=4100)
	{
		writearray[0]=248;
		writearray[1]=7;
	}
	else if(IF_BW<=4300)
	{
		writearray[0]=247;
		writearray[1]=6;
	}
	else if(IF_BW<=4400)
	{
		writearray[0]=247;
		writearray[1]=5;
	}
	else if(IF_BW<=4600)
	{
		writearray[0]=247;
		writearray[1]=4;
	}
	else if(IF_BW<=4800)
	{
		writearray[0]=246;
		writearray[1]=3;
	}
	else if(IF_BW<=5000)
	{
		writearray[0]=246;
		writearray[1]=2;
	}
	else if(IF_BW<=5300)
	{
		writearray[0]=245;
		writearray[1]=1;
	}
	else if(IF_BW<=5500)
	{
		writearray[0]=245;
		writearray[1]=0;
	}
	else
	{
		writearray[0]=0;
		writearray[1]=32;
	}
	status=I2CWriteArray(pTuner, 200,17,2,writearray);
	//printf("\nRegister 11=%d", writearray[0]);
	//printf("\nRegister 12=%d", writearray[1]);
	if(status != E4000_I2C_SUCCESS)
	{
		return E4000_1_FAIL;
	}

	return E4000_1_SUCCESS;
}
/****************************************************************************\
*  Function: freqband
*
*  Detailed Description:
*  Configures the E4000 frequency band. (Registers 0x07, 0x78).
*
\****************************************************************************/
int freqband(RTL2832_NAMESPACE::tuner* pTuner, int Freq)
{
	unsigned char writearray[5];
	int status;

	if (Freq<=140000)
	{
		writearray[0] = 3;
		status=I2CWriteByte(pTuner, 200,120,writearray[0]);
		if(status != E4000_I2C_SUCCESS)
		{
			return E4000_1_FAIL;
		}
	}
	else if (Freq<=350000)
	{
		writearray[0] = 3;
		status=I2CWriteByte(pTuner, 200,120,writearray[0]);
		if(status != E4000_I2C_SUCCESS)
		{
			return E4000_1_FAIL;
		}
	}
	else if (Freq<=1000000)
	{
		writearray[0] = 3;
		status=I2CWriteByte(pTuner, 200,120,writearray[0]);
		if(status != E4000_I2C_SUCCESS)
		{
			return E4000_1_FAIL;
		}
	}
	else
	{
		writearray[0] = 7;
		status=I2CWriteByte(pTuner, 200,7,writearray[0]);
		if(status != E4000_I2C_SUCCESS)
		{
			return E4000_1_FAIL;
		}

		writearray[0] = 0;
		status=I2CWriteByte(pTuner, 200,120,writearray[0]);
		if(status != E4000_I2C_SUCCESS)
		{
			return E4000_1_FAIL;
		}
	}

	return E4000_1_SUCCESS;
}
/****************************************************************************\
*  Function: DCoffLUT
*
*  Detailed Description:
*  Populates DC offset LUT. (Registers 0x50 - 0x53, 0x60 - 0x63).
*
\****************************************************************************/
int DCoffLUT(RTL2832_NAMESPACE::tuner* pTuner)
{
	unsigned char writearray[5];
	int status;

	unsigned char read1[1];
	unsigned char IOFF;
	unsigned char QOFF;
	unsigned char RANGE1;
//	unsigned char RANGE2;
	unsigned char QRANGE;
	unsigned char IRANGE;
	writearray[0] = 0;
	writearray[1] = 126;
	writearray[2] = 36;
	status=I2CWriteArray(pTuner, 200,21,3,writearray);
	if(status != E4000_I2C_SUCCESS)
	{
		return E4000_1_FAIL;
	}

	// Sets mixer & IF stage 1 gain = 00 and IF stg 2+ to max gain.
	writearray[0] = 1;
	status=I2CWriteByte(pTuner, 200,41,writearray[0]);
	// Instructs a DC offset calibration.
	status=I2CReadByte(pTuner, 201,42,read1);
	if(status != E4000_I2C_SUCCESS)
	{
		return E4000_1_FAIL;
	}

	IOFF=read1[0];
	status=I2CReadByte(pTuner, 201,43,read1);
	if(status != E4000_I2C_SUCCESS)
	{
		return E4000_1_FAIL;
	}

	QOFF=read1[0];
	status=I2CReadByte(pTuner, 201,44,read1);
	if(status != E4000_I2C_SUCCESS)
	{
		return E4000_1_FAIL;
	}

	RANGE1=read1[0];
	//reads DC offset values back
	if(RANGE1>=32)
	{
		RANGE1 = RANGE1 -32;
	}
	if(RANGE1>=16)
	{
		RANGE1 = RANGE1 - 16;
	}
	IRANGE=RANGE1;
	QRANGE = (read1[0] - RANGE1) / 16;

	writearray[0] = (IRANGE * 64) + IOFF;
	status=I2CWriteByte(pTuner, 200,96,writearray[0]);
	if(status != E4000_I2C_SUCCESS)
	{
		return E4000_1_FAIL;
	}

	writearray[0] = (QRANGE * 64) + QOFF;
	status=I2CWriteByte(pTuner, 200,80,writearray[0]);
	if(status != E4000_I2C_SUCCESS)
	{
		return E4000_1_FAIL;
	}

	// Populate DC offset LUT
	writearray[0] = 0;
	writearray[1] = 127;
	status=I2CWriteArray(pTuner, 200,21,2,writearray);
	if(status != E4000_I2C_SUCCESS)
	{
		return E4000_1_FAIL;
	}

	// Sets mixer & IF stage 1 gain = 01 leaving IF stg 2+ at max gain.
	writearray[0]= 1;
	status=I2CWriteByte(pTuner, 200,41,writearray[0]);
	if(status != E4000_I2C_SUCCESS)
	{
		return E4000_1_FAIL;
	}

	// Instructs a DC offset calibration.
	status=I2CReadByte(pTuner, 201,42,read1);
	if(status != E4000_I2C_SUCCESS)
	{
		return E4000_1_FAIL;
	}

	IOFF=read1[0];
	status=I2CReadByte(pTuner, 201,43,read1);
	if(status != E4000_I2C_SUCCESS)
	{
		return E4000_1_FAIL;
	}

	QOFF=read1[0];
	status=I2CReadByte(pTuner, 201,44,read1);
	if(status != E4000_I2C_SUCCESS)
	{
		return E4000_1_FAIL;
	}

	RANGE1=read1[0];
	// Read DC offset values
	if(RANGE1>=32)
	{
		RANGE1 = RANGE1 -32;
	}
	if(RANGE1>=16)
    {
		RANGE1 = RANGE1 - 16;
	}
	IRANGE = RANGE1;
	QRANGE = (read1[0] - RANGE1) / 16;

	writearray[0] = (IRANGE * 64) + IOFF;
	status=I2CWriteByte(pTuner, 200,97,writearray[0]);
	if(status != E4000_I2C_SUCCESS)
	{
		return E4000_1_FAIL;
	}

	writearray[0] = (QRANGE * 64) + QOFF;
	status=I2CWriteByte(pTuner, 200,81,writearray[0]);
	if(status != E4000_I2C_SUCCESS)
	{
		return E4000_1_FAIL;
	}

	// Populate DC offset LUT
	writearray[0] = 1;
	status=I2CWriteByte(pTuner, 200,21,writearray[0]);
	if(status != E4000_I2C_SUCCESS)
	{
		return E4000_1_FAIL;
	}

	// Sets mixer & IF stage 1 gain = 11 leaving IF stg 2+ at max gain.
	writearray[0] = 1;
	status=I2CWriteByte(pTuner, 200,41,writearray[0]);
	if(status != E4000_I2C_SUCCESS)
	{
		return E4000_1_FAIL;
	}

	// Instructs a DC offset calibration.
	status=I2CReadByte(pTuner, 201,42,read1);
	if(status != E4000_I2C_SUCCESS)
	{
		return E4000_1_FAIL;
	}

	IOFF=read1[0];
	status=I2CReadByte(pTuner, 201,43,read1);
	if(status != E4000_I2C_SUCCESS)
	{
		return E4000_1_FAIL;
	}

	QOFF=read1[0];
	status=I2CReadByte(pTuner, 201,44,read1);
	if(status != E4000_I2C_SUCCESS)
	{
		return E4000_1_FAIL;
	}

	RANGE1 = read1[0];
	// Read DC offset values
	if(RANGE1>=32)
	{
		RANGE1 = RANGE1 -32;
	}
	if(RANGE1>=16)
	{
		RANGE1 = RANGE1 - 16;
	}
	IRANGE = RANGE1;
	QRANGE = (read1[0] - RANGE1) / 16;
	writearray[0] = (IRANGE * 64) + IOFF;
	status=I2CWriteByte(pTuner, 200,99,writearray[0]);
	if(status != E4000_I2C_SUCCESS)
	{
		return E4000_1_FAIL;
	}

	writearray[0] = (QRANGE * 64) + QOFF;
	status=I2CWriteByte(pTuner, 200,83,writearray[0]);
	if(status != E4000_I2C_SUCCESS)
	{
		return E4000_1_FAIL;
	}

	// Populate DC offset LUT
	writearray[0] = 126;
	status=I2CWriteByte(pTuner, 200,22,writearray[0]);
	if(status != E4000_I2C_SUCCESS)
	{
		return E4000_1_FAIL;
	}

	// Sets mixer & IF stage 1 gain = 11 leaving IF stg 2+ at max gain.
	writearray[0] = 1;
	status=I2CWriteByte(pTuner, 200,41,writearray[0]);
	if(status != E4000_I2C_SUCCESS)
	{
		return E4000_1_FAIL;
	}

	// Instructs a DC offset calibration.
	status=I2CReadByte(pTuner, 201,42,read1);
	if(status != E4000_I2C_SUCCESS)
	{
		return E4000_1_FAIL;
	}
	IOFF=read1[0];

	status=I2CReadByte(pTuner, 201,43,read1);
	if(status != E4000_I2C_SUCCESS)
	{
		return E4000_1_FAIL;
	}
	QOFF=read1[0];

	status=I2CReadByte(pTuner, 201,44,read1);
	if(status != E4000_I2C_SUCCESS)
	{
		return E4000_1_FAIL;
	}
	RANGE1=read1[0];

	// Read DC offset values
	if(RANGE1>=32)
	{
		RANGE1 = RANGE1 -32;
	}
	if(RANGE1>=16)
	{
		RANGE1 = RANGE1 - 16;
	}
	IRANGE = RANGE1;
	QRANGE = (read1[0] - RANGE1) / 16;

	writearray[0]=(IRANGE * 64) + IOFF;
	status=I2CWriteByte(pTuner, 200,98,writearray[0]);
	if(status != E4000_I2C_SUCCESS)
	{
		return E4000_1_FAIL;
	}

	writearray[0] = (QRANGE * 64) + QOFF;
	status=I2CWriteByte(pTuner, 200,82,writearray[0]);
	// Populate DC offset LUT
	if(status != E4000_I2C_SUCCESS)
	{
		return E4000_1_FAIL;
	}

	return E4000_1_SUCCESS;
}
/****************************************************************************\
*  Function: GainControlinit
*
*  Detailed Description:
*  Configures gain control mode. (Registers 0x1a)
*
\****************************************************************************/
int GainControlauto(RTL2832_NAMESPACE::tuner* pTuner)
{
	unsigned char writearray[5];
	int status;

	writearray[0] = 23;
	status=I2CWriteByte(pTuner, 200,26,writearray[0]);
	if(status != E4000_I2C_SUCCESS)
	{
		return E4000_1_FAIL;
	}

	return E4000_1_SUCCESS;
}
/****************************************************************************\
*  Main program
*
*
*
\****************************************************************************/
/*
int main()
{
     Gainmanual();
     PLL(Ref_clk, Freq);
     LNAfilter(Freq);
     IFfilter(bandwidth, Ref_clk);
     freqband(Freq);
     DCoffLUT();
     GainControlauto();
  return(0);
}
*/

// Elonics source code - RT2832_SW_optimisation_rev2.c



/****************************************************************************\
*  Function: E4000_sensitivity
*
*  Detailed Description:
*  The function configures the E4000 for sensitivity mode.
*
\****************************************************************************/

int E4000_sensitivity(RTL2832_NAMESPACE::tuner* pTuner, int Freq, int bandwidth)
{
	unsigned char writearray[2];
	int status;
	int IF_BW;

	writearray[1]=0x00;

	if(Freq<=700000)
	{
		writearray[0] = 0x07;
	}
	else
	{
		writearray[0] = 0x05;
	}
	status = I2CWriteArray(pTuner,200,36,1,writearray);
	if(status != E4000_I2C_SUCCESS)
	{
		return E4000_1_FAIL;
	}

	IF_BW = bandwidth / 2;
	if(IF_BW<=2500)
	{
		writearray[0]=0xfc;
		writearray[1]=0x17;
	}
    else if(IF_BW<=3000)
	{
		writearray[0]=0xfb;
		writearray[1]=0x0f;
	}
	else if(IF_BW<=3500)
	{
		writearray[0]=0xf9;
		writearray[1]=0x0b;
	}
	else if(IF_BW<=4000)
	{
		writearray[0]=0xf8;
		writearray[1]=0x07;
	}
	status = I2CWriteArray(pTuner,200,17,2,writearray);
	if(status != E4000_I2C_SUCCESS)
	{
		return E4000_1_FAIL;
	}

	return E4000_1_SUCCESS;
}
/****************************************************************************\
*  Function: E4000_linearity
*
*  Detailed Description:
*  The function configures the E4000 for linearity mode.
*
\****************************************************************************/
int E4000_linearity(RTL2832_NAMESPACE::tuner* pTuner, int Freq, int bandwidth)
{

	unsigned char writearray[2];
	int status;
	int IF_BW;

	writearray[1]=0x00;

	if(Freq<=700000)
	{
		writearray[0] = 0x03;
	}
	else
	{
		writearray[0] = 0x01;
	}
	status = I2CWriteArray(pTuner,200,36,1,writearray);
	if(status != E4000_I2C_SUCCESS)
	{
		return E4000_1_FAIL;
	}

	IF_BW = bandwidth / 2;
	if(IF_BW<=2500)
	{
		writearray[0]=0xfe;
		writearray[1]=0x19;
	}
	else if(IF_BW<=3000)
	{
		writearray[0]=0xfd;
		writearray[1]=0x11;
	}
	else if(IF_BW<=3500)
	{
		writearray[0]=0xfb;
		writearray[1]=0x0d;
	}
	else if(IF_BW<=4000)
	{
		writearray[0]=0xfa;
		writearray[1]=0x0a;
	}
	status = I2CWriteArray(pTuner,200,17,2,writearray);
	if(status != E4000_I2C_SUCCESS)
	{
		return E4000_1_FAIL;
	}

	return E4000_1_SUCCESS;
}
/****************************************************************************\
*  Function: E4000_nominal
*
*  Detailed Description:
*  The function configures the E4000 for nominal
*
\****************************************************************************/
int E4000_nominal(RTL2832_NAMESPACE::tuner* pTuner, int Freq, int bandwidth)
{
	unsigned char writearray[2];
	int status;
	int IF_BW;

	writearray[1]=0x00;

	if(Freq<=700000)
	{
		writearray[0] = 0x03;
	}
	else
	{
		writearray[0] = 0x01;
	}
	status = I2CWriteArray(pTuner,200,36,1,writearray);
	if(status != E4000_I2C_SUCCESS)
	{
		return E4000_1_FAIL;
	}

	IF_BW = bandwidth / 2;
	if(IF_BW<=2500)
	{
		writearray[0]=0xfc;
		writearray[1]=0x17;
	}
	else if(IF_BW<=3000)
	{
		writearray[0]=0xfb;
		writearray[1]=0x0f;
	}
	else if(IF_BW<=3500)
	{
		writearray[0]=0xf9;
		writearray[1]=0x0b;
	}
	else if(IF_BW<=4000)
	{
		writearray[0]=0xf8;
		writearray[1]=0x07;
	}
	status = I2CWriteArray(pTuner,200,17,2,writearray);
	if(status != E4000_I2C_SUCCESS)
	{
		return E4000_1_FAIL;
	}

	return E4000_1_SUCCESS;
}
