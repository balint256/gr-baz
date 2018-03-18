/* -*- c++ -*- */
/*
 * Copyright 2006,2010,2012 Free Software Foundation, Inc.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "baz_sdl_sink_uc.h"

#include <gnuradio/io_signature.h>
#include <gnuradio/thread/thread.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <iostream>
#include <stdexcept>
#include <string.h>

#include <SDL.h>

#ifdef SDL_TTF_FOUND
#include <SDL_ttf.h>
#else
typedef void* TTF_Font;
#endif // SDL_TTF_FOUND

/*  fourcc (four character code) */
#define vid_fourcc(a,b,c,d) (((unsigned)(a)<<0) | ((unsigned)(b)<<8) | ((unsigned)(c)<<16) | ((unsigned)(d)<<24))
#define IMGFMT_YV12  vid_fourcc('Y','V','1','2') /* 12  YVU 4:2:0 */

namespace gr {
  namespace baz {

    class sdl_sink_uc_impl : public sdl_sink_uc
    {
    private:
      int d_chunk_size;

    protected:
      void copy_line_pixel_interleaved(unsigned char *dst_pixels_u,unsigned char *dst_pixels_v,
                                       const unsigned char * src_pixels,int src_width);
      void copy_line_line_interleaved(unsigned char *dst_pixels_u,unsigned char *dst_pixels_v,
                                      const unsigned char * src_pixels,int src_width);
      void copy_line_single_plane(unsigned char *dst_pixels,const unsigned char * src_pixels,int src_width);
      void copy_line_single_plane_dec2(unsigned char *dst_pixels,const unsigned char * src_pixels,int src_width);
      int copy_plane_to_surface(int plane,int noutput_items,
                                const unsigned char * src_pixels);

      float d_framerate;
      int d_wanted_frametime_ms;
      int d_width;
      int d_height;
      int d_dst_width;
      int d_dst_height;
      int d_format;
      int d_current_line;
      SDL_Surface *d_screen, *d_rgb_image;
      SDL_Overlay *d_image;
      SDL_Rect d_dst_rect;
      float d_avg_delay;
      unsigned int d_wanted_ticks;
      std::string d_filename;
      size_t d_frame_counter;
      bool d_manual_flip;
      bool d_flip_pending;
      gr::thread::mutex d_mutex;
      TTF_Font *d_font;

    public:
      sdl_sink_uc_impl(double framerate,
                   int width, int height,
                   unsigned int format,
                   int dst_width, int dst_height, const std::string filename = "", bool manual_flip = false, const std::string font_path = "");
      ~sdl_sink_uc_impl();

      void flip(void);

      int work(int noutput_items,
               gr_vector_const_void_star &input_items,
               gr_vector_void_star &output_items);
    };


    sdl_sink_uc::sptr
    sdl_sink_uc::make(double framerate, int width, int height,
		  unsigned int format, int dst_width, int dst_height, const std::string filename, bool manual_flip/* = false*/, const std::string font_path/* = ""*/)
    {
      return gnuradio::get_initial_sptr
	(new sdl_sink_uc_impl(framerate, width, height, format, dst_width, dst_height, filename, manual_flip, font_path));
    }

