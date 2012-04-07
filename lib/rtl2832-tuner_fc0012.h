#ifndef __TUNER_FC0012_H
#define __TUNER_FC0012_H

#include "rtl2832.h"

namespace RTL2832_NAMESPACE
{
namespace TUNERS_NAMESPACE
{

class fc0012 : public RTL2832_NAMESPACE::tuner_skeleton
{
IMPLEMENT_INLINE_TUNER_FACTORY(fc0012)
public:
	fc0012(demod* p);
public:
	inline virtual const char* name() const
	{ return "Fitipower FC0012"; }
public:
	int initialise(tuner::PPARAMS params = NULL);
	int set_frequency(double freq);
	int set_bandwidth(double bw);
	int set_gain(double gain);
};

}
}

#define FC0012_OK	0
#define FC0012_ERROR	1

#define FC0012_I2C_ADDR		0xc6
#define FC0012_CHECK_ADDR	0x00
#define FC0012_CHECK_VAL	0xa1

#define FC0012_BANDWIDTH_6MHZ	6
#define FC0012_BANDWIDTH_7MHZ	7
#define FC0012_BANDWIDTH_8MHZ	8
/*
#define FC0012_LNA_GAIN_LOW	0x00
#define FC0012_LNA_GAIN_MID	0x08
#define FC0012_LNA_GAIN_HI	0x17
#define FC0012_LNA_GAIN_MAX	0x10
*/
//int FC0012_Read(void *pTuner, unsigned char RegAddr, unsigned char *pByte);
//int FC0012_Write(void *pTuner, unsigned char RegAddr, unsigned char Byte);

int FC0012_Open(RTL2832_NAMESPACE::tuner* pTuner);
int FC0012_SetFrequency(RTL2832_NAMESPACE::tuner* pTuner, unsigned long Frequency, unsigned short Bandwidth);

// Tuner LNA
enum FC0012_LNA_GAIN_VALUE
{
	FC0012_LNA_GAIN_LOW    = 0x0,
	FC0012_LNA_GAIN_MIDDLE = 0x1,
	FC0012_LNA_GAIN_HIGH   = 0x2,
};

#endif
