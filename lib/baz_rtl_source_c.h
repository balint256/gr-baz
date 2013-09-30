/* -*- c++ -*- */
/*
 * Copyright 2004,2013 Free Software Foundation, Inc.
 * 
 * This file is part of GNU Radio
 * 
 * GNU Radio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 * 
 * GNU Radio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with GNU Radio; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

/*
 * gr-baz by Balint Seeber (http://spench.net/contact)
 * Information, documentation & samples: http://wiki.spench.net/wiki/gr-baz
 */

#ifndef INCLUDED_BAZ_RTL_SOURCE_C_H
#define INCLUDED_BAZ_RTL_SOURCE_C_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gnuradio/block.h>
#include <gnuradio/msg_queue.h>

#if defined(_WIN32) || defined(__WIN32__) || defined(WIN32)
#define NOMINMAX
#endif

#include <boost/thread.hpp>
#include <boost/thread/condition.hpp>
#include <boost/thread/recursive_mutex.hpp>
#ifdef HAVE_XTIME
#include <boost/thread/xtime.hpp>
#endif // HAVE_XTIME

#include <libusb-1.0/libusb.h>	// FIXME: Automake
#include <stdarg.h>	// va_list

#include "rtl2832.h"

class BAZ_API baz_rtl_source_c;
typedef boost::shared_ptr<baz_rtl_source_c> baz_rtl_source_c_sptr;

/*!
 * \brief Return a shared_ptr to a new instance of baz_rtl_source_c.
 */
BAZ_API baz_rtl_source_c_sptr baz_make_rtl_source_c(bool defer_creation = false, int output_size = 0);

/*!
 * \brief capture samples from an RTL2832-based device.
 * \ingroup block
 *
 * \sa gr-baz: http://wiki.spench.net/wiki/gr-baz
 */
