#ifndef __TUNER_E4000_H
#define __TUNER_E4000_H

class baz_rtl_source_c;

/**

@file

@brief   E4000 tuner module declaration
One can manipulate E4000 tuner through E4000 module.
E4000 module is derived from tuner module.

@par Example:
@code

// The example is the same as the tuner example in tuner_base.h except the listed lines.

#include "tuner_e4000.h"
...
int main(void)
{
	TUNER_MODULE        *pTuner;
	E4000_EXTRA_MODULE *pTunerExtra;

	TUNER_MODULE          TunerModuleMemory;
	BASE_INTERFACE_MODULE BaseInterfaceModuleMemory;
//	I2C_BRIDGE_MODULE     I2cBridgeModuleMemory;

	unsigned long BandwidthMode;

	...

	// Build E4000 tuner module.
	BuildE4000Module(
		&pTuner,
		&TunerModuleMemory,
		&BaseInterfaceModuleMemory,
		&I2cBridgeModuleMemory,
		0xac,								// I2C device address is 0xac in 8-bit format.
		CRYSTAL_FREQ_16384000HZ,			// Crystal frequency is 16.384 MHz.
		E4000_AGC_INTERNAL					// The E4000 AGC mode is internal AGC mode.
		);

	// Get E4000 tuner extra module.
	pTunerExtra = (T2266_EXTRA_MODULE *)(pTuner->pExtra);

	// ==== Initialize tuner and set its parameters =====
	...

	// Set E4000 bandwidth.
	pTunerExtra->SetBandwidthMode(pTuner, E4000_BANDWIDTH_6MHZ);

	// ==== Get tuner information =====
	...

	// Get E4000 bandwidth.
	pTunerExtra->GetBandwidthMode(pTuner, &BandwidthMode);

	// See the example for other tuner functions in tuner_base.h

	return 0;
}
@endcode
*/

//#include "tuner_base.h"

// The following context is implemented for E4000 source code.

// Definition (implemeted for E4000)
#define E4000_1_SUCCESS			1
#define E4000_1_FAIL			0
#define E4000_I2C_SUCCESS		1
#define E4000_I2C_FAIL			0

// Function (implemeted for E4000)
int
I2CReadByte(
	baz_rtl_source_c* pTuner,
	unsigned char NoUse,
	unsigned char RegAddr,
	unsigned char *pReadingByte
	);

int
I2CWriteByte(
	baz_rtl_source_c* pTuner,
	unsigned char NoUse,
	unsigned char RegAddr,
	unsigned char WritingByte
	);

int
I2CWriteArray(
	baz_rtl_source_c* pTuner,
	unsigned char NoUse,
	unsigned char RegStartAddr,
	unsigned char ByteNum,
	unsigned char *pWritingBytes
	);

// Functions (from E4000 source code)
int tunerreset (baz_rtl_source_c* pTuner);
int Tunerclock(baz_rtl_source_c* pTuner);
int Qpeak(baz_rtl_source_c* pTuner);
int DCoffloop(baz_rtl_source_c* pTuner);
int GainControlinit(baz_rtl_source_c* pTuner);

int Gainmanual(baz_rtl_source_c* pTuner);
int E4000_gain_freq(baz_rtl_source_c* pTuner, int frequency);
int PLL(baz_rtl_source_c* pTuner, int Ref_clk, int Freq);
int LNAfilter(baz_rtl_source_c* pTuner, int Freq);
int IFfilter(baz_rtl_source_c* pTuner, int bandwidth, int Ref_clk);
int freqband(baz_rtl_source_c* pTuner, int Freq);
int DCoffLUT(baz_rtl_source_c* pTuner);
int GainControlauto(baz_rtl_source_c* pTuner);

int E4000_sensitivity(baz_rtl_source_c* pTuner, int Freq, int bandwidth);
int E4000_linearity(baz_rtl_source_c* pTuner, int Freq, int bandwidth);
int E4000_high_linearity(baz_rtl_source_c* pTuner);
int E4000_nominal(baz_rtl_source_c* pTuner, int Freq, int bandwidth);

// The following context is E4000 tuner API source code

// Definitions

// Bandwidth in Hz
enum E4000_BANDWIDTH_HZ
{
	E4000_BANDWIDTH_6000000HZ = 6000000,
	E4000_BANDWIDTH_7000000HZ = 7000000,
	E4000_BANDWIDTH_8000000HZ = 8000000,
};

// Manipulaing functions
void
e4000_GetTunerType(
	baz_rtl_source_c* pTuner,
	int *pTunerType
	);

void
e4000_GetDeviceAddr(
	baz_rtl_source_c* pTuner,
	unsigned char *pDeviceAddr
	);

int
e4000_Initialize(
	baz_rtl_source_c* pTuner
	);

