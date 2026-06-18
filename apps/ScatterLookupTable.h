#ifndef SCATTER_LOOKUP_TABLE_H
#define SCATTER_LOOKUP_TABLE_H

#include <cstdint>

#ifdef __CUDACC__
#define SLT_HOST_DEVICE __host__ __device__
#else
#define SLT_HOST_DEVICE
#endif

// Constants shared between pcap-to-images2.cxx and pcap-to-images2-kernel.cu.
static constexpr int kChannelsPerPacket = 64;
static constexpr int kTicksPerPacket    = 64;
static constexpr int kBitsPerAdc        = 14;
static constexpr int kMax14BitAdc       = (1 << kBitsPerAdc) - 1;
static constexpr int kAdcWordsPerTs     = 14;   // 64 channels * 14 bits / 64 bits
static constexpr int kPacketsPerGroup   = 20;
static constexpr int kDestsPerEntry     = 2;
static constexpr int kFieldsPerDest     = 3;

// One destination in the scatter lookup table: identifies a pixel row in one
// of the 6 images (U0, U1, V0, V1, Z0, Z1).
struct LookupDest {
    int32_t image_idx;   // 0-5, or -1 if this destination is unused
    int32_t row;         // wire row within the image
    int32_t col_offset;  // reserved (always 0; column comes from time sample)
};

// One entry in the scatter lookup table.  The two dimensions that index into
// the table are (packet_index_within_group [0..19], stream_channel [0..63]).
// Each entry holds up to 2 destinations (for wrapped wires).
struct LookupEntry {
    LookupDest dest[kDestsPerEntry];
};

static_assert(sizeof(LookupDest)  == kFieldsPerDest * sizeof(int32_t),
              "LookupDest must be 3 contiguous int32_t");
static_assert(sizeof(LookupEntry) == kDestsPerEntry * sizeof(LookupDest),
              "LookupEntry must be 2 contiguous LookupDest");

static constexpr int kLookupEntries   = kPacketsPerGroup * kChannelsPerPacket; // 1280
static constexpr int kLookupTotalInts = kLookupEntries * kDestsPerEntry * kFieldsPerDest;

// Row offset of image `image_idx` within one contiguous set of 6 images.
// Images are laid out: U0(316), U1(316), V0(315), V1(315), Z0(240), Z1(240).
SLT_HOST_DEVICE inline int image_row_base_for_lut(int image_idx) {
    switch (image_idx) {
        case 0: return 0;
        case 1: return 316;
        case 2: return 632;
        case 3: return 947;
        case 4: return 1262;
        case 5: return 1502;
        default: return 0;
    }
}

#endif // SCATTER_LOOKUP_TABLE_H
