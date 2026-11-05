#include "miniapp_browser.hpp"

#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QStringList>
#include <QUrlQuery>
#include <QtTranslation>
#include <initializer_list>
#include <utility>

// AppKit includes legacy ColorSync constants that add distinct enum types.
// Clang 22 diagnoses these SDK declarations as errors in C++26. Keep the
// exception at the external-header boundary; project code below remains strict.
#if defined(__clang__)
#pragma clang diagnostic push
#if __cplusplus > 202302L && __has_warning("-Wenum-enum-conversion")
#pragma clang diagnostic ignored "-Wenum-enum-conversion"
#endif
#if __has_warning("-Wnullability-completeness")
#pragma clang diagnostic ignored "-Wnullability-completeness"
#endif
#endif
#import <AppKit/AppKit.h>
#import <WebKit/WebKit.h>
#import <QuartzCore/QuartzCore.h>
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

namespace {
NSString* nativeText(const QString& value) { return value.toNSString(); }
QString translated(const char* id) { return qtTrId(id); }

bool allowedWebUrl(NSURL* url) {
    if (!url) { return false; }
    const QUrl parsed = QUrl::fromNSURL(url);
    const QString host = parsed.host().toLower();
    QHostAddress address;
    return parsed.scheme() == QStringLiteral("https") && parsed.userInfo().isEmpty()
        && parsed.port(443) == 443 && host.contains(QLatin1Char('.'))
        && !host.endsWith(QLatin1Char('.')) && !host.endsWith(QStringLiteral(".local"))
        && !host.endsWith(QStringLiteral(".localhost")) && !host.endsWith(QStringLiteral(".internal"))
        && !host.endsWith(QStringLiteral(".test")) && !address.setAddress(host);
}

NSURL* youtubeWatchUrlForEmbed(NSURL* url) {
    if (!url) { return nil; }
    const QUrl parsed = QUrl::fromNSURL(url);
    const QString host = parsed.host().toLower();
    if (host != QStringLiteral("youtube.com") && host != QStringLiteral("www.youtube.com")
        && host != QStringLiteral("youtube-nocookie.com")
        && host != QStringLiteral("www.youtube-nocookie.com")) {
        return nil;
    }
    const QStringList segments = parsed.path().split(QLatin1Char('/'), Qt::SkipEmptyParts);
    if (segments.size() != 2 || segments.front() != QStringLiteral("embed")) {
        return nil;
    }
    const QString videoId = segments.back().trimmed();
    static const QRegularExpression safeVideoId{QStringLiteral(R"(^[A-Za-z0-9_-]{6,32}$)")};
    if (!safeVideoId.match(videoId).hasMatch()) {
        return nil;
    }
    QUrl watch{QStringLiteral("https://www.youtube.com/watch")};
    QUrlQuery query{};
    query.addQueryItem(QStringLiteral("v"), videoId);
    watch.setQuery(query);
    return watch.toNSURL();
}

NSString* bridgeScript() {
    // Farcaster SDK selects the ReactNativeWebView endpoint at import time.
    // Install at document start, main frame only. This is a wire adapter, not
    // a fake sdk implementation. Reply tokens prevent stale-document delivery.
    return [NSString stringWithUTF8String:R"JS((() => {
        const token = crypto.randomUUID();
        let count = 0, since = performance.now();
        const endpoint = Object.freeze({postMessage(value) {
            if (typeof value !== 'string' || value.length > 32768)
                throw new RangeError('Mini App request exceeds the host limit');
            const now = performance.now();
            if (now - since > 1000) { since = now; count = 0; }
            if (++count > 120) {
                throw new Error('Mini App host rate limit exceeded');
            }
            window.webkit.messageHandlers.genyMiniApp.postMessage({document: token, message: value});
        }});
        Object.defineProperty(window, 'ReactNativeWebView', {value: endpoint});
        Object.defineProperty(window, '__genyMiniAppReply', {value: (documentToken, data) => {
            if (documentToken === token)
                document.dispatchEvent(new MessageEvent('FarcasterFrameCallback', {data}));
        }});
        Object.defineProperty(window, '__genyMiniAppEthProviderEvent', {value: (documentToken, data) => {
            if (documentToken === token)
                document.dispatchEvent(new MessageEvent('FarcasterFrameEthProviderEvent', {data}));
        }});
        Object.defineProperty(window, '__genyMiniAppEvent', {value: (documentToken, data) => {
            if (documentToken === token)
                document.dispatchEvent(new MessageEvent('FarcasterFrameEvent', {data}));
        }});
    })();)JS"];
}
}

// Native input/shaping boundary above the Qt surface. It resizes with the
// parent's content view, clips offscreen animation, and eats backdrop events.
@interface GenyMiniAppDrawer : NSControl {
@public
    NSView* m_panel;
    farcaster::platform::miniapp::PresentationMode m_mode;
    BOOL m_animating;
    BOOL m_fullscreen;
    // Backdrop is a visual layer only: it must not steal input from the
    // underlying Qt shell, including an active wallet-approval dialog.
    BOOL m_passThroughBackdrop;
}
- (NSRect)panelFrame;
- (NSRect)hiddenFrame;
@end

