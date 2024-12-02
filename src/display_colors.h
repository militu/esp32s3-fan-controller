#ifndef DISPLAY_COLORS_H
#define DISPLAY_COLORS_H

namespace DisplayColors {
    // Status Colors
    static constexpr uint32_t SUCCESS = 0x4ADE80;     // Soft mint green - more modern than cyan
    static constexpr uint32_t ERROR = 0xFF4444;       // Vibrant coral red - clear but not harsh
    static constexpr uint32_t WORKING = 0x60A5FA;     // Modern blue - softer and more professional
    static constexpr uint32_t INACTIVE = 0x334155;    // Slate gray - more neutral and sophisticated
    
    // UI Elements
    static constexpr uint32_t BORDER = 0x475569;      // Medium slate - subtle but visible
    static constexpr uint32_t TEXT_PRIMARY = 0xF8FAFC; // Off-white - easier on the eyes
    static constexpr uint32_t TEXT_SECONDARY = 0x94A3B8; // Cool gray - better contrast
    
    // Background Colors
    static constexpr uint32_t BG_DARK = 0x0F172A;     // Deep navy - rich background
    static constexpr uint32_t BG_LIGHT = 0x1E293B;    // Lighter navy - subtle gradient
    
    // Temperature Colors
    static constexpr uint32_t TEMP_GOOD = 0x34D399;   // Emerald green - professional
    static constexpr uint32_t TEMP_WARNING = 0xFBAF24; // Warm amber - clear warning
    static constexpr uint32_t TEMP_CRITICAL = 0xEF4444; // New red - serious but not harsh

    // Speed Colors
    static constexpr uint32_t CURRENT_SPEED = 0x34D399;   // Emerald green - professional
    static constexpr uint32_t TARGET_SPEED = 0xFBAF24; // Warm amber - clear warning
    static constexpr uint32_t SPEED_GOOD = 0x34D399;   // Emerald green - professional
    static constexpr uint32_t SPEED_WARNING = 0xFBAF24; // Warm amber - clear warning
    static constexpr uint32_t SPEED_CRITICAL = 0xEF4444; // New red - serious but not harsh
}

#endif // DISPLAY_COLORS_H