    sdl_sink_uc_impl::sdl_sink_uc_impl(double framerate, int width, int height,
			       unsigned int format, int dst_width, int dst_height, const std::string filename, bool manual_flip/* = false*/, const std::string font_path/* = ""*/)
      : sync_block("baz_sdl_sink_uc",
		      io_signature::make(1, 3, sizeof(unsigned char)),
		      io_signature::make(0, 0, 0)),
	d_chunk_size(width*height), d_framerate(framerate),
	d_wanted_frametime_ms(0), d_width(width), d_height(height),
	d_dst_width(dst_width), d_dst_height(dst_height),
	d_format(format),
	d_current_line(0), d_screen(NULL), d_image(NULL),
	d_avg_delay(0.0), d_wanted_ticks(0),
  d_rgb_image(NULL),
  d_filename(filename),
  d_manual_flip(manual_flip),
  d_flip_pending(false),
  d_frame_counter(0),
  d_font(NULL)
    {
      if(framerate <= 0.0)
	d_wanted_frametime_ms = 0; //Go as fast as possible
      else
	d_wanted_frametime_ms = (int)(1000.0/framerate);

    bool rgb = (format == vid_fourcc('A', 'B', 'G', 'R'));

    if (rgb)
    {
      if (dst_width || dst_height)
      {
        std::cerr << "SDL: Force dest rect to be surface rect" << std::endl;
        d_dst_width = dst_width = width;
        d_dst_height = dst_height = height;
      }
    }

      if(dst_width < 0)
	d_dst_width = d_width;

      if(dst_height < 0)
	d_dst_height = d_height;

      if(0==format) // FIXME: Could determine RGB here instead of 'bool' flag
	d_format= IMGFMT_YV12;

      // FIXME: !
      //atexit(SDL_Quit); //check if this is the way to do this

      if(SDL_Init(SDL_INIT_VIDEO) < 0) {
	std::cerr << "baz::sdl_sink_uc: Couldn't initialize SDL:"
		  << SDL_GetError() << " \n SDL_Init(SDL_INIT_VIDEO) failed\n";
	throw std::runtime_error ("baz::sdl_sink_uc");
      }

      Uint32 flags = SDL_SWSURFACE | SDL_RESIZABLE | SDL_ANYFORMAT;
      if (rgb)
      {
        flags |= SDL_DOUBLEBUF;
        //flags |= SDL_HWSURFACE;
        //flags &= ~SDL_SWSURFACE;
      }
      /* accept any depth */
      d_screen = SDL_SetVideoMode(dst_width, dst_height, (/*rgb ? 32 : */0),
				  flags);//SDL_DOUBLEBUF|SDL_SWSURFACE|SDL_HWSURFACE|SDL_FULLSCREEN

      if(d_screen == NULL) {
	std::cerr << "Unable to set SDL video mode: " << SDL_GetError()
		  <<"\n SDL_SetVideoMode() Failed \n";
	throw std::runtime_error ("baz::sdl_sink_uc");
      }

      printf("SDL screen mode: %d bits-per-pixel, %d bytes-per-pixel, pitch: %d, flip: %s\n",
       d_screen->format->BitsPerPixel, d_screen->format->BytesPerPixel, d_screen->pitch, (d_manual_flip ? "manual" : "auto"));

      if (rgb) {
    Uint32 rmask, gmask, bmask, amask;  // FIXME: Decide on format (needs to match colouriser)
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
    rmask = 0xff000000;
    gmask = 0x00ff0000;
    bmask = 0x0000ff00;
    amask = 0x000000ff;
#else
    rmask = 0x000000ff;
    gmask = 0x0000ff00;
    bmask = 0x00ff0000;
    amask = 0xff000000;
#endif
        d_rgb_image = SDL_CreateRGBSurface(SDL_SWSURFACE, d_width, d_height, 32, rmask, gmask, bmask, amask);
        if (d_rgb_image == NULL)
        {
          std::cerr << "SDL: Couldn't create a RGB surface: \n"<< SDL_GetError() <<"\n";
          throw std::runtime_error("baz::sdl_sink_uc");
        }

        printf("SDL RGB surface mode: %d bits-per-pixel, %d bytes-per-pixel, pitch: %d\n",
       d_rgb_image->format->BitsPerPixel, d_rgb_image->format->BytesPerPixel, d_rgb_image->pitch);
      }
      else
      {
        /* Initialize and create the YUV Overlay used for video out */
        if(!(d_image = SDL_CreateYUVOverlay(d_width, d_height, SDL_YV12_OVERLAY, d_screen))) {
	std::cerr << "SDL: Couldn't create a YUV overlay: \n"<< SDL_GetError() <<"\n";
	throw std::runtime_error("baz::sdl_sink_uc");
      }
      else {
        printf("SDL overlay_mode %i \n",
       d_image->format);
      }
    }

      if (rgb)
      {
        int bytes_per_scanline = width * d_rgb_image->format->BytesPerPixel;  // Note for RGB: 24 bits still is 4 bytes!

        d_chunk_size = bytes_per_scanline;
        //d_chunk_size = bytes_per_scanline * height; // Load whole image
      }
      else
      {
        // FIXME: max?
        d_chunk_size = std::min(1,16384/width); //width*16;
        d_chunk_size = d_chunk_size*width;
        //d_chunk_size = (int)(width);
      }
      std::cerr << "SDL: Chunk size: " << d_chunk_size << std::endl;
      set_output_multiple(d_chunk_size);

      /* Set the default playback area */
      d_dst_rect.x = 0;
      d_dst_rect.y = 0;
      d_dst_rect.w = d_dst_width;
      d_dst_rect.h = d_dst_height;
      //clear the surface to grey

      std::cerr << "SDL dest rect: " << d_dst_rect.w << " x " << d_dst_rect.h << std::endl;

      if (d_image) {
        if(SDL_LockYUVOverlay(d_image)) {
  	std::cerr << "SDL: Couldn't lock YUV overlay: \n" << SDL_GetError() << "\n";
  	throw std::runtime_error ("baz::sdl_sink_uc");
        }

        memset(d_image->pixels[0], 128, d_image->pitches[0]*d_height);
        memset(d_image->pixels[1], 128, d_image->pitches[1]*d_height/2);
        memset(d_image->pixels[2], 128, d_image->pitches[2]*d_height/2);
        SDL_UnlockYUVOverlay(d_image);
      }
      else
      {
        // FIXME: Clear RGB
      }

      if (font_path.empty() == false)
      {
#ifdef SDL_TTF_FOUND
          //if (TTF_WasInit() == 0)
          {
            if (TTF_Init() != 0)
            {
              throw std::runtime_error("baz::sdl_sink_uc TTF_Init");
            }
          }

          d_font = TTF_OpenFont(font_path.c_str(), 10); // E.g. "/opt/local/share/wine/fonts/tahoma.ttf"
          if (d_font == NULL)
            throw std::runtime_error("baz::sdl_sink_uc TTF_Open");
#else
          std::cerr << "Cannot load font with SDL TTF support" << std::endl;
#endif
      }
    }

