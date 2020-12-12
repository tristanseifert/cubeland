#import <Foundation/Foundation.h>

#import "PathHelper.h"

#import <stdexcept>

using namespace io;

/**
 * Returns the path to the Application Support directory.
 */
std::string PathHelper::appDataDir() {
    @autoreleasepool {
        NSArray<NSString *> *paths = NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory
, NSUserDomainMask, YES);

        if(paths.count == 0) {
            throw std::runtime_error("Failed to get app data path");
        }

        NSString *path = [paths.firstObject stringByAppendingPathComponent:@"me.tseifert.cubeland/"];
        return std::string([path UTF8String]);
    }
}

