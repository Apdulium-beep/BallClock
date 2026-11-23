package local.ballclock.controller;

public final class ClockFrameTest {
    public static void main(String[] args) {
        if (args.length > 0 && args[0].equals("--geometry")) {
            for (int i = 0; i < ClockFrame.COUNT; i++) System.out.println(i + "," + ClockFrame.ROW[i] + "," + ClockFrame.X[i]);
            return;
        }
        StringBuilder rgb = new StringBuilder();
        for (int i = 0; i < ClockFrame.COUNT; i++) rgb.append("12aBef");
        int[] decoded = ClockFrame.decode(rgb.toString());
        for (int color : decoded) if (color != 0xff12abef) throw new AssertionError("RGB changed");
        for (String invalid : new String[] {"", rgb.substring(1), rgb.substring(0, 767) + "z"}) {
            try { ClockFrame.decode(invalid); throw new AssertionError("Invalid frame accepted"); }
            catch (IllegalArgumentException expected) { }
        }
        System.out.println("PASS: Android preview decodes exact RGB and rejects malformed frames.");
    }
}
