package local.ballclock.controller;

import android.app.Activity;
import android.app.AlertDialog;
import android.Manifest;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.res.Configuration;
import android.content.pm.PackageManager;
import android.location.LocationManager;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.net.ConnectivityManager;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.net.NetworkRequest;
import android.net.Uri;
import android.net.wifi.WifiManager;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.Gravity;
import android.provider.Settings;
import android.view.View;
import android.view.WindowInsets;
import android.webkit.CookieManager;
import android.webkit.WebResourceError;
import android.webkit.WebResourceRequest;
import android.webkit.WebResourceResponse;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;
import java.io.ByteArrayInputStream;
import java.nio.charset.StandardCharsets;
import org.json.JSONObject;
import org.json.JSONTokener;

/** Adaptive local controller: native LED preview and bundled device-API settings. */
public final class MainActivity extends Activity implements ClockWifiSession.Listener {
    private static final String BASE = "http://192.168.4.1";
    private final Handler ui = new Handler(Looper.getMainLooper());
    private ConnectivityManager connectivity;
    private ClockWifiSession clockWifi;
    private boolean permissionPromptShowing,permissionRequestInFlight,wifiPanelOffered,closing,resumed,wifiReceiverRegistered,userLeaving;
    private final BroadcastReceiver wifiStateReceiver=new BroadcastReceiver(){
        @Override public void onReceive(Context context,Intent intent){
            if(intent.getIntExtra(WifiManager.EXTRA_WIFI_STATE,-1)==WifiManager.WIFI_STATE_ENABLED)ensureAutoConnection(false);
        }
    };
    private volatile Network wifi;
    private volatile boolean visible;
    private boolean registered, pageLoaded;
    private int previewGeneration, lastFrameSeq=-1;
    private WebView web;
    private LedClockView dial;
    private TextView status;
    private TextView previewLabel;
    private Button save;
    private LinearLayout rootLayout,bodyLayout,previewPane,buttonBar;
    private TextView titleView;
    private boolean drawingWorkspace;
    private int insetLeft,insetTop,insetRight,insetBottom;
    // Read-only UI state, independent of Wi-Fi/preview availability. No JS bridge.
    private final Runnable layoutLoop=new Runnable(){
        @Override public void run(){
            if(!visible)return;
            web.evaluateJavascript("typeof BallClockLayout==='object'&&BallClockLayout.isDrawing()",value->{
                if(!visible)return;
                boolean drawing="true".equals(value);
                if(drawing!=drawingWorkspace){drawingWorkspace=drawing;updateWindowLayout();}
                ui.postDelayed(this,150);
            });
        }
    };
    private final Runnable previewLoop=new Runnable() {
        @Override public void run() {
            if(!visible) return;
            if(!pageLoaded || wifi==null || clockWifi.isRequesting() || permissionPromptShowing || permissionRequestInFlight || closing) { ui.postDelayed(this,200); return; }
            final int generation=previewGeneration;
            web.evaluateJavascript("typeof BallClockPreview==='object'?BallClockPreview.tick("+lastFrameSeq+"):null",value->{
                if(!visible || generation!=previewGeneration) return;
                if(clockWifi.isRequesting()||closing){ui.postDelayed(this,200);return;}
                try { handlePreview(value); }
                catch(Exception error) { setStatus("Панель доступна · проверяем загрузку циферблата"); }
                ui.postDelayed(this,50);
            });
        }
    };

    private final ConnectivityManager.NetworkCallback networkCallback = new ConnectivityManager.NetworkCallback() {
        @Override public void onAvailable(Network network) { selectWifi(); }
        @Override public void onLost(Network network) { selectWifi(); }
        @Override public void onCapabilitiesChanged(Network network, NetworkCapabilities capabilities) { selectWifi(); }
    };

