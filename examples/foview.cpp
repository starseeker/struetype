/*
 * foview.cpp - Generate PNG and PDF images showing all available glyphs in a font
 *
 * This example demonstrates how to:
 * 1. Load a TrueType font using struetype.h
 * 2. Scan the entire Unicode range (0-0x10FFFF) to find all available glyphs
 * 3. Render ALL glyphs that exist in the font using stt_GetCodepointBitmap
 * 4. Arrange glyphs in a grid layout with optional grid lines
 * 5. Center each glyph visually in its grid cell using font metrics
 * 6. Render characters darker on light background using simple subtraction
 * 7. Convert grayscale bitmap to RGB format
 * 8. Generate appropriate output based on glyph count:
 *    - Single page: Both PNG and PDF output
 *    - Multiple pages: PDF-only output with all pages
 * 9. Smart output naming: single file as .png/.pdf, multiple as .pdf only
 * 10. Add footer with font name and Unicode range using embedded ProFont
 *
 * Key rendering features:
 * - Uses font ascent/descent metrics to calculate proper baseline positioning
 * - Centers glyphs horizontally and vertically for visual consistency
 * - Ignores xOffset/yOffset for cleaner visual centering
 * - Simple subtraction blending for crisp, dark text on light background
 * - Scans entire Unicode range using stt_FindGlyphIndex to find available glyphs
 * - Target maximum image size of 1500x2000 pixels (portrait, 300dpi print ready)
 * - Adaptive sizing: single-page output sized to fit content, multi-page uses uniform dimensions
 * - Footer displays font name and Unicode range using embedded ProFont
 */

#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <fstream>
#include <algorithm>
#include <cstring>
#include <libgen.h>

#define STRUETYPE_IMPLEMENTATION
#include "struetype.h"
#include "svpng.h"

// Include modern PDF library
#include "pdfimg.hpp"

#include "cxxopts.hpp"

// Embedded ProFont.ttf data for footer rendering
#include "profont_embedded.h"

// Structure to hold image data
class ImagePage {
private:
    std::vector<uint8_t> gray_buffer_;
    std::vector<uint8_t> rgb_buffer_;
    int width_;
    int height_;
    int start_codepoint_;
    int end_codepoint_;
    std::string font_name_;

public:
    ImagePage(int width, int height, int start_cp, int end_cp, const std::string& font_name)
        : width_(width), height_(height), start_codepoint_(start_cp), 
          end_codepoint_(end_cp), font_name_(font_name) {
        gray_buffer_.resize(width * height, 240);  // Light gray background
        rgb_buffer_.resize(width * height * 3);
    }

    // Accessors
    std::vector<uint8_t>& gray_buffer() { return gray_buffer_; }
    const std::vector<uint8_t>& gray_buffer() const { return gray_buffer_; }
    std::vector<uint8_t>& rgb_buffer() { return rgb_buffer_; }
    const std::vector<uint8_t>& rgb_buffer() const { return rgb_buffer_; }
    
    int width() const { return width_; }
    int height() const { return height_; }
    int start_codepoint() const { return start_codepoint_; }
    int end_codepoint() const { return end_codepoint_; }
    const std::string& font_name() const { return font_name_; }
    
    void convert_gray_to_rgb() {
        for (size_t i = 0; i < gray_buffer_.size(); ++i) {
            uint8_t gray = gray_buffer_[i];
            rgb_buffer_[i * 3] = gray;
            rgb_buffer_[i * 3 + 1] = gray;
            rgb_buffer_[i * 3 + 2] = gray;
        }
    }
};