@implementation GenyMiniAppDrawer
- (BOOL)isFlipped { return YES; }
- (NSRect)panelFrame {
    const CGFloat width = self.bounds.size.width;
    const CGFloat height = self.bounds.size.height;
    if (m_fullscreen) return self.bounds;
    using farcaster::platform::miniapp::PresentationMode;
    if (m_mode == PresentationMode::Bottom) {
        const CGFloat panelWidth = MIN(width, 424.0);
        const CGFloat panelHeight = MIN(height, MIN(755.0, MAX(420.0, height * 0.82)));
        return NSMakeRect((width - panelWidth) / 2, height - panelHeight, panelWidth, panelHeight);
    }
    const CGFloat panelWidth = MIN(width, 424.0);
    return NSMakeRect(m_mode == PresentationMode::Right ? width - panelWidth : 0,
        0, panelWidth, height);
}
- (NSRect)hiddenFrame {
    NSRect frame = [self panelFrame];
    using farcaster::platform::miniapp::PresentationMode;
    if (m_mode == PresentationMode::Bottom) { frame.origin.y = self.bounds.size.height; }
    else if (m_mode == PresentationMode::Left) { frame.origin.x = -frame.size.width; }
    else { frame.origin.x = self.bounds.size.width; }
    return frame;
}
- (void)layout {
    [super layout];
    if (!m_animating) { m_panel.frame = [self panelFrame]; }
}
- (void)setFrameSize:(NSSize)size {
    [super setFrameSize:size];
    self.needsLayout = YES;
}
- (NSView*)hitTest:(NSPoint)point {
    // NSView's point is in the *superview* coordinate space. Returning nil
    // outside the actual drawer panel lets AppKit route events to the Qt
    // content view beneath this overlay instead of silently dismissing the app.
    if (m_passThroughBackdrop) {
        const NSPoint local = [self convertPoint:point fromView:self.superview];
        if (!NSPointInRect(local, m_panel.frame))
            return nil;
    }
    return [super hitTest:point];
}
- (void)mouseDown:(NSEvent*)event {
    if (m_passThroughBackdrop) return;
    const NSPoint point = [self convertPoint:event.locationInWindow fromView:nil];
    if (NSPointInRect(point, m_panel.frame)) { return; }
    [NSApp sendAction:self.action to:self.target from:self];
}
- (void)scrollWheel:(NSEvent*)event { (void)event; }
- (void)rightMouseDown:(NSEvent*)event { (void)event; }
- (void)otherMouseDown:(NSEvent*)event { (void)event; }
@end

@interface GenyMiniAppWindow : NSObject <NSWindowDelegate, WKNavigationDelegate, WKUIDelegate, WKScriptMessageHandler> {
@public
    farcaster::platform::miniapp::BrowserCallbacks m_callbacks;
    NSWindow* m_window;
    NSWindow* __weak m_parentWindow;
    NSResponder* __weak m_previousResponder;
    GenyMiniAppDrawer* m_drawer;
    id m_escapeMonitor;
    BOOL m_reduceMotion;
    farcaster::platform::miniapp::PresentationMode m_mode;
    farcaster::platform::miniapp::BrowserRole m_role;
    BOOL m_dismissing;
    WKWebView* m_web;
    NSView* m_overlay;
    NSTextField* m_status;
    NSProgressIndicator* m_spinner;
    NSButton* m_retry;
    NSButton* m_back;
    NSURL* m_origin;
    BOOL m_closed;
    BOOL m_failed;
    BOOL m_sdkBack;
    BOOL m_ready;
    BOOL m_mediaFallbackTriggered;
    NSTimeInterval m_rateSince;
    NSUInteger m_rateCount;
}
- (instancetype)initWithTitle:(NSString*)title origin:(NSURL*)origin
    mode:(farcaster::platform::miniapp::PresentationMode)mode reduceMotion:(BOOL)reduceMotion
    initiallyVisible:(BOOL)initiallyVisible role:(farcaster::platform::miniapp::BrowserRole)role;
- (void)dispose;
- (void)showStatus:(NSString*)message failed:(BOOL)failed;
- (void)showReady;
- (void)checkMediaPlaybackFallback;
- (void)openExternalMediaFallback;
- (void)back:(id)sender;
- (void)retry:(id)sender;
- (void)minimize:(id)sender;
- (void)toggleFullscreen:(id)sender;
- (void)share:(id)sender;
- (void)dismiss:(id)sender;
- (void)notifyClosed;
- (void)parentWillClose:(NSNotification*)notification;
@end

static NSWindow* genycasterHostWindow()
{
    static __weak NSWindow* cachedHostWindow = nil;
    const auto isHostCandidate = [](NSWindow* candidate) -> bool {
        if (candidate == nil) {
            return false;
        }
        id<NSWindowDelegate> delegate = candidate.delegate;
        return delegate == nil || ![delegate isKindOfClass:[GenyMiniAppWindow class]];
    };

    if (isHostCandidate(cachedHostWindow)) {
        return cachedHostWindow;
    }
    if (isHostCandidate(NSApp.mainWindow)) {
        cachedHostWindow = NSApp.mainWindow;
        return cachedHostWindow;
    }
    if (isHostCandidate(NSApp.keyWindow)) {
        cachedHostWindow = NSApp.keyWindow;
        return cachedHostWindow;
    }
    for (NSWindow* candidate in NSApp.orderedWindows) {
        if (isHostCandidate(candidate)) {
            cachedHostWindow = candidate;
            return cachedHostWindow;
        }
    }
    return nil;
}

