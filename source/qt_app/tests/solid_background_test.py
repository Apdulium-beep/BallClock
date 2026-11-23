"""Physical COM5 solid-background regression, restoring every temporary setting."""
import ctypes
import json
import time
import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
from pc_to_clock_test import ROOT, KEYS, Parameters, open_port, command, state

port=open_port()
original=None
try:
    before=state(port);original=before['s'];starts=before['t']['apStarts']
    command(port,f"time {int(time.time())} {original['gmt']}",b'ok')
    engine=ctypes.CDLL(str(ROOT/'build/qt/clockengine.dll'))
    render=engine.clock_render
    render.argtypes=[ctypes.POINTER(Parameters),ctypes.c_int,ctypes.c_int,ctypes.c_uint32,
                     ctypes.POINTER(ctypes.c_uint8),ctypes.POINTER(ctypes.c_uint8)]
    edits={'night':0,'autoB':0,'bright':100,'font':0,'bmode':3,'bbright':255}
    for key,value in edits.items():command(port,f'set {key} {value}',b'ok')
    for color in ('123456','ff0000','00ff00','0000ff','38c3ff'):
        command(port,f'set bcolor {color}',b'ok')
        time.sleep(.08) # apply brightness/night state at the normal frame cadence
        current=state(port)
        assert current['s']['color']==original['color'],'Changed digit color'
        values=[int(current['s'][k],16) if k in ('color','ncolor','bcolor') else int(current['s'][k]) for k in KEYS]
        values[11]=current['t']['night'];values[10]=current['t']['brightness'];params=Parameters(*values)
        raw=(ctypes.c_uint8*384)();shown=(ctypes.c_uint8*384)()
        render(ctypes.byref(params),12,34,123456,raw,shown)
        expected=bytes.fromhex(color)*128
        assert bytes(raw)==expected,'PC solid background RGB'
        assert bytes.fromhex(command(port,'sample 12 34 123456',b'SAMPLE ').decode())==expected,'Firmware solid RGB'
        # Wait for one autonomous render cycle, never stream pixels from PC.
        time.sleep(.08)
        assert bytes.fromhex(command(port,'frame',b'FRAME ').decode())==bytes(shown),'Autonomous displayed RGB'
        assert current['t']['ap']==1 and current['t']['apStarts']==starts,'AP restarted'
    command(port,'save',b'ok');assert state(port)['t']['saved']==1
finally:
    if original is not None:
        for key in ('night','autoB','bright','font','bmode','bbright','bcolor'):
            command(port,f'set {key} {original[key]}',b'ok')
        command(port,'save',b'ok')
    port.close()
port=open_port()
try:
    after=state(port)
    assert after['s']==original and after['t']['apStarts']==starts and after['t']['remote']==0
    report={'pass':True,'colors':5,'pc_firmware_autonomous_rgb_match':True,'settings_restored':True,'telemetry':after['t']}
    (ROOT/'build/solid-background-results.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    print(json.dumps(report))
finally:port.close()