// Font renderer class using RAII
class FontRenderer {
private:
    std::unique_ptr<uint8_t[]> font_buffer_;
    stt_fontinfo font_info_;
    bool initialized_;

public:
    FontRenderer(const std::string& font_path) : initialized_(false) {
        // Load font file
        std::ifstream file(font_path, std::ios::binary | std::ios::ate);
        if (!file) {
            throw std::runtime_error("Failed to open font file: " + font_path);
        }

        auto size = file.tellg();
        file.seekg(0, std::ios::beg);
        
        font_buffer_ = std::make_unique<uint8_t[]>(size);
        if (!file.read(reinterpret_cast<char*>(font_buffer_.get()), size)) {
            throw std::runtime_error("Failed to read font file: " + font_path);
        }

        if (!stt_InitFont(&font_info_, font_buffer_.get(), size, 0)) {
            throw std::runtime_error("Failed to initialize font");
        }
        
        initialized_ = true;
    }

    ~FontRenderer() = default;
    
    // Non-copyable but moveable
    FontRenderer(const FontRenderer&) = delete;
    FontRenderer& operator=(const FontRenderer&) = delete;
    FontRenderer(FontRenderer&&) = default;
    FontRenderer& operator=(FontRenderer&&) = default;

    stt_fontinfo* get_font_info() { return &font_info_; }
    bool is_initialized() const { return initialized_; }
    
    std::vector<int> collect_available_glyphs() const {
        std::vector<int> glyphs;
        glyphs.reserve(1000);  // Reserve reasonable capacity
        
        // Scan the entire Unicode range
        for (int codepoint = 0; codepoint <= 0x10FFFF; ++codepoint) {
            if (stt_FindGlyphIndex(&font_info_, codepoint) != 0) {
                glyphs.push_back(codepoint);
            }
        }
        
        return glyphs;
    }
};

// Utility functions
std::string get_output_prefix(const std::string& font_path, const std::string& user_prefix) {
    if (!user_prefix.empty()) {
        return user_prefix;
    }
    
    // Extract filename without extension from font path
    char* font_path_copy = strdup(font_path.c_str());
    char* base_name = basename(font_path_copy);
    std::string result(base_name);
    
    // Remove extension
    size_t dot_pos = result.find_last_of('.');
    if (dot_pos != std::string::npos) {
        result = result.substr(0, dot_pos);
    }
    
    free(font_path_copy);
    return result;
}

std::string get_font_name(const std::string& font_path) {
    char* font_path_copy = strdup(font_path.c_str());
    char* base_name = basename(font_path_copy);
    std::string result(base_name);
    
    // Remove extension
    size_t dot_pos = result.find_last_of('.');
    if (dot_pos != std::string::npos) {
        result = result.substr(0, dot_pos);
    }
    
    free(font_path_copy);
    return result;
}

