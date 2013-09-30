/* -*- c++ -*- */
/*
 * Copyright 2006,2010,2013 Free Software Foundation, Inc.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <baz_agc_cc.h>
#include <gnuradio/io_signature.h>
//#include <gri_agc_cc.h>
#include <stdio.h>
#include <math.h>
#include <boost/math/special_functions/fpclassify.hpp>

#if defined(_WIN32) || defined(__WIN32__) || defined(WIN32)
#include <float.h>
#define finite _finite
#endif

baz_agc_cc_sptr
baz_make_agc_cc (float rate, float reference, float gain, float max_gain)
{
  return gnuradio::get_initial_sptr(new baz_agc_cc (rate, reference, gain, max_gain));
}

baz_agc_cc::baz_agc_cc (float rate, float reference, float gain, float max_gain)
  : gr::sync_block ("gr_agc_cc",
		   gr::io_signature::make (1, 1, sizeof (gr_complex)),
		   gr::io_signature::make2 (1, 3, sizeof (gr_complex), sizeof(float)))
  , _rate(rate)
  , _reference(reference)
  , _gain(gain)
  , _max_gain(max_gain)
  , _count(0)
  , _env(0.0)
{
  //_reference = log10(reference);
}

int baz_agc_cc::work (int noutput_items, gr_vector_const_void_star &input_items, gr_vector_void_star &output_items)
{
  const gr_complex *in = (const gr_complex *) input_items[0];
  gr_complex *out = (gr_complex *) output_items[0];
  float* env = (output_items.size() >= 2 ? (float*)output_items[1] : NULL);
  float* mul = (output_items.size() >= 3 ? (float*)output_items[2] : NULL);

  double d[2];
  for (unsigned i = 0; i < noutput_items; i++, _count++) {
    //gr_complex output = in[i];
    d[0] = in[i].real();
	d[1] = in[i].imag();
    double mag2 = d[0]*d[0] + d[1]*d[1];
    double mag = sqrt(mag2);
    
    if (_count == 0)
      _env = mag;
    else
      _env = (_env * (1.0 - _rate)) + (mag * _rate);
    
    if (env)
      env[i] = _env;
    
    //double env10 = log10(_env);
    //double diff = _reference - env10;
    _gain = _reference / _env;
    
    if (mul)
      mul[i] = _gain;
    
	//gr_complex output = in[i] * _gain;
	//d[0] = in[i].real() * _gain;
	//d[1] = in[i].imag() * _gain;
    d[0] *= _gain;
    d[1] *= _gain;
    
    out[i] = /*output*/gr_complex(d[0], d[1]);
    
    continue;
///////////////////////////////////////////////////////////////////////////////
    if (!finite(mag2) || boost::math::isnan(mag2) || boost::math::isinf(mag2)) {
	  if (_gain == _max_gain) {
fprintf(stderr, "[%05i] + %f,%f -> %f,%f (%f) %f %f\n", i, in[i].real(), in[i].imag(), /*output.real()*/d[0], /*output.imag()*/d[1], _gain, _reference, _rate);
		out[i] = /*in[i]*//*0*/gr_complex(0, 0);
		continue;
	  }
	  else {
//fprintf(stderr, "gO");
fprintf(stderr, "[%05i]   %f,%f -> %f,%f (%f) %f %f\n", i, in[i].real(), in[i].imag(), /*output.real()*/d[0], /*output.imag()*/d[1], _gain, _reference, _rate);
		_gain = _max_gain;
		i--;
		continue;
	  }
	}
	else {
	  double diff = (double)_reference - sqrt(mag);
	  //if (diff < 0.0)
      //  fprintf(stderr, "[%05i] D {%f} %f,%f -> %f,%f (%f) %f %f\n", i, diff, in[i].real(), in[i].imag(), /*output.real()*/d[0], /*output.imag()*/d[1], _gain, _reference, _rate);
	  _gain += (double)_rate * diff;
	  if (_max_gain > 0.0 && _gain > (double)_max_gain)
	    _gain = _max_gain;
	  else if (_gain <= 0.0) {
		fprintf(stderr, "[%05i] - {%f} %f,%f -> %f,%f (%f) %f %f\n", i, diff, in[i].real(), in[i].imag(), /*output.real()*/d[0], /*output.imag()*/d[1], _gain, _reference, _rate);
		_gain = 1e-9;
	  }
//	  else if (_gain < -1e3) {
//fprintf(stderr, "[%05i] - %f,%f -> %f,%f (%f) %f %f\n", i, in[i].real(), in[i].imag(), /*output.real()*/d[0], /*output.imag()*/d[1], _gain, _reference, _rate);
//		_gain = -1e3;
//	  }
	  out[i] = /*output*/gr_complex(d[0], d[1]);
	}
	  
	  /*float f = out[i].real() * out[i].imag();
	  if (!finite(out[i].real()) || !finite(out[i].imag()) || !finite(f) ||
		  std::isnan(out[i].real()) || std::isnan(out[i].imag()) || std::isnan(f) ||
		  std::isinf(out[i].real()) || std::isinf(out[i].imag()) || std::isinf(f)) {
		fprintf(stderr, "%f,%f %f\n", out[i].real(), out[i].imag(), _gain);
		out[i] = 0.0f;
	  }*/
  }
  
  return noutput_items;
}
