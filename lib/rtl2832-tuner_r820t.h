#ifndef __TUNER_R820T_H
#define __TUNER_R820T_H

#include "rtl2832.h"

////////////////////////////////////////

#ifndef _UINT_X_
#define _UINT_X_ 1
typedef unsigned char  UINT8;
typedef unsigned short UINT16;
typedef unsigned int   UINT32;
#endif

#define TRUE	1
#define FALSE	0

typedef enum _R828_ErrCode
{
	RT_Success,
	RT_Fail
}R828_ErrCode;

//----------------------------------------------------------//
//                   R828 Parameter                        //
//----------------------------------------------------------//

typedef enum _R828_Standard_Type  //Don't remove standand list!!
{
	NTSC_MN = 0,
	PAL_I,
	PAL_DK,
	PAL_B_7M,       //no use
	PAL_BGH_8M,     //for PAL B/G, PAL G/H
	SECAM_L,
	SECAM_L1_INV,   //for SECAM L'
	SECAM_L1,       //no use
	ATV_SIZE,
	DVB_T_6M = ATV_SIZE,
	DVB_T_7M,
	DVB_T_7M_2,
	DVB_T_8M,
	DVB_T2_6M,
	DVB_T2_7M,
	DVB_T2_7M_2,
	DVB_T2_8M,
	DVB_T2_1_7M,
	DVB_T2_10M,
	DVB_C_8M,
	DVB_C_6M,
	ISDB_T,
	DTMB,
	R828_ATSC,
	FM,
	STD_SIZE
}R828_Standard_Type;

typedef enum _R828_SetFreq_Type
{
	FAST_MODE = TRUE,
	NORMAL_MODE = FALSE
}R828_SetFreq_Type;

typedef enum _R828_LoopThrough_Type
{
	LOOP_THROUGH = TRUE,
	SIGLE_IN     = FALSE
}R828_LoopThrough_Type;

typedef enum _R828_InputMode_Type
{
	AIR_IN = 0,
	CABLE_IN_1,
	CABLE_IN_2
}R828_InputMode_Type;

typedef enum _R828_IfAgc_Type
{
	IF_AGC1 = 0,
	IF_AGC2
}R828_IfAgc_Type;

typedef enum _R828_GPIO_Type
{
	HI_SIG = TRUE,
	LO_SIG = FALSE
}R828_GPIO_Type;

typedef struct _R828_Set_Info
{
	UINT32        RF_Hz;
	UINT32        RF_KHz;
	R828_Standard_Type R828_Standard;
	R828_LoopThrough_Type RT_Input;
	R828_InputMode_Type   RT_InputMode;
	R828_IfAgc_Type R828_IfAgc_Select;
}R828_Set_Info;

typedef struct _R828_RF_Gain_Info
{
	UINT8   RF_gain1;
	UINT8   RF_gain2;
	UINT8   RF_gain_comb;
}R828_RF_Gain_Info;

typedef enum _R828_RF_Gain_TYPE
{
	RF_AUTO = 0,
	RF_MANUAL
}R828_RF_Gain_TYPE;

typedef struct _R828_I2C_LEN_TYPE
{
	UINT8 RegAddr;
	UINT8 Data[50];
	UINT8 Len;
}R828_I2C_LEN_TYPE;

typedef struct _R828_I2C_TYPE
{
	UINT8 RegAddr;
	UINT8 Data;
}R828_I2C_TYPE;

typedef struct _R828_SectType
{
	UINT8 Phase_Y;
	UINT8 Gain_X;
	UINT16 Value;
}R828_SectType;

typedef enum _BW_Type
{
	BW_6M = 0,
	BW_7M,
	BW_8M,
	BW_1_7M,
	BW_10M,
	BW_200K
}BW_Type;

typedef struct _Sys_Info_Type
{
	UINT16		IF_KHz;
	BW_Type		BW;
	UINT32		FILT_CAL_LO;
	UINT8		FILT_GAIN;
	UINT8		IMG_R;
	UINT8		FILT_Q;
	UINT8		HP_COR;
	UINT8       EXT_ENABLE;
	UINT8       LOOP_THROUGH;
	UINT8       LT_ATT;
	UINT8       FLT_EXT_WIDEST;
	UINT8       POLYFIL_CUR;
}Sys_Info_Type;