// Render footer text using embedded ProFont
void render_footer(ImagePage& page, int footer_height) {
    auto& buffer = page.gray_buffer();
    const int image_width = page.width();
    const int image_height = page.height();
    
    // Create footer text
    std::string footer_text = "Font: " + page.font_name() + 
                             " U+" + std::to_string(page.start_codepoint()) + 
                             "-U+" + std::to_string(page.end_codepoint());
    
    // Initialize embedded ProFont for footer rendering
    stt_fontinfo footer_font;
    if (!stt_InitFont(&footer_font, profont_ttf_data, profont_ttf_data_len, 0)) {
        return;  // Skip footer if ProFont fails to load
    }
    
    // Calculate font scaling for footer
    float footer_scale = stt_ScaleForPixelHeight(&footer_font, 14);
    
    // Get font metrics
    int ascent, descent, lineGap;
    stt_GetFontVMetrics(&footer_font, &ascent, &descent, &lineGap);
    
    // Calculate text dimensions
    int text_width = 0;
    for (char c : footer_text) {
        int advance, leftSideBearing;
        stt_GetCodepointHMetrics(&footer_font, c, &advance, &leftSideBearing);
        text_width += static_cast<int>(advance * footer_scale);
    }
    
    // Position footer text (right-aligned)
    int footer_y = image_height - footer_height + (footer_height + static_cast<int>(ascent * footer_scale)) / 2;
    int footer_x = image_width - text_width - 20;  // 20 pixel margin from right
    
    // Render each character
    int current_x = footer_x;
    for (char c : footer_text) {
        int glyph_width, glyph_height, x_offset, y_offset;
        uint8_t* glyph_bitmap = stt_GetCodepointBitmap(&footer_font, footer_scale, footer_scale,
                                                       c, &glyph_width, &glyph_height,
                                                       &x_offset, &y_offset);
        
        if (glyph_bitmap) {
            // Calculate glyph position
            int glyph_x = current_x + x_offset;
            int glyph_y = footer_y + y_offset;
            
            // Copy glyph bitmap to main image buffer
            for (int gy = 0; gy < glyph_height; ++gy) {
                for (int gx = 0; gx < glyph_width; ++gx) {
                    int image_x = glyph_x + gx;
                    int image_y = glyph_y + gy;
                    
                    // Check bounds
                    if (image_x >= 0 && image_x < image_width &&
                        image_y >= 0 && image_y < image_height) {
                        uint8_t glyph_pixel = glyph_bitmap[gy * glyph_width + gx];
                        
                        // Render characters darker
                        int bg_pixel = buffer[image_y * image_width + image_x];
                        int darkened = bg_pixel - glyph_pixel;
                        if (darkened < 0) darkened = 0;
                        buffer[image_y * image_width + image_x] = static_cast<uint8_t>(darkened);
                    }
                }
            }
            
            // Advance to next character position
            int advance, leftSideBearing;
            stt_GetCodepointHMetrics(&footer_font, c, &advance, &leftSideBearing);
            current_x += static_cast<int>(advance * footer_scale);
            
            // Free the glyph bitmap
            stt_FreeBitmap(glyph_bitmap, nullptr);
        }
    }
}

// Parse compression type from string
pdfimg::CompressionType parse_compression_type(const std::string& str) {
    if (str == "none") return pdfimg::CompressionType::None;
    if (str == "flate") {
#ifdef ENABLE_MINIZ_COMPRESSION
        return pdfimg::CompressionType::Flate;
#else
        throw std::runtime_error("Flate compression not available (build with ENABLE_MINIZ_COMPRESSION=ON)");
#endif
    }
    if (str == "jpeg") {
#ifdef ENABLE_TOOJPEG_COMPRESSION
        return pdfimg::CompressionType::JPEG;
#else
        throw std::runtime_error("JPEG compression not available (build with ENABLE_TOOJPEG_COMPRESSION=ON)");
#endif
    }
    throw std::runtime_error("Invalid compression method: " + str);
}

// Get default compression type
pdfimg::CompressionType get_default_compression() {
#ifdef ENABLE_MINIZ_COMPRESSION
    return pdfimg::CompressionType::Flate;
#elif defined(ENABLE_TOOJPEG_COMPRESSION)
    return pdfimg::CompressionType::JPEG;
#else
    return pdfimg::CompressionType::None;
#endif
}

// Get available compression methods as string
std::string get_available_compression_methods() {
    std::string methods = "none";
#ifdef ENABLE_MINIZ_COMPRESSION
    methods += ", flate";
#endif
#ifdef ENABLE_TOOJPEG_COMPRESSION
    methods += ", jpeg";
#endif
    return methods;
}

// Convert compression type to string
std::string compression_type_to_string_local(pdfimg::CompressionType type) {
    return pdfimg::compression_type_to_string(type);
}

