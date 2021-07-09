/*****************************************************************************
 * libVVCDecoder_plugin.cpp: h.266/vvc video decoder wrapper
 *****************************************************************************
 * Copyright (C) 2021 interdigital
 *
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

 /*****************************************************************************
  * Preamble
  *****************************************************************************/
#if defined(_MSC_VER)
  #define NOMINMAX
  #include <basetsd.h>
  typedef SSIZE_T ssize_t;
  #define strcasecmp(s1, s2) _stricmp(s1, s2)
  #include <windows.h>
  #include <delayimp.h>
#else
 #include <thread>
#endif

#include <algorithm>
#include <memory>
#include "DecVTMLib.h"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_codec.h>
#include <vlc_dialog.h>

#define N_(str) (str)

  /*****************************************************************************
   * decoder_sys_t : raw video decoder descriptor
   *****************************************************************************/
struct decoder_sys_t
{
  /*
   * Common properties
   */
  date_t pts;

  DecVTMInstance* decVtm = nullptr;

  bool b_first_frame;
  bool b_format_init;
  bool b_frameRateDetect;
  bool enable_hurryMode;
  mtime_t firstBlock_dts; 
  bool firstBlock;
  mtime_t lastOutput_pts; 
  mtime_t firstOutput_pts; 
  mtime_t firstOutput_time; 
  mtime_t lastOutput_time; 
  int speedUpLevel;
  int speedUpLevel_delai_increase;
  mtime_t dts1;
  mtime_t diff_dts;
  mtime_t dts2;
  size_t dec_frame_count;
  size_t out_frame_count;
};

/****************************************************************************
 * Local prototypes
 ****************************************************************************/
static int  OpenDecoder(vlc_object_t*);
static void CloseDec(vlc_object_t*);
static int DecodeFrame(decoder_t* p_dec, block_t* p_block);
static void FillPicture(decoder_t* p_dec, picture_t* p_pic, short* planes[3], int strides[3]);
static void Flush(decoder_t* p_dec);
static bool getOutputFrame(decoder_t* p_dec, bool waitUntilReady, mtime_t i_dts);
static int initVideoFormat(decoder_t* p_dec, decoder_sys_t* p_sys,
  vlc_fourcc_t videoFormat = VLC_CODEC_I420_10L,
  unsigned int frame_width = 0, unsigned int frame_height = 0);
static int initVideoFrameRate(decoder_t* p_dec, decoder_sys_t* p_sys,
  unsigned int i_frame_rate = 0, unsigned int i_frame_rate_base = 0);

namespace VvcDecoder
{
  int  OpenPack(vlc_object_t*);
  void ClosePack(vlc_object_t*);

  int  OpenDemux(vlc_object_t*);
  void CloseDemux(vlc_object_t*);

  block_t* PacketizeVVC(decoder_t* p_dec, block_t** pp_block);
}

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin()
set_shortname(N_("vvcdec"))
set_description(N_("VVC decoder"))
set_capability("video decoder", 50)
set_category(CAT_INPUT)
set_subcategory(SUBCAT_INPUT_VCODEC)
set_callbacks(OpenDecoder, CloseDec)
add_integer("nb-threads", 0, N_("Number of threads for decoding"), N_("number of threads for decoding in the range [1-32]; 0: automatic detection of cores "), false)
add_integer("nb-threads-parsing", -1, N_("Maximum number of threads for CABAC parsing"), N_("Maximum number of threads for CABAC parsing (from same pool as decoding threads) [1-32]; -1: auto; 0: sequantial parsing and decoding"), false)
add_integer("target-layer-set", -1, N_("Target output layer set"), N_("Target output layer set (for multi-layer streams)"), false)
add_bool("vvc-enable-hurry-mode", true, N_("Enable hurry-up mode"), N_("hurry-up mode: skip decoding pictures if late"), false)

add_submodule()
set_description(N_("VVC binary demuxer"))
set_capability("demux", 100)
set_category(CAT_INPUT)
set_subcategory(SUBCAT_INPUT_DEMUX)
set_callbacks(VvcDecoder::OpenDemux, VvcDecoder::CloseDemux)
add_float("vvc-fps", 0, N_("Frames per second"), N_("Frames per Second; 0: try automatic, default 50Hz"), false)