    @Override public void onCreate(Bundle saved) {
        super.onCreate(saved);
        connectivity = (ConnectivityManager) getSystemService(CONNECTIVITY_SERVICE);
        clockWifi=((BallClockApp)getApplication()).clockWifi;
        LinearLayout root = new LinearLayout(this);
        rootLayout=root;
        root.setOrientation(LinearLayout.VERTICAL); root.setBackgroundColor(Color.rgb(11, 16, 24));
        root.setPadding(dp(16), dp(12), dp(16), 0);
        if (Build.VERSION.SDK_INT >= 30) {
            root.setOnApplyWindowInsetsListener((view, insets) -> {
                android.graphics.Insets bars = insets.getInsets(WindowInsets.Type.systemBars() | WindowInsets.Type.displayCutout());
                insetLeft=bars.left;insetTop=bars.top;insetRight=bars.right;insetBottom=bars.bottom;updateRootPadding();
                return insets;
            });
        } else root.setFitsSystemWindows(true);
        TextView title = new TextView(this); title.setText("BallClock");
        titleView=title;
        title.setTextSize(25); title.setTypeface(Typeface.DEFAULT,Typeface.BOLD); title.setLetterSpacing(-0.025f);
        title.setTextColor(Color.rgb(235,242,250)); root.addView(title);
        status = new TextView(this); status.setTextSize(12); status.setTextColor(Color.rgb(131,155,179));
        status.setMinHeight(dp(36)); status.setGravity(Gravity.CENTER_VERTICAL); status.setMaxLines(2);
        status.setText("Подключитесь к Wi-Fi BallClock. Интернет не требуется."); root.addView(status);
        status.setOnLongClickListener(view->{new AlertDialog.Builder(this).setTitle("Диагностика Wi-Fi")
            .setMessage(clockWifi.diagnostics()).setPositiveButton("Скопировать",(dialog,which)->{
                android.content.ClipboardManager clipboard=(android.content.ClipboardManager)getSystemService(CLIPBOARD_SERVICE);
                clipboard.setPrimaryClip(android.content.ClipData.newPlainText("BallClock Wi-Fi",clockWifi.diagnostics()));
            }).setNegativeButton("Закрыть",null).show();return true;});
        LinearLayout buttons = new LinearLayout(this); buttons.setPadding(0,dp(4),0,dp(12));
        buttonBar=buttons;
        Button networks = button("Wi-Fi", buttons); networks.setOnClickListener(view -> ensureAutoConnection(true));
        networks.setContentDescription("Подключиться к BallClock автоматически. Удерживайте для ручного выбора сети.");
        networks.setOnLongClickListener(view->{openWifi();return true;});
        Button reload = button("Обновить", buttons); reload.setOnClickListener(view -> { selectWifi(); reloadPage(); });
        save = button("Сохранить", buttons); save.setEnabled(false);
        save.setBackground(rounded(0xff91e0c2,22,false)); save.setTextColor(0xff11291f);
        save.setOnClickListener(view -> saveInPage()); root.addView(buttons);
        final int widthDp=getResources().getConfiguration().screenWidthDp;
        final int heightDp=getResources().getConfiguration().screenHeightDp;
        final boolean wide=AdaptiveLayout.sideBySide(widthDp,heightDp);
        LinearLayout body=new LinearLayout(this); body.setOrientation(wide?LinearLayout.HORIZONTAL:LinearLayout.VERTICAL);
        bodyLayout=body;
        LinearLayout previewColumn=new LinearLayout(this); previewColumn.setOrientation(LinearLayout.VERTICAL);
        previewPane=previewColumn;
        LinearLayout preview = new LinearLayout(this); preview.setOrientation(LinearLayout.VERTICAL);
        preview.setBackground(rounded(0xff0f1620,24,true)); preview.setClipToOutline(true); preview.setPadding(dp(12),dp(12),dp(12),dp(10));
        previewLabel=new TextView(this); previewLabel.setText("ЦИФЕРБЛАТ  /  ОЖИДАНИЕ"); previewLabel.setTextSize(10);
        previewLabel.setLetterSpacing(0.16f); previewLabel.setTextColor(0xff91e0c2); preview.addView(previewLabel);
        dial = new LedClockView(this);
        preview.addView(dial, new LinearLayout.LayoutParams(-1,dp(AdaptiveLayout.dialHeight(widthDp,heightDp))));
        TextView hint = new TextView(this); hint.setTextSize(10); hint.setTextColor(0xff718499);
        hint.setText("128 LED   ·   Изображение с часов"); preview.addView(hint); previewColumn.addView(preview);
        if(wide && heightDp>=600) {
            TextView details=new TextView(this); details.setTextSize(13); details.setTextColor(0xff91a5ba); details.setLineSpacing(dp(4),1);
            details.setPadding(dp(12),dp(24),dp(12),dp(12));
            details.setText("ВАШИ ЧАСЫ\n\nЦиферблат остаётся перед глазами, пока вы меняете настройки справа.\n\nНажмите кнопку параметра, чтобы включить его. Подсветка означает «включено».\n\nПосле сохранения часы продолжат работу без планшета.");
            previewColumn.addView(details);
        }
        LinearLayout.LayoutParams previewParams=wide?new LinearLayout.LayoutParams(0,-1,0.38f):new LinearLayout.LayoutParams(-1,-2);
        if(wide) previewParams.setMarginEnd(dp(20));
        body.addView(previewColumn,previewParams);
        web = new WebView(this);
        WebSettings settings = web.getSettings();
        settings.setJavaScriptEnabled(true); // Required by the clock's own settings page.
        settings.setAllowFileAccess(false); settings.setAllowContentAccess(false);
        settings.setDomStorageEnabled(false); settings.setGeolocationEnabled(false);
        settings.setMixedContentMode(WebSettings.MIXED_CONTENT_NEVER_ALLOW);
        settings.setCacheMode(WebSettings.LOAD_NO_CACHE);
        settings.setSaveFormData(false);
        WebView.setWebContentsDebuggingEnabled(false);
        CookieManager.getInstance().setAcceptCookie(false);
        web.setBackgroundColor(Color.rgb(11,16,24)); web.setOverScrollMode(View.OVER_SCROLL_NEVER);
        web.setWebViewClient(new WebViewClient() {
            @Override public boolean shouldOverrideUrlLoading(WebView view, WebResourceRequest request) {
                return !trusted(request.getUrl());
            }
            @Override public boolean shouldOverrideUrlLoading(WebView view, String url) {
                return !trusted(Uri.parse(url));
            }
            @Override public WebResourceResponse shouldInterceptRequest(WebView view, WebResourceRequest request) {
                if(trusted(request.getUrl()) && "/".equals(request.getUrl().getPath())) {
                    try { return new WebResourceResponse("text/html","UTF-8",getAssets().open("mobile.html")); }
                    catch(java.io.IOException error) { return new WebResourceResponse("text/plain","UTF-8",new ByteArrayInputStream("Interface unavailable".getBytes(StandardCharsets.UTF_8))); }
                }
                return trusted(request.getUrl()) ? null : new WebResourceResponse("text/plain", "UTF-8",
                        403, "Local device only", null, new ByteArrayInputStream(new byte[0]));
            }
            @Override public void onPageFinished(WebView view, String url) {
                if (trusted(Uri.parse(url))) {
                    view.evaluateJavascript("Boolean(document.getElementById('saveSettings'))", value -> {
                        pageLoaded = "true".equals(value); save.setEnabled(pageLoaded);
                        updateWindowLayout();
                    });
                }
            }
            @Override public void onReceivedError(WebView view, WebResourceRequest request, WebResourceError error) {
                if (request.isForMainFrame()) {
                    pageLoaded = false; save.setEnabled(false);
                    status.setText("Нет связи. Подключитесь к BallClock и нажмите «Обновить».");
                }
            }
        });
        body.addView(web,wide?new LinearLayout.LayoutParams(0,-1,0.62f):new LinearLayout.LayoutParams(-1,0,1));
        root.addView(body,new LinearLayout.LayoutParams(-1,0,1));
        setContentView(root);
    }