typedef struct _Freq_Info_Type
{
	UINT8		OPEN_D;
	UINT8		RF_MUX_PLOY;
	UINT8		TF_C;
	UINT8		XTAL_CAP20P;
	UINT8		XTAL_CAP10P;
	UINT8		XTAL_CAP0P;
	UINT8		IMR_MEM;
}Freq_Info_Type;

typedef struct _SysFreq_Info_Type
{
	UINT8		LNA_TOP;
	UINT8		LNA_VTH_L;
	UINT8		MIXER_TOP;
	UINT8		MIXER_VTH_L;
	UINT8      AIR_CABLE1_IN;
	UINT8      CABLE2_IN;
	UINT8		PRE_DECT;
	UINT8      LNA_DISCHARGE;
	UINT8      CP_CUR;
	UINT8      DIV_BUF_CUR;
	UINT8      FILTER_CUR;
}SysFreq_Info_Type;

////////////////////////////////////////

namespace RTL2832_NAMESPACE { namespace TUNERS_NAMESPACE
{
class r820t : public RTL2832_NAMESPACE::tuner_skeleton
{
IMPLEMENT_INLINE_TUNER_FACTORY(r820t)
public:
	//UINT8  R828_ADDRESS;
	UINT32 R828_IF_khz;
	UINT32 R828_CAL_LO_khz;
	UINT8  R828_IMR_point_num;
	UINT8  R828_IMR_done_flag;
	//UINT8  Rafael_Chip;
	UINT8 R828_Arry[27];
public:
	R828_SectType IMR_Data[5];
	R828_I2C_TYPE R828_I2C;
	R828_I2C_LEN_TYPE R828_I2C_Len;
	UINT8  R828_Fil_Cal_flag[STD_SIZE];
	UINT8 R828_Fil_Cal_code[STD_SIZE];
	UINT8 Xtal_cap_sel;
	UINT8 Xtal_cap_sel_tmp;
	SysFreq_Info_Type SysFreq_Info1;
	Sys_Info_Type Sys_Info1;
	//static Freq_Info_Type R828_Freq_Info;
	Freq_Info_Type Freq_Info1;
public:
	r820t(demod* p);
public:
	inline virtual const char* name() const
	{ return "Rafael Micro R820T"; }
public:
	int initialise(tuner::PPARAMS params = NULL);
	int set_frequency(double freq);
	int set_bandwidth(double bw);
	int set_gain(double gain);
};

} }

//----------------------------------------------------------//
//                   R828 Functions                         //
//----------------------------------------------------------//

R828_ErrCode R828_Init(RTL2832_NAMESPACE::TUNERS_NAMESPACE::r820t* pTuner);
R828_ErrCode R828_Standby(RTL2832_NAMESPACE::TUNERS_NAMESPACE::r820t* pTuner, R828_LoopThrough_Type R828_LoopSwitch);
R828_ErrCode R828_GPIO(RTL2832_NAMESPACE::TUNERS_NAMESPACE::r820t* pTuner, R828_GPIO_Type R828_GPIO_Conrl);
R828_ErrCode R828_SetStandard(RTL2832_NAMESPACE::TUNERS_NAMESPACE::r820t* pTuner, R828_Standard_Type RT_Standard);
R828_ErrCode R828_SetFrequency(RTL2832_NAMESPACE::TUNERS_NAMESPACE::r820t* pTuner, R828_Set_Info R828_INFO, R828_SetFreq_Type R828_SetFreqMode);
R828_ErrCode R828_GetRfGain(RTL2832_NAMESPACE::TUNERS_NAMESPACE::r820t* pTuner, R828_RF_Gain_Info *pR828_rf_gain);
R828_ErrCode R828_SetRfGain(RTL2832_NAMESPACE::TUNERS_NAMESPACE::r820t* pTuner, int gain);
R828_ErrCode R828_RfGainMode(RTL2832_NAMESPACE::TUNERS_NAMESPACE::r820t* pTuner, int manual);
int r820t_SetRfFreqHz(RTL2832_NAMESPACE::TUNERS_NAMESPACE::r820t* pTuner, unsigned long RfFreqHz);
int r820t_SetStandardMode(RTL2832_NAMESPACE::TUNERS_NAMESPACE::r820t* pTuner, int StandardMode);
int r820t_SetStandby(RTL2832_NAMESPACE::TUNERS_NAMESPACE::r820t* pTuner, int LoopThroughType);

#endif /* __TUNER_R820T_H */
