package local.ballclock.controller;
import android.app.Application;

/** Owns only an application-scoped Wi-Fi lease, never an Activity reference. */
public final class BallClockApp extends Application {
    ClockWifiSession clockWifi;
    @Override public void onCreate(){super.onCreate();clockWifi=new ClockWifiSession(this);}
}
