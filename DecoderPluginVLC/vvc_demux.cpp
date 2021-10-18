/*****************************************************************************
 * vvc_demux.cpp: h.266/vvc demuxer
 *****************************************************************************
 * Copyright (C) 2021 interdigital
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
#endif

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <string>
#include <vector>
#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_demux.h>
#include <vlc_codec.h>

#include "vvc_nal.h"

struct demux_sys_t
{
  int    frame_size;

  es_format_t  fmt_video;

  date_t pcr;

  bool b_vvc;

  es_out_id_t *p_es;

  date_t      dts;
  unsigned    frame_rate_num;
  unsigned    frame_rate_den;
  int         baseLayerID;

  decoder_t *p_packetizer;
  struct demux_layer_info
  {
    int order_id;
    int id;
    es_out_id_t *p_es;
    es_format_t layer_es_fmt;
    char layer_name[30];
    std::vector<int> output_layers;
  };
  std::vector<demux_layer_info> inputLayers;
};

typedef struct
{
  bool b_sps;
  bool b_pps;
  bool b_vps;
} vvc_probe_ctx_t;

#define H26X_PACKET_SIZE 2048
#define H26X_PEEK_CHUNK  (H26X_PACKET_SIZE * 4)
#define H26X_MIN_PEEK    (4 + 7 + 10)
#define H26X_MAX_PEEK    (H26X_PEEK_CHUNK * 8) /* max data to check */
static const int H26X_MAX_NAL_SIZE = (H26X_PACKET_SIZE * 16);
#define H26X_NAL_COUNT   16 /* max # or NAL to check */
#define VLC_CODEC_VVC            VLC_FOURCC('h','2','6','6')

/****************************************************************************
 * Local prototypes
 ****************************************************************************/
namespace VvcDecoder
{
  int  OpenDemux(vlc_object_t*);
  void CloseDemux(vlc_object_t*);
}

static int Demux(demux_t*);
static int Control(demux_t*, int i_query, va_list args);

static inline bool check_Property(demux_t* p_demux, const char** pp_psz,
  bool(*pf_check)(demux_t*, const char*));
/**
 * initialization for decoder
 */

static int ProbeVVC(const uint8_t* p_peek, size_t i_peek, vvc_probe_ctx_t* p_ctx)
{

  if (i_peek < 2)
    return -1;

  if (p_peek[0] & 0x80)
    return -1;

  // get next NAL unit type
  int firstByte = 0;
  if (p_peek[0] == 0 && p_peek[1] == 0 && p_peek[2] == 0 && p_peek[3] == 1)
  {
    firstByte = 4;
  }
  else if(p_peek[0] == 0 && p_peek[1] == 0 && p_peek[2] ==1)
  {
    firstByte = 3;
  }
  int nuhLayerId = (firstByte > 0) ? ((p_peek[firstByte]) & 0x3f) : 0;
  vvc_nal_unit_type_e i_nal_type = (firstByte > 0) ? (vvc_nal_unit_type_e)((p_peek[firstByte + 1] >> 3) & 0x1f) : VVC_NAL_INVALID;
  int ret = 0; // Probe more
  switch (i_nal_type)
  {
  case VVC_NAL_CODED_SLICE_IDR_W_RADL:
  case VVC_NAL_CODED_SLICE_IDR_N_LP:
  case VVC_NAL_CODED_SLICE_CRA:
  case VVC_NAL_CODED_SLICE_GDR:
    if (p_ctx->b_sps && p_ctx->b_pps && nuhLayerId == 0)
      ret = 1;
    break;
  case VVC_NAL_OPI:
  case VVC_NAL_DCI:
    break;
  case VVC_NAL_PH:
    break;
  case VVC_NAL_VPS: /* VPS */
    if (nuhLayerId != 0 || i_peek < 7 ||
      ((p_peek[firstByte + 2] >> 4) & 0x0F) == 0) // Check setVPSId 
      ret = -1;
    p_ctx->b_vps = true;
    break;
  case VVC_NAL_SPS:  /* SPS */
    if (nuhLayerId != 0)
      ret = -1;
    p_ctx->b_sps = true;
    break;
  case  VVC_NAL_PPS:  /* PPS */
    if (nuhLayerId != 0)
      ret = -1;
    p_ctx->b_pps = true;
    break;
  case  VVC_NAL_PREFIX_APS: /* Prefix SEI */
    if (p_ctx->b_sps && p_ctx->b_pps && nuhLayerId == 0)
      ret = 1;
    break;
  case VVC_NAL_ACCESS_UNIT_DELIMITER: /* AU */
    if (i_peek < H26X_MIN_PEEK - 4 ||
      p_peek[firstByte + 3] != 0 || p_peek[firstByte + 4] != 0) /* Must prefix another NAL */
      ret = -1;
    break;
  case VVC_NAL_PREFIX_SEI: /* Prefix SEI */
    if (p_peek[2] == 0xFF) /* empty SEI */
      ret = -1;
    break;
  default:
    ret = -1;
  }

  return ret; 
}

