#ifndef INCLUDED_RTL2832
#define INCLUDED_RTL2832

#include <stdio.h>
#include <stdarg.h>

#ifdef LIBUSB_FOUND
#include <libusb-1.0/libusb.h>
#else
#include <libusb/libusb.h>	// libusbx on Windows
#endif // LIBUSB_FOUND

#ifdef _WIN32

#ifdef RTL2832_EXPORTS
#define RTL2832_API			__declspec(dllexport)
#else
#define RTL2832_API			__declspec(dllimport)
#endif

// 'libusb.h' offers uint8_t & uint16_t
typedef __int8   int8_t;
typedef unsigned int uint32_t;
typedef unsigned __int64 uint64_t;

#else

#define RTL2832_API
#define RTL2832_TEMPLATE

#endif // _WIN32

#include <vector>
#include <map>
#include <string>

RTL2832_API extern int get_map_index(int value, const int* map, int pair_count);
RTL2832_API extern const char* libusb_result_to_string(int res);

#ifdef _WIN32
#define CURRENT_FUNCTION	__FUNCTION__
#else
#define CURRENT_FUNCTION	__PRETTY_FUNCTION__
#endif // _WIN32

// Swap following comments to disable I2C reporting
//#define DEBUG_TUNER_I2C(t,r)	// This is empty on purpose
#define DEBUG_TUNER_I2C(t,r) \
	if (t->params().message_output && function && (line_number >= 0) && (line)) \
		t->params().message_output->on_log_message_ex(RTL2832_NAMESPACE::log_sink::LOG_LEVEL_ERROR, "%s: %s [%i] @ %s:%i \"%s\"\n", __FUNCTION__, libusb_result_to_string(r), r, function, line_number, line);

#define RTL2832_NAMESPACE	rtl2832

