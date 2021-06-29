/*****************************************************************************
 * vvc_nal.h: h.266/vvc nal types description
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

#ifndef __VVC_NAL_H__
#define __VVC_NAL_H__

enum vvc_nal_unit_type_e
{
  VVC_NAL_CODED_SLICE_TRAIL = 0,   // 0
  VVC_NAL_CODED_SLICE_STSA,        // 1
  VVC_NAL_CODED_SLICE_RADL,        // 2
  VVC_NAL_CODED_SLICE_RASL,        // 3

  VVC_NAL_RESERVED_VCL_4,
  VVC_NAL_RESERVED_VCL_5,
  VVC_NAL_RESERVED_VCL_6,

  VVC_NAL_CODED_SLICE_IDR_W_RADL,  // 7
  VVC_NAL_CODED_SLICE_IDR_N_LP,    // 8
  VVC_NAL_CODED_SLICE_CRA,         // 9
  VVC_NAL_CODED_SLICE_GDR,         // 10

  VVC_NAL_RESERVED_IRAP_VCL_11,
  VVC_NAL_OPI,                     // 12
  VVC_NAL_DCI,                     // 13
  VVC_NAL_VPS,                     // 14
  VVC_NAL_SPS,                     // 15
  VVC_NAL_PPS,                     // 16
  VVC_NAL_PREFIX_APS,              // 17
  VVC_NAL_SUFFIX_APS,              // 18
  VVC_NAL_PH,                      // 19
  VVC_NAL_ACCESS_UNIT_DELIMITER,   // 20
  VVC_NAL_EOS,                     // 21
  VVC_NAL_EOB,                     // 22
  VVC_NAL_PREFIX_SEI,              // 23
  VVC_NAL_SUFFIX_SEI,              // 24
  VVC_NAL_FD,                      // 25

  VVC_NAL_RESERVED_NVCL_26,
  VVC_NAL_RESERVED_NVCL_27,

  VVC_NAL_UNSPECIFIED_28,
  VVC_NAL_UNSPECIFIED_29,
  VVC_NAL_UNSPECIFIED_30,
  VVC_NAL_UNSPECIFIED_31,
  VVC_NAL_INVALID
};

#endif // __VVC_NAL_H__
