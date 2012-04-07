#ifndef __TUNER_FC0013_H
#define __TUNER_FC0013_H

#include "rtl2832.h"

namespace RTL2832_NAMESPACE
{
namespace TUNERS_NAMESPACE
{

class fc0013 : public RTL2832_NAMESPACE::tuner_skeleton
{
IMPLEMENT_INLINE_TUNER_FACTORY(fc0013)
public:
	fc0013(demod* p);
public:
	inline virtual const char* name() const
	{ return "Fitipower FC0013"; }
public:
	int initialise(tuner::PPARAMS params = NULL);
	int set_frequency(double freq);
	int set_bandwidth(double bw);
	int set_gain(double gain);
};

}
}

/**
@file

@brief   FC0013 tuner module declaration

One can manipulate FC0013 tuner through FC0013 module.
FC0013 module is derived from tuner module.

// The following context is implemented for FC0013 source code.
**/

// Definitions
enum FC0013_TRUE_FALSE_STATUS
{
	FC0013_FALSE,
	FC0013_TRUE,
};

enum FC0013_I2C_STATUS
{
	FC0013_I2C_SUCCESS,
	FC0013_I2C_ERROR,
};

enum FC0013_FUNCTION_STATUS
{
	FC0013_FUNCTION_SUCCESS,
	FC0013_FUNCTION_ERROR,
};
/*
// Functions
int FC0013_Read(RTL2832_NAMESPACE::tuner* pTuner, unsigned char RegAddr, unsigned char *pByte);
int FC0013_Write(RTL2832_NAMESPACE::tuner* pTuner, unsigned char RegAddr, unsigned char Byte);

int
fc0013_SetRegMaskBits(
	RTL2832_NAMESPACE::tuner* pTuner,
	unsigned char RegAddr,
	unsigned char Msb,
	unsigned char Lsb,
	const unsigned char WritingValue
	);

int
fc0013_GetRegMaskBits(
	RTL2832_NAMESPACE::tuner* pTuner,
	unsigned char RegAddr,
	unsigned char Msb,
	unsigned char Lsb,
	unsigned char *pReadingValue
	);
*/
int FC0013_Open(RTL2832_NAMESPACE::tuner* pTuner);
int FC0013_SetFrequency(RTL2832_NAMESPACE::tuner* pTuner, unsigned long Frequency, unsigned short Bandwidth);

// Set VHF Track depends on input frequency
int FC0013_SetVhfTrack(RTL2832_NAMESPACE::tuner* pTuner, unsigned long Frequency);

// The following context is FC0013 tuner API source code

// Definitions

// Bandwidth mode
enum FC0013_BANDWIDTH_MODE
{
	FC0013_BANDWIDTH_6000000HZ = 6,
	FC0013_BANDWIDTH_7000000HZ = 7,
	FC0013_BANDWIDTH_8000000HZ = 8,
};

// Default for initialing
#define FC0013_RF_FREQ_HZ_DEFAULT			50000000
#define FC0013_BANDWIDTH_MODE_DEFAULT		FC0013_BANDWIDTH_8000000HZ

// Tuner LNA
enum FC0013_LNA_GAIN_VALUE
{
	FC0013_LNA_GAIN_LOW     = 0x00,	// -6.3dB
	FC0013_LNA_GAIN_MIDDLE  = 0x08,	//  7.1dB
	FC0013_LNA_GAIN_HIGH_17 = 0x11,	// 19.1dB
	FC0013_LNA_GAIN_HIGH_19 = 0x10,	// 19.7dB
};

// Manipulaing functions
void
fc0013_GetTunerType(
	RTL2832_NAMESPACE::tuner* pTuner,
	int *pTunerType
	);

void
fc0013_GetDeviceAddr(
	RTL2832_NAMESPACE::tuner* pTuner,
	unsigned char *pDeviceAddr
	);

int
fc0013_Initialize(
	RTL2832_NAMESPACE::tuner* pTuner
	);

int
fc0013_SetRfFreqHz(
	RTL2832_NAMESPACE::tuner* pTuner,
	unsigned long RfFreqHz
	);

int
fc0013_GetRfFreqHz(
	RTL2832_NAMESPACE::tuner* pTuner,
	unsigned long *pRfFreqHz
	);

// Extra manipulaing functions
int
fc0013_SetBandwidthMode(
	RTL2832_NAMESPACE::tuner* pTuner,
	int BandwidthMode
	);

int
fc0013_GetBandwidthMode(
	RTL2832_NAMESPACE::tuner* pTuner,
	int *pBandwidthMode
	);

int
fc0013_RcCalReset(
	RTL2832_NAMESPACE::tuner* pTuner
	);

int
fc0013_RcCalAdd(
	RTL2832_NAMESPACE::tuner* pTuner,
	int RcValue
	);

#endif
