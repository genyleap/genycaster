package com.genyleap.genycaster;

import android.app.Activity;
import android.app.Dialog;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
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
    private Button back;
    private boolean disposed, failed, sdkBack, profileAssigned;
    private long generation, rateSince;
    private int rateCount;
    private String document = "", retiredDocument = "";

    private MiniAppBrowser(long id, String origin, int role) {
        this.id=id; this.origin=origin; this.sdk=role==0; this.wallet=role==2;
    }
    private void event(int type, String value, String token) {
        if (!disposed) nativeEvent(id,type,value,token);
    }
    private void event(int type) { event(type,"",""); }
    private void fail(String code) {
        if (disposed || failed) return;
        failed=true; generation++; document="";
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
        if (!WebViewFeature.isFeatureSupported(WebViewFeature.MULTI_PROFILE) ||
            (sdk && (!WebViewFeature.isFeatureSupported(WebViewFeature.WEB_MESSAGE_LISTENER)
                    || !WebViewFeature.isFeatureSupported(WebViewFeature.DOCUMENT_START_SCRIPT))))
            throw new IllegalStateException("Required isolated WebView features unavailable");
        dialog=new Dialog(activity, android.R.style.Theme_DeviceDefault_NoActionBar) {
            @Override public void onBackPressed() { MiniAppBrowser.this.goBack(); }
        };
        dialog.requestWindowFeature(Window.FEATURE_NO_TITLE);
        dialog.setCancelable(false);
        LinearLayout outer=new LinearLayout(activity);
        outer.setOrientation(LinearLayout.VERTICAL);
        LinearLayout bar=new LinearLayout(activity);
        bar.setGravity(android.view.Gravity.CENTER_VERTICAL);
        JSONArray labels;
        try { labels=new JSONArray(controls); } catch(Exception ignored) { labels=new JSONArray(); }
        back=new Button(activity); back.setText("‹"); back.setContentDescription(labels.optString(0,"Back"));
        back.setOnClickListener(v->goBack()); bar.addView(back);
        TextView heading=new TextView(activity); heading.setText(title); heading.setSingleLine(true);
        heading.setEllipsize(android.text.TextUtils.TruncateAt.END);
        bar.addView(heading,new LinearLayout.LayoutParams(0,-2,1));
        Button retry=new Button(activity); retry.setText("↻"); retry.setContentDescription(labels.optString(1,"Refresh"));
        retry.setOnClickListener(v->event(1)); if(!wallet) bar.addView(retry);
        Button minimize=new Button(activity); minimize.setText("−"); minimize.setContentDescription(labels.optString(2,"Minimize"));
        minimize.setOnClickListener(v->event(2)); if(!wallet) bar.addView(minimize);
        Button close=new Button(activity); close.setText("×"); close.setContentDescription(labels.optString(3,"Close"));
        close.setOnClickListener(v->event(0)); bar.addView(close);
        outer.addView(bar,new LinearLayout.LayoutParams(-1,-2));
        status=new TextView(activity); status.setPadding(16,8,16,8); outer.addView(status);
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
                if(!request.isForMainFrame()) return wallet || !"https".equalsIgnoreCase(request.getUrl().getScheme());
                String target=request.getUrl().toString();
                if(wallet && request.hasGesture() && target.length()<=4096
                    && target.matches("wc:[0-9a-fA-F-]+@2\\?.+")) {
                    try { activity.startActivity(new Intent(Intent.ACTION_VIEW,request.getUrl())); }
                    catch (RuntimeException ignored) { /* QR remains available. */ }
                    return true;
                }
                if(!allowed(request.getUrl())) { fail("genycaster.miniapp.navigation_blocked"); return true; }
                return false;
            }
            @Override public void onPageStarted(WebView v,String url,android.graphics.Bitmap icon) {
                generation++; if(!document.isEmpty()) retiredDocument=document; document=""; sdkBack=false;
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
                generation++;document="";failed=true;
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
                        long epoch=generation;
                        // Prove the sender's token still belongs to the live
                        // top-level document after an asynchronous navigation.
                        web.evaluateJavascript("window.__genyMobileDocument || null", result->{
                            if(!disposed && !failed && epoch==generation && JSONObject.quote(token).equals(result)){
                                if(!document.isEmpty() && !document.equals(token)) return;
                                document=token;event(6,payload,token);
                            }
                        });
                    } catch (Exception ignored) { }
                });
            WebViewCompat.addDocumentStartJavaScript(web,bridge,Collections.singleton(origin));
        }
        outer.addView(web,new LinearLayout.LayoutParams(-1,0,1));
        dialog.setContentView(outer);
        if(visible) show();
    }
    private void show() {
        if(dialog==null || disposed) return;
        dialog.show();
        Window window=dialog.getWindow();
        if(window!=null) window.setLayout(WindowManager.LayoutParams.MATCH_PARENT,WindowManager.LayoutParams.MATCH_PARENT);
    }
    private void goBack() {
        if(wallet){event(0);return;}
        if(web!=null && !failed && sdkBack && !document.isEmpty())
            web.evaluateJavascript("window.__genyMiniAppEvent?.("+JSONObject.quote(document)+",{event:'back_navigation_triggered'})",null);
        else if(web!=null && !failed && web.canGoBack()) web.goBack();
        else event(0);
    }
    public static void load(long id,String url,String referrer) { with(id,s->{
        if(s.web==null || !s.allowed(Uri.parse(url))){s.fail("genycaster.miniapp.navigation_blocked");return;}
        s.failed=false;s.web.setVisibility(View.VISIBLE);
        if(referrer.isEmpty()) s.web.loadUrl(url);
        else s.web.loadUrl(url,Collections.singletonMap("Referer",referrer));
    }); }
    public static void status(long id,String text,boolean failure) { with(id,s->{
        if(s.status!=null){s.status.setText(text);s.status.setVisibility(View.VISIBLE);}
        if(failure && s.web!=null) {s.failed=true;s.web.setVisibility(View.INVISIBLE);}
    }); }
    public static void ready(long id,boolean ignored) { with(id,s->{if(!s.failed&&s.status!=null)s.status.setVisibility(View.GONE);}); }
    public static void setBackVisible(long id,boolean value) { with(id,s->{s.sdkBack=value;if(s.back!=null)s.back.setEnabled(value||(s.web!=null&&s.web.canGoBack()));}); }
    public static void setVisible(long id,boolean visible) { with(id,s->{if(visible)s.show();else if(s.dialog!=null)s.dialog.hide();}); }
    public static void evaluate(long id,String script,String token) { with(id,s->{
        if(s.web!=null&&!s.failed&&!token.isEmpty()&&token.equals(s.document))s.web.evaluateJavascript(script,null);
    }); }
    public static void destroy(long id) { UI.post(()->{
        MiniAppBrowser s=sessions.remove(id);
        if(s==null)return;
        s.disposed=true;s.generation++;s.document="";
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