@implementation GenyMiniAppWindow
- (instancetype)initWithTitle:(NSString*)title origin:(NSURL*)origin
    mode:(farcaster::platform::miniapp::PresentationMode)mode reduceMotion:(BOOL)reduceMotion
    initiallyVisible:(BOOL)initiallyVisible role:(farcaster::platform::miniapp::BrowserRole)role {
    self = [super init];
    if (!self) { return nil; }
    m_origin = origin;
    // Never parent a drawer to whichever Mini App window most recently became
    // NSApp.mainWindow. Multiple native Mini App windows can become key/main;
    // anchoring subsequent drawers to them creates cascading close/focus
    // lifetimes. Keep every drawer attached to the stable Qt host window.
    NSWindow* parentWindow = genycasterHostWindow();
    if (parentWindow == nil) {
        return nil;
    }
    m_parentWindow = parentWindow;
    m_previousResponder = parentWindow.firstResponder;
    m_reduceMotion = reduceMotion || NSWorkspace.sharedWorkspace.accessibilityDisplayShouldReduceMotion;
    m_mode = mode;
    m_role = role;
    m_mediaFallbackTriggered = NO;
    NSScreen* screen = parentWindow.screen ?: NSScreen.mainScreen;
    NSRect available = screen.visibleFrame;
    // Mini Apps preserve the Farcaster-oriented 424pt surface. Media playback
    // uses a wider 16:9-capable window so embedded players retain usable native
    // controls instead of being squeezed into the Mini App phone geometry.
    const BOOL mediaRole = role == farcaster::platform::miniapp::BrowserRole::Media;
    CGFloat height = mediaRole
        ? MIN(720.0, MAX(420.0, available.size.height - 96.0))
        : MIN(755.0, MAX(480.0, available.size.height - 72.0));
    CGFloat width = mediaRole
        ? MIN(1120.0, MAX(560.0, available.size.width - 160.0))
        : MIN(424.0, MAX(340.0, available.size.width - 48.0));
    m_window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, width, height)
        styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable
        backing:NSBackingStoreBuffered defer:NO];
    m_window.releasedWhenClosed = NO;
    m_window.title = mediaRole ? @"Genycaster Media" : @"Genycaster";
    m_window.collectionBehavior |= NSWindowCollectionBehaviorFullScreenPrimary;
    m_window.contentMinSize = mediaRole ? NSMakeSize(480, 360) : NSMakeSize(320, 360);
    m_window.delegate = self;
    m_window.backgroundColor = NSColor.windowBackgroundColor;
    NSView* content = m_window.contentView;

    NSView* toolbar = [[NSView alloc] init];
    toolbar.translatesAutoresizingMaskIntoConstraints = NO;
    [content addSubview:toolbar];
    m_back = [NSButton buttonWithImage:[NSImage imageWithSystemSymbolName:@"chevron.backward"
        accessibilityDescription:nativeText(translated("genycaster.miniapp.back"))]
        target:self action:@selector(back:)];
    m_back.bordered = NO;
    m_back.enabled = NO;
    m_back.toolTip = nativeText(translated("genycaster.miniapp.back"));
    m_back.translatesAutoresizingMaskIntoConstraints = NO;
    [toolbar addSubview:m_back];
    NSButton* reload = [NSButton buttonWithImage:[NSImage imageWithSystemSymbolName:@"arrow.clockwise"
        accessibilityDescription:nativeText(translated("genycaster.miniapp.retry"))]
        target:self action:@selector(retry:)];
    reload.bordered = NO;
    reload.toolTip = nativeText(translated("genycaster.miniapp.retry"));
    reload.translatesAutoresizingMaskIntoConstraints = NO;
    [toolbar addSubview:reload];
    NSButton* minimize = [NSButton buttonWithImage:[NSImage imageWithSystemSymbolName:@"minus"
        accessibilityDescription:nativeText(translated("genycaster.miniapp.minimize"))]
        target:self action:@selector(minimize:)];
    minimize.bordered = NO;
    minimize.hidden = mediaRole;
    minimize.toolTip = nativeText(translated("genycaster.miniapp.minimize"));
    minimize.translatesAutoresizingMaskIntoConstraints = NO;
    [toolbar addSubview:minimize];
    NSButton* share = [NSButton buttonWithImage:[NSImage imageWithSystemSymbolName:@"square.and.arrow.up"
        accessibilityDescription:nativeText(translated("genycaster.miniapp.share"))]
        target:self action:@selector(share:)];
    share.bordered = NO;
    share.toolTip = nativeText(translated("genycaster.miniapp.share"));
    share.translatesAutoresizingMaskIntoConstraints = NO;
    [toolbar addSubview:share];
    NSButton* fullscreen = [NSButton buttonWithImage:[NSImage imageWithSystemSymbolName:@"arrow.up.left.and.arrow.down.right"
        accessibilityDescription:nativeText(translated("genycaster.player.fullscreen"))]
        target:self action:@selector(toggleFullscreen:)];
    fullscreen.bordered = NO;
    fullscreen.toolTip = nativeText(translated("genycaster.player.fullscreen"));
    fullscreen.translatesAutoresizingMaskIntoConstraints = NO;
    [toolbar addSubview:fullscreen];
    NSButton* close = [NSButton buttonWithImage:[NSImage imageWithSystemSymbolName:@"xmark"
        accessibilityDescription:nativeText(translated("genycaster.miniapp.close"))]
        target:self action:@selector(dismiss:)];
    close.bordered = NO;
    close.toolTip = nativeText(translated("genycaster.miniapp.close"));
    close.translatesAutoresizingMaskIntoConstraints = NO;
    [toolbar addSubview:close];
    NSTextField* heading = [NSTextField labelWithString:title.length ? title : origin.host];
    heading.font = [NSFont systemFontOfSize:14 weight:NSFontWeightSemibold];
    heading.lineBreakMode = NSLineBreakByTruncatingTail;
    heading.alignment = NSTextAlignmentCenter;
    NSTextField* domain = [NSTextField labelWithString:origin.host];
    domain.font = [NSFont systemFontOfSize:11];
    domain.textColor = NSColor.secondaryLabelColor;
    domain.lineBreakMode = NSLineBreakByTruncatingMiddle;
    domain.alignment = NSTextAlignmentCenter;
    NSStackView* identity = [NSStackView stackViewWithViews:@[heading, domain]];
    identity.orientation = NSUserInterfaceLayoutOrientationVertical;
    identity.spacing = 3;
    identity.translatesAutoresizingMaskIntoConstraints = NO;
    [toolbar addSubview:identity];

    WKWebViewConfiguration* configuration = [[WKWebViewConfiguration alloc] init];
    // Separate process-pool instances no longer provide renderer isolation on modern macOS.
    // Mini Apps remain ephemeral. The media host keeps first-party provider cookies so
    // YouTube does not look like a brand-new anonymous WebView on every playback attempt.
    configuration.websiteDataStore = mediaRole
        ? WKWebsiteDataStore.defaultDataStore
        : WKWebsiteDataStore.nonPersistentDataStore;
    configuration.preferences.javaScriptCanOpenWindowsAutomatically = NO;
    configuration.mediaTypesRequiringUserActionForPlayback = WKAudiovisualMediaTypeAll;
    if (!mediaRole) {
        [configuration.userContentController addScriptMessageHandler:self name:@"genyMiniApp"];
        [configuration.userContentController addUserScript:[[WKUserScript alloc]
            initWithSource:bridgeScript() injectionTime:WKUserScriptInjectionTimeAtDocumentStart
            forMainFrameOnly:YES]];
    }
    m_web = [[WKWebView alloc] initWithFrame:NSZeroRect configuration:configuration];
    m_web.navigationDelegate = self;
    m_web.UIDelegate = self;
    m_web.allowsBackForwardNavigationGestures = YES;
    m_web.translatesAutoresizingMaskIntoConstraints = NO;
    m_web.hidden = NO;
    [content addSubview:m_web];

    NSBox* loadingSurface = [[NSBox alloc] init];
    loadingSurface.boxType = NSBoxCustom;
    loadingSurface.titlePosition = NSNoTitle;
    loadingSurface.borderType = NSNoBorder;
    loadingSurface.fillColor = NSColor.windowBackgroundColor;
    m_overlay = loadingSurface;
    m_overlay.translatesAutoresizingMaskIntoConstraints = NO;
    [content addSubview:m_overlay];
    m_spinner = [[NSProgressIndicator alloc] init];
    m_spinner.style = NSProgressIndicatorStyleSpinning;
    m_spinner.controlSize = NSControlSizeRegular;
    m_spinner.indeterminate = YES;
    [m_spinner startAnimation:nil];
    m_status = [NSTextField wrappingLabelWithString:nativeText(translated("genycaster.miniapp.loading"))];
    m_status.alignment = NSTextAlignmentCenter;
    m_status.textColor = NSColor.secondaryLabelColor;
    m_retry = [NSButton buttonWithTitle:nativeText(translated("genycaster.miniapp.retry"))
        target:self action:@selector(retry:)];
    m_retry.hidden = YES;
    NSStackView* loading = [NSStackView stackViewWithViews:@[m_spinner, m_status, m_retry]];
    loading.orientation = NSUserInterfaceLayoutOrientationVertical;
    loading.spacing = 18;
    loading.translatesAutoresizingMaskIntoConstraints = NO;
    [m_overlay addSubview:loading];
    [NSLayoutConstraint activateConstraints:@[
        [toolbar.topAnchor constraintEqualToAnchor:content.topAnchor],
        [toolbar.leadingAnchor constraintEqualToAnchor:content.leadingAnchor],
        [toolbar.trailingAnchor constraintEqualToAnchor:content.trailingAnchor],
        [toolbar.heightAnchor constraintEqualToConstant:60],
        [m_back.leadingAnchor constraintEqualToAnchor:toolbar.leadingAnchor constant:12],
        [m_back.centerYAnchor constraintEqualToAnchor:toolbar.centerYAnchor],
        [m_back.widthAnchor constraintEqualToConstant:32], [m_back.heightAnchor constraintEqualToConstant:32],
        [close.trailingAnchor constraintEqualToAnchor:toolbar.trailingAnchor constant:-12],
        [close.centerYAnchor constraintEqualToAnchor:toolbar.centerYAnchor],
        [close.widthAnchor constraintEqualToConstant:32], [close.heightAnchor constraintEqualToConstant:32],
        [fullscreen.trailingAnchor constraintEqualToAnchor:close.leadingAnchor constant:-4],
        [fullscreen.centerYAnchor constraintEqualToAnchor:toolbar.centerYAnchor],
        [fullscreen.widthAnchor constraintEqualToConstant:32], [fullscreen.heightAnchor constraintEqualToConstant:32],
        [share.trailingAnchor constraintEqualToAnchor:fullscreen.leadingAnchor constant:-4],
        [share.centerYAnchor constraintEqualToAnchor:toolbar.centerYAnchor],
        [share.widthAnchor constraintEqualToConstant:32], [share.heightAnchor constraintEqualToConstant:32],
        [minimize.trailingAnchor constraintEqualToAnchor:share.leadingAnchor constant:-4],
        [minimize.centerYAnchor constraintEqualToAnchor:toolbar.centerYAnchor],
        [minimize.widthAnchor constraintEqualToConstant:32], [minimize.heightAnchor constraintEqualToConstant:32],
        [reload.trailingAnchor constraintEqualToAnchor:minimize.leadingAnchor constant:-4],
        [reload.centerYAnchor constraintEqualToAnchor:toolbar.centerYAnchor],
        [reload.widthAnchor constraintEqualToConstant:32], [reload.heightAnchor constraintEqualToConstant:32],
        // The toolbar is asymmetric: there is one Back button on the left
        // and several actions on the right. Centering the identity in the
        // *whole toolbar* compresses both labels into “…” and “h…”.
        // Center inside the actual remaining span instead.
        [identity.centerYAnchor constraintEqualToAnchor:toolbar.centerYAnchor],
        [identity.leadingAnchor constraintEqualToAnchor:m_back.trailingAnchor constant:8],
        [identity.trailingAnchor constraintEqualToAnchor:reload.leadingAnchor constant:-8],
        [heading.widthAnchor constraintEqualToAnchor:identity.widthAnchor],
        [domain.widthAnchor constraintEqualToAnchor:identity.widthAnchor],
        [m_web.topAnchor constraintEqualToAnchor:toolbar.bottomAnchor],
        [m_web.leadingAnchor constraintEqualToAnchor:content.leadingAnchor],
        [m_web.trailingAnchor constraintEqualToAnchor:content.trailingAnchor],
        [m_web.bottomAnchor constraintEqualToAnchor:content.bottomAnchor],
        [m_overlay.topAnchor constraintEqualToAnchor:toolbar.bottomAnchor],
        [m_overlay.leadingAnchor constraintEqualToAnchor:content.leadingAnchor],
        [m_overlay.trailingAnchor constraintEqualToAnchor:content.trailingAnchor],
        [m_overlay.bottomAnchor constraintEqualToAnchor:content.bottomAnchor],
        [loading.centerXAnchor constraintEqualToAnchor:m_overlay.centerXAnchor],
        [loading.centerYAnchor constraintEqualToAnchor:m_overlay.centerYAnchor],
        [loading.widthAnchor constraintEqualToAnchor:m_overlay.widthAnchor constant:-48],
        [m_status.widthAnchor constraintEqualToAnchor:loading.widthAnchor]
    ]];
    if (parentWindow) {
        [NSNotificationCenter.defaultCenter addObserver:self selector:@selector(parentWillClose:)
            name:NSWindowWillCloseNotification object:parentWindow];
    }
    using farcaster::platform::miniapp::PresentationMode;
    if (mode != PresentationMode::Window && parentWindow.contentView) {
        NSView* parentContent = parentWindow.contentView;
        m_drawer = [[GenyMiniAppDrawer alloc] initWithFrame:parentContent.bounds];
        m_drawer->m_mode = mode;
        m_drawer->m_fullscreen = NO;
        m_drawer->m_passThroughBackdrop = role
            == farcaster::platform::miniapp::BrowserRole::MiniApp;
        m_drawer.target = self;
        m_drawer.action = @selector(dismiss:);
        m_drawer.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        m_drawer.wantsLayer = YES;
        m_drawer.layer.masksToBounds = YES;
        m_drawer.layer.backgroundColor = [NSColor colorWithWhite:0 alpha:0.24].CGColor;
        // Transfer the already-constructed content, not a second WebKit instance.
        m_window.contentView = [[NSView alloc] initWithFrame:NSZeroRect];
        m_drawer->m_panel = content;
        content.autoresizingMask = NSViewNotSizable;
        content.wantsLayer = YES;
        content.layer.backgroundColor = NSColor.windowBackgroundColor.CGColor;
        content.layer.cornerRadius = 18;
        if (@available(macOS 10.15, *)) { content.layer.cornerCurve = kCACornerCurveContinuous; }
        if (mode == PresentationMode::Left) {
            // The screen-attached left edge is square; only the exposed edge is rounded.
            content.layer.maskedCorners = kCALayerMaxXMinYCorner | kCALayerMaxXMaxYCorner;
        } else if (mode == PresentationMode::Right) {
            // The screen-attached right edge is square; only the exposed edge is rounded.
            content.layer.maskedCorners = kCALayerMinXMinYCorner | kCALayerMinXMaxYCorner;
        } else {
            // Bottom sheets attach to the bottom edge and round only their top corners.
            content.layer.maskedCorners = kCALayerMinXMaxYCorner | kCALayerMaxXMaxYCorner;
        }
        content.layer.masksToBounds = YES;
        [m_drawer addSubview:content];
        [parentContent addSubview:m_drawer positioned:NSWindowAbove relativeTo:nil];
        m_drawer.hidden = initiallyVisible ? NO : YES;
        // Do not leave the composer/feed as first responder beneath the drawer
        // while the SDK is loading. Keep Tab navigation inside the host.
        close.nextKeyView = m_back;
        m_back.nextKeyView = reload;
        reload.nextKeyView = minimize;
        minimize.nextKeyView = share;
        share.nextKeyView = m_web;
        m_web.nextKeyView = close;
        if (initiallyVisible) { [parentWindow makeFirstResponder:close]; }
        [m_drawer layoutSubtreeIfNeeded];
        const NSRect finalFrame = [m_drawer panelFrame];
        if (initiallyVisible && !m_reduceMotion) {
            m_drawer->m_animating = YES;
            content.frame = [m_drawer hiddenFrame];
            __weak GenyMiniAppDrawer* weakDrawer = m_drawer;
            __weak GenyMiniAppWindow* weakOwner = self;
            [NSAnimationContext runAnimationGroup:^(NSAnimationContext* context) {
                context.duration = 0.18;
                context.timingFunction = [CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionEaseOut];
                content.animator.frame = finalFrame;
            } completionHandler:^{
                GenyMiniAppDrawer* drawer = weakDrawer;
                GenyMiniAppWindow* owner = weakOwner;
                if (drawer && owner && !owner->m_closed && !owner->m_dismissing) {
                    drawer->m_animating = NO;
                    drawer.needsLayout = YES;
                }
            }];
        } else { content.frame = finalFrame; }
        __weak GenyMiniAppWindow* weakSelf = self;
        m_escapeMonitor = [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskKeyDown
            handler:^NSEvent*(NSEvent* event) {
                GenyMiniAppWindow* owner = weakSelf;
                const BOOL drawerVisible = owner && owner->m_drawer && !owner->m_drawer.hidden;
                if (owner && !owner->m_closed && drawerVisible
                    && event.window == owner->m_parentWindow && event.keyCode == 53) {
                    if (owner->m_drawer->m_fullscreen) {
                        owner->m_drawer->m_fullscreen = NO;
                        owner->m_drawer->m_panel.layer.cornerRadius = 18;
                        owner->m_drawer.needsLayout = YES;
                    } else [owner dismiss:nil];
                    return nil;
                }
                return event;
            }];
    } else {
        [m_window center];
        if (initiallyVisible) { [m_window makeKeyAndOrderFront:nil]; }
    }
    return self;
}

