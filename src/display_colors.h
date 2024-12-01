#ifndef DISPLAY_COLORS_H
#define DISPLAY_COLORS_H

namespace DisplayColors {
    // Status Colors
    static constexpr uint32_t SUCCESS = 0x00FF88;     // Cyan
    static constexpr uint32_t ERROR = 0xFF5555;       // Soft red
    static constexpr uint32_t WORKING = 0x00A5FF;     // Blue
    static constexpr uint32_t INACTIVE = 0x404060;    // Dark blue-gray
    
    // UI Elements
    static constexpr uint32_t BORDER = 0x304060;      // Border color
    static constexpr uint32_t TEXT_PRIMARY = 0xFFFFFF; // White
    static constexpr uint32_t TEXT_SECONDARY = 0x808080; // Gray
    
    // Background Colors
    static constexpr uint32_t BG_DARK = 0x101020;     // Darker background
    static constexpr uint32_t BG_LIGHT = 0x202040;    // Lighter background
    
    // Temperature Colors
    static constexpr uint32_t TEMP_GOOD = 0x00FF00;   // Green
    static constexpr uint32_t TEMP_WARNING = 0xFFA500; // Orange
    static constexpr uint32_t TEMP_CRITICAL = 0xFF0000; // Red
}

#endif // DISPLAY_COLORS_H