package local.ballclock.controller;

/** Lifecycle decisions independent of Android, including the system Wi-Fi picker. */
public final class WifiLeasePolicy {
    public static final int KEEP=0, RELEASE=1, WAIT_FOR_PICKER=2;
    public static int onBackground(boolean rotating,boolean finishing,boolean awaitingApproval,boolean userLeaving) {
        if(finishing)return RELEASE;
        if(awaitingApproval)return WAIT_FOR_PICKER;
        if(userLeaving)return RELEASE;
        // A stopped window is not necessarily an exit: color/system dialogs
        // and configuration UI can cover it while the user is still editing.
        return KEEP;
    }
    public static String permission(int api) {
        return api>=33?"android.permission.NEARBY_WIFI_DEVICES":"android.permission.ACCESS_FINE_LOCATION";
    }
    private WifiLeasePolicy() {}
}
