/* -*- c++ -*- */

%include "gnuradio.i"			// the common stuff

%{
//#include "howto_square_ff.h"
//#include "howto_square2_ff.h"

#include "baz_print_char.h"
#include "baz_unpacked_to_packed_bb.h"
#include "baz_pow_cc.h"
#include "baz_delay.h"
#include "baz_puncture_bb.h"
#include "baz_depuncture_ff.h"
#include "baz_swap_ff.h"
#include "baz_agc_cc.h"
#include "baz_test_counter_cc.h"
#include "baz_rtl_source_c.h"
%}

//%include "howto_square_ff.i"
//%include "howto_square2_ff.i"

///////////////////////////////////////////////////////////////////////////////

GR_SWIG_BLOCK_MAGIC(baz,rtl_source_c);

baz_rtl_source_c_sptr baz_make_rtl_source_c (bool auto_tuner_mode = false);

class baz_rtl_source_c : public gr_sync_block
{
private:
  baz_rtl_source_c (bool auto_tuner_mode = false);
public:
  bool set_sample_rate(int sample_rate);
  bool set_frequency(float freq);
  bool set_gain(float gain);
public:
  int sample_rate();
  float frequency();
  float gain();
};

///////////////////////////////////////////////////////////////////////////////

GR_SWIG_BLOCK_MAGIC(baz,print_char);

baz_print_char_sptr baz_make_print_char (float threshold = 0.0, int limit = -1, const char* file = NULL);

class baz_print_char : public gr_sync_block
{
private:
  baz_print_char (float threshold, int limit, const char* file);
};

///////////////////////////////////////////////////////////////////////////////

GR_SWIG_BLOCK_MAGIC(baz,unpacked_to_packed_bb);

baz_unpacked_to_packed_bb_sptr
baz_make_unpacked_to_packed_bb (unsigned int bits_per_chunk, unsigned int bits_into_output, /*gr_endianness_t*/int endianness = GR_MSB_FIRST);

class baz_unpacked_to_packed_bb : public gr_block
{
  baz_unpacked_to_packed_bb (unsigned int bits_per_chunk, unsigned int bits_into_output, /*gr_endianness_t*/int endianness);
};

///////////////////////////////////////////////////////////////////////////////

GR_SWIG_BLOCK_MAGIC(baz,pow_cc);

baz_pow_cc_sptr
baz_make_pow_cc (float exponent, float div_exp = 0.0);

class baz_pow_cc : public gr_sync_block
{
  baz_pow_cc (float exponent, float div_exp = 0.0);
public:
  void set_exponent(float exponent);
  void set_division_exponent(float div_exp);
  float exponent() const;
  float division_exponent() const;
};

///////////////////////////////////////////////////////////////////////////////

GR_SWIG_BLOCK_MAGIC(baz,delay)

baz_delay_sptr baz_make_delay (size_t itemsize, int delay);

class baz_delay : public gr_sync_block
{
 private:
  baz_delay (size_t itemsize, int delay);

 public:
  int  delay() const;
  void set_delay (int delay);
};

///////////////////////////////////////////////////////////////////////////////

GR_SWIG_BLOCK_MAGIC(baz,puncture_bb)

baz_puncture_bb_sptr baz_make_puncture_bb (const std::vector<int>& matrix);

class baz_puncture_bb : public gr_block
{
 private:
  baz_puncture_bb (const std::vector<int>& matrix);

 public:
  void set_matrix (const std::vector<int>& matrix);
};

///////////////////////////////////////////////////////////////////////////////

GR_SWIG_BLOCK_MAGIC(baz,depuncture_ff)

baz_depuncture_ff_sptr baz_make_depuncture_ff (const std::vector<int>& matrix);

class baz_depuncture_ff : public gr_block
{
 private:
  baz_depuncture_ff (const std::vector<int>& matrix);

 public:
  void set_matrix (const std::vector<int>& matrix);
};

///////////////////////////////////////////////////////////////////////////////

GR_SWIG_BLOCK_MAGIC(baz,swap_ff)

baz_swap_ff_sptr baz_make_swap_ff (bool bSwap);

class baz_swap_ff : public gr_sync_block
{
 private:
  baz_swap_ff (bool bSwap);

 public:
  void set_swap (bool bSwap);
};

///////////////////////////////////////////////////////////////////////////////

GR_SWIG_BLOCK_MAGIC(baz,agc_cc)

//%include <gri_agc_cc.i>

baz_agc_cc_sptr baz_make_agc_cc (float rate = 1e-4, float reference = 1.0, float gain = 1.0, float max_gain = 0.0);

class baz_agc_cc : public gr_sync_block//, public gri_agc_cc
{
  baz_agc_cc (float rate, float reference, float gain, float max_gain);
};

///////////////////////////////////////////////////////////////////////////////

GR_SWIG_BLOCK_MAGIC(baz,test_counter_cc);

baz_test_counter_cc_sptr baz_make_test_counter_cc ();

class baz_test_counter_cc : public gr_sync_block
{
  baz_test_counter_cc ();
};
