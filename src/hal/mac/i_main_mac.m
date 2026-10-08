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
        // If running inside a macOS .app bundle, add Resources path to search paths
        if (!getenv("DOOMWADDIR"))
        {
            NSString* resPath = [[NSBundle mainBundle] resourcePath];
            if (resPath && [[NSFileManager defaultManager] fileExistsAtPath:resPath])
            {
                setenv("DOOMWADDIR", [resPath UTF8String], 0);
            }
        }

        myargc = argc;
        myargv = argv;
        D_DoomMain();
    }
    return 0;
}

