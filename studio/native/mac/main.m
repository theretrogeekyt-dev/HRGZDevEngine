//
// HRGZDevEngine Studio - Native macOS Desktop Application Wrapper
// Uses Cocoa and WebKit (WKWebView) to provide an authentic, standalone
// native macOS desktop experience without external browser windows.
//

#import <Cocoa/Cocoa.h>
#import <WebKit/WebKit.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

@interface StudioAppDelegate : NSObject <NSApplicationDelegate, NSWindowDelegate>
@property (strong, nonatomic) NSWindow *window;
@property (strong, nonatomic) WKWebView *webView;
@property (strong, nonatomic) NSTask *serverTask;
@end

@implementation StudioAppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

    // Determine path to studio/server.js
    NSBundle *bundle = [NSBundle mainBundle];
    NSString *serverScript = nil;

    NSString *resPath = [bundle resourcePath];
    NSString *bundleServer = [resPath stringByAppendingPathComponent:@"studio/server.js"];
    if ([[NSFileManager defaultManager] fileExistsAtPath:bundleServer]) {
        serverScript = bundleServer;
    } else {
        // Fallback to relative path during development
        NSString *execPath = [[bundle executablePath] stringByDeletingLastPathComponent];
        NSString *devServer = [execPath stringByAppendingPathComponent:@"../../../../studio/server.js"];
        if ([[NSFileManager defaultManager] fileExistsAtPath:devServer]) {
            serverScript = devServer;
        } else {
            serverScript = @"studio/server.js";
        }
    }

    // Launch background Node.js server
    [self startServer:serverScript];

    // Wait for server to listen on 127.0.0.1:4820
    [self waitForServerPort:4820 timeoutSec:5];

    // Create native desktop window
    NSRect frame = NSMakeRect(0, 0, 1240, 820);
    self.window = [[NSWindow alloc] initWithContentRect:frame
                                              styleMask:NSWindowStyleMaskTitled |
                                                        NSWindowStyleMaskClosable |
                                                        NSWindowStyleMaskMiniaturizable |
                                                        NSWindowStyleMaskResizable
                                                backing:NSBackingStoreBuffered
                                                  defer:NO];

    [self.window setTitle:@"HRGZDevEngine Studio"];
    [self.window setDelegate:self];
    [self.window center];
    [self.window setBackgroundColor:[NSColor colorWithCalibratedRed:0.04 green:0.05 blue:0.07 alpha:1.0]];

    // Create native WebKit view
    WKWebViewConfiguration *config = [[WKWebViewConfiguration alloc] init];
    self.webView = [[WKWebView alloc] initWithFrame:frame configuration:config];
    [self.webView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    [self.window setContentView:self.webView];

    // Build standard application menu
    [self setupMenu];

    // Navigate to studio dashboard
    NSURL *url = [NSURL URLWithString:@"http://127.0.0.1:4820"];
    [self.webView loadRequest:[NSURLRequest requestWithURL:url]];

    [self.window makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];
}

- (void)startServer:(NSString *)scriptPath {
    self.serverTask = [[NSTask alloc] init];
    [self.serverTask setLaunchPath:@"/usr/bin/env"];
    [self.serverTask setArguments:@[@"node", scriptPath]];

    // Set working directory to project root
    NSString *repoRoot = [[scriptPath stringByDeletingLastPathComponent] stringByDeletingLastPathComponent];
    if ([[NSFileManager defaultManager] fileExistsAtPath:repoRoot]) {
        [self.serverTask setCurrentDirectoryPath:repoRoot];
    }

    NSMutableDictionary *env = [NSMutableDictionary dictionaryWithDictionary:[[NSProcessInfo processInfo] environment]];
    env[@"PORT"] = @"4820";
    [self.serverTask setEnvironment:env];

    @try {
        [self.serverTask launch];
    } @catch (NSException *e) {
        NSLog(@"Failed to launch server task: %@", e.reason);
    }
}

- (void)waitForServerPort:(int)port timeoutSec:(int)seconds {
    for (int i = 0; i < seconds * 10; i++) {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock >= 0) {
            struct sockaddr_in addr;
            memset(&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_port = htons(port);
            inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

            if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
                close(sock);
                return;
            }
            close(sock);
        }
        usleep(100000); // 100ms
    }
}

- (void)setupMenu {
    NSMenu *menubar = [[NSMenu alloc] init];
    NSMenuItem *appMenuItem = [[NSMenuItem alloc] init];
    [menubar addItem:appMenuItem];
    [NSApp setMainMenu:menubar];

    NSMenu *appMenu = [[NSMenu alloc] init];
    NSMenuItem *quitMenuItem = [[NSMenuItem alloc] initWithTitle:@"Quit HRGZDevEngine Studio"
                                                          action:@selector(terminate:)
                                                   keyEquivalent:@"q"];
    [appMenu addItem:quitMenuItem];
    [appMenuItem setSubmenu:appMenu];
}

- (void)windowWillClose:(NSNotification *)notification {
    if (self.serverTask && [self.serverTask isRunning]) {
        [self.serverTask terminate];
    }
    [NSApp terminate:nil];
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {
    return YES;
}

- (void)applicationWillTerminate:(NSNotification *)notification {
    if (self.serverTask && [self.serverTask isRunning]) {
        [self.serverTask terminate];
    }
}

@end

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        NSApplication *app = [NSApplication sharedApplication];
        StudioAppDelegate *delegate = [[StudioAppDelegate alloc] init];
        [app setDelegate:delegate];
        [app run];
    }
    return 0;
}
