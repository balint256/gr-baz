#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/* (C) 2011-2012 by Harald Welte <laforge@gnumonks.org>
 *
 * All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * This is a port of the tuner code from Osmo-SDR
 */

#include <limits.h>

#ifndef _WIN32
#include <stdint.h>
#endif // _WIN32

#include <errno.h>
#include <string.h>

#include <stdarg.h>

#include "rtl2832-tuner_e4k.h"

using namespace std;

#define LOG_PREFIX	"[e4k] "

#define E4000_I2C_SUCCESS		1
#define E4000_I2C_FAIL			0

#define NO_USE				0

// Tuner gain mode
enum RTL2832_E4K_TUNER_GAIN_MODE
{
	RTL2832_E4000_TUNER_GAIN_SENSITIVE	= RTL2832_NAMESPACE::tuner::DEFAULT,
	RTL2832_E4000_TUNER_GAIN_NORMAL,
	RTL2832_E4000_TUNER_GAIN_LINEAR,
};

///////////////////////////////////////////////////////////////////////////////

#define E4K_I2C_ADDR		0xc8
#define E4K_CHECK_ADDR		0x02
#define E4K_CHECK_VAL		0x40

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#define DTUN	1
#define DVCO	2

/*! \brief different log levels */
#define LOGL_DEBUG	1	/*!< \brief debugging information */
#define LOGL_INFO	3
#define LOGL_NOTICE	5	/*!< \brief abnormal/unexpected condition */
#define LOGL_ERROR	7	/*!< \brief error condition, requires user action */
#define LOGL_FATAL	8	/*!< \brief fatal, program aborted */

void logp2(int subsys, unsigned int level, char *file,
	   int line, int cont, const char *format, ...)
{
	//
}

void LOGP(int ss, unsigned int level, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	logp2(ss, level, /*__FILE__*/0, /*__LINE__*/0, 0, fmt, args);
}

void LOGPC(int ss, unsigned int level, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	logp2(ss, level, /*__FILE__*/0, /*__LINE__*/0, 1, fmt, args);
}

///////////////////////////////////////////////////////////////////////////////

/*********************************************************************** 
 * Register Access */

/*! \brief Write a register of the tuner chip
 *  \param[in] e4k reference to the tuner
 *  \param[in] reg number of the register
 *  \param[in] val value to be written
 *  \returns 0 on success, negative in case of error
 */
int _e4k_reg_write(struct e4k_state *e4k, uint8_t reg, uint8_t val,
	const char* function /*= NULL*/, int line_number /*= -1*/, const char* line /*= NULL*/)
{
	uint8_t data[2];

	data[0] = reg;
	data[1] = val;

	int r = e4k->pTuner->i2c_write(E4K_I2C_ADDR, data, 2);
	if (r <= 0)
	{
	  DEBUG_TUNER_I2C(e4k->pTuner,r);
	  return -1;
	}

	return 0;
}

/*! \brief Read a register of the tuner chip
 *  \param[in] e4k reference to the tuner
 *  \param[in] reg number of the register
 *  \returns positive 8bit register contents on success, negative in case of error
 */
int _e4k_reg_read(struct e4k_state *e4k, uint8_t RegAddr,
	const char* function /*= NULL*/, int line_number /*= -1*/, const char* line /*= NULL*/)
{
	uint8_t data = RegAddr;
	int r;

	r = e4k->pTuner->i2c_write(E4K_I2C_ADDR, &data, 1);
	if (r <= 0)
	{
	  DEBUG_TUNER_I2C(e4k->pTuner,r);
	  return -1;
	}

	r = e4k->pTuner->i2c_read(E4K_I2C_ADDR, &data, 1);
	if (r <= 0)
	{
	  DEBUG_TUNER_I2C(e4k->pTuner,r);
	  return -1;
	}

	return data;
}

#define e4k_reg_read(t,n)		_e4k_reg_read(t,n,CURRENT_FUNCTION,__LINE__,"e4k_reg_read("#t", "#n")")
//#define e4k_reg_read(t,n)		_e4k_reg_read(t,n)

#define e4k_reg_write(t,n,r)		_e4k_reg_write(t,n,r,CURRENT_FUNCTION,__LINE__,"e4k_reg_write("#t", "#n", "#r")")
//#define e4k_reg_write(t,n,r)		_e4k_reg_write(t,n,r)

///////////////////////////////////////////////////////////////////////////////

static int
_I2CReadByte(
	RTL2832_NAMESPACE::tuner* pTuner,
	unsigned char NoUse,
	unsigned char RegAddr,
	unsigned char *pReadingByte,
	const char* function = NULL, int line_number = -1, const char* line = NULL
	)
{
	int r = _e4k_reg_read(&static_cast<RTL2832_NAMESPACE::TUNERS_NAMESPACE::e4k*>(pTuner)->m_stateE4K, RegAddr, function, line_number, line);
	if (r < 0)
		return E4000_I2C_FAIL;
	(*pReadingByte) = (unsigned char)r;
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
	int r = _e4k_reg_write(&static_cast<RTL2832_NAMESPACE::TUNERS_NAMESPACE::e4k*>(pTuner)->m_stateE4K, RegAddr, WritingByte, function, line_number, line);
	if (r < 0)
		return E4000_I2C_FAIL;
	return E4000_I2C_SUCCESS;
}

#define I2CReadByte(t,n,r,b)		_I2CReadByte(t,n,r,b,CURRENT_FUNCTION,__LINE__,"I2CReadByte("#t", "#n", "#r", "#b")")
//#define I2CReadByte(t,n,r,b)		_I2CReadByte(t,n,r,b)

