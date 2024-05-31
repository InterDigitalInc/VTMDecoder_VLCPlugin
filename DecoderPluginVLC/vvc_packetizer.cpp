/*****************************************************************************
 * vvc_packetizer.cpp: h.266/vvc video packetizer
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

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_codec.h>
#include <vlc_block.h>
#include <vlc_bits.h>

#include <vlc_block_helper.h>
#include "packetizer_helper.h"
#include "startcode_helper.h"

#include <limits.h>
#include "vvc_nal.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
namespace VvcDecoder
{
  static const uint8_t* startcode_FindAnnexB(const uint8_t* p, const uint8_t* end);
  int  OpenPack(vlc_object_t*);
  void ClosePack(vlc_object_t*);
}

/****************************************************************************
 * Local prototypes
 ****************************************************************************/
static block_t *PacketizeAnnexB(decoder_t *, block_t **);
static void PacketizeFlush( decoder_t * );
static void PacketizeReset(void *p_private, bool b_broken);
static block_t *PacketizeParse(void *p_private, bool *pb_ts_used, block_t *);
static block_t *ParseNALBlock(decoder_t *, bool *pb_ts_used, block_t *);
static int PacketizeValidate(void *p_private, block_t *);
static block_t* GatherAndValidateChain(block_t* p_outputchain);

struct decoder_sys_t
{
    /* */
    packetizer_t packetizer;

    struct
    {
        block_t *p_chain;
        block_t **pp_chain_last;
    } frame, frame2;

    //uint8_t  i_nal_length_size;
    bool b_init_sequence_complete;
    int  i_nb_frames;

    date_t dts;
    mtime_t pts;
    int lastTid;
    bool b_need_ts;
    bool sliceInPicture;
    bool gotSps;
    bool gotPps;
    int baseLayerID;
};

static const uint8_t p_vcc_startcode[3] = { 0x00, 0x00, 0x01};
/****************************************************************************
 * Helpers
 ****************************************************************************/
static inline void InitQueue( block_t **pp_head, block_t ***ppp_tail )
{
    *pp_head = NULL;
    *ppp_tail = pp_head;
}
#define INITQ(name) InitQueue(&p_sys->name.p_chain, &p_sys->name.pp_chain_last)

static block_t * OutputQueues(decoder_sys_t *p_sys, bool b_valid)
{
    block_t *p_output = NULL;
    block_t **pp_output_last = &p_output;
    uint32_t i_flags = 0; /* Because block_ChainGather does not merge flags or times */

    if (p_sys->frame.p_chain)
    {
      i_flags |= p_sys->frame.p_chain->i_flags;
      block_ChainLastAppend(&pp_output_last, p_sys->frame.p_chain);

      mtime_t dts = VLC_TS_INVALID, pts = VLC_TS_INVALID;
      block_t* lastBlock = p_sys->frame.p_chain;
      while (lastBlock)
      {
        if (lastBlock->i_dts > VLC_TS_INVALID)
        {
          dts = lastBlock->i_dts;
        }
        if (lastBlock->i_pts > VLC_TS_INVALID)
        {
          pts = lastBlock->i_pts;
        }
        lastBlock = lastBlock->p_next;
      }

      if (dts > VLC_TS_INVALID)
      {
        p_output->i_dts = dts;
      }
      else
      {
        p_output->i_dts = date_Get(&p_sys->dts);
      }
      if (pts > VLC_TS_INVALID)
      {
        p_output->i_pts = pts;
      }
      else
      {
        p_output->i_pts = p_sys->pts;
      }
      INITQ(frame);
    }

    if(p_output)
    {
        p_output->i_flags |= i_flags;
        if(!b_valid)
            p_output->i_flags |= BLOCK_FLAG_DROP;
    }

    return p_output;
}

const uint8_t* VvcDecoder::startcode_FindAnnexB(const uint8_t* p, const uint8_t* end)
{
  for (end -= sizeof(p_vcc_startcode) - 1; p < end; p++)
  {
    bool match = true;
    for (int i=0; i<sizeof(p_vcc_startcode); i++)
    {
      match &= p[i] == p_vcc_startcode[i];
    }
    if (match) return p;
  }
  return NULL;
}

/*****************************************************************************
 * Open
 *****************************************************************************/
