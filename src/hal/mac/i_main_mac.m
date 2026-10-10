// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//	Native macOS application entry point.
//
//-----------------------------------------------------------------------------

#import <Cocoa/Cocoa.h>
#include "doomdef.h"
#include "m_argv.h"
#include "d_main.h"

int main(int argc, char** argv)
{
    @autoreleasepool {
        // If running inside a macOS .app bundle, switch working directory to Resources
        NSString* bundlePath = [[NSBundle mainBundle] bundlePath];
        if ([bundlePath hasSuffix:@".app"])
        {
            NSString* resPath = [[NSBundle mainBundle] resourcePath];
            if (resPath && [[NSFileManager defaultManager] fileExistsAtPath:resPath])
            {
                setenv("DOOMWADDIR", [resPath UTF8String], 1);
                chdir([resPath UTF8String]);
            }
        }

        myargc = argc;
        myargv = argv;
        D_DoomMain();
    }
    return 0;
}