- (void)dispose {
    m_closed = YES;
    m_callbacks = {};
    [NSNotificationCenter.defaultCenter removeObserver:self];
    if (m_escapeMonitor) { [NSEvent removeMonitor:m_escapeMonitor]; m_escapeMonitor = nil; }
    [m_web stopLoading];
    if (m_role == farcaster::platform::miniapp::BrowserRole::MiniApp) {
        [m_web.configuration.userContentController removeScriptMessageHandlerForName:@"genyMiniApp"];
        [m_web.configuration.userContentController removeAllUserScripts];
    }
    m_web.navigationDelegate = nil;
    m_web.UIDelegate = nil;
    [m_web removeFromSuperview];
    m_web = nil;
    if (m_drawer) {
        const BOOL wasVisible = !m_drawer.hidden;
        [m_drawer removeFromSuperview];
        m_drawer = nil;
        if (wasVisible && m_parentWindow.isVisible) {
            [m_parentWindow makeFirstResponder:m_previousResponder];
        }
    }
    m_window.delegate = nil;
    [m_window.parentWindow removeChildWindow:m_window];
    [m_window close];
}
- (void)windowDidBecomeKey:(NSNotification*)notification {
    (void)notification;
    if (!m_closed && m_mode == farcaster::platform::miniapp::PresentationMode::Window
        && m_callbacks.activated) {
        m_callbacks.activated();
    }
}
- (void)windowWillClose:(NSNotification*)notification {
    (void)notification;
    [self notifyClosed];
}
- (void)parentWillClose:(NSNotification*)notification {
    (void)notification;
    [self notifyClosed];
}
- (void)notifyClosed {
    if (!m_closed) {
        m_closed = YES;
        [m_web stopLoading];
        if (m_callbacks.closed) { m_callbacks.closed(); }
    }
}
- (void)minimize:(id)sender {
    (void)sender;
    if (!m_closed && m_callbacks.minimize) { m_callbacks.minimize(); }
}
- (void)toggleFullscreen:(id)sender {
    if (m_closed || m_dismissing) return;
    NSButton* button = [sender isKindOfClass:NSButton.class] ? (NSButton*)sender : nil;
    BOOL fullscreen = NO;
    if (m_drawer) {
        m_drawer->m_fullscreen = !m_drawer->m_fullscreen;
        fullscreen = m_drawer->m_fullscreen;
        m_drawer->m_panel.layer.cornerRadius = fullscreen ? 0 : 18;
        m_drawer.needsLayout = YES;
        [m_drawer layoutSubtreeIfNeeded];
    } else {
        fullscreen = (m_window.styleMask & NSWindowStyleMaskFullScreen) == 0;
        [m_window toggleFullScreen:sender];
    }
    if (button) {
        button.image = [NSImage imageWithSystemSymbolName:
            (fullscreen ? @"arrow.down.right.and.arrow.up.left" : @"arrow.up.left.and.arrow.down.right")
            accessibilityDescription:nativeText(translated(fullscreen
                ? "genycaster.player.exit_fullscreen" : "genycaster.player.fullscreen"))];
        button.toolTip = nativeText(translated(fullscreen
            ? "genycaster.player.exit_fullscreen" : "genycaster.player.fullscreen"));
    }
}
- (void)share:(id)sender {
    (void)sender;
    if (!m_closed && m_callbacks.share) { m_callbacks.share(); }
}
- (void)dismiss:(id)sender {
    (void)sender;
    if (m_closed || m_dismissing) { return; }
    m_dismissing = YES;
    if (!m_drawer || m_reduceMotion) { [self notifyClosed]; return; }
    m_drawer->m_animating = YES;
    const NSRect hidden = [m_drawer hiddenFrame];
    __weak GenyMiniAppWindow* weakSelf = self;
    [NSAnimationContext runAnimationGroup:^(NSAnimationContext* context) {
        context.duration = 0.14;
        context.timingFunction = [CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionEaseIn];
        m_drawer->m_panel.animator.frame = hidden;
    } completionHandler:^{ [weakSelf notifyClosed]; }];
}
- (void)showStatus:(NSString*)message failed:(BOOL)failed {
    m_failed = failed;
    m_ready = NO;
    m_status.stringValue = message;
    m_overlay.hidden = NO;
    // Keep the browser drawable beneath an opaque native splash. Hiding it
    // throttles requestAnimationFrame and can deadlock apps that await a frame
    // before calling sdk.actions.ready().
    m_web.hidden = NO;
    m_retry.hidden = !failed;
    m_spinner.hidden = failed;
    if (failed) {
        [m_spinner stopAnimation:nil];
        // Do not destroy WKWebView synchronously from inside one of its own
        // navigation/process delegate callbacks. WebKit may still be unwinding
        // that callback stack. Keep the failed view inert behind the opaque
        // host surface and perform delegate/script/view teardown only in
        // -dispose on the next session-lifecycle boundary.
        [m_web stopLoading];
        if (@available(macOS 12.0, *)) {
            [m_web setAllMediaPlaybackSuspended:YES completionHandler:nil];
        }
        m_back.enabled = NO;
    } else {
        [m_spinner startAnimation:nil];
    }
}
- (void)showReady {
    if (m_failed || m_closed) { return; }
    m_ready = YES;
    m_overlay.hidden = YES;
    m_web.hidden = NO;
    [m_spinner stopAnimation:nil];
    const BOOL hostVisible = m_drawer ? !m_drawer.hidden : m_window.isVisible;
    if (hostVisible) { [m_web.window makeFirstResponder:m_web]; }
}
- (void)openExternalMediaFallback {
    if (m_closed || m_mediaFallbackTriggered
        || m_role != farcaster::platform::miniapp::BrowserRole::Media) {
        return;
    }
    NSURL* watchUrl = youtubeWatchUrlForEmbed(m_origin);
    if (!watchUrl) {
        return;
    }
    m_mediaFallbackTriggered = YES;
    [NSWorkspace.sharedWorkspace openURL:watchUrl];
    [self dismiss:nil];
}
- (void)checkMediaPlaybackFallback {
    if (m_closed || m_mediaFallbackTriggered
        || m_role != farcaster::platform::miniapp::BrowserRole::Media || !m_web) {
        return;
    }
    // YouTube's anti-automation challenge is server-side. It cannot be safely
    // bypassed by spoofing headers or extracting media URLs. Detect the provider
    // challenge and hand the canonical watch URL to the user's normal browser,
    // where their authenticated YouTube session can satisfy it.
    NSString* script = @"(() => { const t = (document.body?.innerText || '').toLowerCase(); "
        @"return t.includes(\"confirm you’re not a bot\") || "
        @"t.includes(\"confirm you're not a bot\") || "
        @"t.includes(\"video player configuration error\") || "
        @"t.includes(\"error 153\") || "
        @"t.includes(\"playback on other websites has been disabled\"); })()";
    __weak GenyMiniAppWindow* weakSelf = self;
    [m_web evaluateJavaScript:script completionHandler:^(id result, NSError* error) {
        GenyMiniAppWindow* owner = weakSelf;
        if (!owner || owner->m_closed || owner->m_mediaFallbackTriggered || error) {
            return;
        }
        if ([result isKindOfClass:NSNumber.class] && [result boolValue]) {
            [owner openExternalMediaFallback];
        }
    }];
}
- (void)back:(id)sender {
    (void)sender;
    if (m_sdkBack) {
        [m_web evaluateJavaScript:@"document.dispatchEvent(new MessageEvent('FarcasterFrameEvent', {data: {event: 'back_navigation_triggered'}}))"
            completionHandler:nil];
    } else if (m_web.canGoBack) { [m_web goBack]; }
}
- (void)retry:(id)sender {
    (void)sender;
    if (!m_closed && m_callbacks.retry) { m_callbacks.retry(); }
}
- (void)userContentController:(WKUserContentController*)controller didReceiveScriptMessage:(WKScriptMessage*)message {
    (void)controller;
    if (m_role != farcaster::platform::miniapp::BrowserRole::MiniApp
        || m_closed || m_failed || !message.frameInfo.isMainFrame || message.webView != m_web) { return; }
    WKSecurityOrigin* securityOrigin = message.frameInfo.securityOrigin;
    if (![securityOrigin.protocol isEqualToString:@"https"]
        || ![securityOrigin.host.lowercaseString isEqualToString:m_origin.host.lowercaseString]
        || (securityOrigin.port != 0 && securityOrigin.port != 443)) { return; }
    const NSTimeInterval now = NSDate.timeIntervalSinceReferenceDate;
    if (now - m_rateSince >= 1.0) { m_rateSince = now; m_rateCount = 0; }
    if (++m_rateCount > 120) {
        if (m_callbacks.failed) { m_callbacks.failed(translated("genycaster.miniapp.rpc_limit")); }
        return;
    }
    if (![message.body isKindOfClass:NSDictionary.class]) { return; }
    NSDictionary* envelope = message.body;
    NSString* payload = envelope[@"message"];
    NSString* document = envelope[@"document"];
    if (![payload isKindOfClass:NSString.class] || payload.length > 32768
        || ![document isKindOfClass:NSString.class] || document.length > 64) { return; }
    const auto parsed = QJsonDocument::fromJson(QString::fromNSString(payload).toUtf8());
    if (parsed.isObject() && m_callbacks.message) {
        m_callbacks.message(parsed.object(), QString::fromNSString(document));
    }
}
- (void)webView:(WKWebView*)webView decidePolicyForNavigationAction:(WKNavigationAction*)action
    decisionHandler:(void (^)(WKNavigationActionPolicy))decisionHandler {
    NSURL* url = action.request.URL;
    const bool sameOrigin = [url.host.lowercaseString isEqualToString:m_origin.host.lowercaseString];
    const bool mainFrame = !action.targetFrame || action.targetFrame.isMainFrame;
    if (!allowedWebUrl(url) || (mainFrame && !sameOrigin)) {
        decisionHandler(WKNavigationActionPolicyCancel);
        if (mainFrame && m_callbacks.failed) {
            m_callbacks.failed(translated("genycaster.miniapp.navigation_blocked"));
        }
        return;
    }
    if (!action.targetFrame) {
        decisionHandler(WKNavigationActionPolicyCancel);
        [webView loadRequest:action.request];
        return;
    }
    decisionHandler(WKNavigationActionPolicyAllow);
}
- (void)webView:(WKWebView*)webView decidePolicyForNavigationResponse:(WKNavigationResponse*)response
    decisionHandler:(void (^)(WKNavigationResponsePolicy))decisionHandler {
    (void)webView;
    const bool sameOrigin = [response.response.URL.host.lowercaseString isEqualToString:m_origin.host.lowercaseString];
    if (!response.canShowMIMEType || !allowedWebUrl(response.response.URL)
        || (response.isForMainFrame && !sameOrigin)) {
        decisionHandler(WKNavigationResponsePolicyCancel);
        if (response.isForMainFrame && m_callbacks.failed) {
            m_callbacks.failed(translated("genycaster.miniapp.navigation_blocked"));
        }
        return;
    }
    decisionHandler(WKNavigationResponsePolicyAllow);
}
- (void)webView:(WKWebView*)webView didStartProvisionalNavigation:(WKNavigation*)navigation {
    (void)webView; (void)navigation;
    if (m_failed || m_closed) { return; }
    m_sdkBack = NO;
    m_back.enabled = NO;
    [self showStatus:nativeText(translated("genycaster.miniapp.loading")) failed:NO];
    if (m_callbacks.navigationStarted) { m_callbacks.navigationStarted(); }
}
- (void)webView:(WKWebView*)webView didFinishNavigation:(WKNavigation*)navigation {
    (void)navigation;
    m_back.enabled = m_sdkBack || webView.canGoBack;
    if (m_role == farcaster::platform::miniapp::BrowserRole::Media) {
        [self showReady];
        __weak GenyMiniAppWindow* weakSelf = self;
        for (const NSTimeInterval delay : {0.65, 2.0}) {
            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(delay * NSEC_PER_SEC)),
                dispatch_get_main_queue(), ^{
                    GenyMiniAppWindow* owner = weakSelf;
                    if (owner && !owner->m_closed && !owner->m_mediaFallbackTriggered) {
                        [owner checkMediaPlaybackFallback];
                    }
                });
        }
    }
    // Mini Apps still wait for the SDK ready signal; document completion alone
    // must not grant or imply Mini App readiness.
}
- (void)webView:(WKWebView*)webView didFailProvisionalNavigation:(WKNavigation*)navigation withError:(NSError*)error {
    (void)webView; (void)navigation;
    if (error.code != NSURLErrorCancelled && !m_closed && !m_failed && m_callbacks.failed) {
        m_callbacks.failed(translated("genycaster.miniapp.load_failed"));
    }
}
- (void)webView:(WKWebView*)webView didFailNavigation:(WKNavigation*)navigation withError:(NSError*)error {
    [self webView:webView didFailProvisionalNavigation:navigation withError:error];
}
- (void)webViewWebContentProcessDidTerminate:(WKWebView*)webView {
    (void)webView;
    if (!m_closed && !m_failed && m_callbacks.failed) {
        m_callbacks.failed(translated("genycaster.miniapp.renderer_failed"));
    }
}
- (WKWebView*)webView:(WKWebView*)webView createWebViewWithConfiguration:(WKWebViewConfiguration*)configuration
    forNavigationAction:(WKNavigationAction*)action windowFeatures:(WKWindowFeatures*)features {
    (void)webView; (void)configuration; (void)action; (void)features;
    return nil;
}
- (void)webView:(WKWebView*)webView runJavaScriptAlertPanelWithMessage:(NSString*)message
    initiatedByFrame:(WKFrameInfo*)frame completionHandler:(void (^)(void))completionHandler {
    (void)webView; (void)message; (void)frame;
    completionHandler();
}
- (void)webView:(WKWebView*)webView runJavaScriptConfirmPanelWithMessage:(NSString*)message
    initiatedByFrame:(WKFrameInfo*)frame completionHandler:(void (^)(BOOL))completionHandler {
    (void)webView; (void)message; (void)frame;
    completionHandler(NO);
}
- (void)webView:(WKWebView*)webView runJavaScriptTextInputPanelWithPrompt:(NSString*)prompt
    defaultText:(NSString*)defaultText initiatedByFrame:(WKFrameInfo*)frame
    completionHandler:(void (^)(NSString*))completionHandler {
    (void)webView; (void)prompt; (void)defaultText; (void)frame;
    completionHandler(nil);
}
- (void)webView:(WKWebView*)webView runOpenPanelWithParameters:(WKOpenPanelParameters*)parameters
    initiatedByFrame:(WKFrameInfo*)frame completionHandler:(void (^)(NSArray<NSURL*>*))completionHandler {
    (void)webView; (void)parameters; (void)frame;
    completionHandler(nil);
}
- (void)webView:(WKWebView*)webView requestMediaCapturePermissionForOrigin:(WKSecurityOrigin*)origin
    initiatedByFrame:(WKFrameInfo*)frame type:(WKMediaCaptureType)type
    decisionHandler:(void (^)(WKPermissionDecision))decisionHandler {
    (void)webView; (void)origin; (void)frame; (void)type;
    decisionHandler(WKPermissionDecisionDeny);
}
@end