int VvcDecoder::OpenDemux(vlc_object_t* p_this)
{
  vvc_probe_ctx_t ctx = { 0, 0, 0 };
  const char* rgi_psz_ext[] = { ".h266", ".266", ".bin", ".bit", ".raw", NULL };
  const char* rgi_psz_mime[] = { "video/H266", "video/h266", "video/vvc", NULL };

  demux_sys_t* p_sys;
  const uint8_t* p_peek;
  es_format_t fmt;
  uint8_t annexb_startcode[] = { 0,0,0,1 };
  int i_ret = 0;

  // Restrict by type first
  demux_t* p_demux = (demux_t*)p_this;
  if (!p_demux->obj.force &&
    !check_Property(p_demux, rgi_psz_ext, demux_IsPathExtension) &&
    !check_Property(p_demux, rgi_psz_mime, demux_IsContentType))
  {
    return VLC_EGENERIC;
  }

  // First check for first AnnexB header
  if (vlc_stream_Peek(p_demux->s, &p_peek, H26X_MIN_PEEK) == H26X_MIN_PEEK &&
    !memcmp(p_peek, annexb_startcode, 4))
  {
    i_ret = 1;
  }

  /* First check for first AnnexB header */
  int i_probe_offset_correct = 4;
  if (vlc_stream_Peek(p_demux->s, &p_peek, H26X_MIN_PEEK) == H26X_MIN_PEEK &&
    !memcmp(p_peek, annexb_startcode, 4))
  {
    size_t i_peek = H26X_MIN_PEEK;
    size_t i_peek_target = H26X_MIN_PEEK;
    size_t i_probe_offset = 4;
    const uint8_t* p_probe = p_peek;
    bool b_synced = true;
    unsigned i_bitflow = 0;

    for (unsigned i = 0; i < H26X_NAL_COUNT; i++)
    {
      while (!b_synced)
      {
        if (i_probe_offset + H26X_MIN_PEEK >= i_peek &&
          i_peek_target + H26X_PEEK_CHUNK <= H26X_MAX_PEEK)
        {
          i_peek_target += H26X_PEEK_CHUNK;
          i_peek = vlc_stream_Peek(p_demux->s, &p_peek, i_peek_target);
        }
        if (i_probe_offset + H26X_MIN_PEEK >= i_peek)
          break;

        p_probe = &p_peek[i_probe_offset];
        i_bitflow = (i_bitflow << 1) | (!p_probe[0]);
        /* Check for annexB */
        if ((p_probe[0] == 0x01 && ((i_bitflow & 0x06) == 0x06))
          || (p_probe[0] == 0x00 && p_probe[-1] == 0x00 && p_probe[-2] == 0x00)
          )
        {
          b_synced = true;
          i_probe_offset_correct = 3;
          i_bitflow = 0;
        }

        i_probe_offset++;
      }


      if (b_synced)
      {
        p_probe = &p_peek[i_probe_offset - i_probe_offset_correct];
        i_ret = ProbeVVC(p_probe, i_peek - i_probe_offset, &ctx);
      }

      if (i_ret != 0)
        break;

      i_probe_offset += 4;
      b_synced = false;
    }
  }

  if (i_ret < 1)
  {
    if (!p_demux->obj.force)
    {
      msg_Warn(p_demux, "vvc module discarded (no startcode)");
      return VLC_EGENERIC;
    }
    msg_Err(p_demux, "this doesn't look like a vvc ES stream ", "continuing anyway");
  }

  p_demux->pf_demux = Demux;
  p_demux->pf_control = Control;
  p_demux->p_sys = p_sys = new demux_sys_t;
  p_sys->p_es = NULL;
  p_sys->frame_rate_num = 0;
  p_sys->frame_rate_den = 0;
  p_sys->baseLayerID = -1;
  p_sys->inputLayers.reserve(100);

  double fps = 0;
  char psz_fpsvar[10];
  if (sprintf(psz_fpsvar, "vvc-fps"))
  {
    fps = var_CreateGetFloat(p_demux, psz_fpsvar);
  }

  if (!fps)
  {
    if (strlen(p_demux->psz_location) > 3)
    {
      std::string path(p_demux->psz_location);
      int pos = (int)path.find("Hz");
      if (pos == std::string::npos) (int)path.find("HZ");
      if (pos == std::string::npos) (int)path.find("hz");
      if (pos == std::string::npos) (int)path.find("fps");
      if (pos == std::string::npos) (int)path.find("FPS");
      if (pos != std::string::npos && pos > 0)
      {
        int length = 1;
        while (pos - length >= 0 && path[pos - length] >= '0' && path[pos - length] <= '9')
        {
          length++;
        }
        length--;
        if (length > 0)
        {
          std::string val = path.substr(std::max(0, pos - length), length);
          fps = atof(val.c_str());
          if (fps)
          {
            msg_Dbg(p_demux, "found frame rate in path %.2f fps", fps);
          }
        }
      }
    }
  }

  if (fps)
  {
    if (fps < 0.001f) fps = 0.001f;
    p_sys->frame_rate_den = 1000;
    p_sys->frame_rate_num = (int) (1000 * fps);
    date_Init(&p_sys->dts, p_sys->frame_rate_num, p_sys->frame_rate_den);
  }
  else
  {
    date_Init(&p_sys->dts, 50000, 1000);
  }
  date_Set(&p_sys->dts, VLC_TS_0);

  // Load the mpegvideo packetizer
  es_format_Init(&fmt, VIDEO_ES, VLC_CODEC_VVC);
  fmt.video.i_frame_rate = p_sys->dts.i_divider_num;
  fmt.video.i_frame_rate_base = p_sys->dts.i_divider_den;
  
  p_sys->p_packetizer = demux_PacketizerNew(p_demux, &fmt, "vvc");

  if (!p_sys->p_packetizer)
  {
    free(p_sys);
    return VLC_EGENERIC;
  }

  return VLC_SUCCESS;
}