int
e4000_SetRfFreqHz(
	baz_rtl_source_c* pTuner,
	unsigned long RfFreqHz
	);

int
e4000_GetRfFreqHz(
	baz_rtl_source_c* pTuner,
	unsigned long *pRfFreqHz
	);

// Extra manipulaing functions
int
e4000_GetRegByte(
	baz_rtl_source_c* pTuner,
	unsigned char RegAddr,
	unsigned char *pReadingByte
	);

int
e4000_SetBandwidthHz(
	baz_rtl_source_c* pTuner,
	unsigned long BandwidthHz
	);

int
e4000_GetBandwidthHz(
	baz_rtl_source_c* pTuner,
	unsigned long *pBandwidthHz
	);

///////////////////////////////////////////////////////////////////////////////
// Definitions
#define RTL2832_E4000_ADDITIONAL_INIT_REG_TABLE_LEN		34

#define RTL2832_E4000_LNA_GAIN_TABLE_LEN				16
#define RTL2832_E4000_LNA_GAIN_ADD_TABLE_LEN			8
#define RTL2832_E4000_MIXER_GAIN_TABLE_LEN				2
#define RTL2832_E4000_IF_STAGE_1_GAIN_TABLE_LEN			2
#define RTL2832_E4000_IF_STAGE_2_GAIN_TABLE_LEN			4
#define RTL2832_E4000_IF_STAGE_3_GAIN_TABLE_LEN			4
#define RTL2832_E4000_IF_STAGE_4_GAIN_TABLE_LEN			4
#define RTL2832_E4000_IF_STAGE_5_GAIN_TABLE_LEN			8
#define RTL2832_E4000_IF_STAGE_6_GAIN_TABLE_LEN			8

#define RTL2832_E4000_LNA_GAIN_BAND_NUM					2
#define RTL2832_E4000_MIXER_GAIN_BAND_NUM				2

#define RTL2832_E4000_RF_BAND_BOUNDARY_HZ				300000000

#define RTL2832_E4000_LNA_GAIN_ADDR						0x14
#define RTL2832_E4000_LNA_GAIN_MASK						0xf
#define RTL2832_E4000_LNA_GAIN_SHIFT					0

#define RTL2832_E4000_LNA_GAIN_ADD_ADDR					0x24
#define RTL2832_E4000_LNA_GAIN_ADD_MASK					0x7
#define RTL2832_E4000_LNA_GAIN_ADD_SHIFT				0

#define RTL2832_E4000_MIXER_GAIN_ADDR					0x15
#define RTL2832_E4000_MIXER_GAIN_MASK					0x1
#define RTL2832_E4000_MIXER_GAIN_SHIFT					0

#define RTL2832_E4000_IF_STAGE_1_GAIN_ADDR				0x16
#define RTL2832_E4000_IF_STAGE_1_GAIN_MASK				0x1
#define RTL2832_E4000_IF_STAGE_1_GAIN_SHIFT				0

#define RTL2832_E4000_IF_STAGE_2_GAIN_ADDR				0x16
#define RTL2832_E4000_IF_STAGE_2_GAIN_MASK				0x6
#define RTL2832_E4000_IF_STAGE_2_GAIN_SHIFT				1

#define RTL2832_E4000_IF_STAGE_3_GAIN_ADDR				0x16
#define RTL2832_E4000_IF_STAGE_3_GAIN_MASK				0x18
#define RTL2832_E4000_IF_STAGE_3_GAIN_SHIFT				3

#define RTL2832_E4000_IF_STAGE_4_GAIN_ADDR				0x16
#define RTL2832_E4000_IF_STAGE_4_GAIN_MASK				0x60
#define RTL2832_E4000_IF_STAGE_4_GAIN_SHIFT				5

#define RTL2832_E4000_IF_STAGE_5_GAIN_ADDR				0x17
#define RTL2832_E4000_IF_STAGE_5_GAIN_MASK				0x7
#define RTL2832_E4000_IF_STAGE_5_GAIN_SHIFT				0

#define RTL2832_E4000_IF_STAGE_6_GAIN_ADDR				0x17
#define RTL2832_E4000_IF_STAGE_6_GAIN_MASK				0x38
#define RTL2832_E4000_IF_STAGE_6_GAIN_SHIFT				3

#define RTL2832_E4000_TUNER_OUTPUT_POWER_UNIT_0P1_DBM	-100

#define RTL2832_E4000_TUNER_MODE_UPDATE_WAIT_TIME_MS	1000

// Tuner gain mode
enum RTL2832_E4000_TUNER_GAIN_MODE
{
	RTL2832_E4000_TUNER_GAIN_SENSITIVE,
	RTL2832_E4000_TUNER_GAIN_NORMAL,
	RTL2832_E4000_TUNER_GAIN_LINEAR,
};

#endif
