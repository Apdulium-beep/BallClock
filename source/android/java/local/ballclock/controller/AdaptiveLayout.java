package local.ballclock.controller;

/** Window-width policy, reevaluated in place on configuration changes. */
public final class AdaptiveLayout {
    public static boolean sideBySide(int widthDp) { return widthDp >= 800; }
    public static boolean sideBySide(int widthDp,int heightDp) {
        return sideBySide(widthDp) || (widthDp>=600 && heightDp<500);
    }
    public static int dialHeight(int widthDp,int heightDp) {
        if(sideBySide(widthDp,heightDp))return Math.max(100,Math.min(300,Math.round(widthDp*0.38f/3.23f)));
        return dialHeight(widthDp);
    }
    public static int dialHeight(int widthDp) {
        if(sideBySide(widthDp)) return Math.max(120,Math.min(300,Math.round(widthDp*0.38f/3.23f)));
        return widthDp>=600?180:126;
    }
    private AdaptiveLayout() {}
}
