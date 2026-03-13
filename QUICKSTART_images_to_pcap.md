# Images-to-PCAP: Quick Start Guide

## What Was Created

The new `images-to-pcap` application has been added to `/workspaces/icebergchanneltowire/apps/` with the following components:

### New Files
1. **PngImageLoader.hpp/.cpp** - PNG image loading library
   - Loads 16-bit grayscale PNG images
   - Validates format and dimensions
   - Can generate test PNG files

2. **PcapWriter.hpp/.cpp** - PCAP file writer
   - Writes standard PCAP format
   - Compatible with tcpdump and Wireshark
   - Handles Ethernet packets with timestamps

3. **images-to-pcap.cxx** - Main application
   - Command-line interface with full argument parsing
   - Image validation and consistency checking
   - Auto-detection/generation of missing images
   - PCAP file output

4. **CMakeLists.txt** - Updated with PNG dependency and new build target

5. **README_images_to_pcap.md** - Comprehensive documentation

## Building the Application

```bash
cd /workspaces/build
cmake ..
make
```

The compiled application will be available at `./install/bin/images-to-pcap`

## Quick Test

### Option 1: Auto-Generate Test Images and PCAP

```bash
./images-to-pcap --columns 256 --output test.pcap --verbose
```

This will generate test PNG files and create `test.pcap` automatically.

### Option 2: Use Your Sample Images

```bash
./images-to-pcap \
  --u0 /path/to/u0.png \
  --u1 /path/to/u1.png \
  --v0 /path/to/v0.png \
  --v1 /path/to/v1.png \
  --z0 /path/to/z0.png \
  --z1 /path/to/z1.png \
  --output detector.pcap \
  --verbose
```

### Option 3: Mix Provided and Auto-Generated

```bash
./images-to-pcap \
  --u0 /path/to/u0.png \
  --u1 /path/to/u1.png \
  --image-dir ./generated_images \
   --image-prefix run42_ \
  --columns 128 \
  --output partial.pcap
```

## Verify PCAP Output

The generated PCAP file can be inspected with standard tools:

```bash
# View PCAP info
file output.pcap

# List packets with tcpdump
tcpdump -r output.pcap

# Open in Wireshark
wireshark output.pcap &
```

## Sample PNG Creation (if needed)

To create a test PNG image compatible with this application:

```bash
# Using ImageMagick
convert -size 256x316 xc:' gray -depth 16 ' test_u0.png

# Or with the application's built-in generator:
./images-to-pcap --columns 256 --verbose
# Look in current directory for U0x256.png, U1x256.png, etc.
```

## PNG Image Format Checklist

Before using custom PNG images, verify they have:

- ✓ 16-bit grayscale color type (no RGB, no 8-bit)
- ✓ Correct dimensions:
  - U planes: 316 rows
  - V planes: 315 rows
  - Z planes: 240 rows
  - Width: 64, 128, 192, 256, 320, 384, 448, or 512 columns
- ✓ Valid PNG format (signature: 89 50 4E 47)

### Convert PNG with ImageMagick

```bash
# Check format
identify -verbose image.png

# Convert to 16-bit grayscale if needed
convert image.png -colorspace Gray -depth 16 image_16bit.png

# Resize to exact dimensions
convert image_16bit.png -resize 256x316! u0_correct.png
```

## Integration with Detector Geometry

The current implementation includes stubs for:
- Wire-to-channel conversion (needs DuneApaWireReadoutGeom)
- Channel-to-crate mapping (needs TPCChannelMap)
- UDP packet construction

To complete the integration:

1. **Link geometry libraries** in CMakeLists.txt:
   ```cmake
   find_package(larcorealg REQUIRED)
   find_package(detchannelmaps REQUIRED)
   ```

2. **Update images-to-pcap.cxx** to call real geometry functions instead of stubs

3. **Build UDP/WIB packets** with actual channel data

See **README_images_to_pcap.md** section "Next Steps" for details.

## File Locations

- Application source: `/workspaces/icebergchanneltowire/apps/`
- Documentation: `/workspaces/icebergchanneltowire/README_images_to_pcap.md`
- Build directory: `/workspaces/build/`
- Installation: `/workspaces/install/bin/images-to-pcap`

## Troubleshooting

### Build Errors

**"PNG not found"**
- Install: `apt-get install libpng-dev`
- Or ensure your build environment has PNG support

**"daq-cmake not found"**
- This should be pre-configured in the daq-buildtools environment
- Verify: `echo $CMAKE_PREFIX_PATH`

### Runtime Errors

**"Cannot open PNG file"**
- Check file path and permissions
- Verify PNG format: `file image.png` should show "PNG image data, 256 x 316, 16-bit gray"

**"Column mismatch"**
- All provided images must have same width
- Use `identify image.png` to check all dimensions

## Next Development Steps

1. **Add real geometry integration** with `DuneApaWireReadoutGeom::PlaneWireToChannel()`
2. **Implement channel mapping** with `TPCChannelMap::get_crate_slot_fiber_chan_from_offline_channel()`
3. **Build WIB frame format** with proper headers and channel data
4. **Test with sample PCAP** file to verify data matches expected format
5. **Implement reverse operation** (PCAP → PNG extraction)

For detailed information, see **README_images_to_pcap.md**.