add_submodule()
set_category(CAT_SOUT)
set_subcategory(SUBCAT_SOUT_PACKETIZER)
set_description(N_("VVC/H.266 video packetizer"))
set_capability("packetizer", 50)
set_callbacks(VvcDecoder::OpenPack, VvcDecoder::ClosePack)

vlc_module_end()

/**
 * initialization for decoder
 */
static int OpenDecoder(vlc_object_t * p_this)
{
  decoder_t* p_dec = (decoder_t*)p_this;
 
  // Allocate the memory needed to store the decoder's structure 
  decoder_sys_t* p_sys = (decoder_sys_t * )calloc(1, sizeof(*p_sys));
  if (unlikely(p_sys == NULL))
    return VLC_ENOMEM;

  ////////////
  char psz_threadsvar[30];
  int nbThreads = 0, nbThreadsForParsing = -1;
  if (sprintf(psz_threadsvar, "nb-threads"))
  {
    nbThreads = std::max(nbThreads, (int)var_CreateGetInteger(p_dec, psz_threadsvar));
  }

  if(nbThreads <= 0)
  {
#if _WIN32
    int processor_count = 1;
    {
      DWORD length = 0;
      const BOOL result_first = GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &length);
      assert(GetLastError() == ERROR_INSUFFICIENT_BUFFER);

      std::unique_ptr< uint8_t[] > buffer(new uint8_t[length]);
      const PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX info =
        reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buffer.get());

      const BOOL result_second = GetLogicalProcessorInformationEx(RelationProcessorCore, info, &length);
      assert(result_second != FALSE);

      processor_count = 0;
      size_t offset = 0;
      do {
        const PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX current_info =
          reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buffer.get() + offset);
        offset += current_info->Size;
        ++processor_count;
      } while (offset < length);
    }

#else
    const auto processor_count = std::thread::hardware_concurrency();
#endif
    msg_Info(p_dec, "found %d cores", processor_count);
    nbThreads = std::max(1, (int)processor_count);
  }
  if (sprintf(psz_threadsvar, "nb-threads-parsing"))
  {
    nbThreadsForParsing = std::max(nbThreadsForParsing, (int)var_CreateGetInteger(p_dec, psz_threadsvar));
  }
  if(nbThreadsForParsing <= -1)
  {
    nbThreadsForParsing = std::max(1, nbThreads / 2);
    msg_Info(p_dec, "use %d threads for parsing", nbThreadsForParsing);
  }

  char psz_hurryvar[30];
  p_sys->enable_hurryMode = true;
  if (sprintf(psz_hurryvar, "vvc-enable-hurry-mode"))
  {
    p_sys->enable_hurryMode = var_CreateGetBool(p_dec, psz_hurryvar);
  }

  char psz_targetLayer[30];
  int targetLayerSet = -1;
  if (sprintf(psz_targetLayer, "target-layer-set"))
  {
    targetLayerSet = var_CreateGetInteger(p_dec, psz_targetLayer);
  }

#ifdef WIN32
  SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
  char* baseName = strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__);
  HRESULT loadLibRes = __HrLoadAllImportsForDll(VTM_LIB_NAME);
  if (loadLibRes != S_OK)
  {
    msg_Info(p_dec, "%s: could not load library %s on default path, try plugin directory", baseName, VTM_LIB_NAME);
    char currentDllDir[MAX_PATH];
    HMODULE hm = NULL;

    if (GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
      GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
      (LPCSTR) & __FUNCTION__, &hm) == 0)
    {
      int ret = GetLastError();
      msg_Info(p_dec, "GetModuleHandle failed, error = %d\n", ret);
      return VLC_EGENERIC;
    }
    if (GetModuleFileName(hm, currentDllDir, sizeof(currentDllDir)) == 0)
    {
      int ret = GetLastError();
      msg_Info(p_dec, "GetModuleFileName failed, error = %d\n", ret);
      return VLC_EGENERIC;
    }
    char *lastOfDir = std::max(strrchr(currentDllDir, '/'), strrchr(currentDllDir, '\\'));
    if (lastOfDir)
    {
      *lastOfDir = '\0';
    }
    msg_Info(p_dec, "found directory %s\n", currentDllDir);

    char dllPath[MAX_PATH];
    dllPath[0] = '\0';
    strcat_s(dllPath, currentDllDir);
    dllPath[strlen(dllPath)+1] = '\0';
    dllPath[strlen(dllPath)] = '/';
    strcat_s(dllPath, VTM_LIB_NAME);


    msg_Info(p_dec, "try to load dll %s", dllPath);
    HMODULE loaded = LoadLibrary(dllPath);

    if (!loaded)
    {
      msg_Info(p_dec, "%s: could not load library %s, in plugin directory %s", baseName, VTM_LIB_NAME, currentDllDir);
      vlc_dialog_display_error(p_dec, "could not load dll",
        "VLC could not decode library %s for codec plugin %s (tried to load from directory %s)",
        VTM_LIB_NAME, baseName, currentDllDir);
      return VLC_EGENERIC;
    }
  }
  msg_Info(p_dec, "%s: successfully loaded library %s", baseName, VTM_LIB_NAME);