namespace farcaster::platform::miniapp {
namespace {
class MacBrowser final : public Browser {
public:
    MacBrowser(const QString& title, const QUrl& origin, BrowserCallbacks callbacks,
        PresentationMode mode, bool reduceMotion, bool initiallyVisible, BrowserRole role)
        : m_controller([[GenyMiniAppWindow alloc] initWithTitle:nativeText(title) origin:origin.toNSURL()
            mode:mode reduceMotion:reduceMotion initiallyVisible:initiallyVisible role:role]) {
        if (m_controller != nil) {
            m_controller->m_callbacks = std::move(callbacks);
        }
    }
    ~MacBrowser() override { [m_controller dispose]; }
    [[nodiscard]] bool valid() const noexcept
    {
        return m_controller != nil && m_controller->m_window != nil && m_controller->m_web != nil;
    }
    void load(const QUrl& url, const QUrl& referrer) override {
        NSMutableURLRequest* request = [NSMutableURLRequest requestWithURL:url.toNSURL()];
        if (referrer.isValid()
            && (referrer.scheme() == QStringLiteral("https")
                || referrer.scheme() == QStringLiteral("http"))
            && !referrer.host().isEmpty()) {
            [request setValue:referrer.toString(QUrl::FullyEncoded).toNSString()
                forHTTPHeaderField:@"Referer"];
        }
        [m_controller->m_web loadRequest:request];
    }
    void status(const QString& message, bool failed) override {
        [m_controller showStatus:nativeText(message) failed:failed];
    }
    void ready(const bool disableNativeGestures) override {
        // Farcaster's flag controls host-owned dismissal/navigation gestures.
        // This desktop host does not install swipe-to-dismiss gestures, so do
        // not reinterpret the flag as WebKit history navigation and break the
        // Mini App's own back/forward UX. Mobile adapters can apply the flag to
        // their native sheet gesture recognizers when those exist.
        (void)disableNativeGestures;
        [m_controller showReady];
    }
    void setBackVisible(bool visible) override {
        m_controller->m_sdkBack = visible;
        m_controller->m_back.enabled = visible || m_controller->m_web.canGoBack;
    }
    void reply(const QJsonObject& value, const QString& document) override {
        // JSON encodes both arguments: untrusted strings never become JS source.
        const QJsonArray arguments{document, value};
        const QString json = QString::fromUtf8(QJsonDocument(arguments).toJson(QJsonDocument::Compact));
        const QString script = QStringLiteral("window.__genyMiniAppReply?.(...") + json + QStringLiteral(")");
        [m_controller->m_web evaluateJavaScript:nativeText(script) completionHandler:nil];
    }
    void ethereumProviderEvent(
        const QString& event, const QJsonArray& params, const QString& document) override {
        const QJsonObject wire{{QStringLiteral("event"), event}, {QStringLiteral("params"), params}};
        const QJsonArray arguments{document, wire};
        const QString json = QString::fromUtf8(QJsonDocument(arguments).toJson(QJsonDocument::Compact));
        const QString script = QStringLiteral("window.__genyMiniAppEthProviderEvent?.(...")
            + json + QStringLiteral(")");
        [m_controller->m_web evaluateJavaScript:nativeText(script) completionHandler:nil];
    }
    void miniAppEvent(const QJsonObject& event, const QString& document) override {
        const QJsonArray arguments{document, event};
        const QString json = QString::fromUtf8(QJsonDocument(arguments).toJson(QJsonDocument::Compact));
        const QString script = QStringLiteral("window.__genyMiniAppEvent?.(...")
            + json + QStringLiteral(")");
        [m_controller->m_web evaluateJavaScript:nativeText(script) completionHandler:nil];
    }
    void setVisible(const bool visible) override {
        if (m_controller->m_web) {
            if (@available(macOS 12.0, *)) {
                [m_controller->m_web setAllMediaPlaybackSuspended:visible ? NO : YES completionHandler:nil];
            }
        }
        if (m_controller->m_drawer) {
            m_controller->m_drawer.hidden = visible ? NO : YES;
            if (visible && m_controller->m_drawer.superview) {
                [m_controller->m_drawer.superview addSubview:m_controller->m_drawer
                    positioned:NSWindowAbove relativeTo:nil];
            }
            if (visible && m_controller->m_parentWindow.isVisible) {
                [m_controller->m_parentWindow makeFirstResponder:m_controller->m_web];
            }
        } else if (visible) {
            [m_controller->m_window makeKeyAndOrderFront:nil];
        } else {
            [m_controller->m_window orderOut:nil];
        }
    }
    qlonglong processIdentifier() const noexcept override {
        // Public WKWebView on macOS does not expose the WebContent PID. Returning zero keeps the
        // Task Manager honest and avoids private selectors that would make release builds fragile.
        return 0;
    }
private:
    GenyMiniAppWindow* __strong m_controller;
};
}

std::unique_ptr<Browser> createBrowser(const QString& title, const QUrl& origin, BrowserCallbacks callbacks,
    PresentationMode mode, bool reduceMotion, bool initiallyVisible, BrowserRole role) {
    auto browser = std::make_unique<MacBrowser>(
        title, origin, std::move(callbacks), mode, reduceMotion, initiallyVisible, role);
    if (!browser->valid()) {
        return {};
    }
    return browser;
}
}
