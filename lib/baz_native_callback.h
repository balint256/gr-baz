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

#ifndef INCLUDED_BAZ_NATIVE_CALLBACK_X_H
#define INCLUDED_BAZ_NATIVE_CALLBACK_X_H

#include <gnuradio/sync_block.h>

class BAZ_API baz_native_callback_x;

/*
 * We use boost::shared_ptr's instead of raw pointers for all access
 * to gr::blocks (and many other data structures).  The shared_ptr gets
 * us transparent reference counting, which greatly simplifies storage
 * management issues.  This is especially helpful in our hybrid
 * C++ / Python system.
 *
 * See http://www.boost.org/libs/smart_ptr/smart_ptr.htm
 *
 * As a convention, the _sptr suffix indicates a boost::shared_ptr
 */
typedef boost::shared_ptr<baz_native_callback_x> baz_native_callback_x_sptr;

class BAZ_API baz_native_callback_target
{
public:
  virtual void callback(float f, unsigned long samples_processed)=0; // FIXME: Item size
};

//typedef boost::shared_ptr<baz_native_callback_target> baz_native_callback_target_sptr;
#define baz_native_callback_target_sptr gr::basic_block_sptr

/*!
 * \brief Return a shared_ptr to a new instance of baz_native_callback_x.
 *
 * To avoid accidental use of raw pointers, baz_native_callback_x's
 * constructor is private.  howto_make_square2_ff is the public
 * interface for creating new instances.
 */
BAZ_API baz_native_callback_x_sptr baz_make_native_callback_x (int size, baz_native_callback_target_sptr target, bool threshold_enable=false, float threshold_level=0.0);

/*!
 * \brief square2 a stream of floats.
 * \ingroup block
 *
 * This uses the preferred technique: subclassing gr::sync_block.
 */
class BAZ_API baz_native_callback_x : public gr::sync_block
{
private:
  // The friend declaration allows howto_make_square2_ff to
  // access the private constructor.

  friend baz_native_callback_x_sptr baz_make_native_callback_x (int size, baz_native_callback_target_sptr target, bool threshold_enable, float threshold_level);

  baz_native_callback_x (int size, baz_native_callback_target_sptr target, bool threshold_enable, float threshold_level);  	// private constructor

  int d_size;
  baz_native_callback_target_sptr d_target;
  bool d_threshold_enable;
  float d_threshold_level;

  bool d_triggered;
  unsigned long d_samples_processed;

 public:
  ~baz_native_callback_x ();	// public destructor

  void set_size(int size);
  void set_target(baz_native_callback_target_sptr target);
  void set_threshold_enable(bool enable);
  void set_threshold_level(float threshold_level);

  inline int size() const
  { return d_size; }
  // Target
  inline bool threshold_enable() const
  { return d_threshold_enable; }
  inline float threshold_level() const
  { return d_threshold_level; }

  int work (int noutput_items,
	    gr_vector_const_void_star &input_items,
	    gr_vector_void_star &output_items);
};

#endif /* INCLUDED_BAZ_NATIVE_CALLBACK_X_H */
