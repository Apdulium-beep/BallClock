"""Read-only settings check of PC RGB -> device displayed-RGB diagnostics on COM5."""
import ctypes
from pathlib import Path
import sys
sys.path.insert(0,str(Path(__file__).resolve().parent))
from pc_to_clock_test import open_port,state,command,Parameters,KEYS,ROOT,drawing_parameters

def main():
    engine=ctypes.CDLL(str(ROOT/'build/qt/clockengine.dll'))
    render=engine.clock_render
    render.argtypes=[ctypes.POINTER(Parameters),ctypes.c_int,ctypes.c_int,ctypes.c_uint32,
                     ctypes.POINTER(ctypes.c_uint8),ctypes.POINTER(ctypes.c_uint8)]
    port=open_port()
    try:
        current=state(port);original=current['s']
        values=[int(original[k],16) if k in ('color','ncolor','bcolor') else int(original[k]) for k in KEYS]
        values[11]=current['t']['night'];values[10]=current['t']['brightness'];params=Parameters(*values)
        params.customGradient=original.get('grad'+str(params.pal-9),'').encode('utf-8')
        drawing_parameters(params,current)
        for ms in (0,123456,876543):
            raw=(ctypes.c_uint8*384)();shown=(ctypes.c_uint8*384)()
            render(ctypes.byref(params),12,34,ms,raw,shown)
            sample=bytes.fromhex(command(port,f'sample 12 34 {ms}',b'SAMPLE ').decode())
            assert bytes(raw)==sample,'Shared firmware/PC renderer mismatch'
            command(port,'pixels '+bytes(raw).hex(),b'ok pixels')
            displayed=bytes.fromhex(command(port,'frame',b'FRAME ').decode())
            assert bytes(shown)==displayed,'Brightness/current-limit RGB changed'
        assert state(port)['s']==original,'Read-only test changed settings'
        print('PASS: PC raw renderer and 384 displayed RGB bytes match after LED-backend change; settings unchanged.')
    finally:
        command(port,'stop',b'ok');port.close()

if __name__=='__main__':main()