namespace RTL2832_NAMESPACE
{

class demod;

enum result_code
{
	// < 0: libusb result code
	FAILURE	= 0,
	SUCCESS	= 1	// > 0 also # bytes transferred
};

#define TUNERS_NAMESPACE	tuners

#define TUNER_FACTORY_FN_NAME	Factory
#define TUNER_PROBE_FN_NAME		Probe
#define DECLARE_TUNER_FACTORY()	\
	public: \
	static tuner* TUNER_FACTORY_FN_NAME(demod* p);  \
	static int TUNER_PROBE_FN_NAME(demod* d);
#define IMPLEMENT_TUNER_FACTORY(class_name) \
	tuner* class_name::TUNER_FACTORY_FN_NAME(demod* p) \
	{ return new class_name(p); }
#define IMPLEMENT_INLINE_TUNER_FACTORY(class_name) \
	public: \
	static tuner* TUNER_FACTORY_FN_NAME(demod* p) \
	{ return new class_name(p); } \
	static int TUNER_PROBE_FN_NAME(demod* d);

// _WIN32: DLL WARNING! These will cause C4251, but only 'vector' can be exported as a DLL interface. You must have exactly the same STL headers in your importing files, otherwise your app will crash. Solution is to recompile this library with your own STL headers.
typedef std::pair<double,double> range_t;
typedef std::vector<double> values_t;
typedef std::map<int,std::string> num_name_map_t;

inline bool values_to_range(const values_t& v, range_t& r)
{ if (v.empty()) return false; r = std::make_pair(v[0], v[v.size() - 1]); return true; }

inline bool is_valid_range(const range_t& r)
{ return !(r.first == r.second); }

inline bool in_range(const range_t& r, double d)
{ return !((d < r.first) || (d > r.second)); }

inline bool in_valid_range(const range_t& r, double d)	// If invalid range, return 'true'
{ return ((is_valid_range(r) == false) || (in_range(r, d))); }

inline double calc_range(const range_t& r)
{ return (r.second - r.first); }

class log_sink
{
public:
	enum level
	{
		LOG_LEVEL_ERROR		= -1,	// Negative: more serious
		LOG_LEVEL_DEFAULT	= 0,
		LOG_LEVEL_VERBOSE	= 1,	// Positive: more verbose
	};
public:
	virtual void on_log_message_va(int level, const char* msg, va_list args)=0;
public:
	inline virtual void on_log_message(const char* msg, ...)
	{ va_list args; va_start(args, msg); on_log_message_va(LOG_LEVEL_DEFAULT, msg, args); }
	inline virtual void on_log_message_ex(int level, const char* msg, ...)
	{ va_list args; va_start(args, msg); on_log_message_va(level, msg, args); }
};

class i2c_interface
{
public:
	virtual int set_i2c_repeater(bool on = true, const char* function_name = NULL, int line_number = -1, const char* line = NULL)=0;
	virtual int i2c_read(uint8_t i2c_addr, uint8_t *buffer, int len)=0;
	virtual int i2c_write(uint8_t i2c_addr, uint8_t *buffer, int len)=0;
	virtual int i2c_write_reg(uint8_t i2c_addr, uint8_t reg, uint8_t val)=0;
	virtual int i2c_read_reg(uint8_t i2c_addr, uint8_t reg, uint8_t& data)=0;
};

class named_interface
{
public:
	virtual const char* name() const=0;
};

class RTL2832_API tuner : public i2c_interface, public named_interface
{
public:
	virtual ~tuner();
public:
	typedef tuner* (*CreateTunerFn)(demod* p);
	typedef int (*ProbeTunerFn)(demod* p);
	typedef struct params
	{
		log_sink*	message_output;
		bool 		verbose;
	} PARAMS, *PPARAMS;
	enum gain_mode {
		NOT_SUPPORTED	= -1,
		DEFAULT			= 0,
		// Specific values > 0 & internal to tuner implementations
	};
public:
	virtual int initialise(PPARAMS params = NULL)=0;
	//virtual const char* name()=0;
	virtual int set_frequency(double freq)=0;
	virtual int set_bandwidth(double bw)=0;
	virtual int set_gain(double gain)=0;
	virtual int set_gain_mode(int mode)=0;
	virtual int set_auto_gain_mode(bool on = true)=0;
public:
	virtual int set_i2c_repeater(bool on = true, const char* function_name = NULL, int line_number = -1, const char* line = NULL)=0;
	virtual int i2c_read(uint8_t i2c_addr, uint8_t *buffer, int len)=0;
	virtual int i2c_write(uint8_t i2c_addr, uint8_t *buffer, int len)=0;
public:
	virtual double frequency() const=0;
	virtual double bandwidth() const=0;
	virtual double gain() const=0;
	virtual int gain_mode() const=0;
public:
	virtual range_t gain_range() const=0;
	virtual values_t gain_values() const=0;
	virtual range_t frequency_range() const=0;
	virtual range_t bandwidth_range() const=0;
	virtual values_t bandwidth_values() const=0;
public:
	virtual num_name_map_t gain_modes() const=0;
	virtual bool calc_appropriate_gain_mode(int& mode)/* const*/=0;
	virtual bool auto_gain_mode() const=0;
public:
	virtual const PARAMS& params() const=0;
	virtual demod* parent() const=0;
};

class RTL2832_API tuner_skeleton : public tuner
{
public:
	tuner_skeleton(demod* p);
	virtual ~tuner_skeleton();
protected:
	demod* m_demod;
	tuner::PARAMS m_params;
	bool m_auto_gain_mode;
	int m_gain_mode;
	double m_freq;
	double m_gain;
	double m_bandwidth;
	range_t m_gain_range;
	values_t m_gain_values;
	range_t m_frequency_range;
	range_t m_bandwidth_range;
	values_t m_bandwidth_values;
	num_name_map_t m_gain_modes;
public:
	virtual int initialise(tuner::PPARAMS params = NULL);
	virtual const char* name() const
	{ return "(dummy)"; }
	virtual int set_frequency(double freq)
	{ return SUCCESS; }
	virtual int set_bandwidth(double bw)
	{ return SUCCESS; }
	virtual int set_gain(double gain)
	{ return SUCCESS; }
	virtual int set_gain_mode(int mode)
	{ return SUCCESS; }
public:
	virtual int set_i2c_repeater(bool on = true, const char* function_name = NULL, int line_number = -1, const char* line = NULL);
	virtual int i2c_read(uint8_t i2c_addr, uint8_t *buffer, int len);
	virtual int i2c_write(uint8_t i2c_addr, uint8_t *buffer, int len);
	virtual int i2c_write_reg(uint8_t i2c_addr, uint8_t reg, uint8_t val);
	virtual int i2c_read_reg(uint8_t i2c_addr, uint8_t reg, uint8_t& data);
public:
	virtual double frequency() const
	{ return m_freq; }
	virtual double bandwidth() const
	{ return m_bandwidth; }
	virtual double gain() const
	{ return m_gain; }
	virtual int gain_mode() const
	{ return m_gain_mode; }
	virtual int set_auto_gain_mode(bool on = true)
	{ m_auto_gain_mode = on; return SUCCESS; }
public:
	virtual range_t gain_range() const
	{ return m_gain_range; }
	virtual values_t gain_values() const
	{ return m_gain_values; }
	virtual range_t frequency_range() const
	{ return m_frequency_range; }
	virtual range_t bandwidth_range() const
	{ return m_bandwidth_range; }
	virtual values_t bandwidth_values() const
	{ return m_bandwidth_values; }
public:
	virtual num_name_map_t gain_modes() const
	{ return m_gain_modes; }
	virtual bool calc_appropriate_gain_mode(int& mode)/* const*/
	{ mode = m_gain_mode; return true; }
	virtual bool auto_gain_mode() const
	{ return m_auto_gain_mode; }
public:
	const tuner::PARAMS& params() const
	{ return m_params; }
	demod* parent() const
	{ return m_demod; }
};

class i2c_repeater_scope
{
private:
	i2c_interface* m_p;
	const char* m_function_name;
	int m_line_number;
	const char* m_line;
public:
	i2c_repeater_scope(i2c_interface* p, const char* function_name = NULL, int line_number = -1, const char* line = NULL)
		: m_p(p)
		, m_function_name(function_name)
		, m_line_number(line_number)
		, m_line(line)
	{ p->set_i2c_repeater(true, function_name, line_number, line); }
	~i2c_repeater_scope()
	{ m_p->set_i2c_repeater(false, m_function_name, m_line_number, m_line); }
};

#define I2C_REPEATER_SCOPE(p)		i2c_repeater_scope _i2c_repeater_scope(p, CURRENT_FUNCTION, __LINE__, p->name())
#define THIS_I2C_REPEATER_SCOPE()	I2C_REPEATER_SCOPE(this)

typedef struct device_info
{
	const char* name;
	uint16_t vid, pid;
	tuner::CreateTunerFn factory;
	uint32_t max_rate, min_rate, crystal_frequency, flags;
} DEVICE_INFO, *PDEVICE_INFO;

//#define CHECK_LIBUSB_RESULT(r)			(r)
//#define CHECK_LIBUSB_NEG_RESULT(r)		(r)
//#define CHECK_LIBUSB_RESULT_EX(r,f,l,s)	(r)
#define CHECK_LIBUSB_RESULT(r)				check_libusb_result(r, false, CURRENT_FUNCTION, __LINE__, #r)
#define CHECK_LIBUSB_NEG_RESULT(r)			check_libusb_result(r, true, CURRENT_FUNCTION, __LINE__, #r)
#define CHECK_LIBUSB_RESULT_EX(r,f,l,s)		check_libusb_result(r, false, f, l, s)

#define CHECK_LIBUSB_RESULT_RETURN_EX(d,r)	\
	{ int res = d->CHECK_LIBUSB_RESULT(r); \
	if (res <= 0) return res; }

#define CHECK_LIBUSB_NEG_RESULT_RETURN_EX(d,r)	\
	{ int res = d->CHECK_LIBUSB_NEG_RESULT(r); \
	if (res < 0) return res; }

#define CHECK_LIBUSB_RESULT_RETURN(r)		CHECK_LIBUSB_RESULT_RETURN_EX(this,r)
#define CHECK_LIBUSB_NEG_RESULT_RETURN(d,r)	CHECK_LIBUSB_NEG_RESULT_RETURN_EX(this,r)

#define RTL2832_FIR_COEFF_COUNT	20
#define RTL2832_TUNER_NAME_LEN	(32+1)

class RTL2832_API demod : public i2c_interface, public named_interface
{
public:
	demod();
	virtual ~demod();
public:
	//static const int FIR_COEFF_COUNT	= 20;
	//static const int TUNER_NAME_LEN		= 32+1;
	typedef struct params
	{
		uint16_t		vid;
		uint16_t		pid;
		bool			verbose;
		int				default_timeout;	// 0: use default, -1: poll only
		log_sink*		message_output;
		bool			use_custom_fir_coefficients;
		uint8_t			fir_coeff[RTL2832_FIR_COEFF_COUNT];
		//bool			use_tuner_params;	// Use if valid tuner::PPARAMS pointer
		tuner::PPARAMS	tuner_params;
		uint32_t		crystal_frequency;
		char			tuner_name[RTL2832_TUNER_NAME_LEN];
	} PARAMS, *PPARAMS;
protected:
	struct libusb_device_handle *m_devh;
	PDEVICE_INFO m_current_info;
	tuner *m_tuner, *m_dummy_tuner;
	bool m_libusb_init_done;
	PARAMS m_params;
	range_t m_sample_rate_range;
	double m_sample_rate;
	uint32_t m_crystal_frequency;
	bool m_tuner_was_active;	// True if the kernel driver was detached
public:
	int initialise(PPARAMS params = NULL);
	const char* name() const;
	void destroy();
	int reset();
	int set_sample_rate(uint32_t samp_rate, double* real_rate = NULL);
	int set_if(double frequency);
	int read_samples(unsigned char* buffer, uint32_t buffer_size, int* bytes_read, int timeout = -1);
protected:
	int find_device();
	int init_demod();
	int demod_write_reg(uint8_t page, uint16_t addr, uint16_t val, uint8_t len);
	int demod_read_reg(uint8_t page, uint8_t addr, uint8_t len, uint16_t& reg);
	int write_reg(uint8_t block, uint16_t addr, uint16_t val, uint8_t len);
	int read_reg(uint8_t block, uint16_t addr, uint8_t len, uint16_t& reg);
	int write_array(uint8_t block, uint16_t addr, uint8_t *array, uint8_t len);
	int read_array(uint8_t block, uint16_t addr, uint8_t *array, uint8_t len);
public:
	int check_libusb_result(int res, bool zero_okay, const char* function_name = NULL, int line_number = -1, const char* line = NULL);
	void log(const char* message, ...);
public:
	int set_i2c_repeater(bool on = true, const char* function_name = NULL, int line_number = -1, const char* line = NULL);
	int i2c_read(uint8_t i2c_addr, uint8_t *buffer, int len);
	int i2c_write(uint8_t i2c_addr, uint8_t *buffer, int len);
	int i2c_write_reg(uint8_t i2c_addr, uint8_t reg, uint8_t val);
	int i2c_read_reg(uint8_t i2c_addr, uint8_t reg, uint8_t& data);
public:
	int set_gpio_output(uint8_t gpio);
	int set_gpio_bit(uint8_t gpio, int val);
public:
	inline tuner* active_tuner() const
	{ return m_tuner; }
	inline double sample_rate() const
	{ return m_sample_rate; }
	inline uint32_t crystal_frequency() const
	{ return m_crystal_frequency; }
	range_t sample_rate_range() const
	{ return m_sample_rate_range; }
protected:
	enum usb_reg {
		USB_SYSCTL			= 0x2000,
		USB_CTRL			= 0x2010,
		USB_STAT			= 0x2014,
		USB_EPA_CFG			= 0x2144,
		USB_EPA_CTL			= 0x2148,
		USB_EPA_MAXPKT		= 0x2158,
		USB_EPA_MAXPKT_2	= 0x215a,
		USB_EPA_FIFO_CFG	= 0x2160,
	};
	enum sys_reg {
		DEMOD_CTL		= 0x3000,
		GPO				= 0x3001,
		GPI				= 0x3002,
		GPOE			= 0x3003,
		GPD				= 0x3004,
		SYSINTE			= 0x3005,
		SYSINTS			= 0x3006,
		GP_CFG0			= 0x3007,
		GP_CFG1			= 0x3008,
		SYSINTE_1		= 0x3009,
		SYSINTS_1		= 0x300a,
		DEMOD_CTL_1		= 0x300b,
		IR_SUSPEND		= 0x300c,
	};
	enum blocks {
		DEMODB			= 0,
		USBB			= 1,
		SYSB			= 2,
		TUNB			= 3,
		ROMB			= 4,
		IRB				= 5,
		IICB			= 6,
	};
};

}

#endif // INCLUDED_RTL2832