int VvcDecoder::OpenPack(vlc_object_t *p_this)
{
    decoder_t     *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys;

    if (p_dec->fmt_in.i_codec != VLC_FOURCC('h', '2', '6', '6')
      && p_dec->fmt_in.i_codec != VLC_FOURCC('v', 'v', 'c', '1'))
        return VLC_EGENERIC;

    p_dec->p_sys = p_sys = (decoder_sys_t *)calloc(1, sizeof(decoder_sys_t));
    if (!p_dec->p_sys)
        return VLC_ENOMEM;

    INITQ(frame);
    INITQ(frame2);
    p_sys->sliceInPicture = false;
    p_sys->lastTid = 0;
    p_sys->gotPps = false;
    p_sys->gotSps = false;
    p_sys->b_init_sequence_complete  = false;
    p_sys->i_nb_frames = 0;
    p_sys->baseLayerID = -1;

    packetizer_t* p_pack = &p_dec->p_sys->packetizer;
    p_pack->i_state = STATE_NOSYNC;
    block_BytestreamInit(&p_pack->bytestream);
    p_pack->i_offset = 0;

    packetizer_Init(&p_dec->p_sys->packetizer,
      p_vcc_startcode, sizeof(p_vcc_startcode), VvcDecoder::startcode_FindAnnexB,
      p_vcc_startcode, 1, 5,
      PacketizeReset, PacketizeParse, PacketizeValidate, p_dec);
    
    /* Copy properties */
    es_format_Copy(&p_dec->fmt_out, &p_dec->fmt_in);
    p_dec->fmt_out.b_packetized = true;

    /* Init timings */
    if( p_dec->fmt_in.video.i_frame_rate_base &&
        p_dec->fmt_in.video.i_frame_rate &&
        p_dec->fmt_in.video.i_frame_rate <= UINT_MAX  )
        date_Init( &p_sys->dts, p_dec->fmt_in.video.i_frame_rate,
                                p_dec->fmt_in.video.i_frame_rate_base );
    else
        date_Init( &p_sys->dts, 50000, 1000 );
    date_Set( &p_sys->dts, VLC_TS_INVALID );
    p_sys->pts = VLC_TS_INVALID;
    p_sys->b_need_ts = true;

    /* Set callbacks */
    const uint8_t *p_extra = (uint8_t *)p_dec->fmt_in.p_extra;
    const size_t i_extra = p_dec->fmt_in.i_extra;

    p_dec->pf_packetize = PacketizeAnnexB;
    p_dec->pf_flush = PacketizeFlush;

    if(p_dec->fmt_out.i_extra)
    {
        /* Feed with AnnexB VPS/SPS/PPS/SEI extradata */
        packetizer_Header(&p_sys->packetizer,
                          (uint8_t*)p_dec->fmt_out.p_extra, p_dec->fmt_out.i_extra);
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close
 *****************************************************************************/
void VvcDecoder::ClosePack(vlc_object_t *p_this)
{
    decoder_t *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

    block_t *b = p_sys->frame.p_chain;
    while (b)
    {
      msg_Warn(p_dec, "close packetizer with chain %d ",  b->i_buffer);
      b = b->p_next;
    }
    b = p_sys->packetizer.bytestream.p_chain;
    while (b)
    {
      msg_Warn(p_dec, "close packetizer with bytestream %d - offset %d  ",  b->i_buffer, p_sys->packetizer.bytestream.i_block_offset);
      b = b->p_next;
    }
    msg_Warn(p_dec, "close packetizer - packetized %d frames ", p_sys->i_nb_frames);


    packetizer_Clean(&p_sys->packetizer);

    block_ChainRelease(p_sys->frame.p_chain);

    free(p_sys);
}

/****************************************************************************
 * Packetize
 ****************************************************************************/
static block_t *PacketizeAnnexB(decoder_t *p_dec, block_t **pp_block)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t *output = packetizer_Packetize(&p_sys->packetizer, pp_block);
    if (!output && !pp_block)
    {
      if (p_sys->frame2.p_chain)
      {
        block_ChainLastAppend(&p_sys->frame.pp_chain_last, p_sys->frame2.p_chain);
        INITQ(frame2);
      }
      block_t* flushOutput = OutputQueues(p_sys, p_sys->b_init_sequence_complete);
      block_ChainAppend(&output, flushOutput);

      output = GatherAndValidateChain(output);
    }
    if (output)
    {
      p_sys->i_nb_frames++;
    }

    return output;
}

static void PacketizeFlush( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    msg_Warn(p_dec, "packetizer flush called at pts: %d ", p_sys->pts);
    packetizer_Flush( &p_sys->packetizer );
}

/****************************************************************************
 * Packetizer Helpers
 ****************************************************************************/
static void PacketizeReset(void *p_private, bool b_broken)
{
    VLC_UNUSED(b_broken);

    decoder_t *p_dec = (decoder_t *)p_private;
    decoder_sys_t *p_sys = p_dec->p_sys;

    msg_Warn(p_dec, "packetizer reset called at pts: %d ", p_sys->pts);
    if (p_sys->frame2.p_chain)
    {
      block_ChainLastAppend(&p_sys->frame.pp_chain_last, p_sys->frame2.p_chain);
      INITQ(frame2);
    }

    block_t *p_out = OutputQueues(p_sys, false);
    if(p_out)
        block_ChainRelease(p_out);

    INITQ(frame);
    INITQ(frame2);
    p_sys->b_init_sequence_complete = false;
    p_sys->sliceInPicture = false;
    p_sys->lastTid = 0;
    p_sys->gotPps = false;
    p_sys->gotSps = false;
    p_sys->i_nb_frames = 0;
    date_Set(&p_sys->dts, VLC_TS_INVALID);
    p_sys->pts = VLC_TS_INVALID;
    p_sys->b_need_ts = true;
}


static block_t *GatherAndValidateChain(block_t *p_outputchain)
{
    block_t *p_output = NULL;

    if(p_outputchain)
    {
        if(p_outputchain->i_flags & BLOCK_FLAG_DROP)
            p_output = p_outputchain; /* Avoid useless gather */
        else
            p_output = block_ChainGather(p_outputchain);
    }

    if(p_output && (p_output->i_flags & BLOCK_FLAG_DROP))
    {
        block_ChainRelease(p_output); /* Chain! see above */
        p_output = NULL;
    }

    return p_output;
}

/*static void SetOutputBlockProperties(decoder_t *p_dec, block_t *p_output)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    // Set frame duration 
    if(p_sys->p_active_sps)
    {
        uint8_t i_num_clock_ts = hevc_get_num_clock_ts(p_sys->p_active_sps,
                                                       p_sys->p_timing);
        const mtime_t i_start = date_Get(&p_sys->dts);
        if( i_start != VLC_TS_INVALID )
        {
            date_Increment(&p_sys->dts, i_num_clock_ts);
            p_output->i_length = date_Get(&p_sys->dts) - i_start;
        }
        p_sys->pts = VLC_TS_INVALID;
    }
    hevc_release_sei_pic_timing(p_sys->p_timing);
    p_sys->p_timing = NULL;
}*/

/*****************************************************************************
 * ParseNALBlock: parses annexB type NALs
 * All p_frag blocks are required to start with 0 0 0 1 4-byte startcode
 *****************************************************************************/
static block_t *ParseNALBlock(decoder_t *p_dec, bool *pb_ts_used, block_t *p_frag)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    *pb_ts_used = false;

    if(p_sys->b_need_ts)
    {
        if(p_frag->i_dts > VLC_TS_INVALID)
            date_Set(&p_sys->dts, p_frag->i_dts);
        p_sys->pts = p_frag->i_pts;
        if(date_Get( &p_sys->dts ) != VLC_TS_INVALID)
            p_sys->b_need_ts = false;
        *pb_ts_used = true;
    }

    if(unlikely(p_frag->i_buffer < 5))
    {
        msg_Warn(p_dec,"NAL too small");
        block_Release(p_frag);
        return NULL;
    }

    if(p_frag->p_buffer[4] & 0x80)
    {
        msg_Warn(p_dec,"Forbidden zero bit not null, corrupted NAL");
        block_Release(p_frag);
        return GatherAndValidateChain(OutputQueues(p_sys, false)); // will drop 
    }

    // get next NAL unit type
    int firstByte = 4;
    if (p_frag->p_buffer[0] == 0 && p_frag->p_buffer[1] == 0 && p_frag->p_buffer[2] == 0 && p_frag->p_buffer[3] == 1)
    {
      firstByte = 4;
    }
    else if (p_frag->p_buffer[0] == 0 && p_frag->p_buffer[1] == 0 && p_frag->p_buffer[2] == 1)
    {
      firstByte = 3;
    }
    uint32_t nuhLayerId = ((p_frag->p_buffer[firstByte]) & 0x3f);
    vvc_nal_unit_type_e i_nal_type = (vvc_nal_unit_type_e) ((p_frag->p_buffer[firstByte+1] >> 3) & 0x1f);
    int i_nal_temporal_ID = ((p_frag->p_buffer[firstByte+1]) & 0x07) - 1;
    const mtime_t dts = p_frag->i_dts, pts = p_frag->i_pts;
    block_t * p_output = NULL;

    bool isNewPicture = false;
    bool currentIsFirstSlice = false;
    bool maybeNew = false;
    switch (i_nal_type)//nalu.m_nalUnitType)
    {
      // NUT that indicate the start of a new picture
    case VVC_NAL_ACCESS_UNIT_DELIMITER:
    case VVC_NAL_OPI:
    case VVC_NAL_DCI:
    case VVC_NAL_VPS:
    case VVC_NAL_SPS:
    case VVC_NAL_PPS:
    case VVC_NAL_PH:
      isNewPicture = p_sys->sliceInPicture;
      break;

      // NUT that may be the start of a new picture - check first bit in slice header
    case VVC_NAL_CODED_SLICE_TRAIL:
    case VVC_NAL_CODED_SLICE_STSA:
    case VVC_NAL_CODED_SLICE_RASL:
    case VVC_NAL_CODED_SLICE_RADL:
    case VVC_NAL_CODED_SLICE_IDR_W_RADL:
    case VVC_NAL_CODED_SLICE_IDR_N_LP:
    case VVC_NAL_CODED_SLICE_CRA:
    case VVC_NAL_CODED_SLICE_GDR:
    case VVC_NAL_RESERVED_VCL_4:
    case VVC_NAL_RESERVED_VCL_5:
    case VVC_NAL_RESERVED_VCL_6:
    case VVC_NAL_RESERVED_IRAP_VCL_11:
      p_frag->i_flags |= BLOCK_FLAG_TYPE_P;
      // checkPictureHeaderInSliceHeaderFlag
      currentIsFirstSlice = ((p_frag->p_buffer[6] >> 7) & 0x1) || !p_sys->sliceInPicture;
      isNewPicture = p_sys->sliceInPicture && currentIsFirstSlice;
      p_sys->sliceInPicture = true;
      break;

    case VVC_NAL_EOS:
    case VVC_NAL_EOB:
      isNewPicture = true;
      break;
    case VVC_NAL_SUFFIX_SEI:
    case VVC_NAL_SUFFIX_APS:
      break;
    default:
      maybeNew = p_sys->sliceInPicture;
      break;
    }
 
    if (!p_sys->gotPps && i_nal_type == VVC_NAL_PPS)
    {
      p_sys->gotPps = true;
    }
    if (!p_sys->gotSps && i_nal_type == VVC_NAL_SPS)
    {
      p_sys->gotSps = true;
    }
    if (isNewPicture)
    {
      if (!p_sys->b_init_sequence_complete && p_sys->gotPps && p_sys->gotSps)
      {
        p_sys->b_init_sequence_complete = true;
      }
      if (p_sys->frame.p_chain)
      {
        p_sys->frame.p_chain->i_flags |= (p_sys->lastTid < 2 )? BLOCK_FLAG_TYPE_P: BLOCK_FLAG_TYPE_B;
        // Starting new frame: return previous frame data for output 
        if (p_sys->baseLayerID < 0 || (int)nuhLayerId < p_sys->baseLayerID)
        {
          p_sys->baseLayerID = nuhLayerId;
        }
        if (nuhLayerId == p_sys->baseLayerID)
        {
          date_Increment(&p_sys->dts, 1);
          if (dts > VLC_TS_INVALID)
            date_Set(&p_sys->dts, dts);
          p_sys->pts = pts;
        }
        *pb_ts_used = true;
        p_output = OutputQueues(p_sys, p_sys->b_init_sequence_complete);
      }
      p_sys->sliceInPicture = currentIsFirstSlice;
    }
    p_sys->lastTid = i_nal_temporal_ID;
    if (maybeNew)
    {
      block_ChainLastAppend(&p_sys->frame2.pp_chain_last, p_frag);
    }
    else
    {
      if (p_sys->frame2.p_chain)
      {
        block_ChainLastAppend(&p_sys->frame.pp_chain_last, p_sys->frame2.p_chain);
        INITQ(frame2);
      }
      block_ChainLastAppend(&p_sys->frame.pp_chain_last, p_frag);
    }

    p_output = GatherAndValidateChain(p_output);

    return p_output;
}

static block_t *PacketizeParse(void *p_private, bool *pb_ts_used, block_t *p_block)
{
    decoder_t *p_dec = (decoder_t *)p_private;
    decoder_sys_t *p_sys = p_dec->p_sys;

    /* Remove trailing 0 bytes */
    while (p_block->i_buffer > 5 && p_block->p_buffer[p_block->i_buffer-1] == 0x00 )
        p_block->i_buffer--;

    p_block = ParseNALBlock( p_dec, pb_ts_used, p_block );

    return p_block;
}

static int PacketizeValidate( void *p_private, block_t *p_au )
{
    VLC_UNUSED(p_private);
    VLC_UNUSED(p_au);
    return VLC_SUCCESS;
}