#endif

  msg_Info(p_dec, "using decoder with cfg --nbThreads=%d --nbThreadsForParsing=%d", nbThreads, nbThreadsForParsing);
  // create & initialize internal classes
  if (!(p_sys->decVtm = decVTM_create(nbThreads, nbThreadsForParsing, targetLayerSet)))
  {
    return VLC_EGENERIC;
  }

  p_sys->b_first_frame = true;
  p_sys->lastOutput_pts = VLC_TS_INVALID;
  p_sys->firstOutput_pts = VLC_TS_INVALID;
  p_sys->firstOutput_time = VLC_TS_INVALID;
  p_sys->lastOutput_time = VLC_TS_INVALID;
  p_sys->firstBlock_dts = VLC_TS_INVALID;
  p_sys->firstBlock = true;
  p_sys->b_format_init = true;
  p_sys->b_frameRateDetect = false;
  p_sys->dec_frame_count = 0;
  p_sys->out_frame_count = 0;
  p_sys->speedUpLevel = 0;
  p_sys->speedUpLevel_delai_increase = 0;

  /////////////////////////////

  if (initVideoFrameRate(p_dec, p_sys) != VLC_SUCCESS)
  {
    return VLC_EGENERIC;
  }
  p_dec->p_sys = p_sys;
  p_dec->pf_decode = DecodeFrame;
  p_dec->pf_flush = Flush;
  p_dec->i_extra_picture_buffers = 32;

  return VLC_SUCCESS;
}

static int initVideoFormat(decoder_t* p_dec, decoder_sys_t* p_sys,
  vlc_fourcc_t videoFormat,
  unsigned int frame_width, unsigned int frame_height)
{
  const vlc_chroma_description_t* dsc =
    vlc_fourcc_GetChromaDescription(videoFormat);
  if (dsc == NULL || dsc->plane_count == 0)
    return VLC_EGENERIC;

  if (!p_dec->fmt_in.video.i_visible_width)
    p_dec->fmt_in.video.i_visible_width = p_dec->fmt_in.video.i_width;
  if (!p_dec->fmt_in.video.i_visible_height)
    p_dec->fmt_in.video.i_visible_height = p_dec->fmt_in.video.i_height;

  es_format_Init(&p_dec->fmt_out, VIDEO_ES, videoFormat);

  if (frame_width == 0 || frame_height == 0)
  {
    frame_width = p_dec->fmt_in.video.i_width;
    frame_height = p_dec->fmt_in.video.i_height;
  }

  video_format_Setup(&p_dec->fmt_out.video, videoFormat,
    frame_width, frame_height, frame_width, frame_height, 1, 1);

  return VLC_SUCCESS;
}

static int initVideoFrameRate(decoder_t* p_dec, decoder_sys_t* p_sys,
  unsigned int i_frame_rate, unsigned int i_frame_rate_base)
{
  decVTM_getFrameRate(p_sys->decVtm, &i_frame_rate, &i_frame_rate_base);

  if (i_frame_rate == 0 || i_frame_rate_base == 0)
  {
    if (p_dec->fmt_out.video.i_frame_rate != 0 &&
      p_dec->fmt_out.video.i_frame_rate_base != 0)
    {
      return VLC_SUCCESS;
    }
    i_frame_rate = p_dec->fmt_in.video.i_frame_rate;
    i_frame_rate_base = p_dec->fmt_in.video.i_frame_rate_base;
  }
  p_dec->fmt_out.video.i_frame_rate = i_frame_rate;
  p_dec->fmt_out.video.i_frame_rate_base = i_frame_rate_base;

  if (!p_sys->b_frameRateDetect)
  {
    if (p_dec->fmt_out.video.i_frame_rate == 0 ||
      p_dec->fmt_out.video.i_frame_rate_base == 0)
    {
      msg_Warn(p_dec, "invalid frame rate %d/%d, using 50 fps while detecting",
        p_dec->fmt_out.video.i_frame_rate,
        p_dec->fmt_out.video.i_frame_rate_base);
      date_Init(&p_sys->pts, 50, 1);
      p_sys->b_frameRateDetect = true;
      p_sys->dts1 = VLC_TS_INVALID;
      p_sys->dts2 = VLC_TS_INVALID;
    }
    else
      date_Init(&p_sys->pts, p_dec->fmt_out.video.i_frame_rate,
        p_dec->fmt_out.video.i_frame_rate_base);
  }
  return VLC_SUCCESS;
}

