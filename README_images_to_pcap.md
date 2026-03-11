# Images to PCAP Application

## Overview

This application converts 16-bit grayscale PNG images representing ICEBERG TPC wire data into PCAP (packet capture) files suitable for network analysis and simulation.

### Project Structure

- **PngImageLoader.hpp/cpp** - PNG image loading and validation utility
  - Loads 16-bit grayscale PNG images
  - Validates image dimensions and format
  - Can generate test PNG images for development

- **PcapWriter.hpp/cpp** - PCAP file writing utility
  - Writes standard PCAP format compatible with tcpdump and Wireshark
  - Supports timestamped packet capture
  - Uses Ethernet data link type (DLT_EN10MB)

- **images-to-pcap.cxx** - Main application
  - Command-line interface for image selection and configuration
  - Image validation and consistency checking
  - PCAP file generation

## Image Format Requirements

### Dimensions

| Plane | TPC | Rows | Columns |
|-------|-----|------|---------|
| U     | 0/1 | 316  | 64-512* |
| V     | 0/1 | 315  | 64-512* |
| Z     | 0/1 | 240  | 64-512* |

*Must be a multiple of 64 columns

### Pixel Format

- **Bit Depth**: 16-bit unsigned integer
- **Color Type**: Grayscale
- **Byte Order**: Native endian (will be handled by libpng)
- **Data Organization**: Row-major (pixels fill row by row)

## Usage

```bash
./images-to-pcap [OPTIONS]
```

### Command-Line Options

```
  --u0 FILE              Path to U plane 0 PNG image
  --u1 FILE              Path to U plane 1 PNG image
  --v0 FILE              Path to V plane 0 PNG image
  --v1 FILE              Path to V plane 1 PNG image
  --z0 FILE              Path to Z plane 0 PNG image
  --z1 FILE              Path to Z plane 1 PNG image
  
  --image-dir DIR        Directory for auto-generated images (default: '.')
  --output FILE          Output PCAP file (default: 'output.pcap')
  --columns N            Force column count (must be multiple of 64)
  --verbose              Enable verbose output
  
  -h, --help             Show help message
```

### Examples

#### Load all provided images:
```bash
./images-to-pcap \
  --u0 u0.png --u1 u1.png \
  --v0 v0.png --v1 v1.png \
  --z0 z0.png --z1 z1.png \
  --output detector.pcap
```

#### Mix provided images with auto-generated:
```bash
./images-to-pcap \
  --u0 u0.png \
  --image-dir ./images \
  --columns 256 \
  --output partial.pcap
```

#### Generate all test images:
```bash
./images-to-pcap --columns 128 --output test.pcap
```

## Image Auto-Generation

If an image is not provided via command-line option, the application will:

1. Search for an existing PNG file in `--image-dir` with naming pattern: `<plane><tpc>x<columns>.png`
   - Example: `U0x256.png` for U plane TPC 0 with 256 columns

2. If not found and `--columns` is specified, generate a test image with:
   - Dimensions: `<columns> × <expected_rows>`
   - Pixel values: `1000 + tpc_number`
   - Saved to `--image-dir` with the standard naming pattern

## Consistency Requirements

- **All provided images must have the same column count**
- **Column count must be a multiple of 64 and at most 512**
- **Images must match their plane's expected row count**

## Building

From the workspace root:

```bash
mkdir -p build
cd build
cmake ..
make
```

The application will be installed to `./install/bin/images-to-pcap` (depending on daq-cmake configuration).

## Dependencies

- **libpng**: PNG image I/O library
- **TRACE**: DUNE DAQ logging library
- **daq-cmake**: DUNE DAQ CMake build system

## Implementation Notes

### Current Status

The application skeleton is complete with:
- ✅ PNG loading and validation
- ✅ PCAP file format implementation
- ✅ Command-line argument parsing
- ✅ Image auto-generation for testing
- 🟡 PCAP payload generation (stub implementation)

### Next Steps

To integrate with actual detector geometry:

1. **Wire-to-Channel Mapping**
   - Link with `geo::DuneApaWireReadoutGeom` for wire → channel conversion
   - Use: `PlaneWireToChannel(WireID(cry, tpc, plane, wire))`

2. **Channel-to-Crate Mapping**
   - Use `TPCChannelMap::get_crate_slot_fiber_chan_from_offline_channel()`
   - Obtain crate, slot, fiber, and channel numbers

3. **UDP Packet Construction**
   - Build WIB (Wire Interface Board) frames with channel data
   - Include timestamps and metadata
   - Proper byte ordering and padding

4. **PCAP Packet Structure**
   - Ethernet frame (14 bytes)
   - IP header (20 bytes)
   - UDP header (8 bytes)
   - WIB frame payload

## Data Flow

```
PNG Images (6 planes)
        ↓
    Validation
        ↓
    Load/Generate
        ↓
   Resolution
        ↓
For each 64-column group:
  For each wire (row):
    Get offline channel via geometry
    Get crate/slot/fiber via channel map
    Build UDP frame
    Write to PCAP
        ↓
   PCAP File
```

## Performance Considerations

- Images are loaded entirely into memory (9 × height × width × 2 bytes)
- For maximum resolution (512 columns):
  - U planes: 316 × 512 × 2 = ~324 KB each
  - V planes: 315 × 512 × 2 = ~322 KB each
  - Z planes: 240 × 512 × 2 = ~245 KB each
  - Total: ~2.7 MB per complete set

## Troubleshooting

### PNG Loading Errors

**"Invalid PNG format: must be 16-bit grayscale"**
- Ensure PNG is in grayscale mode with 16-bit depth
- Use ImageMagick to convert: `convert input.png -depth 16 -colorspace Gray output.png`

**"Cannot open PNG file"**
- Check file paths and permissions
- Verify `--image-dir` exists for auto-generated images

### Image Dimension Errors

**"wrong height"**
- U/V planes must be 316/315 rows respectively
- Z planes must be 240 rows

**"invalid width"**
- Width must be multiple of 64
- Width cannot exceed 512 columns

### PCAP Issues

**Empty or corrupted output file**
- Check disk space
- Verify write permissions to output directory
- Enable `--verbose` for more information

## Testing

With sample PNG and PCAP files provided, you can:

1. Verify PNG loading: Check console output for dimension confirmation
2. Validate image format: Test with both valid and invalid images
3. Test PCAP generation: Open output file in Wireshark or tcpdump
4. Verify geometry integration: When real geometry is integrated, test with known wire-to-channel mappings

## Future Enhancements

- [ ] Reverse operation: PCAP → PNG images (data extraction)
- [ ] Support for other image formats (FITS, HDF5)
- [ ] Real-time streaming mode
- [ ] Batch processing multiple sets of images
- [ ] Image statistics and validation reporting
- [ ] Parallel column group processing