    private void updateRootPadding(){
        if(rootLayout==null)return;
        int horizontal=dp(drawingWorkspace?4:16),top=dp(drawingWorkspace?2:12);
        rootLayout.setPadding(insetLeft+horizontal,insetTop+top,insetRight+horizontal,insetBottom);
    }
    private void updateWindowLayout(){
        if(web==null||bodyLayout==null)return;
        int width=getResources().getConfiguration().screenWidthDp,height=getResources().getConfiguration().screenHeightDp;
        boolean wide=!drawingWorkspace&&AdaptiveLayout.sideBySide(width,height);
        int chrome=drawingWorkspace?View.GONE:View.VISIBLE;
        titleView.setVisibility(chrome);status.setVisibility(chrome);buttonBar.setVisibility(chrome);previewPane.setVisibility(chrome);
        bodyLayout.setOrientation(wide?LinearLayout.HORIZONTAL:LinearLayout.VERTICAL);
        LinearLayout.LayoutParams preview=wide?new LinearLayout.LayoutParams(0,-1,.38f):new LinearLayout.LayoutParams(-1,-2);
        if(wide)preview.setMarginEnd(dp(20));previewPane.setLayoutParams(preview);
        web.setLayoutParams(wide?new LinearLayout.LayoutParams(0,-1,.62f):new LinearLayout.LayoutParams(-1,0,1));
        LinearLayout.LayoutParams dialParams=(LinearLayout.LayoutParams)dial.getLayoutParams();dialParams.height=dp(AdaptiveLayout.dialHeight(width,height));dial.setLayoutParams(dialParams);
        updateRootPadding();
        boolean landscape=getResources().getConfiguration().orientation==Configuration.ORIENTATION_LANDSCAPE;
        web.evaluateJavascript("if(typeof BallClockLayout==='object')BallClockLayout.setOrientation("+landscape+")",null);
    }
    @Override public void onConfigurationChanged(Configuration configuration){
        super.onConfigurationChanged(configuration);
        // Reflow the existing WebView: preserve unsaved pixels, tools, undo and
        // the network request. Rotation must not recreate/reload the editor.
        updateWindowLayout();
    }

