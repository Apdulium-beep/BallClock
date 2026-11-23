"""Explicit COM5 hardware test. Restores the original values in a finally block."""
import ctypes
import json
from pathlib import Path
import subprocess
import sys
import time
import serial

ROOT = Path(__file__).resolve().parents[3]
KEYS = ['font','color','lead','blink','bmode','pal','bbright','speed','scale','angle','bright','night','ncolor','bcolor']
class Parameters(ctypes.Structure):
    _fields_ = [(k, ctypes.c_int) for k in KEYS]+[('customGradient',ctypes.c_char*96),('drawDigits',ctypes.c_int),('art',ctypes.c_ubyte*1920)]

def drawing_parameters(parameters,current):
    parameters.drawDigits=current['s'].get('drawDigits',0)
    data=current.get('art',{}).get('data','')
    if len(data)==3712:
        raw=bytearray()
        for i in range(128):
            record=data[i*29:(i+1)*29];raw.append(int(record[0]));raw.extend(bytes.fromhex(record[1:]))
        parameters.art[:]=raw

def open_port():
    p = serial.Serial()
    p.port = 'COM5'; p.baudrate = 115200; p.timeout = .2; p.write_timeout = 3
    p.dtr = False; p.rts = False; p.open()
    return p

def command(p, text, prefix):
    p.write(text.encode('utf-8') + b'\n')
    last_send = time.monotonic()
    end = time.monotonic() + 10
    incoming = bytearray()
    while time.monotonic() < end:
        incoming.extend(p.read(p.in_waiting or 1))
        while b'\n' in incoming:
            line, _, tail = incoming.partition(b'\n')
            incoming = bytearray(tail)
            line = line.strip()
            if line.startswith(b'error '): raise RuntimeError(line.decode())
            if line.startswith(prefix): return line[len(prefix):]
        # A read-only query can be lost during ESP8266 startup after a reset.
        if text == 'state' and time.monotonic() - last_send > 1:
            p.write(b'state\n'); last_send = time.monotonic()
    raise TimeoutError(text[:30])

def state(p): return json.loads(command(p, 'state', b'STATE '))

def main():
    library = ctypes.CDLL(str(ROOT / 'build/qt/clockengine.dll'))
    render = library.clock_render
    render.argtypes = [ctypes.POINTER(Parameters),ctypes.c_int,ctypes.c_int,ctypes.c_uint32,
                       ctypes.POINTER(ctypes.c_uint8),ctypes.POINTER(ctypes.c_uint8)]
    render.restype = None
    p = open_port(); original = None
    try:
        time.sleep(3)
        initial = state(p); assert initial['protocol'] >= 3
        original = initial['s']
        (ROOT/'backups/pre-usb-v3-settings.json').write_text(json.dumps(initial,ensure_ascii=False,indent=2),encoding='utf-8')
        values = [int(original[k],16) if k in ('color','ncolor','bcolor') else int(original[k]) for k in KEYS]
        values[11] = int(initial['t']['night'])
        values[10] = int(initial['t']['brightness'])
        parameters = Parameters(*values)
        parameters.customGradient=original.get('grad'+str(parameters.pal-9),'').encode('utf-8')
        drawing_parameters(parameters,initial)
        for ms in (0,123456,876543):
            raw=(ctypes.c_uint8*384)(); shown=(ctypes.c_uint8*384)()
            render(ctypes.byref(parameters),12,34,ms,raw,shown)
            if '--compare-device-renderer' in sys.argv:
                reference=bytes.fromhex(command(p,f'sample 12 34 {ms}',b'SAMPLE ').decode())
                assert bytes(raw)==reference, f'PC renderer differs at ms={ms}'
            command(p,'pixels '+bytes(raw).hex(),b'ok pixels')
            actual=bytes.fromhex(command(p,'frame',b'FRAME ').decode())
            assert actual==bytes(shown), 'Physical output differs from PC preview'
        print('PASS: clocks show the PC-generated frame byte-for-byte.')
        if '--matrix' in sys.argv:
            for mode in (1,2):
                command(p,f'set bmode {mode}',b'ok')
                for palette in range(9):
                    command(p,f'set pal {palette}',b'ok')
                    for font in (1,2):
                        command(p,f'set font {font}',b'ok')
                        parameters.bmode=mode; parameters.pal=palette; parameters.font=font
                        render(ctypes.byref(parameters),7,9,123456,raw,shown)
                        if '--compare-device-renderer' in sys.argv:
                            reference=bytes.fromhex(command(p,'sample 7 9 123456',b'SAMPLE ').decode())
                            assert bytes(raw)==reference, f'Render mismatch: mode={mode}, palette={palette}, font={font}'
                        command(p,'pixels '+bytes(raw).hex(),b'ok pixels')
                        actual=bytes.fromhex(command(p,'frame',b'FRAME ').decode())
                        assert actual==bytes(shown), f'Output mismatch: mode={mode}, palette={palette}, font={font}'
            print('PASS: both fonts, all 9 palettes, gradient and Perlin agree between PC and physical output.')
            for key in ('font','bmode','pal'):
                command(p,f'set {key} {original[key]}',b'ok')
        changed=1-int(original['blink'])
        command(p,f'set blink {changed}',b'ok'); command(p,'save',b'ok')
        p.close(); time.sleep(2.2); p=open_port()
        assert state(p)['t']['remote']==0, 'Remote lease did not expire'
        esptool=Path(sys.executable).parents[1]/'platformio/packages/tool-esptoolpy/esptool.py'
        p.close()
        subprocess.run([sys.executable,str(esptool),'--port','COM5','run'],check=True)
        time.sleep(3); p=open_port()
        assert state(p)['s']['blink']==changed, 'Saved parameter did not survive reboot'
        print('PASS: autonomous fallback and flash settings retention after reboot.')
    finally:
        if not p.is_open: p=open_port(); time.sleep(3)
        if original is not None:
            for key in ('font','bmode','pal','blink'):
                command(p,f'set {key} {original[key]}',b'ok')
            command(p,'save',b'ok')
            assert state(p)['s']==original, 'Settings were not restored'
            command(p,f"time {int(time.time())} {original['gmt']}",b'ok')
            command(p,'stop',b'ok')
            print('PASS: all original setting values restored; clock time initialized.')
        p.close()

if __name__=='__main__': main()
