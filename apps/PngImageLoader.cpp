#include "PngImageLoader.hpp"
#include <png.h>
#include <fstream>
#include <cstring>
#include "TRACE/trace.h"

struct PngReadState {
  std::ifstream file;
  const uint8_t* data;
  size_t pos;
};

// Callback for libpng to read from file
static void png_read_callback(png_structp png_ptr, png_bytep data, png_size_t length) {
  PngReadState* state = static_cast<PngReadState*>(png_get_io_ptr(png_ptr));
  state->file.read(reinterpret_cast<char*>(data), length);
  if (state->file.fail() && !state->file.eof()) {
    png_error(png_ptr, "Read error");
  }
}

PngImageLoader::ImageData PngImageLoader::loadImage(const std::string& filename) {
  ImageData result = {0, 0, {}};
  
  // Open the file
  std::ifstream file(filename, std::ios::binary);
  if (!file.is_open()) {
    throw std::runtime_error("Cannot open PNG file: " + filename);
  }

  // Read and validate PNG signature
  png_byte sig[8];
  file.read(reinterpret_cast<char*>(sig), 8);
  if (png_sig_cmp(sig, 0, 8)) {
    throw std::runtime_error("Invalid PNG file: " + filename);
  }

  // Create PNG structures
  png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
  if (!png) {
    throw std::runtime_error("Failed to create PNG read structure");
  }

  png_infop info = png_create_info_struct(png);
  if (!info) {
    png_destroy_read_struct(&png, nullptr, nullptr);
    throw std::runtime_error("Failed to create PNG info structure");
  }

  try {
    // Set up error handling
    if (setjmp(png_jmpbuf(png))) {
      throw std::runtime_error("PNG reading error: " + filename);
    }

    // Set read callback
    PngReadState state;
    state.file.open(filename, std::ios::binary);
    state.file.read(reinterpret_cast<char*>(sig), 8);  // Skip signature
    png_set_read_fn(png, &state, png_read_callback);
    png_set_sig_bytes(png, 8);

    // Read PNG info
    png_read_info(png, info);

    result.width = png_get_image_width(png, info);
    result.height = png_get_image_height(png, info);
    png_byte bit_depth = png_get_bit_depth(png, info);
    png_byte color_type = png_get_color_type(png, info);

    // Validate format: must be 16-bit grayscale
    if (color_type != PNG_COLOR_TYPE_GRAY || bit_depth != 16) {
      throw std::runtime_error(
        "Invalid PNG format: must be 16-bit grayscale. Got color_type=" +
        std::to_string(color_type) + ", bit_depth=" + std::to_string(bit_depth)
      );
    }

    // Set up transformations for big-endian 16-bit data
    png_set_swap(png);  // Convert big-endian to native byte order

    // Update the info structure
    png_read_update_info(png, info);

    // Read the image data
    result.pixels.resize(result.width * result.height);
    std::vector<png_bytep> row_pointers(result.height);
    
    for (uint32_t y = 0; y < result.height; ++y) {
      row_pointers[y] = reinterpret_cast<png_bytep>(
        &result.pixels[y * result.width]
      );
    }

    png_read_image(png, row_pointers.data());
    png_read_end(png, info);

  } catch (...) {
    png_destroy_read_struct(&png, &info, nullptr);
    throw;
  }

  png_destroy_read_struct(&png, &info, nullptr);
  return result;
}

// Callback for libpng to write to file
static void png_write_callback(png_structp png_ptr, png_bytep data, png_size_t length) {
  std::ofstream* file = static_cast<std::ofstream*>(png_get_io_ptr(png_ptr));
  file->write(reinterpret_cast<char*>(data), length);
  if (file->fail()) {
    png_error(png_ptr, "Write error");
  }
}

