/**
 * @file pcap-to-images2-kernel.cu
 *
 * CUDA kernel for pcap-to-images2: extracts 14-bit ADC values from packed
 * adc_words and scatters them to image pixel locations using a lookup table.
 *
 * This is a placeholder. The GPU path is activated with --gpu in
 * pcap-to-images2.  When compiled without CUDA this file is not linked.
 */

#include <cstdint>
#include <cstdio>

// Constants (must match pcap-to-images2.cxx)
static constexpr int kChannelsPerPacket  = 64;
static constexpr int kTicksPerPacket     = 64;
static constexpr int kBitsPerAdc         = 14;
static constexpr int kMax14BitAdc        = (1 << kBitsPerAdc) - 1;
static constexpr int kAdcWordsPerTs      = 14;   // 64 channels * 14 bits / 64 bits
static constexpr int kPacketsPerGroup    = 20;
static constexpr int kDestsPerEntry      = 2;
static constexpr int kFieldsPerDest      = 3;

// ---------------------------------------------------------------------------
// Device: extract a single 14-bit ADC value from packed adc_words
// ---------------------------------------------------------------------------
__device__ static inline uint16_t device_extract_adc(
    const uint8_t* adc_data,
    int channel,
    int sample)
{
  const uint64_t* words = reinterpret_cast<const uint64_t*>(
      adc_data + sample * kAdcWordsPerTs * sizeof(uint64_t));

  unsigned bit_pos  = static_cast<unsigned>(channel) * kBitsPerAdc;
  unsigned word_idx = bit_pos / 64u;
  unsigned bit_off  = bit_pos % 64u;

  uint64_t w0 = words[word_idx];

  if (bit_off + kBitsPerAdc <= 64u) {
    return static_cast<uint16_t>((w0 >> bit_off) & kMax14BitAdc);
  } else {
    unsigned bits_lo = 64u - bit_off;
    uint64_t w1 = words[word_idx + 1];
    uint64_t val = (w0 >> bit_off) | (w1 << bits_lo);
    return static_cast<uint16_t>(val & kMax14BitAdc);
  }
}

// ---------------------------------------------------------------------------
// Kernel: one thread per (image_group, sub_group, pkt_in_20, channel, sample)
// ---------------------------------------------------------------------------
// Grid dimensions should cover:
//   x: sample  [0..63]
//   y: channel [0..63]
//   z: flattened (image_group * sub_groups_per_image * 20 + sg * 20 + pkt_in_20)

__global__ void scatter_adc_kernel(
    const uint8_t*  __restrict__ adc_block,
    const size_t*   __restrict__ adc_offsets,
    const int32_t*  __restrict__ lookup,
    uint16_t*       __restrict__ image_block,
    int             packets_per_image_group,
    int             num_image_groups,
    int             columns,
    int             pixels_per_image_set)
{
  int sample  = blockIdx.x * blockDim.x + threadIdx.x;
  int channel = blockIdx.y * blockDim.y + threadIdx.y;
  int flat_z  = blockIdx.z;

  if (sample >= kTicksPerPacket || channel >= kChannelsPerPacket)
    return;

  int sub_groups_per_image = packets_per_image_group / kPacketsPerGroup;
  int total_z = num_image_groups * sub_groups_per_image * kPacketsPerGroup;
  if (flat_z >= total_z)
    return;

  int ig        = flat_z / (sub_groups_per_image * kPacketsPerGroup);
  int remainder = flat_z % (sub_groups_per_image * kPacketsPerGroup);
  int sg        = remainder / kPacketsPerGroup;
  int pkt_in_20 = remainder % kPacketsPerGroup;

  int col = sg * kTicksPerPacket + sample;
  if (col >= columns)
    return;

  // Global packet index
  int global_pkt = ig * packets_per_image_group + sg * kPacketsPerGroup + pkt_in_20;
  const uint8_t* adc_data = adc_block + adc_offsets[global_pkt];

  uint16_t adc = device_extract_adc(adc_data, channel, sample);

  // Lookup destinations
  int lk_base = (pkt_in_20 * kChannelsPerPacket + channel) * kDestsPerEntry * kFieldsPerDest;
  int image_set_pixel_base = ig * pixels_per_image_set;

  for (int d = 0; d < kDestsPerEntry; ++d) {
    int img_idx = lookup[lk_base + d * kFieldsPerDest + 0];
    int row     = lookup[lk_base + d * kFieldsPerDest + 1];
    if (img_idx < 0) continue;

    // image_row_base inlined (must match the host code)
    int row_base;
    switch (img_idx) {
      case 0: row_base = 0;                     break; // U0
      case 1: row_base = 316;                   break; // U1
      case 2: row_base = 632;                   break; // V0
      case 3: row_base = 947;                   break; // V1
      case 4: row_base = 1262;                  break; // Z0
      case 5: row_base = 1502;                  break; // Z1
      default: continue;
    }

    int row_in_set = row_base + row;
    int px = image_set_pixel_base + row_in_set * columns + col;
    image_block[px] = adc;
  }
}

