#ifndef SYSTEM_THEME_DETECTOR_H
#define SYSTEM_THEME_DETECTOR_H

enum class SystemTheme {
    Light,
    Dark,
    Unknown
};

class SystemThemeDetector {
public:
    static SystemTheme detectSystemTheme();
    static bool isSystemInDarkMode();
};

#endif