/*****************************************************************************
 * Flush:
 *****************************************************************************/
static void Flush(decoder_t* p_dec)
{
  decoder_sys_t* p_sys = p_dec->p_sys;

  msg_Warn(p_dec, "decoder flush called at pts: %d ", date_Get(&p_sys->pts));
  //date_Set(&p_sys->pts, VLC_TS_INVALID);
 
  //getOutputFrame(p_dec, nullptr);
}

/*****************************************************************************
 * FillPicture:
 *****************************************************************************/
static void FillPicture(decoder_t* p_dec, picture_t* p_pic, short* planes[3], int strides[3])
{
  decoder_sys_t* p_sys = p_dec->p_sys;
  for (int i = 0; i < p_pic->i_planes; i++)
  {
    if (planes[i])
    {
      uint8_t* p_dstPlane = p_pic->p[i].p_pixels;
      short* p_src = planes[i];

      for (int y = 0; y < p_pic->p[i].i_visible_lines; y++)
      {
        if (p_pic->p[i].i_pixel_pitch == 1)
        {
          for (int x = 0; x < p_pic->p[i].i_visible_pitch; x++)
          {
            p_dstPlane[x] = (uint8_t)p_src[x];
          }
        }
        else
        {
          memcpy(p_dstPlane, p_src, p_pic->p[i].i_visible_pitch);
        }
        p_dstPlane += p_pic->p[i].i_pitch;
        p_src += strides[i];
      }
    }
    else if (i > 0 && p_pic->p[i].p_pixels)
    {
      uint8_t* p_dstPlane = p_pic->p[i].p_pixels;

      for (int y = 0; y < p_pic->p[i].i_visible_lines; y++)
      {
        short* dst = (short*)p_dstPlane;
        if (p_dec->fmt_out.video.i_chroma == VLC_CODEC_I420_10L)
        {
          for (int x = 0; x < p_pic->p[i].i_visible_pitch; x++)
          {
            dst[x] = 1 << (10 - 1);
          }
        }
        else if (p_dec->fmt_out.video.i_chroma == VLC_CODEC_I420_12L)
        {
          for (int x = 0; x < p_pic->p[i].i_visible_pitch; x++)
          {
            dst[x] = 1 << (12 - 1);
          }
        }
        p_dstPlane += p_pic->p[i].i_pitch;
      }
    }
  }
}


static vlc_fourcc_t getVideoFormat(decoder_t* p_dec, int chromaFormat, int bitDepths)
{
  vlc_fourcc_t videoFormat = p_dec->fmt_out.video.i_chroma; 
  switch (chromaFormat)
  {
  case 400:
    if (bitDepths == 8)
    {
      videoFormat = VLC_CODEC_GREY;
    }
    else if (bitDepths == 10)
    {
      videoFormat = VLC_CODEC_I420_10L;
    }
    else if (bitDepths == 12)
    {
      videoFormat = VLC_CODEC_I420_12L;
    }
    break;
  case 420:
    if (bitDepths == 8)
    {
      videoFormat = VLC_CODEC_I420;
    }
    else if (bitDepths == 10)
    {
      videoFormat = VLC_CODEC_I420_10L;
    }
    else if (bitDepths == 12)
    {
      videoFormat = VLC_CODEC_I420_12L;
    }
    break;
  case 422:
    if (bitDepths == 8)
    {
      videoFormat = VLC_CODEC_I422;
    }
    else if (bitDepths == 10)
    {
      videoFormat = VLC_CODEC_I422_10L;
    }
    else if (bitDepths == 12)
    {
      videoFormat = VLC_CODEC_I422_12L;
    }
    break;
  case 444:
    if (bitDepths == 8)
    {
      videoFormat = VLC_CODEC_I444;
    }
    else if (bitDepths == 10)
    {
      videoFormat = VLC_CODEC_I444_10L;
    }
    else if (bitDepths == 12)
    {
      videoFormat = VLC_CODEC_I444_12L;
    }
    break;
  }
  return videoFormat;
}