void PngImageLoader::generateTestImage(const std::string& filename,
                                       uint16_t width,
                                       uint16_t height) {
  // Validate dimensions
  if (width % 64 != 0 || width > 512 || width == 0) {
    throw std::runtime_error("Width must be a multiple of 64 and at most 512");
  }

  // Create PNG structures
  png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
  if (!png) {
    throw std::runtime_error("Failed to create PNG write structure");
  }

  png_infop info = png_create_info_struct(png);
  if (!info) {
    png_destroy_write_struct(&png, nullptr);
    throw std::runtime_error("Failed to create PNG info structure");
  }

  // Open output file
  std::ofstream file(filename, std::ios::binary);
  if (!file.is_open()) {
    png_destroy_write_struct(&png, &info);
    throw std::runtime_error("Cannot create PNG file: " + filename);
  }

  try {
    // Set up error handling
    if (setjmp(png_jmpbuf(png))) {
      throw std::runtime_error("PNG writing error: " + filename);
    }

    // Set write callback
    png_set_write_fn(png, &file, png_write_callback, nullptr);

    // Set PNG info
    png_set_IHDR(
      png, info,
      width, height,
      16,                    // bit_depth
      PNG_COLOR_TYPE_GRAY,   // color_type
      PNG_INTERLACE_NONE,
      PNG_COMPRESSION_TYPE_DEFAULT,
      PNG_FILTER_TYPE_DEFAULT
    );

    // Keep generated test images close to raw size for easier inspection.
    png_set_compression_level(png, 0);
    png_set_filter(png, PNG_FILTER_TYPE_BASE, PNG_FILTER_NONE);

    png_write_info(png, info);

    // Set byte order for 16-bit values
    png_set_swap(png);

    // Encode the row in the high byte and the column in the low byte.
    std::vector<uint16_t> image_data(static_cast<size_t>(width) * height);
    std::vector<png_bytep> row_pointers(height);

    TLOG_DEBUG(1) << "Generating test image: " << filename
                 << " (" << width << "x" << height
                 << ") with value formula (row << 8) | (col & 0xff)";
    for (uint32_t y = 0; y < height; ++y) {
      for (uint32_t x = 0; x < width; ++x) {
        image_data[(static_cast<size_t>(y) * width) + x] =
          static_cast<uint16_t>(((x & 0x3f) << 8) | (y & 0xff));
      }
      row_pointers[y] = reinterpret_cast<png_bytep>(
        image_data.data() + (static_cast<size_t>(y) * width)
      );
    }

    png_write_image(png, row_pointers.data());

    png_write_end(png, nullptr);

  } catch (...) {
    png_destroy_write_struct(&png, &info);
    throw;
  }

  png_destroy_write_struct(&png, &info);
}

void PngImageLoader::saveImage(const std::string& filename,
                               uint16_t width,
                               uint16_t height,
                               const std::vector<uint16_t>& pixels) {
  // Validate input
  if (pixels.size() != static_cast<size_t>(width) * height) {
    throw std::runtime_error("Pixel data size mismatch: expected " +
                            std::to_string(static_cast<size_t>(width) * height) +
                            ", got " + std::to_string(pixels.size()));
  }

  // Create PNG structures
  png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
  if (!png) {
    throw std::runtime_error("Failed to create PNG write structure");
  }

  png_infop info = png_create_info_struct(png);
  if (!info) {
    png_destroy_write_struct(&png, nullptr);
    throw std::runtime_error("Failed to create PNG info structure");
  }

  // Open output file
  std::ofstream file(filename, std::ios::binary);
  if (!file.is_open()) {
    png_destroy_write_struct(&png, &info);
    throw std::runtime_error("Cannot create PNG file: " + filename);
  }

  try {
    // Set up error handling
    if (setjmp(png_jmpbuf(png))) {
      throw std::runtime_error("PNG writing error: " + filename);
    }

    // Set write callback
    png_set_write_fn(png, &file, png_write_callback, nullptr);

    // Set PNG info
    png_set_IHDR(
      png, info,
      width, height,
      16,                    // bit_depth
      PNG_COLOR_TYPE_GRAY,   // color_type
      PNG_INTERLACE_NONE,
      PNG_COMPRESSION_TYPE_DEFAULT,
      PNG_FILTER_TYPE_DEFAULT
    );

    // Keep output images close to raw size for easier inspection (no compression).
    png_set_compression_level(png, 0);
    png_set_filter(png, PNG_FILTER_TYPE_BASE, PNG_FILTER_NONE);

    png_write_info(png, info);

    // Set byte order for 16-bit values
    png_set_swap(png);

    // Set up row pointers
    std::vector<png_bytep> row_pointers(height);
    for (uint32_t y = 0; y < height; ++y) {
      row_pointers[y] = reinterpret_cast<png_bytep>(
        const_cast<uint16_t*>(pixels.data() + (static_cast<size_t>(y) * width))
      );
    }

    png_write_image(png, row_pointers.data());

    png_write_end(png, nullptr);

  } catch (...) {
    png_destroy_write_struct(&png, &info);
    throw;
  }

  png_destroy_write_struct(&png, &info);
}