// ---------------------------------------------------------------------------
// Host entry point (called from pcap-to-images2.cxx when --gpu is used)
// ---------------------------------------------------------------------------

extern "C" void scatter_adc_to_images_gpu(
    const uint8_t* adc_block,
    const size_t*  adc_offsets,
    const int32_t* lookup,
    uint16_t*      image_block,
    uint32_t       packets_per_image_group,
    uint32_t       num_image_groups,
    uint16_t       columns,
    uint32_t       pixels_per_image_set)
{
  // --- Allocate device memory ---
  size_t total_packets = static_cast<size_t>(num_image_groups) * packets_per_image_group;
  size_t adc_block_size    = total_packets * kAdcWordsPerTs * kTicksPerPacket * sizeof(uint64_t);
  size_t offsets_size      = total_packets * sizeof(size_t);
  size_t lookup_size       = kPacketsPerGroup * kChannelsPerPacket
                             * kDestsPerEntry * kFieldsPerDest * sizeof(int32_t);
  size_t image_block_size  = static_cast<size_t>(num_image_groups) * pixels_per_image_set
                             * sizeof(uint16_t);

  uint8_t*  d_adc_block    = nullptr;
  size_t*   d_adc_offsets  = nullptr;
  int32_t*  d_lookup       = nullptr;
  uint16_t* d_image_block  = nullptr;

  cudaMalloc(&d_adc_block,   adc_block_size);
  cudaMalloc(&d_adc_offsets, offsets_size);
  cudaMalloc(&d_lookup,      lookup_size);
  cudaMalloc(&d_image_block, image_block_size);

  cudaMemcpy(d_adc_block,   adc_block,   adc_block_size,   cudaMemcpyHostToDevice);
  cudaMemcpy(d_adc_offsets, adc_offsets, offsets_size,      cudaMemcpyHostToDevice);
  cudaMemcpy(d_lookup,      lookup,      lookup_size,       cudaMemcpyHostToDevice);
  cudaMemset(d_image_block, 0,           image_block_size);

  // --- Launch kernel ---
  int sub_groups_per_image = packets_per_image_group / kPacketsPerGroup;
  int total_z = num_image_groups * sub_groups_per_image * kPacketsPerGroup;

  dim3 threads(kTicksPerPacket, kChannelsPerPacket, 1);  // 64 x 64 x 1 = 4096 threads
  // Note: 4096 exceeds typical max threads per block (1024).
  // A production implementation would tile this differently.
  // For now, use 16x16 threads with grid covering the remainder.
  dim3 block_dim(16, 16, 1);
  dim3 grid_dim(
      (kTicksPerPacket + block_dim.x - 1) / block_dim.x,     // 4
      (kChannelsPerPacket + block_dim.y - 1) / block_dim.y,   // 4
      total_z);

  scatter_adc_kernel<<<grid_dim, block_dim>>>(
      d_adc_block, d_adc_offsets, d_lookup, d_image_block,
      static_cast<int>(packets_per_image_group),
      static_cast<int>(num_image_groups),
      static_cast<int>(columns),
      static_cast<int>(pixels_per_image_set));

  cudaDeviceSynchronize();

  // Check for errors
  cudaError_t err = cudaGetLastError();
  if (err != cudaSuccess) {
    fprintf(stderr, "CUDA error: %s\n", cudaGetErrorString(err));
  }

  // --- Copy results back ---
  cudaMemcpy(image_block, d_image_block, image_block_size, cudaMemcpyDeviceToHost);

  // --- Free device memory ---
  cudaFree(d_adc_block);
  cudaFree(d_adc_offsets);
  cudaFree(d_lookup);
  cudaFree(d_image_block);
}
