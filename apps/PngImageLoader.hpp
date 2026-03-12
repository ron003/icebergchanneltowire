#ifndef ICEBERG_CHANNEL_TO_WIRE_PNG_IMAGE_LOADER_HPP
#define ICEBERG_CHANNEL_TO_WIRE_PNG_IMAGE_LOADER_HPP

#include <string>
#include <vector>
#include <stdexcept>
#include <cstdint>
#include <memory>

/**
 * @brief Utility class for loading 16-bit grayscale PNG images
 */
class PngImageLoader {
public:
  struct ImageData {
    uint16_t width;
    uint16_t height;
    std::vector<uint16_t> pixels;  // 16-bit grayscale data, row-major order
  };

  /**
   * @brief Load a 16-bit grayscale PNG image
   * @param filename Path to the PNG file
   * @return ImageData structure with width, height, and pixel data
   * @throws std::runtime_error if file cannot be loaded or format is invalid
   */
  static ImageData loadImage(const std::string& filename);

  /**
   * @brief Generate a test PNG image with specified dimensions
   * @param filename Path where to save the PNG file
   * @param width Image width (must be multiple of 64, max 512)
   * @param height Image height
    * Pixel values are generated as `(row << 8) | (col & 0xff)`.
   */
  static void generateTestImage(const std::string& filename, 
                                uint16_t width, 
                        uint16_t height);
};

#endif // ICEBERG_CHANNEL_TO_WIRE_PNG_IMAGE_LOADER_HPP
