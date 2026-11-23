"""COM5: named presets, exact renderer parity, actual restart persistence; restores state."""
import ctypes
import json
from pathlib import Path
import subprocess
import sys
import time
sys.path.insert(0,str(Path(__file__).resolve().parent))
from pc_to_clock_test import ROOT,KEYS,Parameters,open_port,command,state

def main():
    port=open_port();original=None;slot=None;report={}
    try:
        initial=state(port);original=initial['s']
        slot=next(i for i in range(8) if not original['grad'+str(i)])
        key='grad'+str(slot);starts=initial['t']['apStarts']
        command(port,f"time {int(time.time())} {original['gmt']}",b'ok')
        for k,v in {'night':0,'autoB':0,'bright':100,'font':0,'bmode':1,'pal':9+slot,'speed':0,'scale':50,'angle':0,'bbright':255}.items():command(port,f'set {k} {v}',b'ok')
        engine=ctypes.CDLL(str(ROOT/'build/qt/clockengine.dll'));render=engine.clock_render
        render.argtypes=[ctypes.POINTER(Parameters),ctypes.c_int,ctypes.c_int,ctypes.c_uint32,ctypes.POINTER(ctypes.c_uint8),ctypes.POINTER(ctypes.c_uint8)]
        text='Северное сияние|000000,38c3ff,91e0c2,ffffff'
        for preset in (text,'Проверка|ff0000,ff0000,ff0000,ff0000','Проверка|0000ff,0000ff,0000ff,0000ff',text):
            command(port,f'set {key} {preset}',b'ok');time.sleep(.08)
            current=state(port)
            assert current['s'][key]==preset,'UTF-8 name roundtrip failed'
            values=[int(current['s'][k],16) if k in ('color','ncolor','bcolor') else int(current['s'][k]) for k in KEYS]
            values[11]=current['t']['night'];values[10]=current['t']['brightness'];p=Parameters(*values);p.customGradient=preset.encode('utf-8')
            raw=(ctypes.c_uint8*384)();shown=(ctypes.c_uint8*384)();render(ctypes.byref(p),12,34,0,raw,shown)
            assert bytes(raw)==bytes.fromhex(command(port,'sample 12 34 0',b'SAMPLE ').decode()),'PC/firmware custom palette mismatch'
            assert bytes(shown)==bytes.fromhex(command(port,'frame',b'FRAME ').decode()),'Autonomous displayed custom palette mismatch'
            assert current['t']['ap']==1 and current['t']['apStarts']==starts,'AP restarted during preset edits'
        command(port,'save',b'ok');before_reset=state(port);port.close();port=None
        tool=Path(sys.executable).parent.parent/'platformio/packages/tool-esptoolpy/esptool.py'
        result=subprocess.run([sys.executable,str(tool),'--port','COM5','--baud','115200','chip_id'],capture_output=True,text=True,timeout=25)
        if result.returncode:raise RuntimeError('Read-only chip-id/reset failed: '+result.stdout[-1000:])
        port=open_port();restarted=state(port)
        assert restarted['s'][key]==text and restarted['s']['pal']==9+slot,'Saved gradient lost after hardware reset'
        assert restarted['t']['uptimeMs']<before_reset['t']['uptimeMs'],'Hardware reset not observed'
        assert restarted['t']['remote']==0 and restarted['t']['ap']==1
        report={'pass':True,'palette_roundtrips':4,'utf8_name_preserved':True,'actual_hardware_restart_persistence':True,'pc_firmware_autonomous_rgb_match':True}
    finally:
        if port is None:port=open_port()
        if original is not None:
            restore=['night','autoB','bright','font','bmode','pal','speed','scale','angle','bbright']
            if slot is not None:restore.append('grad'+str(slot))
            for k in restore:command(port,f'set {k} {original[k]}',b'ok')
            command(port,f"time {int(time.time())} {original['gmt']}",b'ok');command(port,'save',b'ok')
        port.close()
    port=open_port()
    try:
        final=state(port);assert final['s']==original,'Settings not restored'
        report.update(settings_restored=True,final_telemetry=final['t'])
        (ROOT/'build/named-gradient-results.json').write_text(json.dumps(report,indent=2),encoding='utf-8');print(json.dumps(report))
    finally:port.close()

if __name__=='__main__':main()