class BAZ_API baz_rtl_source_c : public gr::block, public RTL2832_NAMESPACE::log_sink
{
private:
	friend BAZ_API baz_rtl_source_c_sptr baz_make_rtl_source_c(bool defer_creation, int output_size);
private:
	baz_rtl_source_c(bool defer_creation = false, int output_size = 0);
 public:
	~baz_rtl_source_c();
private:
	RTL2832_NAMESPACE::demod m_demod;
	size_t m_recv_samples_per_packet;
	uint64_t m_nSamplesReceived;
	uint32_t m_nOverflows;
	bool m_bRunning;
	boost::recursive_mutex d_mutex;
	boost::thread m_pCaptureThread;
	uint32_t m_nBufferSize;
	uint32_t m_nBufferStart;
	uint32_t m_nBufferItems;
	boost::condition m_hPacketEvent;
	uint8_t* m_pUSBBuffer;
	bool m_bBuffering;
	uint32_t m_nReadLength;
	uint32_t m_nBufferMultiplier;
	bool m_bUseBuffer;
	float m_fBufferLevel;
	uint32_t m_nReadPacketCount;
	uint32_t m_nBufferOverflowCount;
	uint32_t m_nBufferUnderrunCount;
#ifdef HAVE_XTIME
	boost::xtime m_wait_delay, m_wait_next;
#endif // HAVE_XTIME
private:
	RTL2832_NAMESPACE::demod::PARAMS m_demod_params;
	bool m_verbose;
	bool m_relative_gain;
	int m_output_size;
	gr::msg_queue::sptr m_status_queue;
private:
	enum log_level
	{
		LOG_LEVEL_ERROR		= RTL2832_NAMESPACE::log_sink::LOG_LEVEL_ERROR,
		LOG_LEVEL_INFO		= RTL2832_NAMESPACE::log_sink::LOG_LEVEL_DEFAULT,
		LOG_LEVEL_VERBOSE	= RTL2832_NAMESPACE::log_sink::LOG_LEVEL_VERBOSE
	};
private: // log_sink
	void on_log_message_va(int level, const char* msg, va_list args)
	{ log(level, msg, args); }
private:
	void log(int level, const char* message, va_list args);
#define IMPLEMENT_LOG_FUNCTION(suffix,level) \
	inline void log_##suffix(const char* message, ...) \
	{ va_list args; va_start(args, message); log(level, message, args); }
	IMPLEMENT_LOG_FUNCTION(error, LOG_LEVEL_ERROR)
	IMPLEMENT_LOG_FUNCTION(verbose, LOG_LEVEL_VERBOSE)
private:
	void reset();
	static void _capture_thread(baz_rtl_source_c* p);
	void capture_thread();
	void report_status(int status);
public:
	void set_defaults();
	bool set_output_format(int size);
	void set_status_msgq(gr::msg_queue::sptr queue);
	bool create(bool reset_defaults = false);
	void destroy();
public:	// SWIG demod params set only
	inline void set_vid(uint16_t vid)
	{ m_demod_params.vid = vid; }
	inline void set_pid(uint16_t pid)
	{ m_demod_params.pid = pid; }
	inline void set_default_timeout(int timeout)	// 0: use default, -1: poll only
	{ m_demod_params.default_timeout = timeout; }
	inline void set_fir_coefficients(const std::vector</*uint8_t*/int>& coeffs)
	{ m_demod_params.use_custom_fir_coefficients = !coeffs.empty(); for (size_t i = 0; i < std::min(/*RTL2832_NAMESPACE::demod::*/RTL2832_FIR_COEFF_COUNT, (int)coeffs.size()); ++i) m_demod_params.fir_coeff[i] = (uint8_t)coeffs[i]; }
	inline void set_crystal_frequency(uint32_t freq)
	{ m_demod_params.crystal_frequency = freq; }
	inline void set_tuner_name(const char* name)
	{ if (name == NULL) m_demod_params.tuner_name[0] = '\0'; else strncpy(m_demod_params.tuner_name, name, /*RTL2832_NAMESPACE::demod::*/RTL2832_TUNER_NAME_LEN-1); }
public:	// SWIG get only
	inline size_t recv_samples_per_packet() const
	{ return m_recv_samples_per_packet; }
	inline uint64_t samples_received() const
	{ return m_nSamplesReceived; }
	inline uint32_t overflows() const
	{ return m_nOverflows; }
	inline bool running() const
	{ return m_bRunning; }
	inline uint32_t buffer_size() const
	{ return m_nBufferSize; }
	inline uint32_t buffer_times() const
	{ return m_nBufferItems; }
	inline bool buffering() const
	{ return m_bBuffering; }
	inline uint32_t read_packet_count() const
	{ return m_nReadPacketCount; }
	inline uint32_t buffer_overflow_count() const
	{ return m_nBufferOverflowCount; }
	inline uint32_t buffer_underrun_count() const
	{ return m_nBufferUnderrunCount; }
public:	// SWIG set (pre-create)
	inline void set_relative_gain(bool on = true)
	{ m_relative_gain = on; }
	inline void set_verbose(bool on = true)
	{ m_verbose = on; }
	inline void set_read_length(uint32_t length)
	{ if (length > 0) m_nReadLength = length; }
	inline void set_buffer_multiplier(uint32_t mul)
	{ m_nBufferMultiplier = mul; }
	inline void set_use_buffer(bool use = true)
	{ m_bUseBuffer = use; }
	inline void set_buffer_level(float level)
	{ m_fBufferLevel = level; }
public:	// SWIG get
	inline bool relative_gain() const
	{ return m_relative_gain; }
	inline bool verbose() const
	{ return m_verbose; }
	inline uint32_t read_length() const
	{ return m_nReadLength; }
	inline uint32_t buffer_multiplier() const
	{ return m_nBufferMultiplier; }
	inline bool use_buffer() const
	{ return m_bUseBuffer; }
	inline float buffer_level() const
	{ return m_fBufferLevel; }
public:	// SWIG set
	bool set_sample_rate(double sample_rate);
	bool set_frequency(double freq);
	bool set_gain(double gain);
	bool set_bandwidth(double bandwidth);
	bool set_gain_mode(int mode);
	bool set_gain_mode(const char* mode);
	bool set_auto_gain_mode(bool on = true);
public:	// SWIG get
	inline const char* name() const
	{ return m_demod.name(); }
	inline double sample_rate() const
	{ return m_demod.sample_rate(); }
	inline RTL2832_NAMESPACE::range_t sample_rate_range() const
	{ return m_demod.sample_rate_range(); }
	inline double frequency() const
	{ return m_demod.active_tuner()->frequency(); }
	inline double gain() const
	{ return m_demod.active_tuner()->gain(); }
	inline double bandwidth() const
	{ return m_demod.active_tuner()->bandwidth(); }
	inline int gain_mode() const
	{ return m_demod.active_tuner()->gain_mode(); }
	std::string gain_mode_string() const;
	inline bool auto_gain_mode() const
	{ return m_demod.active_tuner()->auto_gain_mode(); }
public:	// SWIG get: tuner ranges/values
	inline RTL2832_NAMESPACE::range_t gain_range() const
	{ return m_demod.active_tuner()->gain_range(); }
	inline RTL2832_NAMESPACE::values_t gain_values() const
	{ return m_demod.active_tuner()->gain_values(); }
	inline RTL2832_NAMESPACE::range_t frequency_range() const
	{ return m_demod.active_tuner()->frequency_range(); }
	inline RTL2832_NAMESPACE::range_t bandwidth_range() const
	{ return m_demod.active_tuner()->bandwidth_range(); }
	inline RTL2832_NAMESPACE::values_t bandwidth_values() const
	{ return m_demod.active_tuner()->bandwidth_values(); }
	inline RTL2832_NAMESPACE::num_name_map_t gain_modes() const
	{ return m_demod.active_tuner()->gain_modes(); }
	inline std::pair<bool,int> calc_appropriate_gain_mode()/* const*/
	{ int i; bool b = m_demod.active_tuner()->calc_appropriate_gain_mode(i); return std::make_pair(b,i); }
public:	// gr::block overrides
	bool start();
	bool stop();
public:
	int general_work (int noutput_items,
		gr_vector_int &ninput_items,
		gr_vector_const_void_star &input_items,
		gr_vector_void_star &output_items);
};

#endif /* INCLUDED_BAZ_RTL_SOURCE_C_H */