    sdl_sink_uc_impl::~sdl_sink_uc_impl()
    {
      std::cerr << "SDL: Exiting...";

      if (d_image)
      {
        SDL_FreeYUVOverlay(d_image);
      }
      if (d_rgb_image)
      {
        SDL_FreeSurface(d_rgb_image);
      }
      if (d_screen)
      {
        SDL_FreeSurface(d_screen);
      }
#ifdef SDL_TTF_FOUND
      if (d_font)
      {
        TTF_CloseFont(d_font);

        TTF_Quit();
      }
#endif // SDL_TTF_FOUND
      SDL_Quit();
    }

    void
    sdl_sink_uc_impl::copy_line_pixel_interleaved(unsigned char *dst_pixels_u,
					      unsigned char *dst_pixels_v,
					      const unsigned char *src_pixels,
					      int src_width)
    {
      for(int i=0;i<src_width;i++) {
	dst_pixels_u[i]=src_pixels[i*2];
	dst_pixels_v[i]=src_pixels[i*2+1];
      }
    }

    void
    sdl_sink_uc_impl::copy_line_line_interleaved(unsigned char *dst_pixels_u,
					     unsigned char *dst_pixels_v,
					     const unsigned char *src_pixels,
					     int src_width)
    {
      memcpy(dst_pixels_u, src_pixels, src_width);
      memcpy(dst_pixels_v, src_pixels+src_width, src_width);
    }