static vlc_fourcc_t getVideoFormat(decoder_t* p_dec, decoder_sys_t* p_sys)
{
  vlc_fourcc_t videoFormat = p_dec->fmt_out.video.i_chroma;
  int chromaFormat;
  int bitDepths;
  if (decVTM_getvideoFormat(p_sys->decVtm, &chromaFormat, &bitDepths))
  {
    videoFormat = getVideoFormat(p_dec, chromaFormat, bitDepths);
  }
  return videoFormat;
}
/*****************************************************************************
 * DecodeFrame: decodes a video frame.
 *****************************************************************************/
static int DecodeFrame(decoder_t* p_dec, block_t* p_block)
{
  decoder_sys_t* p_sys = p_dec->p_sys;
  if (p_sys->b_frameRateDetect)
  {
    if (p_sys->dts1 == VLC_TS_INVALID)
    {
      p_sys->dts1 = p_block->i_dts;
    }
    else if (p_sys->dts2 == VLC_TS_INVALID && p_block->i_dts > p_sys->dts1)
    {
      p_sys->diff_dts = p_block->i_dts - p_sys->dts1;
      p_sys->dts2 = p_block->i_dts;
    }
    else if (p_block->i_dts > p_sys->dts2)
    {
      mtime_t diff_dts = p_block->i_dts - p_sys->dts2;
      if (diff_dts == p_sys->diff_dts)
      {
        p_sys->b_frameRateDetect = false;
        initVideoFrameRate(p_dec, p_sys, (unsigned int)(1000000 / (diff_dts / 1000)), 1000);
        msg_Info(p_dec, "detected frame rate %d/%d, from timestamps",
          p_dec->fmt_out.video.i_frame_rate,
          p_dec->fmt_out.video.i_frame_rate_base);
      }
      else
      {
        p_sys->dts1 = VLC_TS_INVALID;
        p_sys->dts2 = VLC_TS_INVALID;
      }
    }
  }
  if (p_block && p_sys->firstBlock)
  {
    p_sys->firstBlock = false;
    if (p_block->i_dts > VLC_TS_INVALID)
    {
      p_sys->firstBlock_dts = p_block->i_dts;
    }
    else
    {
      p_sys->firstBlock_dts = VLC_TS_0;
    }
  }

  mtime_t sys_pts = date_Get(&p_sys->pts);
  if (p_sys->enable_hurryMode && sys_pts > VLC_TS_INVALID && p_block)
  {
    mtime_t period = CLOCK_FREQ * p_sys->pts.i_divider_den / p_sys->pts.i_divider_num;
    mtime_t lateness = p_sys->lastOutput_time - p_sys->lastOutput_pts;
    if (lateness > (p_sys->speedUpLevel) * period && p_sys->speedUpLevel_delai_increase-- <= 0 && p_sys->speedUpLevel < 3)
    {
      p_sys->speedUpLevel++;
      p_sys->speedUpLevel_delai_increase = 7;
    }
    else if (p_sys->speedUpLevel > 0 && ((lateness < 0 && p_sys->speedUpLevel_delai_increase-- <= 0) || (lateness < - 4 * period )))
    {
      p_sys->speedUpLevel--;
      p_sys->speedUpLevel_delai_increase = 20;
    }
    if (p_sys->speedUpLevel > 0) 
     msg_Info(p_dec, "decoding frame %d (delay %d) - speed up %d", p_sys->dec_frame_count, lateness, p_sys->speedUpLevel);
  } 

  //msg_Warn(p_dec, "decVtm decode frame %d with nalu size %d ", p_sys->dec_frame_count, (p_block != nullptr) ? p_block->i_buffer : 0);
  decVTM_decode(p_sys->decVtm, (const char*)((p_block != nullptr) ? p_block->p_buffer : nullptr), (p_block != nullptr) ? p_block->i_buffer : 0, p_sys->speedUpLevel);
  if (p_block != nullptr)
  {
    p_sys->dec_frame_count++;
  }

  if (p_sys->b_format_init)
  {
    int width = 0, height = 0;
    decVTM_getFrameSize(p_sys->decVtm, &width, &height);
    vlc_fourcc_t videoFormat = getVideoFormat(p_dec, p_sys);

    if (width > 0 && height > 0)
    {
      p_sys->b_format_init = false;
      initVideoFormat(p_dec, p_sys, videoFormat, width, height);
      if(decoder_UpdateVideoFormat(p_dec))
	  {
		  return false;
	  }
    }
  }

  while (getOutputFrame(p_dec, false, p_block? p_block->i_dts: VLC_TS_INVALID));

  if (p_block)
  {
    block_Release(p_block);
  }
  else
  {
    msg_Warn(p_dec, "flushDecoder called at pts: %d ", date_Get(&p_sys->pts));
    decVTM_flush(p_dec->p_sys->decVtm);
    msg_Warn(p_dec, "decoder flushed ! ");
    while (getOutputFrame(p_dec, true, VLC_TS_INVALID));
  }
  return VLCDEC_SUCCESS;
}

