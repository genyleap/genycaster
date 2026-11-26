package com.genyleap.genycaster;

import android.app.Activity;
import android.app.Dialog;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.graphics.Color;
import android.graphics.BitmapFactory;
import android.util.Base64;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.RippleDrawable;
import android.content.res.ColorStateList;
import android.util.Log;
import android.net.http.SslError;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.webkit.*;
import android.widget.*;
import androidx.webkit.ProfileStore;
import androidx.webkit.WebViewCompat;
import androidx.webkit.WebViewFeature;
import org.json.JSONObject;
import org.json.JSONArray;
import java.util.Collections;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;
import java.util.UUID;

// All views and session state belong to Android's UI thread. JNI events carry
// monotonically assigned IDs, never C++ pointers.
public final class MiniAppBrowser {
    private static final Handler UI = new Handler(Looper.getMainLooper());
    private static final Map<Long, MiniAppBrowser> sessions = new HashMap<>();
    private static boolean cleanedAbandonedProfiles;
    private static native void nativeEvent(long id, int event, String payload, String document);
    private final long id;
    private final String origin;
    private final boolean sdk, wallet;
    private final String profile = "geny-miniapp-" + UUID.randomUUID();
    private Dialog dialog;
    private WebView web;
    private TextView status;
    private ImageButton back;
    private LinearLayout outer;
    private boolean initializing, wantsVisible;
    private String pendingUrl="", pendingReferrer="";
    private int ink, surface, border;
    private JSONObject icons;
    private long openedAt;
    private int dp(float value) {
        return Math.round(value * outer.getResources().getDisplayMetrics().density);
    }
    // Use the same quiet 24dp line icons and 44dp touch areas as the
    // shared toolbar; avoid Android's default raised text buttons.
    private ImageButton tool(Activity activity, String glyph, String label, Runnable click) {
        ImageButton button=new ImageButton(activity);
        button.setContentDescription(label);
        button.setTooltipText(label);
        button.setPadding(dp(13),dp(13),dp(13),dp(13));
        button.setScaleType(ImageView.ScaleType.FIT_CENTER);
        button.setBackground(new RippleDrawable(ColorStateList.valueOf(
            (ink & 0x00ffffff) | 0x22000000),null,new GradientDrawable()));
        // The source SVG is rasterized by Qt from the actual Genyleap
        // icon pack; Android only scales and tints its alpha mask.
        if (icons != null) {
            try {
                byte[] bytes=Base64.decode(icons.optString(glyph,""),Base64.DEFAULT);
                button.setImageBitmap(BitmapFactory.decodeByteArray(bytes,0,bytes.length));
                button.setImageTintList(new ColorStateList(
                    new int[][]{new int[]{-android.R.attr.state_enabled},new int[]{}},
                    new int[]{(ink & 0x00ffffff) | 0x55000000,ink}));
            } catch(IllegalArgumentException ignored) {}
        }
        button.setOnClickListener(v->click.run());
        return button;
    }
    private boolean disposed, failed, sdkBack, profileAssigned;
    private long generation, rateSince;
    private int rateCount;
    private String document = "", retiredDocument = "";
    private static final class Rpc {
        final long epoch;
        final String token, payload;
        Rpc(long epoch,String token,String payload){this.epoch=epoch;this.token=token;this.payload=payload;}
    }
    private final ArrayList<Rpc> queuedRpc=new ArrayList<>();
    private boolean rpcScheduled, rpcVerifying;
    private void scheduleRpc(){
        if(rpcScheduled || rpcVerifying || queuedRpc.isEmpty() || disposed || failed || web==null)return;
        rpcScheduled=true;
        UI.post(()->{
            rpcScheduled=false;
            if(disposed || failed || web==null){queuedRpc.clear();return;}
            final ArrayList<Rpc> batch=new ArrayList<>(queuedRpc);
            queuedRpc.clear();
            final long epoch=generation;
            rpcVerifying=true;
            web.evaluateJavascript("window.__genyMobileDocument || null",result->{
                rpcVerifying=false;
                if(!disposed && !failed && epoch==generation){
                    for(Rpc rpc:batch){
                        if(rpc.epoch!=epoch || !JSONObject.quote(rpc.token).equals(result))continue;
                        if(!document.isEmpty() && !document.equals(rpc.token))continue;
                        document=rpc.token;event(6,rpc.payload,rpc.token);
                    }
                }
                scheduleRpc();
            });
        });
    }