    private boolean trusted(Uri uri) {
        return "http".equals(uri.getScheme()) && "192.168.4.1".equals(uri.getHost())
                && (uri.getPort() == -1 || uri.getPort() == 80) && uri.getUserInfo() == null;
    }
    private int dp(int value) { return Math.round(value * getResources().getDisplayMetrics().density); }
    private GradientDrawable rounded(int color,int radius,boolean border) {
        GradientDrawable background=new GradientDrawable(); background.setColor(color); background.setCornerRadius(dp(radius));
        if(border) background.setStroke(dp(1),0xff223143); return background;
    }
    private void setStatus(String text) { if(!text.contentEquals(status.getText())) status.setText(text); }
    private void setPreviewLabel(String text) { if(!text.contentEquals(previewLabel.getText())) previewLabel.setText(text); }
    private Button button(String label, LinearLayout parent) {
        Button button = new Button(this); button.setText(label); button.setTextSize(13);
        button.setAllCaps(false); button.setTextColor(0xffc8d5e4); button.setTypeface(Typeface.DEFAULT,Typeface.BOLD);
        button.setBackground(rounded(0xff172230,22,false)); button.setMinHeight(0); button.setMinimumHeight(0);
        LinearLayout.LayoutParams params=new LinearLayout.LayoutParams(0,dp(44),1); params.setMarginEnd(dp(6));
        parent.addView(button,params); return button;
    }
    private void openWifi() {
        try {
            openSystemActivity(new Intent(Build.VERSION.SDK_INT >= 29 ? Settings.Panel.ACTION_WIFI : Settings.ACTION_WIFI_SETTINGS));
        } catch (RuntimeException error) { openSystemActivity(new Intent(Settings.ACTION_WIFI_SETTINGS)); }
    }
    private void openSystemActivity(Intent intent){userLeaving=false;intent.addFlags(Intent.FLAG_ACTIVITY_NO_USER_ACTION);startActivity(intent);}
    private void ensureAutoConnection(boolean force) {
        if(!visible || !resumed || permissionPromptShowing || permissionRequestInFlight)return;
        String permission=WifiLeasePolicy.permission(Build.VERSION.SDK_INT);
        if(checkSelfPermission(permission)!=PackageManager.PERMISSION_GRANTED){
            boolean asked=getPreferences(MODE_PRIVATE).getBoolean("wifiPermissionAsked",false);
            if(force||!asked){
                permissionPromptShowing=true;
                new AlertDialog.Builder(this).setTitle("Автоподключение к часам")
                    .setMessage(Build.VERSION.SDK_INT>=33?"Разрешите доступ к ближайшим Wi-Fi-устройствам, чтобы подключаться к BallClock при открытии приложения.":"Эта версия Android требует разрешение на точное местоположение для управления Wi-Fi. Приложение не получает GPS-координаты и не отслеживает вас.")
                    .setPositiveButton("Разрешить",(dialog,which)->{
                        permissionPromptShowing=false;permissionRequestInFlight=true;
                        getPreferences(MODE_PRIVATE).edit().putBoolean("wifiPermissionAsked",true).apply();
                        requestPermissions(Build.VERSION.SDK_INT>=33?new String[]{permission}:new String[]{Manifest.permission.ACCESS_COARSE_LOCATION,Manifest.permission.ACCESS_FINE_LOCATION},91);
                    }).setNegativeButton("Вручную",(dialog,which)->{
                        getPreferences(MODE_PRIVATE).edit().putBoolean("wifiPermissionAsked",true).apply();openWifi();
                    }).setOnDismissListener(dialog->permissionPromptShowing=false).show();
            }else setStatus("Автоподключению нужно разрешение · нажмите Wi-Fi");
            return;
        }
        if(Build.VERSION.SDK_INT>=29&&!clockWifi.wifiEnabled()){
            setStatus("Включите Wi-Fi: Android не разрешает приложению включить его скрыто.");
            if(force||!wifiPanelOffered){wifiPanelOffered=true;openWifi();}return;
        }
        if(Build.VERSION.SDK_INT>=29&&Build.VERSION.SDK_INT<=32){
            LocationManager location=(LocationManager)getSystemService(LOCATION_SERVICE);
            if(location!=null&&!location.isLocationEnabled()){
                setStatus("Для автоподключения Android 10–12 требует включённую геолокацию.");
                if(force)new AlertDialog.Builder(this).setMessage("Включите системную геолокацию для поиска Wi-Fi. GPS-координаты приложением не запрашиваются.")
                    .setPositiveButton("Настройки",(dialog,which)->openSystemActivity(new Intent(Settings.ACTION_LOCATION_SOURCE_SETTINGS)))
                    .setNegativeButton("Вручную",(dialog,which)->openWifi()).show();
                return;
            }
        }
        clockWifi.connect(force);
    }
    @Override public void onRequestPermissionsResult(int code,String[] permissions,int[] results){
        super.onRequestPermissionsResult(code,permissions,results);
        if(code==91){permissionRequestInFlight=false;
            if(!visible)return;
            if(checkSelfPermission(WifiLeasePolicy.permission(Build.VERSION.SDK_INT))==PackageManager.PERMISSION_GRANTED)ensureAutoConnection(false);
            else new AlertDialog.Builder(this).setMessage("Автоподключение не разрешено. Можно подключиться вручную или разрешить доступ в настройках приложения.")
                .setPositiveButton("Настройки приложения",(dialog,which)->openSystemActivity(new Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS,Uri.parse("package:"+getPackageName()))))
                .setNegativeButton("Вручную",(dialog,which)->openWifi()).show();
        }
    }
    @Override public void onWifiSessionChanged(){
        if(!visible)return;
        if(clockWifi.isRequesting()){
            ++previewGeneration;ui.removeCallbacks(previewLoop);ui.post(previewLoop);
            setStatus(clockWifi.getMessage());
        }else{
            selectWifi();if(!clockWifi.getMessage().isEmpty())setStatus(clockWifi.getMessage());
        }
    }
    private void saveInPage() {
        if (pageLoaded) web.evaluateJavascript("if(document.activeElement)document.activeElement.blur();document.getElementById('saveSettings').click();document.getElementById('saveStatus').scrollIntoView()", null);
    }
    private void reloadPage() {
        pageLoaded = false; save.setEnabled(false);
        ++previewGeneration; ui.removeCallbacks(previewLoop);
        lastFrameSeq=-1;
        if (wifi == null) return;
        web.loadUrl(BASE + "/");
        if(visible)ui.post(previewLoop);
    }
    private void selectWifi() {
        if (!visible) return;
        Network found=clockWifi.getNetwork();
        if(found==null&&clockWifi.isRequesting()){setStatus(clockWifi.getMessage());return;}
        if(found==null)for (Network network : connectivity.getAllNetworks()) {
            NetworkCapabilities caps = connectivity.getNetworkCapabilities(network);
            if (caps != null && caps.hasTransport(NetworkCapabilities.TRANSPORT_WIFI)
                    && !caps.hasTransport(NetworkCapabilities.TRANSPORT_VPN)) {
                if (network.equals(wifi)) { found = network; break; }
                found = network;
            }
        }
        if (found != null && found.equals(wifi)) return;
        wifi = found;
        connectivity.bindProcessToNetwork(found); // Prefer local Wi-Fi even with working mobile data.
        dial.setFrame(null);
        if (found == null) {
            ++previewGeneration; ui.removeCallbacks(previewLoop); if(visible)ui.post(previewLoop);
            pageLoaded = false; save.setEnabled(false);
            status.setText("Нет Wi-Fi. Подключитесь к BallClock; «без интернета» — нормально.");
        } else reloadPage();
    }

    private void handlePreview(String value) throws Exception {
        if(value==null || value.length()>8192 || "null".equals(value)) return;
        Object decoded=new JSONTokener(value).nextValue();
        JSONObject result=decoded instanceof JSONObject?(JSONObject)decoded:new JSONObject((String)decoded);
        int sequence=result.optInt("seq",-1);
        if(!result.isNull("rgb") && sequence!=lastFrameSeq) {
            dial.setFrame(ClockFrame.decode(result.getString("rgb"))); lastFrameSeq=sequence;
        }
        String kind=result.optString("kind"),error=result.optString("error");
        if("live".equals(kind)) {
            setStatus("●  На связи   ·   BallClock / 192.168.4.1"); setPreviewLabel("LIVE  /  ЦИФЕРБЛАТ");
        } else if("preview_error".equals(kind)) {
            String detail="HTTP 404".equals(error)?"нужно обновление прошивки":
                "timeout".equals(error)?"задержка ответа":"invalid_frame".equals(error)?"неверный формат кадра":
                error.startsWith("HTTP ")?error:"повторяем загрузку";
            setStatus("Связь есть · циферблат: "+detail);
            setPreviewLabel(lastFrameSeq>0?"ПОСЛЕДНИЙ КАДР / ОБНОВЛЕНИЕ":"ЦИФЕРБЛАТ / ЗАГРУЗКА");
        } else if("offline".equals(kind)) {
            setStatus("Часы не отвечают · выберите Wi-Fi BallClock");
            setPreviewLabel("ЦИФЕРБЛАТ / НЕТ ОБНОВЛЕНИЙ");
        } else setStatus("Подключаемся к часам…");
    }

    @Override protected void onStart() {
        super.onStart(); visible = true; web.onResume();
        userLeaving=false;
        clockWifi.attach(this);
        NetworkRequest request = new NetworkRequest.Builder().addTransportType(NetworkCapabilities.TRANSPORT_WIFI).build();
        try { connectivity.registerNetworkCallback(request, networkCallback, ui); registered = true; }
        catch (RuntimeException error) { status.setText("Выберите Wi-Fi BallClock и нажмите «Обновить»."); }
        if(Build.VERSION.SDK_INT>=29){
            IntentFilter events=new IntentFilter(WifiManager.WIFI_STATE_CHANGED_ACTION);
            if(Build.VERSION.SDK_INT>=33)registerReceiver(wifiStateReceiver,events,Context.RECEIVER_NOT_EXPORTED);
            else registerReceiver(wifiStateReceiver,events);
            wifiReceiverRegistered=true;
        }
        ensureAutoConnection(false);selectWifi();
        if (wifi != null && !pageLoaded) reloadPage();
        ++previewGeneration;ui.removeCallbacks(previewLoop);ui.post(previewLoop);
        ui.removeCallbacks(layoutLoop);ui.post(layoutLoop);
    }
    @Override protected void onResume(){super.onResume();resumed=true;if(clockWifi!=null)ensureAutoConnection(false);}
    @Override protected void onPause(){resumed=false;super.onPause();}
    @Override protected void onStop() {
        visible = false;
        ++previewGeneration;
        ui.removeCallbacks(previewLoop);
        ui.removeCallbacks(layoutLoop);
        if(pageLoaded&&!clockWifi.isRequesting()&&!isFinishing())saveInPage(); // best effort for Home; explicit close waits for confirmation
        if(pageLoaded)web.evaluateJavascript("if(typeof BallClockPreview==='object')BallClockPreview.stop()",null);
        if (registered) { connectivity.unregisterNetworkCallback(networkCallback); registered = false; }
        if(wifiReceiverRegistered){unregisterReceiver(wifiStateReceiver);wifiReceiverRegistered=false;}
        closing=false;clockWifi.background(isChangingConfigurations(),isFinishing(),userLeaving);
        web.onPause(); super.onStop();
    }
    @Override protected void onUserLeaveHint(){super.onUserLeaveHint();userLeaving=true;}
    @Override protected void onDestroy() {
        ui.removeCallbacks(previewLoop);ui.removeCallbacks(layoutLoop); web.stopLoading(); web.destroy();
        if(isFinishing())clockWifi.release();super.onDestroy();
    }
    @Override public void onBackPressed() {
        if(closing)return;
        new AlertDialog.Builder(this).setTitle("Закрыть BallClock?")
                .setMessage("Часы продолжат ход с последними сохранёнными настройками. Дождитесь подтверждения сохранения в панели.")
                .setPositiveButton("Сохранить и выйти", (dialog, which) -> closeAfterSave())
                .setNegativeButton("Остаться", null).show();
    }
    private void closeAfterSave(){
        if(!pageLoaded||wifi==null){finish();return;}
        closing=true;setStatus("Сохраняем настройки перед возвратом к прежней сети…");
        web.evaluateJavascript("if(document.activeElement)document.activeElement.blur();window.__ballExit={done:false,ok:false};saveAll().then(ok=>{window.__ballExit={done:true,ok:!!ok}}).catch(()=>{window.__ballExit={done:true,ok:false}})",ignored->pollClose(android.os.SystemClock.elapsedRealtime()+4000));
    }
    private void pollClose(long deadline){
        if(!visible||!closing||isFinishing())return;
        web.evaluateJavascript("window.__ballExit||null",value->{
            if(!visible||!closing||isFinishing())return;
            boolean done=false,ok=false;
            try{Object object=new JSONTokener(value).nextValue();JSONObject result=object instanceof JSONObject?(JSONObject)object:new JSONObject((String)object);done=result.optBoolean("done");ok=result.optBoolean("ok");}catch(Exception ignored){}
            if(done&&ok){finish();return;}
            if(!done&&android.os.SystemClock.elapsedRealtime()<deadline){ui.postDelayed(()->pollClose(deadline),100);return;}
            closing=false;new AlertDialog.Builder(this).setMessage("Сохранение не подтверждено. Оставить приложение открытым для повтора?")
                .setPositiveButton("Остаться",null).setNegativeButton("Выйти без сохранения",(dialog,which)->finish()).show();
        });
    }
}
