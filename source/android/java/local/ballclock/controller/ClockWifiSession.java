package local.ballclock.controller;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.ConnectivityManager;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.net.NetworkRequest;
import android.net.wifi.WifiConfiguration;
import android.net.wifi.WifiInfo;
import android.net.wifi.WifiManager;
import android.net.wifi.WifiNetworkSpecifier;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import java.util.List;
import java.util.ArrayDeque;

/** Exact-SSID, foreground-scoped connection. No passwords or previous SSIDs stored. */
final class ClockWifiSession {
    interface Listener { void onWifiSessionChanged(); }
    private final Context app;
    private final ConnectivityManager connectivity;
    private final WifiManager manager;
    private final Handler handler=new Handler(Looper.getMainLooper());
    private Listener listener;
    private ConnectivityManager.NetworkCallback callback;
    private BroadcastReceiver receiver;
    private Network network;
    private boolean foreground,requesting,attempted,active,awaitingApproval;
    private boolean legacy,legacySwitched,temporaryProfile,enabledRadio;
    private int generation,previousId=-1,clockId=-1;
    private String message="";
    private final ArrayDeque<String> events=new ArrayDeque<>();
    private void event(String text){if(events.size()>=24)events.removeFirst();events.addLast(android.os.SystemClock.uptimeMillis()+" "+text);}
    String diagnostics(){StringBuilder result=new StringBuilder();for(String entry:events)result.append(entry).append('\n');return result.toString();}
    private final Runnable releaseWhenAway=()->{if(!foreground)release();};
    private final Runnable legacyTimeout=()->{if(requesting&&legacy)fail("Не удалось подключиться к BallClock. Нажмите Wi-Fi для повтора.");};

    ClockWifiSession(Context context) {
        app=context.getApplicationContext();
        connectivity=(ConnectivityManager)app.getSystemService(Context.CONNECTIVITY_SERVICE);
        manager=(WifiManager)app.getSystemService(Context.WIFI_SERVICE);
    }
    void attach(Listener observer){listener=observer;foreground=true;handler.removeCallbacks(releaseWhenAway);}
    void background(boolean rotating,boolean finishing,boolean userLeaving) {
        listener=null;
        int action=WifiLeasePolicy.onBackground(rotating,finishing,awaitingApproval,userLeaving);
        event("background: rotate="+rotating+", exit="+finishing+", leave="+userLeaving+", picker="+awaitingApproval+", action="+action);
        if(action==WifiLeasePolicy.KEEP)return;
        foreground=false;
        if(finishing){release();return;}
        if(action==WifiLeasePolicy.RELEASE)handler.postDelayed(releaseWhenAway,1500);
        // Keep a pending request while Android's approval Activity covers ours.
        // The request itself times out; a late success is released unless we return.
    }
    Network getNetwork(){return network;}
    boolean isRequesting(){return requesting;}
    String getMessage(){return message;}
    boolean wifiEnabled(){return manager!=null&&manager.isWifiEnabled();}