/*****************************************************************************
 * Demux: reads and demuxes data packets
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, 1 otherwise
 *****************************************************************************/

static int Demux(demux_t* p_demux)
{
  demux_sys_t* p_sys = p_demux->p_sys;
  block_t* p_block_in, * p_block_out;
  bool b_eof = false;

  p_block_in = vlc_stream_Block(p_demux->s, H26X_PACKET_SIZE);
  if (p_block_in == NULL)
  {
    b_eof = true;
  }
  else
  {
    p_block_in->i_dts = date_Get(&p_sys->dts);
  }
  while ((p_block_out = p_sys->p_packetizer->pf_packetize(p_sys->p_packetizer,
    p_block_in ? &p_block_in : NULL)))
  {
    while (p_block_out)
    {
      block_t* p_next = p_block_out->p_next;

      p_block_out->p_next = NULL;


      if (p_sys->p_es == NULL)
      {
        p_sys->p_packetizer->fmt_out.b_packetized = true;
        p_sys->p_es = es_out_Add(p_demux->out, &p_sys->p_packetizer->fmt_out);
        if (!p_sys->p_es)
        {
          block_ChainRelease(p_block_out);
          return VLC_DEMUXER_EOF;
        }
      }

      bool frame = p_block_out->i_flags & BLOCK_FLAG_TYPE_MASK;
      const mtime_t i_frame_dts = p_block_out->i_dts;
      const mtime_t i_frame_length = p_block_out->i_length;
      uint32_t nuhLayerId = 0;
      if (frame)
      {
        int firstByte = 4;
        if (p_block_out->p_buffer[0] == 0 && p_block_out->p_buffer[1] == 0 && p_block_out->p_buffer[2] == 0 && p_block_out->p_buffer[3] == 1)
        {
          firstByte = 4;
        }
        else if (p_block_out->p_buffer[0] == 0 && p_block_out->p_buffer[1] == 0 && p_block_out->p_buffer[2] == 1)
        {
          firstByte = 3;
        }
        nuhLayerId = ((p_block_out->p_buffer[firstByte]) & 0x3f);

        bool inputLayerNew = true;
        unsigned int inputLayerIdx = 0;
        for (unsigned int i = 0; i < p_sys->inputLayers.size(); i++)
        {
          if (p_sys->inputLayers[i].id == nuhLayerId)
          {
            inputLayerIdx = i;
            inputLayerNew = false;
            break;
          }
        }
        if (inputLayerNew)
        {
          inputLayerIdx = p_sys->inputLayers.empty() ? 0 : p_sys->inputLayers.back().order_id + 1;
          es_out_id_t* es = nullptr;
          es_format_t layer_es_fmt;
          es_format_Copy(&layer_es_fmt, &p_sys->p_packetizer->fmt_out);
          if (inputLayerIdx == 0)
          {
            es = p_sys->p_es;
            demux_sys_t::demux_layer_info layer = { inputLayerIdx, nuhLayerId, es, layer_es_fmt };
            p_sys->inputLayers.push_back(layer);
          }
          else 
          {
            if (inputLayerIdx == 1)
            {
              demux_sys_t::demux_layer_info layer0 = { 0, nuhLayerId, es, layer_es_fmt };
              sprintf(p_sys->inputLayers.back().layer_name, "decide from ols");
              p_sys->inputLayers.back().layer_es_fmt.psz_language = p_sys->inputLayers.back().layer_name;
              p_sys->inputLayers.push_back(layer0);
              sprintf(p_sys->inputLayers.back().layer_name, "layer 0 only");
              p_sys->inputLayers.back().layer_es_fmt.psz_language = p_sys->inputLayers.back().layer_name;
              p_sys->inputLayers.back().output_layers.push_back(0);
              p_sys->inputLayers.back().layer_es_fmt.p_extra = &p_sys->inputLayers.back().output_layers;
              p_sys->inputLayers.back().layer_es_fmt.i_extra = sizeof(&p_sys->inputLayers.back().output_layers);
              es = es_out_Add(p_demux->out, &p_sys->inputLayers.back().layer_es_fmt);
            }
            demux_sys_t::demux_layer_info layeri = { inputLayerIdx, nuhLayerId, es, layer_es_fmt };
            p_sys->inputLayers.push_back(layeri);
            sprintf(p_sys->inputLayers.back().layer_name, "layer %d only", nuhLayerId);
            p_sys->inputLayers.back().layer_es_fmt.psz_language = p_sys->inputLayers.back().layer_name;
            p_sys->inputLayers.back().output_layers.push_back(inputLayerIdx);
            p_sys->inputLayers.back().layer_es_fmt.p_extra = &p_sys->inputLayers.back().output_layers;
            p_sys->inputLayers.back().layer_es_fmt.i_extra = sizeof(&p_sys->inputLayers.back().output_layers);
            es = es_out_Add(p_demux->out, &p_sys->inputLayers.back().layer_es_fmt);

            demux_sys_t::demux_layer_info layer = { inputLayerIdx, nuhLayerId, es, layer_es_fmt };
            p_sys->inputLayers.push_back(layer);
            sprintf(p_sys->inputLayers.back().layer_name, "layer ids 0 -> %d", inputLayerIdx);
            p_sys->inputLayers.back().layer_es_fmt.psz_language = p_sys->inputLayers.back().layer_name;
            for (demux_sys_t::demux_layer_info& l : p_sys->inputLayers)
            {
              if (l.id < nuhLayerId)
              {
                p_sys->inputLayers.back().output_layers.push_back(l.id);
              }
            }
            p_sys->inputLayers.back().layer_es_fmt.p_extra = &p_sys->inputLayers.back().output_layers;
            p_sys->inputLayers.back().layer_es_fmt.i_extra = sizeof(&p_sys->inputLayers.back().output_layers);
            es = es_out_Add(p_demux->out, &p_sys->inputLayers.back().layer_es_fmt);
          }
          msg_Dbg(p_demux, "new layer defined as a stream nb %d: %d (total options %d) ", inputLayerIdx, nuhLayerId, p_sys->inputLayers.size());
        }
      }

      es_out_Send(p_demux->out, p_sys->p_es, p_block_out);
      if (frame)
      {
        if (!p_sys->frame_rate_den)
        {
          // Use packetizer's one
          if (p_sys->p_packetizer->fmt_out.video.i_frame_rate_base &&
            p_sys->p_packetizer->fmt_out.video.i_frame_rate)
          {
            p_sys->frame_rate_num = p_sys->p_packetizer->fmt_out.video.i_frame_rate;
            p_sys->frame_rate_den = p_sys->p_packetizer->fmt_out.video.i_frame_rate_base;
          }
          else
          {
            p_sys->frame_rate_num = 50000;
            p_sys->frame_rate_den = 1000;
          }
          date_Init(&p_sys->dts, p_sys->frame_rate_num, p_sys->frame_rate_den);
          date_Set(&p_sys->dts, VLC_TS_0);
          msg_Dbg(p_demux, "using %.2f fps", (double)p_sys->frame_rate_num / p_sys->frame_rate_den);
        }

        es_out_SetPCR(p_demux->out, date_Get(&p_sys->dts));
        unsigned i_nb_frames;
        if (p_sys->baseLayerID < 0 || (int)nuhLayerId < p_sys->baseLayerID)
        {
          p_sys->baseLayerID = nuhLayerId;
        }
        if (i_frame_length > 0)
        {
          i_nb_frames = (unsigned int)(i_frame_length * p_sys->frame_rate_num /
            (p_sys->frame_rate_den * CLOCK_FREQ));
        }
        else i_nb_frames = 1;
        if (i_nb_frames <= 3) 
        {
          if (nuhLayerId == p_sys->baseLayerID)
          {
            date_Increment(&p_sys->dts, i_nb_frames);
          }
        }
        else // Somehow some discontinuity 
        {
          date_Set(&p_sys->dts, i_frame_dts);
        }
      }

      if (p_block_in)
      {
        p_block_in->i_dts = date_Get(&p_sys->dts);
        p_block_in->i_pts = VLC_TS_INVALID;
      }
      p_block_out = p_next;
    }
  }
  return (b_eof) ? VLC_DEMUXER_EOF : VLC_DEMUXER_SUCCESS;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control(demux_t* p_demux, int i_query, va_list args)
{
  switch (i_query)
  {
  case DEMUX_SET_TIME:
    return VLC_EGENERIC;
  case DEMUX_SET_ES:
    msg_Dbg(p_demux, "changed stream !");
  default:
    return demux_vaControlHelper(p_demux->s,
      0, 0,
      0, 0, i_query, args);
  }
}

static inline bool check_Property(demux_t* p_demux, const char** pp_psz,
  bool(*pf_check)(demux_t*, const char*))
{
  while (*pp_psz)
  {
    if (pf_check(p_demux, *pp_psz))
      return true;
    pp_psz++;
  }
  return false;
}

/**
 * Common deinitialization
 */

void VvcDecoder::CloseDemux(vlc_object_t* p_this)
{
  demux_t* p_demux = (demux_t*)p_this;
  demux_sys_t* p_sys = p_demux->p_sys;

  demux_PacketizerDestroy(p_sys->p_packetizer);
  delete p_sys;
}
