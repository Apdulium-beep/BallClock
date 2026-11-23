"""Physical local-AP test: HTTP night edits while reading displayed frames at 20Hz.

Requires the PC to be associated with the real BallClock AP. Restores night.
"""
import http.client
import argparse
import json
from pathlib import Path
import sys
import threading
import time
import urllib.parse
sys.path.insert(0,str(Path(__file__).resolve().parent))
from pc_to_clock_test import open_port,command

def request(connection,path,fields=None,stats=None):
    body=urllib.parse.urlencode(fields) if fields is not None else None
    headers={'Content-Type':'application/x-www-form-urlencoded'} if body is not None else {}
    if body is not None:headers['Connection']='close'
    for attempt in range(2):
        try:
            connection.request('POST' if body is not None else 'GET',path,body,headers)
            response=connection.getresponse();data=response.read();break
        except (http.client.RemoteDisconnected,ConnectionResetError,BrokenPipeError):
            # ESP8266 legitimately closes idle keep-alive sockets to serve another
            # client. Browsers retry safe GETs; raw http.client needs this explicitly.
            connection.close()
            if body is not None or attempt:raise
            if stats is not None:stats['get_connection_retries']+=1
    assert response.status==200,f'HTTP {response.status} for {path}'
    return json.loads(data)

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--toggles',type=int,default=100);args=parser.parse_args()
    control=http.client.HTTPConnection('192.168.4.1',timeout=3)
    preview=http.client.HTTPConnection('192.168.4.1',timeout=3)
    stopped=threading.Event();errors=[];metrics={'preview_frames':0,'max_frame_ms':0,'get_connection_retries':0}
    before=request(control,'/api/state');assert before['device']=='ballclock' and before['t']['ap']==1
    original=before['s'];starts=before['t']['apStarts'];uptime=before['t']['uptimeMs'];heap=before['t']['freeHeap']
    def read_frames():
        try:
            while not stopped.is_set():
                began=time.monotonic();frame=request(preview,'/api/v1/frame',stats=metrics)
                assert frame['device']=='ballclock' and frame['led_count']==128 and len(frame['rgb'])==768
                assert len(bytes.fromhex(frame['rgb']))==384
                elapsed=time.monotonic()-began
                metrics['max_frame_ms']=max(metrics['max_frame_ms'],round(elapsed*1000,2));metrics['preview_frames']+=1
                stopped.wait(max(0,.05-elapsed))
        except Exception as error:errors.append(str(error))
    worker=threading.Thread(target=read_frames,daemon=True);began=time.monotonic();worker.start()
    try:
        for i in range(args.toggles):
            value=i%2;current=request(control,'/api/set',{'night':value});t=current['t']
            assert current['s']['night']==value
            assert t['ap']==1 and t['apStarts']==starts,'AP restarted/disabled during HTTP night edit'
            assert t['uptimeMs']>=uptime and t['synced']==1,'ESP rebooted/time lost'
            heap=min(heap,t['freeHeap']);time.sleep(.15)
        stopped.set();worker.join(4)
        assert not worker.is_alive() and not errors,f'Preview failures: {errors}'
        assert metrics['preview_frames']>=args.toggles*.75,'Insufficient concurrent frame coverage'
    finally:
        stopped.set();worker.join(4);preview.close()
        try:
            request(control,'/api/set',{'night':original['night']});saved=request(control,'/api/save',{})
            assert saved.get('saved') is True
        except Exception:
            port=open_port()
            try:command(port,f"set night {original['night']}",b'ok');command(port,'save',b'ok')
            finally:port.close()
    after=request(control,'/api/state');control.close();assert after['s']==original
    report={'pass':True,'night_http_toggles':args.toggles,'preview':metrics,'elapsed_s':round(time.monotonic()-began,2),
            'ap_starts_before':starts,'ap_starts_after':after['t']['apStarts'],'lowest_free_heap':heap,
            'original_settings_restored':True,'final_telemetry':after['t']}
    output=Path(__file__).resolve().parents[3]/'build/wifi-night-stress-results.json'
    output.write_text(json.dumps(report,indent=2),encoding='utf-8');print(json.dumps(report))

if __name__=='__main__':main()
