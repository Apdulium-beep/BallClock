package local.ballclock.controller;

/** Firmware geometry and strict displayed-RGB decoder; no Android dependencies. */
public final class ClockFrame {
    public static final int COUNT = 128, ROWS = 7, WIDTH = 20;
    public static final int[] X = new int[COUNT], ROW = new int[COUNT];
    static {
        int[] lengths = {17, 18, 19, 20, 19, 18, 17};
        int index = 0;
        for (int group = 0; group < WIDTH; group++) {
            for (int step = 0; step < ROWS; step++) {
                int row = (group & 1) != 0 ? step : ROWS - 1 - step;
                int first = WIDTH - lengths[row];
                for (int delta = -1; delta <= 0; delta++) {
                    int x = 2 * group + delta;
                    if (x >= first && x <= first + 2 * (lengths[row] - 1)
                            && x < 2 * WIDTH - 1 && ((x - first) & 1) == 0) {
                        if (index >= COUNT) throw new IllegalStateException("Invalid LED geometry");
                        X[index] = x; ROW[index++] = row;
                    }
                }
            }
        }
        if (index != COUNT) throw new IllegalStateException("Invalid LED count");
    }

    public static int[] decode(String hex) {
        if (hex == null || hex.length() != COUNT * 6) throw new IllegalArgumentException("Invalid frame length");
        int[] colors = new int[COUNT];
        for (int led = 0; led < COUNT; led++) {
            int rgb = 0;
            for (int n = 0; n < 6; n++) {
                char c = hex.charAt(led * 6 + n);
                int digit = c >= '0' && c <= '9' ? c - '0'
                        : c >= 'a' && c <= 'f' ? c - 'a' + 10
                        : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
                if (digit < 0) throw new IllegalArgumentException("Invalid RGB hex");
                rgb = (rgb << 4) | digit;
            }
            colors[led] = 0xff000000 | rgb;
        }
        return colors;
    }
    private ClockFrame() {}
}
