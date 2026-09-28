// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_JXL_DEC_GROUP_H_
#define LIB_JXL_DEC_GROUP_H_

#include <cstddef>
#include <memory>
#include <vector>

#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/dct_util.h"
#include "lib/jxl/dec_bit_reader.h"
#include "lib/jxl/dec_cache.h"
#include "lib/jxl/frame_header.h"
#include "lib/jxl/jpeg/jpeg_data.h"
#include "lib/jxl/render_pipeline/render_pipeline.h"
#include <cstdint>
#include <map>

namespace jxl {

struct AuxOut;

// Optional per-frame coefficient statistics, enabled with the JXL_COEFF_STATS environment variable
// and printed by FrameDecoder::DumpCoefficientStats. The AC half is accumulated in dec_group.cc,
// which is the only place where a whole quantized varblock exists before dequantization, and the DC
// half in dec_modular.cc, where the integers of the modular stream exist before DequantDC. Groups
// are decoded in parallel, so both accumulators are merged under a lock, once per group
// Coefficient magnitudes whose run lengths we track: 0, 1 and 2
constexpr int32_t kCoeffRunMagnitudes = 3;

struct AcCoeffStats {
  // Per XYB channel, over the coefficients the entropy coder actually walks: everything of the block
  // up to and including its last non-zero coefficient, in coefficient order
  uint64_t non_zeros[3] = {0, 0, 0};
  uint64_t other_zeros[3] = {0, 0, 0};
  // Coefficients after the last non-zero one of the block, which the bitstream never spells out
  uint64_t tail_zeros[3] = {0, 0, 0};
  // Value histogram of the walked coefficients, i.e. of non_zeros plus other_zeros
  std::map<int32_t, uint64_t> histogram[3];
  // Length histograms of the runs of contiguous coefficients of the same small magnitude among the
  // walked ones: index 0 holds the runs of zeros, 1 the runs of |q|==1, 2 the runs of |q|==2. A run
  // is bounded by a coefficient of another magnitude or by the start of the block's AC region, and
  // the walk stops at the block's last non-zero, so the tail of zeros is not part of any run. The
  // second index is the XYB channel
  std::map<int32_t, uint64_t> value_runs[kCoeffRunMagnitudes][3];
};

struct DcCoeffStats {
  // One LLF integer per 8x8 cell and per channel, which is what the modular stream codes
  uint64_t count[3] = {0, 0, 0};
  std::map<int32_t, uint64_t> histogram[3];
};

// Reads the environment switch once, so that the decoding loops only ever test a bool
bool CollectingCoeffStats();

void MergeAcCoeffStats(const AcCoeffStats& group_stats);
void MergeDcCoeffStats(const DcCoeffStats& group_stats);

// Hand over the accumulated statistics and reset the accumulator, so that a multi-frame file reports
// one set of numbers per frame rather than a running total
AcCoeffStats TakeAcCoeffStats();
DcCoeffStats TakeDcCoeffStats();

Status DecodeGroup(const FrameHeader& frame_header,
                   BitReader* JXL_RESTRICT* JXL_RESTRICT readers,
                   size_t num_passes, size_t group_idx,
                   PassesDecoderState* JXL_RESTRICT dec_state,
                   GroupDecCache* JXL_RESTRICT group_dec_cache, size_t thread,
                   RenderPipelineInput& render_pipeline_input,
                   jpeg::JPEGData* JXL_RESTRICT jpeg_data, size_t first_pass,
                   bool force_draw, bool dc_only, bool* should_run_pipeline);

Status DecodeGroupForRoundtrip(const FrameHeader& frame_header,
                               const std::vector<std::unique_ptr<ACImage>>& ac,
                               size_t group_idx,
                               PassesDecoderState* JXL_RESTRICT dec_state,
                               GroupDecCache* JXL_RESTRICT group_dec_cache,
                               size_t thread,
                               RenderPipelineInput& render_pipeline_input,
                               jpeg::JPEGData* JXL_RESTRICT jpeg_data,
                               AuxOut* aux_out);

}  // namespace jxl

#endif  // LIB_JXL_DEC_GROUP_H_
