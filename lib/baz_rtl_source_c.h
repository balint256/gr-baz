/* -*- c++ -*- */
/*
 * Copyright 2004 Free Software Foundation, Inc.
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
 * This block uses source from rtl-sdr: http://sdr.osmocom.org/trac/wiki/rtl-sdr
 */

#ifndef INCLUDED_BAZ_RTL_SOURCE_C_H
#define INCLUDED_BAZ_RTL_SOURCE_C_H

#include <gr_block.h>
#include <gruel/thread.h>
#include <boost/thread/condition.hpp>

#include <libusb-1.0/libusb.h>	// FIXME: Automake

class baz_rtl_source_c;

/*
 * We use boost::shared_ptr's instead of raw pointers for all access
 * to gr_blocks (and many other data structures).  The shared_ptr gets
 * us transparent reference counting, which greatly simplifies storage
 * management issues.  This is especially helpful in our hybrid
 * C++ / Python system.
 *
 * See http://www.boost.org/libs/smart_ptr/smart_ptr.htm
 *
 * As a convention, the _sptr suffix indicates a boost::shared_ptr
 */
typedef boost::shared_ptr<baz_rtl_source_c> baz_rtl_source_c_sptr;

/*!
 * \brief Return a shared_ptr to a new instance of howto_square_ff.
 *
 * To avoid accidental use of raw pointers, howto_square_ff's
 * constructor is private.  howto_make_square_ff is the public
 * interface for creating new instances.
 */
baz_rtl_source_c_sptr baz_make_rtl_source_c (bool auto_tuner_mode = false);

/*!
 * \brief square a stream of floats.
 * \ingroup block
 *
 * \sa howto_square2_ff for a version that subclasses gr_sync_block.
 */
class baz_rtl_source_c : public gr_block
{
private:
  // The friend declaration allows howto_make_square_ff to
  // access the private constructor.

  friend baz_rtl_source_c_sptr baz_make_rtl_source_c (bool auto_tuner_mode);

  baz_rtl_source_c (bool auto_tuner_mode = false);  	// private constructor

 public:
  ~baz_rtl_source_c ();	// public destructor

private:
	enum TUNER_TYPE {
		TUNER_UNKNOWN,
		TUNER_E4000,
		TUNER_FC0013
	} tuner_type;
	struct libusb_device_handle *devh;
	bool m_auto_tuner_mode;
	bool m_libusb_init_done;
public:	// rtl-sdr
	int find_device();
	bool tuner_init(int frequency);
	void rtl_init();
	void set_i2c_repeater(int on);
	bool set_samp_rate(uint32_t samp_rate);
	void demod_write_reg(uint8_t page, uint16_t addr, uint16_t val, uint8_t len);
	uint16_t demod_read_reg(uint8_t page, uint8_t addr, uint8_t len);
	void rtl_write_reg(uint8_t block, uint16_t addr, uint16_t val, uint8_t len);
	uint16_t rtl_read_reg(uint8_t block, uint16_t addr, uint8_t len);
	int rtl_i2c_read(uint8_t i2c_addr, uint8_t *buffer, int len);
	int rtl_i2c_write(uint8_t i2c_addr, uint8_t *buffer, int len);
	int rtl_write_array(uint8_t block, uint16_t addr, uint8_t *array, uint8_t len);
	int rtl_read_array(uint8_t block, uint16_t addr, uint8_t *array, uint8_t len);
public:	// COMPAT
	size_t m_recv_samples_per_packet;
	uint64_t m_nSamplesReceived;
	uint32_t m_nOverflows;
	bool m_bRunning;
public:
	gruel::mutex d_mutex;
	gruel::thread m_pCaptureThread;
	uint32_t m_nBufferSize;
	uint32_t m_nBufferStart;
	uint32_t m_nBufferItems;
	boost::condition m_hPacketEvent;
	uint8_t* m_pUSBBuffer;
	bool m_bBuffering;
	int m_iTunerGainMode;
public:	// COMPAT
	bool Create(/*const char* strHint = NULL*/);
	bool Start();
	void Stop();
	void Destroy();
	void Reset();
	void CaptureThreadProc();
	int SetTunerMode();
public:	// COMPAT
	double m_dFreq;
	double m_dGain;
	double m_dSampleRate;
public:	// SWIG set
  bool set_sample_rate(int sample_rate);
  bool set_frequency(float freq);
  bool set_gain(float gain);
public:	// SWIG get
  inline int sample_rate()
  { return (int)m_dSampleRate; }
  inline float frequency()
  { return m_dFreq; }
  inline float gain()
  { return m_dGain; }
public:	// COMPAT
  inline float GetFreq()
  { return frequency(); }
public:	// gr_block overrides
	bool start()
	{ return Start(); }
	bool stop()
	{ Stop(); return true; }
public:
  int general_work (int noutput_items,
		    gr_vector_int &ninput_items,
		    gr_vector_const_void_star &input_items,
		    gr_vector_void_star &output_items);
};

#endif /* INCLUDED_BAZ_RTL_SOURCE_C_H */
