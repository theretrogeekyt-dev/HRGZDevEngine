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

static NSString *FindNodeExecutable(void) {
    NSFileManager *fm = [NSFileManager defaultManager];

    // 1. Check known standard macOS paths (Apple Silicon Homebrew, Intel Homebrew, MacPorts, system)
    NSArray *standardPaths = @[
        @"/opt/homebrew/bin/node",
        @"/usr/local/bin/node",
        @"/opt/local/bin/node",
        @"/usr/bin/node"
    ];
    for (NSString *p in standardPaths) {
        if ([fm isExecutableFileAtPath:p]) {
            return p;
        }
    }

    // 2. Scan NVM installations: ~/.nvm/versions/node/*/bin/node
    NSString *home = NSHomeDirectory();
    NSString *nvmDir = [home stringByAppendingPathComponent:@".nvm/versions/node"];
    if ([fm fileExistsAtPath:nvmDir]) {
        NSArray *versions = [fm contentsOfDirectoryAtPath:nvmDir error:nil];
        NSArray *sorted = [versions sortedArrayUsingSelector:@selector(localizedStandardCompare:)];
        for (NSString *ver in [sorted reverseObjectEnumerator]) {
            NSString *nodePath = [nvmDir stringByAppendingPathComponent:[NSString stringWithFormat:@"%@/bin/node", ver]];
            if ([fm isExecutableFileAtPath:nodePath]) {
                return nodePath;
            }
        }
    }

    // 3. Scan Volta, ASDF, and FNM version managers
    NSArray *managerPaths = @[
        [home stringByAppendingPathComponent:@".volta/bin/node"],
        [home stringByAppendingPathComponent:@".asdf/shims/node"],
        [home stringByAppendingPathComponent:@".fnm/current/bin/node"]
    ];
    for (NSString *p in managerPaths) {
        if ([fm isExecutableFileAtPath:p]) {
            return p;
        }
    }

    // 4. Query user's login shell via zsh to extract dynamic PATH
    NSTask *task = [[NSTask alloc] init];
    [task setLaunchPath:@"/bin/zsh"];
    [task setArguments:@[@"-l", @"-c", @"which node"]];
    NSPipe *pipe = [NSPipe pipe];
    [task setStandardOutput:pipe];
    [task setStandardError:[NSPipe pipe]];
    @try {
        [task launch];
        [task waitUntilExit];
        if ([task terminationStatus] == 0) {
            NSData *data = [[pipe fileHandleForReading] readDataToEndOfFile];
            NSString *outStr = [[[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding]
                                stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
            if (outStr.length > 0 && [fm isExecutableFileAtPath:outStr]) {
                return outStr;
            }
        }
    } @catch (NSException *e) {}

    return nil;
}

@interface StudioAppDelegate : NSObject <NSApplicationDelegate, NSWindowDelegate, WKNavigationDelegate>
@property (strong, nonatomic) NSWindow *window;
@property (strong, nonatomic) WKWebView *webView;
@property (strong, nonatomic) NSTask *serverTask;
@property (assign, nonatomic) NSInteger loadRetries;
@property (strong, nonatomic) NSString *nodePath;
@end

@implementation StudioAppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    self.loadRetries = 0;

    // Create native desktop window early with dark background
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

    // Create native WebKit view with developer tools and security configuration
    WKWebViewConfiguration *config = [[WKWebViewConfiguration alloc] init];
    [config.preferences setValue:@YES forKey:@"developerExtrasEnabled"];

    self.webView = [[WKWebView alloc] initWithFrame:frame configuration:config];
    [self.webView setNavigationDelegate:self];
    [self.webView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    [self.window setContentView:self.webView];

    // Build standard application menu
    [self setupMenu];

    [self.window makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];

    // Discover Node.js executable
    self.nodePath = FindNodeExecutable();
    if (!self.nodePath) {
        NSLog(@"[HRGZStudio] Node.js not detected on host.");
        [self showMissingNodeUI];
        return;
    }

    NSLog(@"[HRGZStudio] Found Node.js executable: %@", self.nodePath);

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

    // Load initial dashboard request
    NSURL *url = [NSURL URLWithString:@"http://127.0.0.1:4820"];
    [self.webView loadRequest:[NSURLRequest requestWithURL:url]];
}

- (void)startServer:(NSString *)scriptPath {
    self.serverTask = [[NSTask alloc] init];
    [self.serverTask setLaunchPath:self.nodePath];
    [self.serverTask setArguments:@[scriptPath]];

    // Set working directory to project root or resource directory
    NSString *repoRoot = [[scriptPath stringByDeletingLastPathComponent] stringByDeletingLastPathComponent];
    if ([[NSFileManager defaultManager] fileExistsAtPath:repoRoot]) {
        [self.serverTask setCurrentDirectoryPath:repoRoot];
    }

    // Ensure environment PATH has all development toolchains
    NSMutableDictionary *env = [NSMutableDictionary dictionaryWithDictionary:[[NSProcessInfo processInfo] environment]];
    NSString *nodeDir = [self.nodePath stringByDeletingLastPathComponent];
    NSString *currentPath = env[@"PATH"] ?: @"";
    env[@"PATH"] = [NSString stringWithFormat:@"%@:/opt/homebrew/bin:/usr/local/bin:/opt/local/bin:/usr/bin:/bin:/usr/sbin:/sbin:%@", nodeDir, currentPath];
    env[@"PORT"] = @"4820";
    env[@"HRGZ_EMBEDDED"] = @"1";
    env[@"HRGZ_ENGINE_ROOT"] = repoRoot;
    [self.serverTask setEnvironment:env];

    @try {
        [self.serverTask launch];
        NSLog(@"[HRGZStudio] Background server launched (PID: %d)", self.serverTask.processIdentifier);
    } @catch (NSException *e) {
        NSLog(@"[HRGZStudio] Failed to launch server task: %@", e.reason);
    }
}

- (void)showMissingNodeUI {
    NSString *html = @"<!DOCTYPE html><html><head><meta charset='utf-8'>"
    "<style>"
    "body { background: #0b0f19; color: #f3f4f6; font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; display: flex; align-items: center; justify-content: center; height: 100vh; margin: 0; }"
    ".card { background: #111827; border: 1px solid #1f2937; border-radius: 14px; padding: 40px 48px; max-width: 540px; box-shadow: 0 25px 50px -12px rgba(0,0,0,0.6); text-align: center; }"
    ".logo { font-size: 42px; margin-bottom: 12px; }"
    "h1 { color: #f97316; margin-top: 0; font-size: 24px; font-weight: 800; letter-spacing: -0.5px; }"
    "p { color: #9ca3af; font-size: 15px; line-height: 1.6; margin: 12px 0; }"
    ".cmd-box { background: #030712; border: 1px solid #374151; padding: 12px 16px; border-radius: 8px; margin: 18px 0; font-family: ui-monospace, Menlo, Monaco, monospace; color: #38bdf8; font-size: 14px; text-align: left; }"
    ".btn { display: inline-block; margin-top: 14px; padding: 12px 24px; background: #ea580c; color: #fff; text-decoration: none; border-radius: 8px; font-weight: 700; font-size: 14px; transition: background 0.2s; }"
    ".btn:hover { background: #c2410c; }"
    "</style></head><body>"
    "<div class='card'>"
    "<div class='logo'>🎮</div>"
    "<h1>HRGZDevEngine Studio</h1>"
    "<p><strong>Node.js Runtime Required</strong></p>"
    "<p>HRGZDevEngine Studio requires Node.js to manage project builds, parse WAD archives, and distribute retro games.</p>"
    "<div class='cmd-box'>brew install node</div>"
    "<p>Or download the official macOS installer from Node.js:</p>"
    "<a class='btn' href='https://nodejs.org' target='_blank'>Download Node.js Installer</a>"
    "</div></body></html>";

    [self.webView loadHTMLString:html baseURL:nil];
}

#pragma mark - WKNavigationDelegate

- (void)webView:(WKWebView *)webView didFailProvisionalNavigation:(WKNavigation *)navigation withError:(NSError *)error {
    NSLog(@"[HRGZStudio] Connection pending (%@)... retry %ld", error.localizedDescription, (long)self.loadRetries);

    // If server is still booting up (port not ready), retry connecting every 400ms up to 25 times
    if (self.loadRetries < 25) {
        self.loadRetries++;
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.4 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
            NSURL *url = [NSURL URLWithString:@"http://127.0.0.1:4820"];
            [self.webView loadRequest:[NSURLRequest requestWithURL:url]];
        });
    } else {
        NSLog(@"[HRGZStudio] Exhausted server connection retries.");
    }
}

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation {
    NSLog(@"[HRGZStudio] Studio dashboard loaded successfully.");
    self.loadRetries = 0;
}

#pragma mark - Menu & App Lifecycle

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
