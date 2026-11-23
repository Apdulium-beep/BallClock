"""Explicit COM5 regression: visual settings must never restart/disable AP.

Temporarily toggles night mode, restores it, and initializes correct PC time.
This verifies device telemetry, not independent RF visibility/Android behavior.
"""
import json
from pathlib import Path
import sys
import time
sys.path.insert(0,str(Path(__file__).resolve().parent))
from pc_to_clock_test import open_port,state,command

def main():
    port=open_port();original=None
    try:
        before=state(port);original=before['s'];expected_starts=before['t']['apStarts']
        start_uptime=before['t']['uptimeMs'];lowest_heap=before['t']['freeHeap']
        command(port,f"time {int(time.time())} {original['gmt']}",b'ok')
        for step in range(40):
            command(port,f'set night {step%2}',b'ok')
            time.sleep(.06) # let the physical renderer enter/leave its night branch
            current=state(port);t=current['t']
            assert t['ap']==1 and t['wifiMode']&2,'AP disabled'
            assert t['apStarts']==expected_starts,'Visual setting restarted AP'
            assert t['uptimeMs']>=start_uptime and t['synced']==1,'Device rebooted/time reset'
            assert current['s']['night']==step%2,'Night setting not applied'
            lowest_heap=min(lowest_heap,t['freeHeap'])
        command(port,f"set night {original['night']}",b'ok');command(port,'save',b'ok')
        command(port,'wifi_connect',b'ok')
        after=state(port)
        assert after['s']==original,'Original visual settings not restored'
        assert after['t']['ap']==1 and after['t']['apStarts']==expected_starts,'Healthy network apply restarted AP'
        print(json.dumps({'pass':True,'night_toggles':40,'ap_starts_before':expected_starts,
              'ap_starts_after':after['t']['apStarts'],'lowest_free_heap':lowest_heap,
              'final_telemetry':after['t']},ensure_ascii=True))
    finally:
        if original is not None:
            command(port,f"set night {original['night']}",b'ok');command(port,'save',b'ok')
        port.close()

if __name__=='__main__':main()