    private MiniAppBrowser(long id, String origin, int role) {
        this.id=id; this.origin=origin; this.sdk=role==0; this.wallet=role==2;
    }
    private void event(int type, String value, String token) {
        if (!disposed) nativeEvent(id,type,value,token);
    }
    private void event(int type) { event(type,"",""); }
    private void fail(String code) {
        if (disposed || failed) return;
        failed=true; generation++; document=""; queuedRpc.clear();
        if (web!=null) web.stopLoading();
        event(5,code,"");
    }
    private boolean allowed(Uri uri) {
        if (wallet) return uri!=null && uri.toString().equals(origin)
            && origin.matches("http://127\\.0\\.0\\.1:[0-9]+/session/[0-9a-f]{64}");
        if (uri==null || !"https".equalsIgnoreCase(uri.getScheme()) || uri.getUserInfo()!=null) return false;
        String host=uri.getHost();
        if (host==null || (uri.getPort()!=-1 && uri.getPort()!=443)) return false;
        return origin.equalsIgnoreCase("https://"+host);
    }
    private static void with(long id, java.util.function.Consumer<MiniAppBrowser> action) {
        UI.post(()->{ MiniAppBrowser s=sessions.get(id); if(s!=null && !s.disposed) action.accept(s); });
    }
    public static void create(Context context,long id,String title,String origin,String bridge,String controls,boolean visible,int role) {
        UI.post(()->{
            MiniAppBrowser s=new MiniAppBrowser(id,origin,role);
            sessions.put(id,s);
            try {
                if (!(context instanceof Activity)) throw new IllegalStateException("No activity");
                s.initialize((Activity)context,title,bridge,controls,visible);
            } catch (RuntimeException error) { s.fail("genycaster.miniapp.unavailable"); }
        });
    }
    private void initialize(Activity activity,String title,String bridge,String controls,boolean visible) {
        openedAt=SystemClock.elapsedRealtime();
        initializing=true; wantsVisible=visible;
        dialog=new Dialog(activity, android.R.style.Theme_DeviceDefault_NoActionBar) {
            @Override public void onBackPressed() { MiniAppBrowser.this.goBack(); }
        };
        dialog.requestWindowFeature(Window.FEATURE_NO_TITLE);
        dialog.setCancelable(false);
        outer=new LinearLayout(activity);
        outer.setOrientation(LinearLayout.VERTICAL);
        LinearLayout bar=new LinearLayout(activity);
        bar.setGravity(android.view.Gravity.CENTER_VERTICAL);
        JSONArray labels;
        try { labels=new JSONArray(controls); } catch(Exception ignored) { labels=new JSONArray(); }
        boolean dark=labels.optBoolean(6,false);
        icons=labels.optJSONObject(7);
        surface=Color.parseColor(dark?"#1B1D23":"#FFFFFF");
        ink=Color.parseColor(dark?"#F7F7F8":"#17181B");
        border=Color.parseColor(dark?"#22252B":"#ECEDEF");
        outer.setBackgroundColor(surface);
        bar.setBackgroundColor(surface);bar.setPadding(dp(8),0,dp(8),0);
        back=tool(activity,"back",labels.optString(0,"Back"),this::goBack);
        bar.addView(back,new LinearLayout.LayoutParams(dp(44),dp(44)));
        back.setEnabled(wallet);
        TextView heading=new TextView(activity); heading.setText(title); heading.setSingleLine(true);
        heading.setTextColor(ink);heading.setTextSize(14);
        heading.setTypeface(Typeface.DEFAULT,Typeface.BOLD);
        heading.setGravity(android.view.Gravity.CENTER);
        heading.setEllipsize(android.text.TextUtils.TruncateAt.END);
        bar.addView(heading,new LinearLayout.LayoutParams(0,-1,1));
        if(!wallet){
            bar.addView(tool(activity,"refresh",labels.optString(1,"Refresh"),()->event(1)),new LinearLayout.LayoutParams(dp(44),dp(44)));
            if(sdk) bar.addView(tool(activity,"share",labels.optString(4,"Share"),()->event(3)),new LinearLayout.LayoutParams(dp(44),dp(44)));
            bar.addView(tool(activity,"minimize",labels.optString(2,"Minimize"),()->event(2)),new LinearLayout.LayoutParams(dp(44),dp(44)));
        }
        bar.addView(tool(activity,"close",labels.optString(3,"Close"),()->event(0)),new LinearLayout.LayoutParams(dp(44),dp(44)));
        outer.addView(bar,new LinearLayout.LayoutParams(-1,dp(52)));
        View divider=new View(activity);divider.setBackgroundColor(border);
        outer.addView(divider,new LinearLayout.LayoutParams(-1,dp(1)));
        status=new TextView(activity);status.setTextColor(ink);status.setTextSize(13);
        status.setText(labels.optString(5,"Loading…"));status.setPadding(dp(16),dp(8),dp(16),dp(8));outer.addView(status);
        dialog.setContentView(outer);
        if(wantsVisible)show();
        // Let the host paint first. WebView and profile creation must stay
        // on Android's UI thread; don't move them to an unsafe worker.
        UI.postDelayed(()->{
            if(disposed || failed){initializing=false;return;}
            try { initializeWeb(activity,bridge); }
            catch(RuntimeException error) {
                initializing=false;
                Log.w("GenycasterMiniApp","WebView initialization failed: "+error.getClass().getSimpleName());
                fail("genycaster.miniapp.unavailable");
            }
        },32);
    }
    private void initializeWeb(Activity activity,String bridge) {
        if (!WebViewFeature.isFeatureSupported(WebViewFeature.MULTI_PROFILE) ||
            (sdk && (!WebViewFeature.isFeatureSupported(WebViewFeature.WEB_MESSAGE_LISTENER)
                    || !WebViewFeature.isFeatureSupported(WebViewFeature.DOCUMENT_START_SCRIPT))))
            throw new IllegalStateException("Required isolated WebView features unavailable");
        web=new WebView(activity);
        // Set isolated profile before any navigation or JS call.
        ProfileStore store=ProfileStore.getInstance();
        if(!cleanedAbandonedProfiles){
            cleanedAbandonedProfiles=true;
            for(String stale:store.getAllProfileNames())
                if(stale.startsWith("geny-miniapp-"))try{store.deleteProfile(stale);}catch(RuntimeException ignored){}
        }
        store.getOrCreateProfile(profile);
        profileAssigned=true;
        WebViewCompat.setProfile(web,profile);
        WebSettings settings=web.getSettings();
        settings.setJavaScriptEnabled(true);
        settings.setDomStorageEnabled(true);
        settings.setAllowFileAccess(false);
        settings.setAllowContentAccess(false);
        settings.setMixedContentMode(WebSettings.MIXED_CONTENT_NEVER_ALLOW);
        settings.setJavaScriptCanOpenWindowsAutomatically(false);
        settings.setSupportMultipleWindows(false);
        settings.setGeolocationEnabled(false);
        web.setDownloadListener((url,ua,disposition,mime,length)->{});
        web.setWebChromeClient(new WebChromeClient() {
            @Override public boolean onJsAlert(WebView view,String url,String message,JsResult result){result.cancel();return true;}
            @Override public boolean onJsConfirm(WebView view,String url,String message,JsResult result){result.cancel();return true;}
            @Override public boolean onJsPrompt(WebView view,String url,String message,String defaultValue,JsPromptResult result){result.cancel();return true;}
            @Override public void onPermissionRequest(PermissionRequest request) { request.deny(); }
            @Override public void onGeolocationPermissionsShowPrompt(String origin,GeolocationPermissions.Callback callback) {
                callback.invoke(origin,false,false);
            }
            @Override public boolean onShowFileChooser(WebView v,ValueCallback<Uri[]> callback,FileChooserParams params) {
                callback.onReceiveValue(null); return true;
            }
        });
        web.setWebViewClient(new WebViewClient() {
            @Override public boolean shouldOverrideUrlLoading(WebView v,WebResourceRequest request) {
                if(!request.isForMainFrame()) {
                    Uri target=request.getUrl();
                    if(wallet) return !("https".equalsIgnoreCase(target.getScheme())
                        && "verify.walletconnect.org".equalsIgnoreCase(target.getHost())
                        && target.getUserInfo()==null && (target.getPort()==-1 || target.getPort()==443));
                    return !"https".equalsIgnoreCase(target.getScheme());
                }
                String target=request.getUrl().toString();
                if(wallet && request.hasGesture() && target.length()<=4096
                    && target.matches("wc:[0-9a-fA-F-]+@2\\?.+")) {
                    try {
                        Intent walletIntent=new Intent(Intent.ACTION_VIEW,request.getUrl());
                        walletIntent.addCategory(Intent.CATEGORY_BROWSABLE);
                        activity.startActivity(walletIntent);
                    } catch (android.content.ActivityNotFoundException | SecurityException ignored) {
                        // No compatible installed handler: keep pairing QR and
                        // copy-link available instead of opening a browser.
                        android.widget.Toast.makeText(activity,
                            "No compatible wallet app found. Use the pairing QR or copy the link.",
                            android.widget.Toast.LENGTH_LONG).show();
                    }
                    return true;
                }
                if(!allowed(request.getUrl())) { fail("genycaster.miniapp.navigation_blocked"); return true; }
                return false;
            }
            @Override public void onPageStarted(WebView v,String url,android.graphics.Bitmap icon) {
                generation++; queuedRpc.clear(); if(!document.isEmpty()) retiredDocument=document; document=""; sdkBack=false;
                if (!allowed(Uri.parse(url))) { fail("genycaster.miniapp.navigation_blocked"); return; }
                if (!failed) event(4);
            }
            @Override public void onPageFinished(WebView v,String url) {
                if (!allowed(Uri.parse(url))) { fail("genycaster.miniapp.navigation_blocked"); return; }
                if(!sdk && !failed) status.setVisibility(View.GONE);
                back.setEnabled(wallet || sdkBack || v.canGoBack());
            }
            @Override public void onReceivedSslError(WebView v,SslErrorHandler handler,SslError error) {
                handler.cancel(); fail("genycaster.miniapp.load_failed");
            }
            @Override public void onReceivedError(WebView v,WebResourceRequest request,WebResourceError error) {
                if(request.isForMainFrame()) fail("genycaster.miniapp.load_failed");
            }
            @Override public void onReceivedHttpError(WebView v,WebResourceRequest request,WebResourceResponse response) {
                if(request.isForMainFrame()) fail("genycaster.miniapp.load_failed");
            }
            @Override public boolean onRenderProcessGone(WebView v,RenderProcessGoneDetail detail) {
                // A terminated renderer's view cannot be reused.
                generation++;document="";failed=true;queuedRpc.clear();
                if(v.getParent() instanceof android.view.ViewGroup)
                    ((android.view.ViewGroup)v.getParent()).removeView(v);
                v.destroy();web=null;
                event(5,"genycaster.miniapp.renderer_failed","");
                return true;
            }
        });
        if (sdk) {
            WebViewCompat.addWebMessageListener(web,"genyMobileTransport",Collections.singleton(origin),
                (view,message,sourceOrigin,isMainFrame,replyProxy)->{
                    if(!isMainFrame || !allowed(sourceOrigin) || view!=web || failed || disposed) return;
                    String raw;
                    try { raw=message.getData(); } catch(RuntimeException invalidMessage) { return; }
                    if(raw==null || raw.length()>65536) return;
                    long now=SystemClock.elapsedRealtime();
                    if(now-rateSince>=1000){rateSince=now;rateCount=0;}
                    if(++rateCount>120){fail("genycaster.miniapp.rpc_limit");return;}
                    try {
                        JSONObject envelope=new JSONObject(raw);
                        String token=envelope.getString("document"), payload=envelope.getString("message");
                        if(!token.matches("[0-9a-fA-F-]{36}") || token.equals(retiredDocument) || payload.length()>32768) return;
                        // Main-frame/origin validation above remains per message.
                        // Batch only the expensive document verification.
                        if(queuedRpc.size()>=120){fail("genycaster.miniapp.rpc_limit");return;}
                        queuedRpc.add(new Rpc(generation,token,payload));
                        scheduleRpc();
                    } catch (Exception ignored) { }
                });
            WebViewCompat.addDocumentStartJavaScript(web,bridge,Collections.singleton(origin));
        }
        outer.addView(web,new LinearLayout.LayoutParams(-1,0,1));
        initializing=false;
        Log.i("GenycasterMiniApp","Native WebView ready in "+(SystemClock.elapsedRealtime()-openedAt)+" ms");
        if(!wantsVisible)web.onPause();
        if(!pendingUrl.isEmpty()){
            String url=pendingUrl, referrer=pendingReferrer;
            pendingUrl="";pendingReferrer="";
            loadPage(url,referrer);
        }
    }
    private void show() {
        if(dialog==null || disposed) return;
        if(!dialog.isShowing()) dialog.show();
        Window window=dialog.getWindow();
        if(window!=null) {
            window.setBackgroundDrawable(new android.graphics.drawable.ColorDrawable(surface));
            window.setLayout(WindowManager.LayoutParams.MATCH_PARENT,WindowManager.LayoutParams.MATCH_PARENT);
        }
    }
    private void goBack() {
        if(wallet){event(0);return;}
        if(web!=null && !failed && sdkBack && !document.isEmpty())
            web.evaluateJavascript("window.__genyMiniAppEvent?.("+JSONObject.quote(document)+",{event:'back_navigation_triggered'})",null);
        else if(web!=null && !failed && web.canGoBack()) web.goBack();
        else event(0);
    }
    private void loadPage(String url,String referrer) {
        if(!allowed(Uri.parse(url))){fail("genycaster.miniapp.navigation_blocked");return;}
        if(initializing){pendingUrl=url;pendingReferrer=referrer;return;}
        if(web==null){fail("genycaster.miniapp.unavailable");return;}
        failed=false;web.setVisibility(View.VISIBLE);
        if(referrer.isEmpty()) web.loadUrl(url);
        else web.loadUrl(url,Collections.singletonMap("Referer",referrer));
    }
    public static void load(long id,String url,String referrer) { with(id,s->s.loadPage(url,referrer)); }
    public static void status(long id,String text,boolean failure) { with(id,s->{
        if(s.status!=null){s.status.setText(text);s.status.setVisibility(View.VISIBLE);}
        if(failure) {
            s.failed=true;s.generation++;s.document="";s.queuedRpc.clear();
            s.pendingUrl="";s.pendingReferrer="";
            if(s.web!=null){s.web.stopLoading();s.web.setVisibility(View.INVISIBLE);}
        }
    }); }
    public static void ready(long id,boolean ignored) { with(id,s->{if(!s.failed&&s.status!=null)s.status.setVisibility(View.GONE);}); }
    public static void setBackVisible(long id,boolean value) { with(id,s->{s.sdkBack=value;if(s.back!=null)s.back.setEnabled(value||(s.web!=null&&s.web.canGoBack()));}); }
    public static void setVisible(long id,boolean visible) { with(id,s->{
        if(s.wantsVisible==visible && (!visible || (s.dialog!=null && s.dialog.isShowing())))return;
        s.wantsVisible=visible;
        if(visible){s.show();if(s.web!=null)s.web.onResume();}
        else {if(s.web!=null)s.web.onPause();if(s.dialog!=null)s.dialog.hide();}
    }); }
    public static void evaluate(long id,String script,String token) { with(id,s->{
        if(s.web!=null&&!s.failed&&!token.isEmpty()&&token.equals(s.document))s.web.evaluateJavascript(script,null);
    }); }
    public static void destroy(long id) { UI.post(()->{
        MiniAppBrowser s=sessions.remove(id);
        if(s==null)return;
        s.disposed=true;s.generation++;s.document="";s.queuedRpc.clear();
        if(s.web!=null){
            if(s.sdk)try{WebViewCompat.removeWebMessageListener(s.web,"genyMobileTransport");}catch(RuntimeException ignored){}
            s.web.stopLoading();
            if(s.web.getParent() instanceof android.view.ViewGroup)((android.view.ViewGroup)s.web.getParent()).removeView(s.web);
            s.web.destroy();s.web=null;
        }
        if(s.dialog!=null){s.dialog.dismiss();s.dialog=null;}
        if(s.profileAssigned)try{ProfileStore.getInstance().deleteProfile(s.profile);}catch(RuntimeException ignored){}
    }); }
}