static bool getOutputFrame(decoder_t* p_dec, bool waitUntilReady, mtime_t i_dts)
{
  decoder_sys_t* p_sys = p_dec->p_sys;
  short* planes[3];
  int strides[3];
  int width = 0, height = 0;
  int chromaFormat;
  int bitDepths;
  if (decVTM_getNextOutputFrame(p_sys->decVtm, waitUntilReady, planes, strides, &width, &height, &chromaFormat, &bitDepths))
  {
    // Get a new picture 
    picture_t* p_pic = NULL;
    p_sys->out_frame_count++;
    vlc_fourcc_t videoFormat = getVideoFormat(p_dec, chromaFormat, bitDepths);

    if (width != p_dec->fmt_out.video.i_width
      || height != p_dec->fmt_out.video.i_height
      || videoFormat != p_dec->fmt_out.video.i_chroma)
    {
      initVideoFormat(p_dec, p_sys, videoFormat, width, height);
      if(decoder_UpdateVideoFormat(p_dec))
	  {
		  return false;
	  }
      initVideoFrameRate(p_dec, p_sys);
    }

    mtime_t dat = mdate();
    if (planes[0] != nullptr)
    {
      if (!decoder_UpdateVideoFormat(p_dec))
        p_pic = decoder_NewPicture(p_dec);
      if (p_pic == NULL)
      {
        return false;
      }

      FillPicture(p_dec, p_pic, planes, strides);
    }
    decVTM_setlastPicDisplayed(p_sys->decVtm);

    // Date management: 1 frame per packet 
    if (p_sys->b_first_frame)
    {
      initVideoFrameRate(p_dec, p_sys);
      p_sys->b_first_frame = false;

      mtime_t i_pts = date_Get(&p_sys->pts);
      if (i_pts == VLC_TS_INVALID)
      {
        i_pts = p_sys->firstBlock_dts ;
      }
      if (i_pts > VLC_TS_INVALID)
        date_Set(&p_sys->pts, i_pts);

      p_sys->firstOutput_time = mdate();
      p_sys->firstOutput_pts = i_pts;
    }

    mtime_t i_pts = date_Get(&p_sys->pts);
    date_Increment(&p_sys->pts, 1);

    if (planes[0] != nullptr)
    {
      p_sys->lastOutput_pts = i_pts - p_sys->firstOutput_pts;
      p_sys->lastOutput_time = mdate() - p_sys->firstOutput_time;

      p_pic->date = i_pts;
      p_pic->b_force = true;
      p_pic->i_nb_fields = 2;
      p_pic->b_progressive = true;

      decoder_QueueVideo(p_dec, p_pic);
    }
    return true;
  }
  return false;
}

/**
 * Common deinitialization
 */
static void CloseDec(vlc_object_t* p_this)
{
  decoder_t* p_dec = (decoder_t*)p_this;
  msg_Info(p_dec, "decoded %d frames",p_dec->p_sys->dec_frame_count);
  msg_Info(p_dec, "output %d frames",p_dec->p_sys->out_frame_count);
  decVTM_flush(p_dec->p_sys->decVtm);
  decVTM_destroy(p_dec->p_sys->decVtm);
  free(p_dec->p_sys);
}