int main(int argc, const char* argv[]) {
    try {
        // Configuration constants
        constexpr int cell_width = 48;
        constexpr int cell_height = 48;
        constexpr int font_size = 24;
        constexpr bool draw_grid_lines = true;
        constexpr int max_image_width = 1500;
        constexpr int max_image_height = 2000;
        constexpr int footer_height = 80;
        
        // Calculate grid limits
        constexpr int available_height = max_image_height - footer_height;
        constexpr int max_grid_cols = max_image_width / cell_width;
        constexpr int max_grid_rows = available_height / cell_height;
        constexpr int max_glyphs_per_file = max_grid_cols * max_grid_rows;
        
        // Default values
        std::string font_path = "profont/ProFont.ttf";
        std::string output_prefix;
        pdfimg::CompressionType compression = get_default_compression();
        
        // Set up command line options
        cxxopts::Options options("foview", "Generate PNG and/or PDF images showing all available glyphs in a font");
        
        options.add_options()
            ("f,font", "TrueType font file", cxxopts::value<std::string>()->default_value("profont/ProFont.ttf"))
            ("o,output", "Output file prefix (default: derived from font filename)", cxxopts::value<std::string>())
            ("c,compression", "Compression method for PDF images: " + get_available_compression_methods(), 
             cxxopts::value<std::string>()->default_value(compression_type_to_string_local(get_default_compression())))
            ("h,help", "Show this help message")
            ("positional", "Positional arguments", cxxopts::value<std::vector<std::string>>())
            ;
        
        // Allow positional arguments
        options.parse_positional({"positional"});
        
        // Parse command line arguments
        auto result = options.parse(argc, argv);
        
        // Handle help
        if (result.count("help")) {
            std::cout << options.help() << std::endl;
            std::cout << "\nUsage flexibility:" << std::endl;
            std::cout << "  Font file and output prefix can be specified as positional arguments:" << std::endl;
            std::cout << "  foview [font_file] [output_prefix]" << std::endl;
            std::cout << "  Or use named options:" << std::endl;
            std::cout << "  foview -f font_file -o output_prefix" << std::endl;
            std::cout << "  Options and positional arguments can be mixed." << std::endl;
            std::cout << "\nOutput behavior:" << std::endl;
            std::cout << "  Single page:  Creates both <prefix>.png and <prefix>.pdf" << std::endl;
            std::cout << "  Multiple pages: Creates only <prefix>.pdf with all pages" << std::endl;
            std::cout << "\nPDF Features:" << std::endl;
            std::cout << "  - Strict PDF 1.4 compliance for maximum compatibility" << std::endl;
            std::cout << "  - Configurable image compression (" << get_available_compression_methods() << ")" << std::endl;
            std::cout << "\nExamples:" << std::endl;
            std::cout << "  foview                                    # Uses default font and compression" << std::endl;
            std::cout << "  foview arial.ttf                         # Font as positional argument" << std::endl;
            std::cout << "  foview arial.ttf myfont                  # Font and output prefix as positional" << std::endl;
            std::cout << "  foview -f arial.ttf                      # Uses arial.ttf font with named option" << std::endl;
            std::cout << "  foview -f arial.ttf -o myfont            # Custom output prefix with named options" << std::endl;
            std::cout << "  foview -c flate arial.ttf                # Mixed named and positional arguments" << std::endl;
            return 0;
        }
        
        // Get values from named options
        font_path = result["font"].as<std::string>();
        if (result.count("output")) {
            output_prefix = result["output"].as<std::string>();
        }
        std::string compression_str = result["compression"].as<std::string>();
        
        // Handle backward compatibility with positional arguments
        if (result.count("positional")) {
            const auto& positional = result["positional"].as<std::vector<std::string>>();
            if (!positional.empty()) {
                font_path = positional[0];
                if (positional.size() > 1) {
                    output_prefix = positional[1];
                }
            }
        }
        
        // Parse compression type
        compression = parse_compression_type(compression_str);
        
        // Get final output prefix
        std::string final_output_prefix = get_output_prefix(font_path, output_prefix);
        
        std::cout << "Font: " << font_path << std::endl;
        std::cout << "Output prefix: " << final_output_prefix << std::endl;
        std::cout << "Compression: " << compression_str << std::endl;
        std::cout << "Max glyphs per file: " << max_glyphs_per_file << " (grid: " 
                  << max_grid_cols << "x" << max_grid_rows << ")" << std::endl;
        
        // Initialize font renderer
        FontRenderer font_renderer(font_path);
        
        // Collect all available glyphs
        auto available_glyphs = font_renderer.collect_available_glyphs();
        if (available_glyphs.empty()) {
            throw std::runtime_error("No glyphs found in font");
        }
        
        int total_glyphs = static_cast<int>(available_glyphs.size());
        int num_files = (total_glyphs + max_glyphs_per_file - 1) / max_glyphs_per_file;
        
        std::cout << "Found " << total_glyphs << " glyphs, will create " << num_files << " file(s)" << std::endl;
        
        // Get font name for footer
        std::string font_name = get_font_name(font_path);
        
        // Calculate font scaling
        float scale = stt_ScaleForPixelHeight(font_renderer.get_font_info(), font_size);
        
        // Get font metrics for baseline calculation
        int ascent, descent, lineGap;
        stt_GetFontVMetrics(font_renderer.get_font_info(), &ascent, &descent, &lineGap);
        
        // Calculate baseline position to center font vertically in cell
        float baseline = (cell_height / 2.0f) + ((ascent - descent) / 2.0f * scale) - (ascent * scale);
        
        // Calculate image dimensions
        int image_width, image_height;
        if (num_files == 1) {
            // Single page: size just large enough for grid + footer
            int actual_glyphs = total_glyphs;
            int actual_cols = std::min(actual_glyphs, max_grid_cols);
            int actual_rows = (actual_glyphs + actual_cols - 1) / actual_cols;
            
            image_width = actual_cols * cell_width;
            image_height = actual_rows * cell_height + footer_height;
            
            // Ensure minimum reasonable size
            image_width = std::max(image_width, 200);
            image_height = std::max(image_height, 200);
        } else {
            // Multi-page: use uniform maximum dimensions
            image_width = max_image_width;
            image_height = max_image_height;
        }
        
        // Generate image pages
        std::vector<std::unique_ptr<ImagePage>> pages;
        
        for (int file_index = 0; file_index < num_files; ++file_index) {
            int start_glyph = file_index * max_glyphs_per_file;
            int end_glyph = std::min(start_glyph + max_glyphs_per_file, total_glyphs);
            int glyphs_in_file = end_glyph - start_glyph;
            
            // Calculate grid dimensions for this file
            int grid_cols = std::min(glyphs_in_file, max_grid_cols);
            int grid_rows = (glyphs_in_file + grid_cols - 1) / grid_cols;
            
            // Get Unicode range for this page
            int start_codepoint = available_glyphs[start_glyph];
            int end_codepoint = available_glyphs[end_glyph - 1];
            
            std::cout << "File " << (file_index + 1) << ": " << image_width << "x" << image_height 
                      << " pixels, " << grid_cols << "x" << grid_rows << " cells, " << glyphs_in_file 
                      << " glyphs, U+" << std::hex << start_codepoint << "–U+" << end_codepoint 
                      << std::dec << std::endl;
            
            // Create image page
            auto page = std::make_unique<ImagePage>(image_width, image_height, start_codepoint, 
                                                   end_codepoint, font_name);
            
            // Render glyphs for this file
            for (int i = 0; i < glyphs_in_file; ++i) {
                int codepoint = available_glyphs[start_glyph + i];
                int row = i / grid_cols;
                int col = i % grid_cols;
                
                // Get glyph bitmap
                int glyph_width, glyph_height, x_offset, y_offset;
                uint8_t* glyph_bitmap = stt_GetCodepointBitmap(font_renderer.get_font_info(), 
                                                               scale, scale, codepoint, 
                                                               &glyph_width, &glyph_height,
                                                               &x_offset, &y_offset);
                
                if (glyph_bitmap) {
                    // Calculate glyph position using baseline-centered approach
                    int cell_x = col * cell_width;
                    int cell_y = row * cell_height;
                    int glyph_x = cell_x + (cell_width - glyph_width) / 2;
                    int glyph_y = cell_y + static_cast<int>(baseline);
                    
                    auto& buffer = page->gray_buffer();
                    
                    // Copy glyph bitmap to main image buffer
                    for (int gy = 0; gy < glyph_height; ++gy) {
                        for (int gx = 0; gx < glyph_width; ++gx) {
                            int image_x = glyph_x + gx;
                            int image_y = glyph_y + gy;
                            
                            // Check bounds
                            if (image_x >= 0 && image_x < image_width &&
                                image_y >= 0 && image_y < image_height) {
                                uint8_t glyph_pixel = glyph_bitmap[gy * glyph_width + gx];
                                
                                // Render characters darker
                                int bg_pixel = buffer[image_y * image_width + image_x];
                                int darkened = bg_pixel - glyph_pixel;
                                if (darkened < 0) darkened = 0;
                                buffer[image_y * image_width + image_x] = static_cast<uint8_t>(darkened);
                            }
                        }
                    }
                    
                    // Free the glyph bitmap
                    stt_FreeBitmap(glyph_bitmap, nullptr);
                }
            }
            
            // Draw grid lines if enabled
            if (draw_grid_lines) {
                auto& buffer = page->gray_buffer();
                int grid_height = grid_rows * cell_height;
                
                // Vertical grid lines
                for (int x = 0; x <= grid_cols; ++x) {
                    int line_x = x * cell_width;
                    if (line_x < image_width) {
                        for (int y = 0; y < grid_height; ++y) {
                            buffer[y * image_width + line_x] = 200;
                        }
                    }
                }
                
                // Horizontal grid lines
                for (int y = 0; y <= grid_rows; ++y) {
                    int line_y = y * cell_height;
                    if (line_y < grid_height) {
                        for (int x = 0; x < image_width; ++x) {
                            buffer[line_y * image_width + x] = 200;
                        }
                    }
                }
            }
            
            // Render footer
            render_footer(*page, footer_height);
            
            // Convert grayscale to RGB
            page->convert_gray_to_rgb();
            
            pages.push_back(std::move(page));
        }
        
        // Generate output based on number of pages
        if (num_files == 1) {
            // Single page: Generate both PNG and PDF
            const auto& page = pages[0];
            
            // Generate PNG file
            std::string png_filename = final_output_prefix + ".png";
            FILE* png_file = fopen(png_filename.c_str(), "wb");
            if (png_file) {
                svpng(png_file, page->width(), page->height(), page->rgb_buffer().data(), 0);
                fclose(png_file);
                std::cout << "Font grid saved to " << png_filename << std::endl;
            } else {
                std::cerr << "Failed to create PNG file: " << png_filename << std::endl;
            }
            
            // Generate PDF file
            std::string pdf_filename = final_output_prefix + ".pdf";
            pdfimg::PDFDocument pdf;
            pdf.add_image_page(page->rgb_buffer().data(), page->width(), page->height(), 
                              page->width() * 3, true, compression);
            
            if (pdf.save(pdf_filename)) {
                std::cout << "Font grid saved to " << pdf_filename << std::endl;
            } else {
                std::cerr << "Failed to save PDF file: " << pdf_filename << std::endl;
            }
            
        } else {
            // Multiple pages: Generate only PDF with all pages
            std::string pdf_filename = final_output_prefix + ".pdf";
            pdfimg::PDFDocument pdf;
            
            for (const auto& page : pages) {
                pdf.add_image_page(page->rgb_buffer().data(), page->width(), page->height(), 
                                  page->width() * 3, true, compression);
            }
            
            if (pdf.save(pdf_filename)) {
                std::cout << "Multi-page font grid saved to " << pdf_filename << " (" 
                          << num_files << " pages)" << std::endl;
            } else {
                std::cerr << "Failed to save PDF file: " << pdf_filename << std::endl;
            }
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
