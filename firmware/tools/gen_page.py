# PlatformIO: shared source/android/assets/mobile.html -> src/webpage.h (gzip).
import gzip, os
try:
    Import("env")                      # внутри PlatformIO
    root = env["PROJECT_DIR"]
except NameError:                      # запуск вручную: python tools/gen_page.py
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# One settings/color/gradient UI for the APK and the clock's browser page.
# tools/page.html is retained as the previous lightweight page, not generated.
src = os.path.join(root, "..", "source", "android", "assets", "mobile.html")
dst = os.path.join(root, "src", "webpage.h")
data = gzip.compress(open(src, "rb").read(), 9, mtime=0)
rows = [", ".join("0x%02x" % b for b in data[i:i + 16]) for i in range(0, len(data), 16)]
out = ("#pragma once\n#include <pgmspace.h>\n"
        "// Generated from source/android/assets/mobile.html. Do not edit.\n"
       "const size_t PAGE_GZ_LEN = %d;\nconst uint8_t PAGE_GZ[] PROGMEM = {\n  %s\n};\n" % (len(data), ",\n  ".join(rows)))
if not os.path.exists(dst) or open(dst).read() != out:
    open(dst, "w").write(out)
print("webpage.h: %d байт -> %d байт (gzip)" % (os.path.getsize(src), len(data)))