    void connect(boolean force) {
        if(manager==null){setMessage("Wi-Fi недоступен на этом устройстве.");return;}
        if(active&&force&&network==null&&Build.VERSION.SDK_INT>=29)cleanup();
        if(active || (attempted&&!force))return;
        attempted=true;requesting=true;active=true;awaitingApproval=Build.VERSION.SDK_INT>=29;network=null;
        event("request BallClock");
        setMessage(Build.VERSION.SDK_INT>=29?"Подтвердите подключение к BallClock в системном окне Android.":"Подключаемся к BallClock…");
        final int token=++generation;
        try {
            if(Build.VERSION.SDK_INT>=29)startModern(token);else startLegacy(token);
        }catch(SecurityException error){fail("Нет разрешения на подключение Wi-Fi. Нажмите Wi-Fi.");}
        catch(RuntimeException error){fail("Автоподключение недоступно. Удерживайте Wi-Fi для ручного выбора сети.");}
    }
    private void startModern(final int token) {
        NetworkRequest request=new NetworkRequest.Builder().addTransportType(NetworkCapabilities.TRANSPORT_WIFI)
                .removeCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET)
                .setNetworkSpecifier(new WifiNetworkSpecifier.Builder().setSsid("BallClock").build()).build();
        callback=new ConnectivityManager.NetworkCallback(){
            @Override public void onAvailable(Network value){if(current(token))acquired(value);}
            @Override public void onLost(Network value){if(current(token)&&value.equals(network)){event("Android reported network lost; request retained");network=null;requesting=true;setMessage("Связь с BallClock потеряна. Android восстанавливает подключение…");}}
            @Override public void onUnavailable(){if(current(token))fail("Подключение не подтверждено или BallClock недоступна. Нажмите Wi-Fi для повтора.");}
        };
        connectivity.requestNetwork(request,callback,handler,30000);
    }
    private boolean current(int token){return active&&generation==token;}
    private void acquired(Network value) {
        network=value;requesting=false;awaitingApproval=false;handler.removeCallbacks(legacyTimeout);
        event("network available");
        connectivity.bindProcessToNetwork(value);
        setMessage("Подключено к BallClock. При выходе подключение будет освобождено.");
        if(!foreground)handler.postDelayed(releaseWhenAway,3000);
    }
    private void setMessage(String value){message=value;if(listener!=null)listener.onWifiSessionChanged();}

    @SuppressWarnings("deprecation")
    private void startLegacy(final int token) {
        legacy=true;
        WifiInfo before=manager.getConnectionInfo();previousId=before==null?-1:before.getNetworkId();
        receiver=new BroadcastReceiver(){@Override public void onReceive(Context context,Intent intent){
            if(!current(token))return;
            try{if(manager.isWifiEnabled()&&clockId<0)configureLegacy();findLegacyNetwork();}
            catch(SecurityException error){fail("Нет разрешения на подключение Wi-Fi.");}
        }};
        IntentFilter events=new IntentFilter(WifiManager.WIFI_STATE_CHANGED_ACTION);events.addAction(WifiManager.NETWORK_STATE_CHANGED_ACTION);
        app.registerReceiver(receiver,events);
        callback=new ConnectivityManager.NetworkCallback(){
            @Override public void onAvailable(Network value){if(current(token))findLegacyNetwork();}
            @Override public void onCapabilitiesChanged(Network value,NetworkCapabilities caps){if(current(token))findLegacyNetwork();}
            @Override public void onLost(Network value){if(current(token)&&value.equals(network)){network=null;requesting=true;setMessage("Ожидаем восстановления сети BallClock…");}}
        };
        connectivity.registerNetworkCallback(new NetworkRequest.Builder().addTransportType(NetworkCapabilities.TRANSPORT_WIFI).build(),callback,handler);
        if(manager.isWifiEnabled())configureLegacy();
        else {enabledRadio=true;if(!manager.setWifiEnabled(true))throw new IllegalStateException("Wi-Fi cannot be enabled");}
        if(active)handler.postDelayed(legacyTimeout,30000);
    }
    @SuppressWarnings("deprecation")
    private void configureLegacy() {
        if(clockId>=0)return;
        List<WifiConfiguration> profiles=manager.getConfiguredNetworks();
        if(profiles==null){fail("Android не предоставил список сетей. Используйте ручное подключение.");return;}
        if(profiles!=null)for(WifiConfiguration profile:profiles)if("\"BallClock\"".equals(profile.SSID)) {
            if(!profile.allowedKeyManagement.get(WifiConfiguration.KeyMgmt.NONE)){fail("Сохранённая сеть BallClock защищена паролем. Проверьте её вручную.");return;}
            clockId=profile.networkId;break;
        }
        if(clockId<0){WifiConfiguration profile=new WifiConfiguration();profile.SSID="\"BallClock\"";profile.allowedKeyManagement.set(WifiConfiguration.KeyMgmt.NONE);clockId=manager.addNetwork(profile);temporaryProfile=clockId>=0;}
        if(clockId<0){fail("Android не разрешил добавить BallClock. Удерживайте Wi-Fi для ручного выбора.");return;}
        legacySwitched=previousId!=clockId;
        if(legacySwitched){manager.disconnect();if(!manager.enableNetwork(clockId,true)){fail("Не удалось выбрать BallClock.");return;}manager.reconnect();}
        findLegacyNetwork();
    }
    @SuppressWarnings("deprecation")
    private void findLegacyNetwork() {
        if(!legacy||!active||clockId<0)return;
        WifiInfo info=manager.getConnectionInfo();if(info==null||info.getNetworkId()!=clockId)return;
        for(Network candidate:connectivity.getAllNetworks()){
            NetworkCapabilities caps=connectivity.getNetworkCapabilities(candidate);
            if(caps!=null&&caps.hasTransport(NetworkCapabilities.TRANSPORT_WIFI)&&!caps.hasTransport(NetworkCapabilities.TRANSPORT_VPN)){
                if(!candidate.equals(network))acquired(candidate);return;
            }
        }
    }
    private void fail(String reason){event("request failed/unavailable");cleanup();attempted=true;setMessage(reason);if(!foreground)handler.postDelayed(releaseWhenAway,1500);}
    void release(){event("release owned connection");cleanup();attempted=false;message="";}
    @SuppressWarnings("deprecation")
    private void cleanup() {
        ++generation;active=false;requesting=false;awaitingApproval=false;network=null;
        handler.removeCallbacks(releaseWhenAway);handler.removeCallbacks(legacyTimeout);
        connectivity.bindProcessToNetwork(null);
        if(callback!=null){try{connectivity.unregisterNetworkCallback(callback);}catch(IllegalArgumentException ignored){}callback=null;}
        if(receiver!=null){try{app.unregisterReceiver(receiver);}catch(IllegalArgumentException ignored){}receiver=null;}
        if(legacy&&manager!=null){
            try{
                WifiInfo current=manager.getConnectionInfo();int id=current==null?-1:current.getNetworkId();
                boolean stillOurs=id==clockId||id<0;
                if(temporaryProfile&&clockId>=0)manager.removeNetwork(clockId); // only a profile created by this session
                if(enabledRadio&&stillOurs)manager.setWifiEnabled(false);
                else if(legacySwitched&&stillOurs&&previousId>=0&&manager.isWifiEnabled()){
                    manager.enableNetwork(previousId,true);manager.reconnect();
                }
            }catch(RuntimeException ignored){/* Manual Wi-Fi selection remains available. */}
        }
        legacy=false;legacySwitched=false;temporaryProfile=false;enabledRadio=false;previousId=-1;clockId=-1;
    }
}
