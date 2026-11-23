package local.ballclock.controller;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.os.SystemClock;
import android.view.View;

/** Same circle layout, colors and spacing as Qt LedMatrixWidget. */
final class LedClockView extends View {
    private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final FrameInterpolator animation = new FrameInterpolator();
    private final int[] colors = new int[ClockFrame.COUNT];

    LedClockView(Context context) { super(context); setContentDescription("Циферблат часов, 128 светодиодов"); }
    void setFrame(int[] frame) {
        if(frame == null) animation.clear(); else animation.accept(frame,SystemClock.uptimeMillis());
        postInvalidateOnAnimation();
    }

    @Override protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        canvas.drawColor(Color.rgb(15, 22, 32));
        boolean moving=animation.sample(SystemClock.uptimeMillis(),colors);
        double dy = Math.sqrt(3.0) / 2.0;
        float margin = 12 * getResources().getDisplayMetrics().density;
        double diameter = Math.max(0, Math.min((getWidth() - 2 * margin) / ClockFrame.WIDTH,
                (getHeight() - 2 * margin) / (1 + (ClockFrame.ROWS - 1) * dy)));
        double ox = (getWidth() - ClockFrame.WIDTH * diameter) / 2;
        double oy = (getHeight() - (1 + (ClockFrame.ROWS - 1) * dy) * diameter) / 2;
        for (int led = 0; led < ClockFrame.COUNT; led++) {
            float x = (float) (ox + (ClockFrame.X[led] * 0.5 + 0.5) * diameter);
            float y = (float) (oy + (ClockFrame.ROW[led] * dy + 0.5) * diameter);
            paint.setStyle(Paint.Style.FILL);
            paint.setColor(animation.isReady() ? colors[led] : Color.rgb(30, 42, 55));
            canvas.drawCircle(x, y, (float) (diameter * 0.46), paint);
            paint.setStyle(Paint.Style.STROKE); paint.setStrokeWidth(getResources().getDisplayMetrics().density * 0.5f);
            paint.setColor(Color.rgb(48, 64, 80));
            canvas.drawCircle(x, y, (float) (diameter * 0.46), paint);
        }
        if(moving) postInvalidateOnAnimation();
    }
}