    void
    sdl_sink_uc_impl::copy_line_single_plane(unsigned char *dst_pixels,
					 const unsigned char *src_pixels,
					 int src_width)
    {
      memcpy(dst_pixels, src_pixels, src_width);
    }

    void
    sdl_sink_uc_impl::copy_line_single_plane_dec2(unsigned char *dst_pixels,
					      const unsigned char *src_pixels,
					      int src_width)
    {
      for(int i = 0,j = 0; i < src_width; i += 2, j++) {
	dst_pixels[j] = (unsigned char)src_pixels[i];
      }
    }

    int
    sdl_sink_uc_impl::copy_plane_to_surface(int plane,int noutput_items,
					const unsigned char * src_pixels)
    {
      const int first_dst_plane = (12==plane || 1122 == plane) ? 1 : plane;
      const int second_dst_plane = (12==plane || 1122 == plane) ? 2 : plane;
      int current_line = (0 == plane) ? d_current_line : d_current_line/2;

      unsigned char *dst_pixels = (unsigned char*)d_image->pixels[first_dst_plane];
      dst_pixels =& dst_pixels[current_line*d_image->pitches[first_dst_plane]];

      unsigned char * dst_pixels_2 = (unsigned char *)d_image->pixels[second_dst_plane];
      dst_pixels_2 =& dst_pixels_2[current_line*d_image->pitches[second_dst_plane]];

      int src_width = (0 == plane || 12 == plane || 1122 == plane) ? d_width : d_width/2;
      int noutput_items_produced = 0;
      int max_height = (0 == plane)? d_height-1 : d_height/2-1;

      for(int i = 0; i < noutput_items; i += src_width) {
        //output one line at a time
        if(12 == plane) {
          copy_line_pixel_interleaved(dst_pixels, dst_pixels_2, src_pixels, src_width);
          dst_pixels_2 += d_image->pitches[second_dst_plane];
        }
        else if(1122 == plane) {
          copy_line_line_interleaved(dst_pixels, dst_pixels_2, src_pixels, src_width);
          dst_pixels_2 += d_image->pitches[second_dst_plane];
          src_pixels += src_width;
        }
        else if(0 == plane)
          copy_line_single_plane(dst_pixels, src_pixels, src_width);
        else /* 1==plane || 2==plane*/
          copy_line_single_plane_dec2(dst_pixels, src_pixels, src_width); //decimate by two horizontally

        src_pixels += src_width;
        dst_pixels += d_image->pitches[first_dst_plane];
        noutput_items_produced+=src_width;
        current_line++;

        if(current_line > max_height) {
          //Start new frame
          //TODO, do this all in a seperate thread
          current_line = 0;
          dst_pixels = d_image->pixels[first_dst_plane];
          dst_pixels_2 = d_image->pixels[second_dst_plane];
          if(0 == plane) {
            if (d_image)
            {
              if (d_manual_flip == false)
              {
                SDL_DisplayYUVOverlay(d_image, &d_dst_rect);
              }
              else
              {
                gr::thread::scoped_lock guard(d_mutex);
                d_flip_pending = true;
              }
            }

            //SDL_Flip(d_screen);

            if (!d_filename.empty())
            {
              std::string filename;
              
              filename = boost::str(boost::format(d_filename) % d_frame_counter);

              if (SDL_SaveBMP(d_screen, filename.c_str()) != 0)
                std::cerr << "Failed to save image to: " << filename << " (" << SDL_GetError() << ")" << std::endl;
            }

            unsigned int ticks = SDL_GetTicks();//milliseconds
            d_wanted_ticks += d_wanted_frametime_ms;
            float avg_alpha = 0.1;
            int time_diff = d_wanted_ticks-ticks;
            d_avg_delay = time_diff*avg_alpha + d_avg_delay*(1.0-avg_alpha);

            ++d_frame_counter;
          }
        }
      }

      if(0==plane)
        d_current_line=current_line;

      return noutput_items_produced;
    }

