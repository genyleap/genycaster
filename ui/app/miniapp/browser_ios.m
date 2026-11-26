#import <UIKit/UIKit.h>
#import <WebKit/WebKit.h>
#include "browser_macos_native.h"
#include "browser_mobile_bridge.h"

// The existing C++ Browser adapter uses this C ABI on Apple platforms.
// UIKit and WebKit remain outside the C++26 module boundary.
@interface GenyIOSBrowser : NSObject <WKNavigationDelegate, WKUIDelegate, WKScriptMessageHandler>
@property(nonatomic) GenyMacBrowserCallbacks callbacks;
@property(nonatomic,strong) NSURL* origin;
@property(nonatomic,strong) UIView* surface;
@property(nonatomic,strong) WKWebView* web;
@property(nonatomic,strong) UILabel* statusLabel;
@property(nonatomic,strong) UIButton* back;
@property(nonatomic,copy) NSString* document;
@property(nonatomic,copy) NSString* retiredDocument;
@property(nonatomic) NSUInteger generation;
@property(nonatomic) NSTimeInterval rateSince;
@property(nonatomic) NSUInteger rateCount;
@property(nonatomic) BOOL failed,closed,sdkBack;
@property(nonatomic) GenyMacBrowserRole role;
@end

static UIWindow* hostWindow(void) {
    for (UIScene* scene in UIApplication.sharedApplication.connectedScenes) {
        if (![scene isKindOfClass:UIWindowScene.class] ||
            scene.activationState != UISceneActivationStateForegroundActive) continue;
        for (UIWindow* window in ((UIWindowScene*)scene).windows)
            if (window.isKeyWindow) return window;
    }
    // Qt may use the application delegate rather than a scene delegate.
    if (UIApplication.sharedApplication.connectedScenes.count == 0) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        return UIApplication.sharedApplication.keyWindow;
#pragma clang diagnostic pop
    }
    return nil;
}
@implementation GenyIOSBrowser
- (NSString*)localized:(const char*)key {
    const char* value=self.callbacks.localize ? self.callbacks.localize(self.callbacks.context,key) : key;
    return value ? [NSString stringWithUTF8String:value] : @"";
}
- (void)event:(GenyMacBrowserEvent)event {
    if(!self.closed && self.callbacks.event) self.callbacks.event(self.callbacks.context,event);
}
- (BOOL)allowed:(NSURL*)url main:(BOOL)mainFrame {
    if(!url || !self.callbacks.allowed_url || !self.callbacks.allowed_url(self.callbacks.context,url.absoluteString.UTF8String)) return NO;
    if(self.role==GENY_MAC_WALLET_APPROVAL) return mainFrame && [url isEqual:self.origin];
    return !mainFrame || [url.host.lowercaseString isEqualToString:self.origin.host.lowercaseString];
}
- (void)fail:(const char*)key {
    if(self.closed || self.failed) return;
    self.failed=YES;self.generation++;self.document=nil;
    [self.web stopLoading];
    if(self.callbacks.failed) self.callbacks.failed(self.callbacks.context,key);
}
- (UIButton*)button:(NSString*)title action:(SEL)action label:(const char*)label {
    UIButton* button=[UIButton buttonWithType:UIButtonTypeSystem];
    [button setTitle:title forState:UIControlStateNormal];
    button.accessibilityLabel=[self localized:label];
    [button addTarget:self action:action forControlEvents:UIControlEventTouchUpInside];
    [button.widthAnchor constraintGreaterThanOrEqualToConstant:44].active=YES;
    return button;
}
- (BOOL)initializeTitle:(NSString*)title visible:(BOOL)visible {
    if(!hostWindow()) return NO;
    self.surface=[[UIView alloc] init];
    self.surface.backgroundColor=UIColor.systemBackgroundColor;
    self.surface.accessibilityViewIsModal=YES;
    WKWebViewConfiguration* config=[[WKWebViewConfiguration alloc] init];
    config.websiteDataStore=WKWebsiteDataStore.nonPersistentDataStore;
    config.preferences.javaScriptCanOpenWindowsAutomatically=NO;
    config.allowsInlineMediaPlayback=YES;
    if(self.role==GENY_MAC_MINIAPP) {
        [config.userContentController addScriptMessageHandler:self name:@"genyMiniApp"];
        [config.userContentController addUserScript:[[WKUserScript alloc]
            initWithSource:[NSString stringWithUTF8String:GENY_MOBILE_BRIDGE_SCRIPT]
            injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:YES]];
    }
    self.web=[[WKWebView alloc] initWithFrame:CGRectZero configuration:config];
    self.web.navigationDelegate=self;self.web.UIDelegate=self;
    self.web.allowsBackForwardNavigationGestures=NO;
    self.back=[self button:@"‹" action:@selector(goBack) label:"genycaster.miniapp.back"];
    UILabel* heading=[[UILabel alloc] init];
    heading.text=title;heading.font=[UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
    heading.adjustsFontForContentSizeCategory=YES;heading.lineBreakMode=NSLineBreakByTruncatingTail;
    [heading setContentCompressionResistancePriority:UILayoutPriorityDefaultLow forAxis:UILayoutConstraintAxisHorizontal];
    UIStackView* bar=[[UIStackView alloc] initWithArrangedSubviews:@[self.back,heading,
        [self button:@"↻" action:@selector(retry) label:"genycaster.miniapp.retry"],
        [self button:@"−" action:@selector(minimize) label:"genycaster.miniapp.minimize"],
        [self button:@"×" action:@selector(close) label:"genycaster.miniapp.close"]]];
    if(self.role==GENY_MAC_WALLET_APPROVAL){bar.arrangedSubviews[2].hidden=YES;bar.arrangedSubviews[3].hidden=YES;}
    bar.axis=UILayoutConstraintAxisHorizontal;bar.alignment=UIStackViewAlignmentCenter;
    [bar.heightAnchor constraintGreaterThanOrEqualToConstant:44].active=YES;
    self.statusLabel=[[UILabel alloc] init];self.statusLabel.numberOfLines=0;
    self.statusLabel.font=[UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    self.statusLabel.hidden=YES;
    UIStackView* stack=[[UIStackView alloc] initWithArrangedSubviews:@[bar,self.statusLabel,self.web]];
    stack.axis=UILayoutConstraintAxisVertical;stack.translatesAutoresizingMaskIntoConstraints=NO;
    [self.surface addSubview:stack];
    UILayoutGuide* safe=self.surface.safeAreaLayoutGuide;
    [NSLayoutConstraint activateConstraints:@[
        [stack.topAnchor constraintEqualToAnchor:safe.topAnchor],
        [stack.bottomAnchor constraintEqualToAnchor:safe.bottomAnchor],
        [stack.leadingAnchor constraintEqualToAnchor:safe.leadingAnchor],
        [stack.trailingAnchor constraintEqualToAnchor:safe.trailingAnchor]]];
    if(visible) [self setBrowserVisible:YES];
    return YES;
}
- (void)setBrowserVisible:(BOOL)visible {
    if(self.closed) return;
    if(!visible){[self.surface removeFromSuperview];return;}
    UIWindow* window=hostWindow();
    if(!window)return;
    self.surface.frame=window.bounds;
    self.surface.autoresizingMask=UIViewAutoresizingFlexibleWidth|UIViewAutoresizingFlexibleHeight;
    [window addSubview:self.surface];
}
- (void)goBack {
    if(self.role==GENY_MAC_WALLET_APPROVAL){[self event:GENY_MAC_CLOSED];return;}
    if(!self.failed && self.sdkBack && self.document.length) {
        [self.web evaluateJavaScript:@"document.dispatchEvent(new MessageEvent('FarcasterFrameEvent',{data:{event:'back_navigation_triggered'}}))" completionHandler:nil];
    } else if(!self.failed && self.web.canGoBack) [self.web goBack];
    else [self event:GENY_MAC_CLOSED];
}
- (void)retry { [self event:GENY_MAC_RETRY]; }
- (void)minimize { [self event:GENY_MAC_MINIMIZE]; }
- (void)close { [self event:GENY_MAC_CLOSED]; }
- (void)dispose {
    self.closed=YES;self.generation++;self.document=nil;
    self.web.navigationDelegate=nil;self.web.UIDelegate=nil;
    [self.web.configuration.userContentController removeScriptMessageHandlerForName:@"genyMiniApp"];
    [self.web.configuration.userContentController removeAllUserScripts];
    [self.web stopLoading];[self.surface removeFromSuperview];
    self.web=nil;self.surface=nil;
}
- (void)userContentController:(WKUserContentController*)controller didReceiveScriptMessage:(WKScriptMessage*)message {
    (void)controller;
    if(self.closed || self.failed || self.role!=GENY_MAC_MINIAPP || message.webView!=self.web ||
       !message.frameInfo.isMainFrame) return;
    WKSecurityOrigin* origin=message.frameInfo.securityOrigin;
    if(![origin.protocol isEqualToString:@"https"] ||
       ![origin.host.lowercaseString isEqualToString:self.origin.host.lowercaseString] ||
       (origin.port!=0 && origin.port!=443)) return;
    NSTimeInterval now=NSProcessInfo.processInfo.systemUptime;
    if(now-self.rateSince>=1){self.rateSince=now;self.rateCount=0;}
    if(++self.rateCount>120){[self fail:"genycaster.miniapp.rpc_limit"];return;}
    if(![message.body isKindOfClass:NSDictionary.class]) return;
    NSDictionary* envelope=message.body;
    NSString* token=envelope[@"document"];NSString* payload=envelope[@"message"];
    if(![token isKindOfClass:NSString.class] || token.length!=36 || [token isEqualToString:self.retiredDocument] ||
       ![payload isKindOfClass:NSString.class] || payload.length>32768) return;
    NSUInteger epoch=self.generation;
    __weak GenyIOSBrowser* weakSelf=self;
    [self.web evaluateJavaScript:@"window.__genyMobileDocument || null" completionHandler:^(id result,NSError* error){
        GenyIOSBrowser* owner=weakSelf;
        if(!owner || owner.closed || owner.failed || error || epoch!=owner.generation ||
           ![result isKindOfClass:NSString.class] || ![token isEqualToString:result]) return;
        if(owner.document.length && ![owner.document isEqualToString:token])return;
        owner.document=token;
        if(owner.callbacks.message)owner.callbacks.message(owner.callbacks.context,payload.UTF8String,token.UTF8String);
    }];
}
- (void)webView:(WKWebView*)web decidePolicyForNavigationAction:(WKNavigationAction*)action decisionHandler:(void (^)(WKNavigationActionPolicy))decision {
    (void)web;
    BOOL mainFrame=!action.targetFrame || action.targetFrame.isMainFrame;
    NSString* target=action.request.URL.absoluteString;
    if(self.role==GENY_MAC_WALLET_APPROVAL && mainFrame
       && action.navigationType==WKNavigationTypeLinkActivated && target.length<=4096
       && [target rangeOfString:@"^wc:[0-9a-fA-F-]+@2\\?.+$" options:NSRegularExpressionSearch].location!=NSNotFound){
        decision(WKNavigationActionPolicyCancel);
        [UIApplication.sharedApplication openURL:action.request.URL options:@{} completionHandler:nil];
        return;
    }
    if(!action.targetFrame || ![self allowed:action.request.URL main:mainFrame]) {
        decision(WKNavigationActionPolicyCancel);
        if(mainFrame)[self fail:"genycaster.miniapp.navigation_blocked"];
        return;
    }
    decision(WKNavigationActionPolicyAllow);
}
- (void)webView:(WKWebView*)web decidePolicyForNavigationResponse:(WKNavigationResponse*)response decisionHandler:(void (^)(WKNavigationResponsePolicy))decision {
    (void)web;
    if(!response.canShowMIMEType || ![self allowed:response.response.URL main:response.isForMainFrame]){
        decision(WKNavigationResponsePolicyCancel);
        if(response.isForMainFrame)[self fail:"genycaster.miniapp.navigation_blocked"];
        return;
    }
    decision(WKNavigationResponsePolicyAllow);
}
- (void)webView:(WKWebView*)web didStartProvisionalNavigation:(WKNavigation*)navigation {
    (void)web;(void)navigation;
    self.generation++;if(self.document.length)self.retiredDocument=self.document;self.document=nil;self.sdkBack=NO;
    if(!self.failed){self.statusLabel.text=[self localized:"genycaster.miniapp.loading"];self.statusLabel.hidden=NO;[self event:GENY_MAC_NAVIGATION_STARTED];}
}
- (void)webView:(WKWebView*)web didFinishNavigation:(WKNavigation*)navigation {
    (void)web;(void)navigation;
    if(self.role!=GENY_MAC_MINIAPP && !self.failed)self.statusLabel.hidden=YES;
}
- (void)webView:(WKWebView*)web didFailNavigation:(WKNavigation*)navigation withError:(NSError*)error {
    (void)web;(void)navigation;if(error.code!=NSURLErrorCancelled)[self fail:"genycaster.miniapp.load_failed"];
}
- (void)webView:(WKWebView*)web didFailProvisionalNavigation:(WKNavigation*)navigation withError:(NSError*)error {
    [self webView:web didFailNavigation:navigation withError:error];
}
- (void)webView:(WKWebView*)web runJavaScriptAlertPanelWithMessage:(NSString*)message initiatedByFrame:(WKFrameInfo*)frame completionHandler:(void (^)(void))completion {
    (void)web;(void)message;(void)frame;completion();
}
- (void)webView:(WKWebView*)web runJavaScriptConfirmPanelWithMessage:(NSString*)message initiatedByFrame:(WKFrameInfo*)frame completionHandler:(void (^)(BOOL))completion {
    (void)web;(void)message;(void)frame;completion(NO);
}
- (void)webView:(WKWebView*)web runJavaScriptTextInputPanelWithPrompt:(NSString*)prompt defaultText:(NSString*)text initiatedByFrame:(WKFrameInfo*)frame completionHandler:(void (^)(NSString*))completion {
    (void)web;(void)prompt;(void)text;(void)frame;completion(nil);
}
- (void)webViewWebContentProcessDidTerminate:(WKWebView*)web {
    (void)web;[self fail:"genycaster.miniapp.renderer_failed"];
}
- (void)webView:(WKWebView*)web requestMediaCapturePermissionForOrigin:(WKSecurityOrigin*)origin initiatedByFrame:(WKFrameInfo*)frame type:(WKMediaCaptureType)type decisionHandler:(void (^)(WKPermissionDecision))decision {
    (void)web;(void)origin;(void)frame;(void)type;decision(WKPermissionDecisionDeny);
}
@end

static GenyIOSBrowser* owner(GenyMacBrowser* browser) {return browser ? (__bridge GenyIOSBrowser*)(void*)browser : nil;}
GenyMacBrowser* geny_mac_browser_create(const char* title,const char* origin,GenyMacBrowserMode mode,int reduce_motion,int initially_visible,GenyMacBrowserRole role,GenyMacBrowserCallbacks callbacks) {
    (void)mode;(void)reduce_motion;
    GenyIOSBrowser* browser=[[GenyIOSBrowser alloc] init];
    browser.callbacks=callbacks;browser.origin=[NSURL URLWithString:[NSString stringWithUTF8String:origin]];browser.role=role;
    if(![browser allowed:browser.origin main:YES] || ![browser initializeTitle:[NSString stringWithUTF8String:title] visible:initially_visible])return NULL;
    return (GenyMacBrowser*)CFBridgingRetain(browser);
}
int geny_mac_browser_valid(GenyMacBrowser* browser){return owner(browser)!=nil;}
void geny_mac_browser_destroy(GenyMacBrowser* browser){
    if(browser){GenyIOSBrowser* b=(__bridge_transfer GenyIOSBrowser*)(void*)browser;[b dispose];}
}
void geny_mac_browser_load(GenyMacBrowser* browser,const char* url,const char* referrer){
    GenyIOSBrowser* b=owner(browser);if(!b)return;
    NSURL* destination=[NSURL URLWithString:[NSString stringWithUTF8String:url]];
    if(![b allowed:destination main:YES]){[b fail:"genycaster.miniapp.navigation_blocked"];return;}
    b.failed=NO;b.web.hidden=NO;
    NSMutableURLRequest* request=[NSMutableURLRequest requestWithURL:destination];
    if(referrer && *referrer)[request setValue:[NSString stringWithUTF8String:referrer] forHTTPHeaderField:@"Referer"];
    [b.web loadRequest:request];
}
void geny_mac_browser_status(GenyMacBrowser* browser,const char* message,int failed){
    GenyIOSBrowser* b=owner(browser);if(!b)return;
    b.statusLabel.text=[NSString stringWithUTF8String:message];b.statusLabel.hidden=NO;
    if(failed){b.failed=YES;b.web.hidden=YES;}
}
void geny_mac_browser_ready(GenyMacBrowser* browser){GenyIOSBrowser* b=owner(browser);if(b&&!b.failed)b.statusLabel.hidden=YES;}
void geny_mac_browser_set_back(GenyMacBrowser* browser,int visible){owner(browser).sdkBack=visible;}
void geny_mac_browser_evaluate(GenyMacBrowser* browser,const char* script){
    GenyIOSBrowser* b=owner(browser);if(b&&!b.failed&&!b.closed)[b.web evaluateJavaScript:[NSString stringWithUTF8String:script] completionHandler:nil];
}
void geny_mac_browser_set_visible(GenyMacBrowser* browser,int visible){[owner(browser) setBrowserVisible:visible];}