#define I2CWriteByte(t,n,r,w)		_I2CWriteByte(t,n,r,w,CURRENT_FUNCTION,__LINE__,"I2CReadByte("#t", "#n", "#r", "#w")")
//#define I2CWriteByte(t,n,r,w)		_I2CWriteByte(t,n,r,w)

///////////////////////////////////////////////////////////////////////////////

namespace RTL2832_NAMESPACE { namespace TUNERS_NAMESPACE {

static const int _mapGainsE4K[] = {	// nim_rtl2832_e4000.c
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

int e4k::TUNER_PROBE_FN_NAME(demod* d)
{
	I2C_REPEATER_SCOPE(d);

	uint8_t reg = 0;
	//CHECK_LIBUSB_RESULT_RETURN_EX(d,d->i2c_read_reg(E4K_I2C_ADDR, E4K_CHECK_ADDR, reg));
	int r = d->i2c_read_reg(E4K_I2C_ADDR, E4K_CHECK_ADDR, reg);
	if (r <= 0) return r;
	return ((reg == E4K_CHECK_VAL) ? SUCCESS : FAILURE);
}

e4k::e4k(demod* p)
	: tuner_skeleton(p)
{
	for (int i = 0; i < sizeof(_mapGainsE4K)/sizeof(_mapGainsE4K[0]); i += 2)
		m_gain_values.push_back((double)_mapGainsE4K[i] / 10.0);

	values_to_range(m_gain_values, m_gain_range);

	//m_frequency_range	// 64-1700 MHz, but can do more so leave it undefined

	//m_bandwidth_values	// 2150-5500 kHz
	//values_to_range(m_bandwidth_values, m_bandwidth_range);

	m_bandwidth = 8000000;	// Default

	m_gain_modes.insert(make_pair(RTL2832_E4000_TUNER_GAIN_NORMAL, "nominal"));
	m_gain_modes.insert(make_pair(RTL2832_E4000_TUNER_GAIN_LINEAR, "linear"));
	m_gain_modes.insert(make_pair(RTL2832_E4000_TUNER_GAIN_SENSITIVE, "sensitive"));

	memset(&m_stateE4K, 0x00, sizeof(m_stateE4K));
	m_stateE4K.vco.fosc = p->crystal_frequency();
	m_stateE4K.pTuner = this;
	m_stateE4K.i2c_addr = E4K_I2C_ADDR;
}

int e4k::initialise(PPARAMS params /*= NULL*/)
{
	if (tuner_skeleton::initialise(params) != SUCCESS)
		return FAILURE;

	THIS_I2C_REPEATER_SCOPE();

	bool enable_dc_offset_loop = false;	// FIXME: Parameterise this (e.g. flag field in tuner::PARAMS, or another explicit property?)
	bool set_manual_gain = true;

	if (e4k_init(&m_stateE4K, enable_dc_offset_loop, set_manual_gain) != 0)
		return FAILURE;

	if (set_bandwidth(bandwidth()) != SUCCESS)
		return FAILURE;

	if (m_params.message_output && m_params.verbose)
		m_params.message_output->on_log_message_ex(rtl2832::log_sink::LOG_LEVEL_VERBOSE, LOG_PREFIX"Initialised (default bandwidth: %i Hz)\n", (uint32_t)bandwidth());

	return SUCCESS;
}

int e4k::set_frequency(double freq)
{
	if ((freq <= 0) || (in_valid_range(m_frequency_range, freq) == false))
		return FAILURE;

	THIS_I2C_REPEATER_SCOPE();

	//bool update_gain_control = false;	// FIXME: This should be based on 'set_manual_gain' in 'initialise'
	//bool enable_dc_offset_lut = true;

	int r = e4k_tune_freq(&m_stateE4K, (unsigned long)freq);
	if (r < 0)
		return FAILURE;

	m_freq = /*(int)((freq + 500.0) / 1000.0) * 1000*/r;

	// FIXME: Technically should call 'update_gain_mode' if 'm_auto_gain_mode' is enabled, but I2C will become overloaded

	return SUCCESS;
}

int e4k::set_bandwidth(double bw)
{
	if ((bw <= 0) || (in_valid_range(m_bandwidth_range, bw) == false))
		return FAILURE;

	THIS_I2C_REPEATER_SCOPE();

	if (in_valid_range(m_bandwidth_range, bw) == false)
		return FAILURE;

	int r = e4k_if_filter_bw_set(&m_stateE4K, E4K_IF_FILTER_CHAN, bw);
	if (r < 0)
		return FAILURE;
 
	m_bandwidth = /*(int)((bw + 500.0) / 1000.0) * 1000*/r;

	return SUCCESS;
}

int e4k::set_gain(double gain)
{
	const int iCount = (sizeof(_mapGainsE4K)/sizeof(_mapGainsE4K[0])) / 2;

	int iGain = (int)(gain * 10.0);
	int i = get_map_index(iGain, _mapGainsE4K, iCount);

	if ((i == -1) || (i == iCount))
		return FAILURE;

	//if (i == -1)	// Below first -> select first
	//  i = 0;
	//else if (i == iCount)	// Above last -> select last
	//  i = iCount - 1;

	unsigned char u8Write = _mapGainsE4K[i + 1];

	THIS_I2C_REPEATER_SCOPE();

	unsigned char u8Read = 0;
	int res;
	//if (I2CReadByte(this, 0, RTL2832_E4000_LNA_GAIN_ADDR, &u8Read) != E4000_I2C_SUCCESS)
	if ((res = e4k_reg_read(&m_stateE4K, RTL2832_E4000_LNA_GAIN_ADDR)) < 0)
		return FAILURE;
	u8Read = (unsigned char)res;

	u8Write |= (u8Read & ~RTL2832_E4000_LNA_GAIN_MASK);
  
	//if (I2CWriteByte(this, 0, RTL2832_E4000_LNA_GAIN_ADDR, u8Write) != E4000_I2C_SUCCESS)
	if (e4k_reg_write(&m_stateE4K, RTL2832_E4000_LNA_GAIN_ADDR, u8Write) < 0)
		return FAILURE;

	m_gain = (double)_mapGainsE4K[i] / 10.0;

	if (m_auto_gain_mode)
	{
		if (update_gain_mode() != SUCCESS)
			return FAILURE;
	}

	return SUCCESS;
}

int e4k::update_gain_mode()
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

int e4k::set_auto_gain_mode(bool on /*= true*/)
{
	if (on)
	{
		if (update_gain_mode() != SUCCESS)
			return FAILURE;
	}

	return tuner_skeleton::set_auto_gain_mode(on);
}

int e4k::set_gain_mode(int mode)
{
	int freq= (int)(frequency() / 1000.0);
	int bw	= (int)(bandwidth() / 1000.0);

	THIS_I2C_REPEATER_SCOPE();

	switch (mode)
	{
		default:
		case RTL2832_E4000_TUNER_GAIN_SENSITIVE:
			//if (E4000_sensitivity(this, freq, bw) != E4000_I2C_SUCCESS)
			//	return FAILURE;
			break;
		case RTL2832_E4000_TUNER_GAIN_LINEAR:
			//if (E4000_linearity(this, freq, bw) != E4000_I2C_SUCCESS)
			//	return FAILURE;
		  break;
		case RTL2832_E4000_TUNER_GAIN_NORMAL:
			//if (E4000_nominal(this, freq, bw) != E4000_I2C_SUCCESS)
			//	return FAILURE;
		  break;
	}

	m_gain_mode = mode;

	return SUCCESS;
}

bool e4k::calc_appropriate_gain_mode(int& mode)/* const*/
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

/* If this is defined, the limits are somewhat relaxed compared to what the
 * vendor claims is possible */
#define OUT_OF_SPEC

#define MHZ(x)	((x)*1000*1000)
#define KHZ(x)	((x)*1000)

uint32_t unsigned_delta(uint32_t a, uint32_t b)
{
	if (a > b)
		return a - b;
	else
		return b - a;
}

/* look-up table bit-width -> mask */
static const uint8_t width2mask[] = {
	0, 1, 3, 7, 0xf, 0x1f, 0x3f, 0x7f, 0xff
};

/*! \brief Set or clear some (masked) bits inside a register
 *  \param[in] e4k reference to the tuner
 *  \param[in] reg number of the register
 *  \param[in] mask bit-mask of the value
 *  \param[in] val data value to be written to register
 *  \returns 0 on success, negative in case of error
 */
static int e4k_reg_set_mask(struct e4k_state *e4k, uint8_t reg,
		     uint8_t mask, uint8_t val)
{
	uint8_t tmp = e4k_reg_read(e4k, reg);

	if ((tmp & mask) == val)
		return 0;

	return e4k_reg_write(e4k, reg, (tmp & ~mask) | (val & mask));
}

/*! \brief Write a given field inside a register
 *  \param[in] e4k reference to the tuner
 *  \param[in] field structure describing the field
 *  \param[in] val value to be written
 *  \returns 0 on success, negative in case of error
 */
static int e4k_field_write(struct e4k_state *e4k, const struct reg_field *field, uint8_t val)
{
	int rc;
	uint8_t mask;

	rc = e4k_reg_read(e4k, field->reg);
	if (rc < 0)
		return rc;

	mask = width2mask[field->width] << field->shift;

	return e4k_reg_set_mask(e4k, field->reg, mask, val << field->shift);
}

/*! \brief Read a given field inside a register
 *  \param[in] e4k reference to the tuner
 *  \param[in] field structure describing the field
 *  \returns positive value of the field, negative in case of error
 */
static int e4k_field_read(struct e4k_state *e4k, const struct reg_field *field)
{
	int rc;

	rc = e4k_reg_read(e4k, field->reg);
	if (rc < 0)
		return rc;

	rc = (rc >> field->shift) & width2mask[field->width];

	return rc;
}


/*********************************************************************** 
 * Filter Control */

static const uint32_t rf_filt_center_uhf[] = {
	MHZ(360), MHZ(380), MHZ(405), MHZ(425),
	MHZ(450), MHZ(475), MHZ(505), MHZ(540),
	MHZ(575), MHZ(615), MHZ(670), MHZ(720),
	MHZ(760), MHZ(840), MHZ(890), MHZ(970)
};

static const uint32_t rf_filt_center_l[] = {
	MHZ(1300), MHZ(1320), MHZ(1360), MHZ(1410),
	MHZ(1445), MHZ(1460), MHZ(1490), MHZ(1530),
	MHZ(1560), MHZ(1590), MHZ(1640), MHZ(1660),
	MHZ(1680), MHZ(1700), MHZ(1720), MHZ(1750)
};

static int closest_arr_idx(const uint32_t *arr, unsigned int arr_size, uint32_t freq)
{
	unsigned int i, bi;
	uint32_t best_delta = 0xffffffff;

	/* iterate over the array containing a list of the center
	 * frequencies, selecting the closest one */
	for (i = 0; i < arr_size; i++) {
		uint32_t delta = unsigned_delta(freq, arr[i]);
		if (delta < best_delta) {
			best_delta = delta;
			bi = i;
		}
	}

	return bi;
}

/* return 4-bit index as to which RF filter to select */
static int choose_rf_filter(enum e4k_band band, uint32_t freq)
{
	int rc;

	switch (band) {
	case E4K_BAND_VHF2:
		if (freq < MHZ(268))
			rc = 0;
		else
			rc = 8;
		break;
	case E4K_BAND_VHF3:
		if (freq < MHZ(509))
			rc = 0;
		else
			rc = 8;
		break;
	case E4K_BAND_UHF:
		rc = closest_arr_idx(rf_filt_center_uhf,
				     ARRAY_SIZE(rf_filt_center_uhf),
				     freq);
		break;
	case E4K_BAND_L:
		rc = closest_arr_idx(rf_filt_center_l,
				     ARRAY_SIZE(rf_filt_center_l),
				     freq);
		break;
	default:
		rc = -EINVAL;
		break;
	}

	return rc;
}

/* \brief Automatically select apropriate RF filter based on e4k state */
int e4k_rf_filter_set(struct e4k_state *e4k)
{
	int rc;

	rc = choose_rf_filter(e4k->band, e4k->vco.flo);
	if (rc < 0)
		return rc;

	return e4k_reg_set_mask(e4k, E4K_REG_FILT1, 0xF, rc);
}

/* Mixer Filter */
static const uint32_t mix_filter_bw[] = {
	KHZ(27000), KHZ(27000), KHZ(27000), KHZ(27000),
	KHZ(27000), KHZ(27000), KHZ(27000), KHZ(27000),
	KHZ(4600), KHZ(4200), KHZ(3800), KHZ(3400),
	KHZ(3300), KHZ(2700), KHZ(2300), KHZ(1900)
};

/* IF RC Filter */
static const uint32_t ifrc_filter_bw[] = {
	KHZ(21400), KHZ(21000), KHZ(17600), KHZ(14700),
	KHZ(12400), KHZ(10600), KHZ(9000), KHZ(7700),
	KHZ(6400), KHZ(5300), KHZ(4400), KHZ(3400),
	KHZ(2600), KHZ(1800), KHZ(1200), KHZ(1000)
};

/* IF Channel Filter */
static const uint32_t ifch_filter_bw[] = {
	KHZ(5500), KHZ(5300), KHZ(5000), KHZ(4800),
	KHZ(4600), KHZ(4400), KHZ(4300), KHZ(4100),
	KHZ(3900), KHZ(3800), KHZ(3700), KHZ(3600),
	KHZ(3400), KHZ(3300), KHZ(3200), KHZ(3100),
	KHZ(3000), KHZ(2950), KHZ(2900), KHZ(2800),
	KHZ(2750), KHZ(2700), KHZ(2600), KHZ(2550),
	KHZ(2500), KHZ(2450), KHZ(2400), KHZ(2300),
	KHZ(2280), KHZ(2240), KHZ(2200), KHZ(2150)
};

static const uint32_t *if_filter_bw[] = {
	/*[E4K_IF_FILTER_MIX] = */mix_filter_bw,
	/*[E4K_IF_FILTER_CHAN] = */ifch_filter_bw,
	/*[E4K_IF_FILTER_RC] = */ifrc_filter_bw,
};

static const uint32_t if_filter_bw_len[] = {
	/*[E4K_IF_FILTER_MIX] = */ARRAY_SIZE(mix_filter_bw),
	/*[E4K_IF_FILTER_CHAN] = */ARRAY_SIZE(ifch_filter_bw),
	/*[E4K_IF_FILTER_RC] = */ARRAY_SIZE(ifrc_filter_bw),
};

static const struct reg_field if_filter_fields[] = {
	/*[E4K_IF_FILTER_MIX] = */{
		/*.reg = */E4K_REG_FILT2, /*.shift = */4, /*.width = */4,
	},
	/*[E4K_IF_FILTER_CHAN] = */{
		/*.reg = */E4K_REG_FILT3, /*.shift = */0, /*.width = */5,
	},
	/*[E4K_IF_FILTER_RC] = */{
		/*.reg = */E4K_REG_FILT2, /*.shift = */0, /*.width = */4,
	}
};

static int find_if_bw(enum e4k_if_filter filter, uint32_t bw)
{
	if (filter >= ARRAY_SIZE(if_filter_bw))
		return -EINVAL;

	return closest_arr_idx(if_filter_bw[filter],
			       if_filter_bw_len[filter], bw);
}

/*! \brief Set the filter band-width of any of the IF filters
 *  \param[in] e4k reference to the tuner chip
 *  \param[in] filter filter to be configured
 *  \param[in] bandwidth bandwidth to be configured
 *  \returns positive actual filter band-width, negative in case of error
 */
int e4k_if_filter_bw_set(struct e4k_state *e4k, enum e4k_if_filter filter,
		         uint32_t bandwidth)
{
	int bw_idx;
	uint8_t mask;
	const struct reg_field *field;

	if (filter >= ARRAY_SIZE(if_filter_bw))
		return -EINVAL;

	bw_idx = find_if_bw(filter, bandwidth);

	field = &if_filter_fields[filter];

	return e4k_field_write(e4k, field, bw_idx);
}

/*! \brief Enables / Disables the channel filter
 *  \param[in] e4k reference to the tuner chip
 *  \param[in] on 1=filter enabled, 0=filter disabled
 *  \returns 0 success, negative errors
 */
int e4k_if_filter_chan_enable(struct e4k_state *e4k, int on)
{
	return e4k_reg_set_mask(e4k, E4K_REG_FILT3, E4K_FILT3_DISABLE,
	                        on ? 0 : E4K_FILT3_DISABLE);
}

int e4k_if_filter_bw_get(struct e4k_state *e4k, enum e4k_if_filter filter)
{
	const uint32_t *arr;
	int rc;
	const struct reg_field *field;

	if (filter >= ARRAY_SIZE(if_filter_bw))
		return -EINVAL;

	field = &if_filter_fields[filter];

	rc = e4k_field_read(e4k, field);
	if (rc < 0)
		return rc;

	arr = if_filter_bw[filter];

	return arr[rc];
}


/*********************************************************************** 
 * Frequency Control */

#define E4K_FVCO_MIN_KHZ	2600000	/* 2.6 GHz */
#define E4K_FVCO_MAX_KHZ	3900000	/* 3.9 GHz */
#define E4K_PLL_Y		65536

#ifdef OUT_OF_SPEC
#define E4K_FLO_MIN_MHZ		50
#define E4K_FLO_MAX_MHZ		1900
#else
#define E4K_FLO_MIN_MHZ		64
#define E4K_FLO_MAX_MHZ		1700
#endif

/* \brief table of R dividers in case 3phase mixing is enabled,
 * the values have to be halved if it's 2phase */
static const uint8_t vco_r_table_3ph[] = {
	4, 8, 12, 16, 24, 32, 40, 48
};

static int is_fvco_valid(uint32_t fvco_z)
{
	/* check if the resulting fosc is valid */
	if (fvco_z/1000 < E4K_FVCO_MIN_KHZ ||
	    fvco_z/1000 > E4K_FVCO_MAX_KHZ) {
		LOGP(DTUN, LOGL_ERROR, "Fvco %u invalid\n", fvco_z);
		return 0;
	}

	return 1;
}

static int is_fosc_valid(uint32_t fosc)
{
	if (fosc < MHZ(16) || fosc > MHZ(30)) {
		LOGP(DTUN, LOGL_ERROR, "Fosc %u invalid\n", fosc);
		return 0;
	}

	return 1;
}

static int is_flo_valid(uint32_t flo)
{
	if (flo < MHZ(E4K_FLO_MIN_MHZ) || flo > MHZ(E4K_FLO_MAX_MHZ)) {
		LOGP(DTUN, LOGL_ERROR, "Flo %u invalid\n", flo);
		return 0;
	}

	return 1;
}

static int is_z_valid(uint32_t z)
{
	if (z > 255) {
		LOGP(DTUN, LOGL_ERROR, "Z %u invalid\n", z);
		return 0;
	}

	return 1;
}

/*! \brief Determine if 3-phase mixing shall be used or not */
static int use_3ph_mixing(uint32_t flo)
{
	/* this is a magic number somewhre between VHF and UHF */
	if (flo < MHZ(300))
		return 1;

	return 0;
}

/* \brief compute Fvco based on Fosc, Z and X
 * \returns positive value (Fvco in Hz), 0 in case of error */
static unsigned int compute_fvco(uint32_t f_osc, uint8_t z, uint16_t x)
{
	uint64_t fvco_z, fvco_x, fvco;

	/* We use the following transformation in order to
	 * handle the fractional part with integer arithmetic:
	 *  Fvco = Fosc * (Z + X/Y) <=> Fvco = Fosc * Z + (Fosc * X)/Y
	 * This avoids X/Y = 0.  However, then we would overflow a 32bit
	 * integer, as we cannot hold e.g. 26 MHz * 65536 either.
	 */
	fvco_z = (uint64_t)f_osc * z;

	if (!is_fvco_valid(fvco_z))
		return 0;

	fvco_x = ((uint64_t)f_osc * x) / E4K_PLL_Y;

	fvco = fvco_z + fvco_x;

	/* this shouldn't happen, but better to check explicitly for integer
	 * overflows before converting uint64_t to "int" */
	if (fvco > UINT_MAX) {
		LOGP(DTUN, LOGL_ERROR, "Fvco %llu > INT_MAX\n", fvco);
		return 0;
	}

	return fvco;
}

static int compute_flo(uint32_t f_osc, uint8_t z, uint16_t x, uint8_t r)
{
	unsigned int fvco = compute_fvco(f_osc, z, x);
	if (fvco == 0)
		return -EINVAL;

	return fvco / r;
}

static int e4k_band_set(struct e4k_state *e4k, enum e4k_band band)
{
	int rc;

	switch (band) {
	case E4K_BAND_VHF2:
	case E4K_BAND_VHF3:
	case E4K_BAND_UHF:
		e4k_reg_write(e4k, E4K_REG_BIAS, 3);
		break;
	case E4K_BAND_L:
		e4k_reg_write(e4k, E4K_REG_BIAS, 0);
		break;
	}

	rc = e4k_reg_set_mask(e4k, E4K_REG_SYNTH1, 0x06, band << 1);
	if (rc >= 0)
		e4k->band = band;

	return rc;
}

#if 0
static int compute_lowest_r_idx(uint32_t flo, uint32_t fosc)
{
	int three_phase_mixing = use_3ph_mixing(intended_flo);
	uint32_t r_ideal;

	/* determine what would be the idael R divider, taking into account
	 * fractional remainder of the division */
	r_ideal = flo / fosc;
	if (flo % fosc)
		r_ideal++;

	/* find the next best (bigger) possible R value */
	for (i = 0; i < ARRAY_SIZE(vco_r_table_3ph); i++) {
		uint32_t r = vco_r_table_3ph[i];

		if (!three_phase_mixing)
			r = r / 2;

		if (r < r_ideal)
			continue;

		return i;
	}

	/* this shouldn't happen!!! */
	return 0;
}
#endif

/*! \brief Compute PLL parameters for givent target frequency
 *  \param[out] oscp Oscillator parameters, if computation successful
 *  \param[in] fosc Clock input frequency applied to the chip (Hz)
 *  \param[in] intended_flo target tuning frequency (Hz)
 *  \returns actual PLL frequency, as close as possible to intended_flo,
 *	     negative in case of error
 */
int e4k_compute_pll_params(struct e4k_pll_params *oscp, uint32_t fosc, uint32_t intended_flo)
{
	int i;
	int three_phase_mixing = use_3ph_mixing(intended_flo);

	if (!is_fosc_valid(fosc))
		return -EINVAL;

	if (!is_flo_valid(intended_flo))
		return -EINVAL;

	for (i = 0; i < ARRAY_SIZE(vco_r_table_3ph); i++) {
		uint8_t r = vco_r_table_3ph[i];
		uint64_t intended_fvco, z, remainder;
		uint32_t x;
		int flo;

		if (!three_phase_mixing)
			r = r / 2;

		LOGP(DTUN, LOGL_DEBUG, "Fint=%u, R=%u\n", intended_flo, r);

		/* flo(max) = 1700MHz, R(max) = 48, we need 64bit! */
		intended_fvco = (uint64_t)intended_flo * r;
		/* check if fvco is in range, if not continue */
		if (intended_fvco > UINT_MAX) {
			LOGP(DTUN, LOGL_DEBUG, "intended_fvco > UINT_MAX\n");
			continue;
		}

		if (!is_fvco_valid(intended_fvco))
			continue;

		/* compute integral component of multiplier */
		z = intended_fvco / fosc;
		if (!is_z_valid(z))
			continue;

		/* compute fractional part.  this will not overflow,
		 * as fosc(max) = 30MHz and z(max) = 255 */
		remainder = intended_fvco - (fosc * z);
		/* remainder(max) = 30MHz, E4K_PLL_Y = 65536 -> 64bit! */
		x = (remainder * E4K_PLL_Y) / fosc;
		/* x(max) as result of this computation is 65536 */

		flo = compute_flo(fosc, z, x, r);
		if (flo < 0)
			continue;

		oscp->fosc = fosc;
		oscp->flo = flo;
		oscp->intended_flo = intended_flo;
		oscp->r = r;
		oscp->r_idx = i;
		oscp->threephase = three_phase_mixing;
		oscp->x = x;
		oscp->z = z;

		return flo;
	}

	LOGP(DTUN, LOGL_ERROR, "No valid set of PLL params found for %u\n",
		intended_flo);
	return -EINVAL;
}

int e4k_tune_params(struct e4k_state *e4k, struct e4k_pll_params *p)
{
	uint8_t val;

	/* program R index + 3phase/2phase */
	val = (p->r_idx & 0x7) | ((p->threephase & 0x1) << 3);
	e4k_reg_write(e4k, E4K_REG_SYNTH7, val);
	/* program Z */
	e4k_reg_write(e4k, E4K_REG_SYNTH3, p->z);
	/* program X */
	e4k_reg_write(e4k, E4K_REG_SYNTH4, p->x & 0xff);
	e4k_reg_write(e4k, E4K_REG_SYNTH5, p->x >> 8);

	/* we're in auto calibration mode, so there's no need to trigger it */

	memcpy(&e4k->vco, p, sizeof(e4k->vco));

	/* set the band */
	if (e4k->vco.flo < MHZ(139))
		e4k_band_set(e4k, E4K_BAND_VHF2);
	else if (e4k->vco.flo < MHZ(350))
		e4k_band_set(e4k, E4K_BAND_VHF3);
	else if (e4k->vco.flo < MHZ(1135))
		e4k_band_set(e4k, E4K_BAND_UHF);
	else
		e4k_band_set(e4k, E4K_BAND_L);

	/* select and set proper RF filter */
	e4k_rf_filter_set(e4k);

	return e4k->vco.flo;
}

/*! \brief High-level tuning API, just specify frquency
 *
 *  This function will compute matching PLL parameters, program them into the
 *  hardware and set the band as well as RF filter.
 *
 *  \param[in] e4k reference to tuner
 *  \param[in] freq frequency in Hz
 *  \returns actual tuned frequency, negative in case of error
 */
int e4k_tune_freq(struct e4k_state *e4k, uint32_t freq)
{
	int rc;
	struct e4k_pll_params p;

	/* determine PLL parameters */
	rc = e4k_compute_pll_params(&p, e4k->vco.fosc, freq);
	if (rc < 0)
		return rc;

	/* actually tune to those parameters */
	return e4k_tune_params(e4k, &p);
}

/*********************************************************************** 
 * Gain Control */

static const int8_t if_stage1_gain[] = {
	-3, 6
};

static const int8_t if_stage23_gain[] = {
	0, 3, 6, 9
};

static const int8_t if_stage4_gain[] = {
	0, 1, 2, 2
};

static const int8_t if_stage56_gain[] = {
	3, 6, 9, 12, 15, 15, 15, 15
};

static const int8_t *if_stage_gain[] = {
	NULL,
	/*[1] = */if_stage1_gain,
	/*[2] = */if_stage23_gain,
	/*[3] = */if_stage23_gain,
	/*[4] = */if_stage4_gain,
	/*[5] = */if_stage56_gain,
	/*[6] = */if_stage56_gain
};

static const uint8_t if_stage_gain_len[] = {
	/*0] = */0,
	/*[1] = */ARRAY_SIZE(if_stage1_gain),
	/*[2] = */ARRAY_SIZE(if_stage23_gain),
	/*[3] = */ARRAY_SIZE(if_stage23_gain),
	/*[4] = */ARRAY_SIZE(if_stage4_gain),
	/*[5] = */ARRAY_SIZE(if_stage56_gain),
	/*[6] = */ARRAY_SIZE(if_stage56_gain)
};

static const struct reg_field if_stage_gain_regs[] = {
	{ },
	/*[1] = */{ /*.reg = */E4K_REG_GAIN3, /*.shift = */0, /*.width = */1 },
	/*[2] = */{ /*.reg = */E4K_REG_GAIN3, /*.shift = */1, /*.width = */2 },
	/*[3] = */{ /*.reg = */E4K_REG_GAIN3, /*.shift = */3, /*.width = */2 },
	/*[4] = */{ /*.reg = */E4K_REG_GAIN3, /*.shift = */5, /*.width = */2 },
	/*[5] = */{ /*.reg = */E4K_REG_GAIN4, /*.shift = */0, /*.width = */3 },
	/*[6] = */{ /*.reg = */E4K_REG_GAIN4, /*.shift = */3, /*.width = */3 }
};

static int find_stage_gain(uint8_t stage, int8_t val)
{
	const int8_t *arr;
	int i;

	if (stage >= ARRAY_SIZE(if_stage_gain))
		return -EINVAL;

	arr = if_stage_gain[stage];

	for (i = 0; i < if_stage_gain_len[stage]; i++) {
		if (arr[i] == val)
			return i;
	}
	return -EINVAL;
}

/*! \brief Set the gain of one of the IF gain stages
 *  \param[e4k] handle to the tuner chip
 *  \param [stage] numbere of the stage (1..6)
 *  \param [value] gain value in dBm
 *  \returns 0 on success, negative in case of error
 */
int e4k_if_gain_set(struct e4k_state *e4k, uint8_t stage, int8_t value)
{
	int rc;
	uint8_t mask;
	const struct reg_field *field;

	rc = find_stage_gain(stage, value);
	if (rc < 0)
		return rc;

	/* compute the bit-mask for the given gain field */
	field = &if_stage_gain_regs[stage];
	mask = width2mask[field->width] << field->shift;

	return e4k_reg_set_mask(e4k, field->reg, mask, rc << field->shift);
}

int e4k_mixer_gain_set(struct e4k_state *e4k, int8_t value)
{
	uint8_t bit;

	switch (value) {
	case 4:
		bit = 0;
		break;
	case 12:
		bit = 1;
		break;
	default:
		return -EINVAL;
	}

	return e4k_reg_set_mask(e4k, E4K_REG_GAIN2, 1, bit);
}

int e4k_commonmode_set(struct e4k_state *e4k, int8_t value)
{
	if(value < 0)
		return -EINVAL;
	else if(value > 7)
		return -EINVAL;

	return e4k_reg_set_mask(e4k, E4K_REG_DC7, 7, value);
}

/*********************************************************************** 
 * DC Offset */

int e4k_manual_dc_offset(struct e4k_state *e4k, int8_t iofs, int8_t irange, int8_t qofs, int8_t qrange)
{
	int res;

	if((iofs < 0x00) || (iofs > 0x3f))
		return -EINVAL;
	if((irange < 0x00) || (irange > 0x03))
		return -EINVAL;
	if((qofs < 0x00) || (qofs > 0x3f))
		return -EINVAL;
	if((qrange < 0x00) || (qrange > 0x03))
		return -EINVAL;

	res = e4k_reg_set_mask(e4k, E4K_REG_DC2, 0x3f, iofs);
	if(res < 0)
		return res;

	res = e4k_reg_set_mask(e4k, E4K_REG_DC3, 0x3f, qofs);
	if(res < 0)
		return res;

	res = e4k_reg_set_mask(e4k, E4K_REG_DC4, 0x33, (qrange << 4) | irange);
	return res;
}

/*! \brief Perform a DC offset calibration right now
 *  \param[e4k] handle to the tuner chip
 */
int e4k_dc_offset_calibrate(struct e4k_state *e4k)
{
	/* make sure the DC range detector is enabled */
	e4k_reg_set_mask(e4k, E4K_REG_DC5, E4K_DC5_RANGE_DET_EN, E4K_DC5_RANGE_DET_EN);

	return e4k_reg_write(e4k, E4K_REG_DC1, 0x01);
}


static const int8_t if_gains_max[] = {
	0, 6, 9, 9, 2, 15, 15
};

struct gain_comb {
	int8_t mixer_gain;
	int8_t if1_gain;
	uint8_t reg;
};

static const struct gain_comb dc_gain_comb[] = {
	{ 4, -3, 0x50 },
	{ 4, 6, 0x51 },
	{ 12, -3, 0x52 },
	{ 12, 6, 0x53 },
};

#define TO_LUT(offset, range)	(offset | (range << 6))

int e4k_dc_offset_gen_table(struct e4k_state *e4k)
{
	int i;

	/* FIXME: read ont current gain values and write them back
	 * before returning to the caller */

	/* disable auto mixer gain */
	e4k_reg_set_mask(e4k, E4K_REG_AGC7, E4K_AGC7_MIX_GAIN_AUTO, 0);

	/* set LNA/IF gain to full manual */
	e4k_reg_set_mask(e4k, E4K_REG_AGC1, E4K_AGC1_MOD_MASK,
			 E4K_AGC_MOD_SERIAL);

	/* set all 'other' gains to maximum */
	for (i = 2; i <= 6; i++)
		e4k_if_gain_set(e4k, i, if_gains_max[i]);

	/* iterate over all mixer + if_stage_1 gain combinations */
	for (i = 0; i < ARRAY_SIZE(dc_gain_comb); i++) {
		uint8_t offs_i, offs_q, range, range_i, range_q;

		/* set the combination of mixer / if1 gain */
		e4k_mixer_gain_set(e4k, dc_gain_comb[i].mixer_gain);
		e4k_if_gain_set(e4k, 1, dc_gain_comb[i].if1_gain);

		/* perform actual calibration */
		e4k_dc_offset_calibrate(e4k);

		/* extract I/Q offset and range values */
		offs_i = e4k_reg_read(e4k, E4K_REG_DC2) & 0x3F;
		offs_q = e4k_reg_read(e4k, E4K_REG_DC3) & 0x3F;
		range  = e4k_reg_read(e4k, E4K_REG_DC4);
		range_i = range & 0x3;
		range_q = (range >> 4) & 0x3;

		LOGP(DTUN, LOGL_DEBUG, "Table %u I=%u/%u, Q=%u/%u\n",
			i, range_i, offs_i, range_q, offs_q);

		/* write into the table */
		e4k_reg_write(e4k, dc_gain_comb[i].reg,
			      TO_LUT(offs_q, range_q));
		e4k_reg_write(e4k, dc_gain_comb[i].reg+0x10,
			      TO_LUT(offs_i, range_i));
	}

	return 0;
}


/*********************************************************************** 
 * Initialization */

static int magic_init(struct e4k_state *e4k)
{
	e4k_reg_write(e4k, 0x7e, 0x01);
	e4k_reg_write(e4k, 0x7f, 0xfe);
	e4k_reg_write(e4k, 0x82, 0x00);
	e4k_reg_write(e4k, 0x86, 0x50);	/* polarity A */
	e4k_reg_write(e4k, 0x87, 0x20);
	e4k_reg_write(e4k, 0x88, 0x01);
	e4k_reg_write(e4k, 0x9f, 0x7f);
	e4k_reg_write(e4k, 0xa0, 0x07);

	return 0;
}

/*! \brief Initialize the E4K tuner
 */
int e4k_init(struct e4k_state *e4k, bool enable_dc_offset_loop /*= true*/, bool set_manual_gain /*= false*/)
{
	/* make a dummy i2c read or write command, will not be ACKed! */
	e4k_reg_read(e4k, 0);

	/* Make sure we reset everything and clear POR indicator */
	e4k_reg_write(e4k, E4K_REG_MASTER1,
		E4K_MASTER1_RESET |
		E4K_MASTER1_NORM_STBY |
		E4K_MASTER1_POR_DET
	);

	/* Configure clock input */
	e4k_reg_write(e4k, E4K_REG_CLK_INP, 0x00);

	/* Disable clock output */
	e4k_reg_write(e4k, E4K_REG_REF_CLK, 0x00);
	e4k_reg_write(e4k, E4K_REG_CLKOUT_PWDN, 0x96);

	/* Write some magic values into registers */
	magic_init(e4k);

	/* Set common mode voltage a bit higher for more margin 850 mv */
	e4k_commonmode_set(e4k, 4);

	/* Initialize DC offset lookup tables */
	e4k_dc_offset_gen_table(e4k);

	/* Enable time variant DC correction */
	if (enable_dc_offset_loop)
	{
		e4k_reg_write(e4k, E4K_REG_DCTIME1, 0x01);
		e4k_reg_write(e4k, E4K_REG_DCTIME2, 0x01);
	}

	/* Set LNA mode to autnonmous */
	e4k_reg_write(e4k, E4K_REG_AGC4, 0x10); /* High threshold */
	e4k_reg_write(e4k, E4K_REG_AGC5, 0x04);	/* Low threshold */
	e4k_reg_write(e4k, E4K_REG_AGC6, 0x1a);	/* LNA calib + loop rate */

	if (set_manual_gain == false)
	{
		e4k_reg_set_mask(e4k, E4K_REG_AGC1, E4K_AGC1_MOD_MASK,
				 E4K_AGC_MOD_IF_SERIAL_LNA_AUTON);

		/* Set Mixer Gain Control to autonomous */
		e4k_reg_set_mask(e4k, E4K_REG_AGC7, E4K_AGC7_MIX_GAIN_AUTO,
							E4K_AGC7_MIX_GAIN_AUTO);
	}

	/* Enable LNA Gain enhancement */
#if 0
	e4k_reg_set_mask(e4k, E4K_REG_AGC11, 0x7,
			 E4K_AGC11_LNA_GAIN_ENH | (2 << 1));
#endif

	/* Enable automatic IF gain mode switching */
	e4k_reg_set_mask(e4k, E4K_REG_AGC8, 0x1, E4K_AGC8_SENS_LIN_AUTO);

	/* Select moderate gain levels */
	e4k_if_gain_set(e4k, 1, 6);
	e4k_if_gain_set(e4k, 2, 3);
	e4k_if_gain_set(e4k, 3, 3);
	e4k_if_gain_set(e4k, 4, 1);
	e4k_if_gain_set(e4k, 5, 9);
	e4k_if_gain_set(e4k, 6, 9);

	/* Set the most narrow filter we can possibly use */
	e4k_if_filter_bw_set(e4k, E4K_IF_FILTER_MIX, KHZ(1900));
	e4k_if_filter_bw_set(e4k, E4K_IF_FILTER_RC, KHZ(1000));
	e4k_if_filter_bw_set(e4k, E4K_IF_FILTER_CHAN, KHZ(2150));
	e4k_if_filter_chan_enable(e4k, 1);

	return 0;
}