    void sdl_sink_uc_impl::flip(void)
    {
      gr::thread::scoped_lock guard(d_mutex);

      if (d_flip_pending)
      {
        if (d_rgb_image)
          SDL_Flip(d_screen);
        else if (d_image)
          SDL_DisplayYUVOverlay(d_image, &d_dst_rect);

        d_flip_pending = false;
      }
    }

    int
    sdl_sink_uc_impl::work(int noutput_items,
		       gr_vector_const_void_star &input_items,
		       gr_vector_void_star &output_items)
    {
      unsigned char *src_pixels_0,*src_pixels_1,*src_pixels_2,*src_pixels_3;
      int noutput_items_produced = 0;
      int plane;
      int delay = (int)d_avg_delay;

      if(0 == d_wanted_ticks)
        d_wanted_ticks=SDL_GetTicks();

      if(delay > 0)
        SDL_Delay((unsigned int)delay);//compensate if running too fast

      if (d_image) {
        if(SDL_LockYUVOverlay(d_image)) {
          return 0;
        }
    }
    else
    {
      /*if (SDL_LockSurface(d_screen)) {
        std::cerr << "Failed to lock screen" << std::endl;
        return 0;
      }*/
    }

    if (d_rgb_image)
    {
      src_pixels_0 = (unsigned char *) input_items[0];

      if (SDL_LockSurface(d_rgb_image))  {
        std::cerr << "Failed to lock surface" << std::endl;
        return 0;
      }

      bool flip = false;

      int i = 0;
      /*int images = noutput_items / (d_screen->format->BytesPerPixel * d_width * d_height);
      if (images > 1)
      {
        i += ((d_height - d_current_line) * (d_screen->format->BytesPerPixel * d_width)); // Finish current
        i += ((images - 1) * (d_screen->format->BytesPerPixel * d_width * d_height));
      }*/

      for (; i < noutput_items; i += d_chunk_size) {  // [Chunk size should be one scanline]
        memcpy((char*)d_rgb_image->pixels + (d_rgb_image->pitch * d_current_line), src_pixels_0, d_chunk_size);
        src_pixels_0 += d_chunk_size;
        noutput_items_produced += d_chunk_size;
        d_current_line += (d_chunk_size / (d_screen->format->BytesPerPixel * d_width));

        if (d_current_line > d_height)
        {
          std::cerr << "Off image: " << d_current_line << std::endl;
          d_current_line = d_height;
        }

        if (d_current_line == d_height)
        {
          SDL_UnlockSurface(d_rgb_image);

          //printf("SDL: Frame done\n");
#ifdef SDL_TTF_FOUND
          if (d_font != NULL)
          {
            time_t time_now;
            char buffer[26];
            struct tm tm_info;

            time(&time_now);
            localtime_r(&time_now, &tm_info);

            strftime(buffer, 26, "%Y-%m-%d %H:%M:%S", &tm_info);

            SDL_Color textColor = { 0, 255, 0 };
            SDL_Surface* message = TTF_RenderText_Solid(d_font, buffer, textColor);
            if (message != NULL)
            {
              if (SDL_BlitSurface(message, NULL, d_rgb_image, /*NULL*/&d_dst_rect) != 0)
              {
                std::cerr << "Failed to blit message" << std::endl;
              }
              SDL_FreeSurface(message);
            }
            else
            {
              std::cerr << "Failed to render text" << std::endl;
            }
          }
#endif // SDL_TTF_FOUND
          flip = true;

          //SDL_UnlockSurface(d_rgb_image);

          //SDL_BlitSurface(d_rgb_image, NULL, d_screen, &d_dst_rect);
          //SDL_Flip(d_screen);

          if (!d_filename.empty())
          {
            std::string filename;
            
            filename = boost::str(boost::format(d_filename) % d_frame_counter);

            if (SDL_SaveBMP(d_rgb_image, filename.c_str()) != 0)
              std::cerr << "Failed to save image to: " << filename << " (" << SDL_GetError() << ")" << std::endl;
          }

          if (SDL_LockSurface(d_rgb_image)) {
            std::cerr << "Failed to lock surface" << std::endl;
            return 0;
          }

          d_current_line = 0;
          ++d_frame_counter;
        }
      }

      SDL_UnlockSurface(d_rgb_image);

      if (flip)
      {
        SDL_BlitSurface(d_rgb_image, NULL, d_screen, &d_dst_rect);

        if (d_manual_flip == false)
        {
          SDL_Flip(d_screen);
        }
        else
        {
          gr::thread::scoped_lock guard(d_mutex);
          d_flip_pending = true;
        }
      }
    }
    else if (d_image)
    {
      switch(input_items.size ()) {
      case 3:		// first channel=Y, second channel is  U , third channel is V
	src_pixels_0 = (unsigned char *) input_items[0];
	src_pixels_1 = (unsigned char *) input_items[1];
	src_pixels_2 = (unsigned char *) input_items[2];
	for(int i = 0; i < noutput_items; i += d_chunk_size) {
	  copy_plane_to_surface (1,d_chunk_size, src_pixels_1);
	  copy_plane_to_surface (2,d_chunk_size, src_pixels_2);
	  noutput_items_produced+=copy_plane_to_surface(0,d_chunk_size, src_pixels_0);
	  src_pixels_0 += d_chunk_size;
	  src_pixels_1 += d_chunk_size;
	  src_pixels_2 += d_chunk_size;
	}
	break;

      case 2:
	if(1) {//if(pixel_interleaved_uv)
	  // first channel=Y, second channel is alternating pixels U and V
	  src_pixels_0 = (unsigned char *) input_items[0];
	  src_pixels_1 = (unsigned char *) input_items[1];
	  for(int i = 0; i < noutput_items; i += d_chunk_size) {
	    copy_plane_to_surface(12, d_chunk_size/2, src_pixels_1);
	    noutput_items_produced += copy_plane_to_surface(0, d_chunk_size, src_pixels_0);
	    src_pixels_0 += d_chunk_size;
	    src_pixels_1 += d_chunk_size;
	  }
	}
	else {
	  // first channel=Y, second channel is alternating lines U and V
	  src_pixels_0 = (unsigned char*)input_items[0];
	  src_pixels_1 = (unsigned char*)input_items[1];
	  for(int i = 0; i < noutput_items; i += d_chunk_size) {
	    copy_plane_to_surface (1222,d_chunk_size/2, src_pixels_1);
	    noutput_items_produced += copy_plane_to_surface(0, d_chunk_size, src_pixels_0);
	    src_pixels_0 += d_chunk_size;
	    src_pixels_1 += d_chunk_size;
	  }
	}
	break;

      case 1:		// grey (Y) input
	/* Y component */
	plane=0;
	src_pixels_0 = (unsigned char*)input_items[plane];
	for(int i = 0; i < noutput_items; i += d_chunk_size) {
	  noutput_items_produced += copy_plane_to_surface(plane, d_chunk_size, src_pixels_0);
	  src_pixels_0 += d_chunk_size;
	}
	break;

      default: //0 or more then 3 channels
	std::cerr << "baz::sdl_sink_uc: Wrong number of channels: ";
	std::cerr <<"1, 2 or 3 channels are supported.\n  Requested number of channels is "
		  << input_items.size () <<"\n";
	throw std::runtime_error("baz::sdl_sink_uc");
      }
    }

      if (d_image)
      {
        SDL_UnlockYUVOverlay(d_image);
      }
      else
      {
        //SDL_UnlockSurface(d_screen);
        //SDL_Flip(d_screen);
      }
      return noutput_items_produced;
    }

  } /* namespace baz */
} /* namespace gr */
