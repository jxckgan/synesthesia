#include "system_theme_detector.h"

#ifdef __APPLE__
#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>

SystemTheme SystemThemeDetector::detectSystemTheme() {
    @autoreleasepool {
        if (@available(macOS 10.14, *)) {
            NSString* osxMode = [[NSUserDefaults standardUserDefaults] stringForKey:@"AppleInterfaceStyle"];
            if ([osxMode isEqualToString:@"Dark"]) {
                return SystemTheme::Dark;
            } else {
                return SystemTheme::Light;
            }
        } else {
            return SystemTheme::Light;
        }
    }
}

bool SystemThemeDetector::isSystemInDarkMode() {
    return detectSystemTheme() == SystemTheme::Dark;
}

#endif