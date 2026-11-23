"""Explicit COM5 drawing test; restores settings and deletes its temporary free-slot picture."""
import ctypes
import json
from pathlib import Path
import subprocess
import sys
import time
sys.path.insert(0,str(Path(__file__).resolve().parent))
from pc_to_clock_test import ROOT,KEYS,Parameters,open_port,command,state,drawing_parameters

def main():
    port=open_port();original=None;slot=None;report={}
    try:
        before=state(port);original=before['s'];slot=next(i for i,name in enumerate(before['art']['names']) if not name)
        colors=['ff0000','00ff00','0000ff','ffffff'];records=[]
        for i in range(128):records.append(str(i%4)+f'{10+i%100:02x}{i*17%256:02x}'+''.join(colors[i%4:]+colors[:i%4]))
        data=''.join(records);name='Моя картина';assert len(data)==3712
        command(port,f'art put {slot} {name}|{data}',b'ok')
        for k,v in {'bmode':4,'drawSlot':slot,'drawDigits':0,'night':0,'autoB':0,'bright':80,'bbright':255,'font':1}.items():command(port,f'set {k} {v}',b'ok')
        time.sleep(.08);current=state(port);assert current['art']['data']==data and current['art']['names'][slot]==name
        assert len(json.dumps(current,ensure_ascii=False).encode())<8192
        engine=ctypes.CDLL(str(ROOT/'build/qt/clockengine.dll'));render=engine.clock_render
        render.argtypes=[ctypes.POINTER(Parameters),ctypes.c_int,ctypes.c_int,ctypes.c_uint32,ctypes.POINTER(ctypes.c_uint8),ctypes.POINTER(ctypes.c_uint8)]
        values=[int(current['s'][k],16) if k in ('color','ncolor','bcolor') else int(current['s'][k]) for k in KEYS]
        values[11]=current['t']['night'];values[10]=current['t']['brightness'];p=Parameters(*values);drawing_parameters(p,current)
        for overlay in (0,1):
            command(port,f'set drawDigits {overlay}',b'ok');p.drawDigits=overlay
            for ms in (0,250,999,876543,0xffffffff):
                raw=(ctypes.c_uint8*384)();shown=(ctypes.c_uint8*384)();render(ctypes.byref(p),12,34,ms,raw,shown)
                assert bytes(raw)==bytes.fromhex(command(port,f'sample 12 34 {ms}',b'SAMPLE ').decode()),'PC/firmware drawing RGB mismatch'
        first=command(port,'frame',b'FRAME ');time.sleep(.3);second=command(port,'frame',b'FRAME ')
        assert first!=second and state(port)['t']['remote']==0,'Autonomous animation not advancing'
        try:command(port,f'art put {slot} Bad|{data[:-1]}',b'ok');raise AssertionError('Accepted incomplete picture')
        except RuntimeError:pass
        assert state(port)['art']['data']==data,'Rejected picture partly changed valid data'
        command(port,'save',b'ok');pre_reset=state(port);port.close();port=None
        tool=Path(sys.executable).parent.parent/'platformio/packages/tool-esptoolpy/esptool.py'
        result=subprocess.run([sys.executable,str(tool),'--port','COM5','--baud','115200','chip_id'],capture_output=True,text=True,timeout=25)
        assert result.returncode==0,'Read-only chip-id reset failed'
        port=open_port();after=state(port);assert after['art']['names'][slot]==name and after['art']['data']==data,'Picture lost after reset'
        assert after['s']['drawDigits']==1 and after['s']['bmode']==4
        assert after['t']['uptimeMs']<pre_reset['t']['uptimeMs']
        report={'pass':True,'pixels':128,'individual_effects_periods_phases':True,'rgb_comparisons':10,'optional_time_layer':True,'autonomous_animation':True,'invalid_save_atomic':True,'actual_restart_persistence':True,'ap_starts_before_reset':pre_reset['t']['apStarts']}
    finally:
        if port is None:port=open_port()
        if original is not None:
            for key in ('bmode','drawSlot','drawDigits','night','autoB','bright','bbright','font'):command(port,f'set {key} {original[key]}',b'ok')
            if slot is not None:command(port,f'art delete {slot}',b'ok')
            command(port,f"time {int(time.time())} {original['gmt']}",b'ok');command(port,'save',b'ok')
        port.close()
    port=open_port()
    try:
        final=state(port);assert final['s']==original and not final['art']['names'][slot]
        report.update(settings_restored=True,test_picture_deleted=True,final_telemetry=final['t'])
        (ROOT/'build/drawing-live-results.json').write_text(json.dumps(report,indent=2),encoding='utf-8');print(json.dumps(report))
    finally:port.close()

if __name__=='__main__':main()
