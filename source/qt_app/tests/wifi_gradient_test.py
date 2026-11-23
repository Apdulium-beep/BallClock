"""Real local HTTP: clock-owned UTF-8 gradient shared by independent clients and USB."""
import gzip
import http.client
import json
from pathlib import Path
import sys
import time
sys.path.insert(0,str(Path(__file__).resolve().parent))
from wifi_night_stress_test import request
from pc_to_clock_test import ROOT,open_port,state

first=http.client.HTTPConnection('192.168.4.1',timeout=4)
second=http.client.HTTPConnection('192.168.4.1',timeout=4)
original=None;restore={}
try:
    before=request(first,'/api/state');original=before['s'];starts=before['t']['apStarts']
    slot=next(i for i in range(8) if not original['grad'+str(i)]);key='grad'+str(slot)
    fields={key:'Северное сияние|ff0000,ff0000,ff0000,ff0000','pal':9+slot,'bmode':1,'font':0,'night':0,'autoB':0,'bright':100,'bbright':255}
    restore={k:original[k] for k in fields}
    request(first,'/api/set',fields);assert request(first,'/api/save',{})['saved']
    other=request(second,'/api/v1/state');assert other['s'][key]==fields[key] and other['s']['pal']==9+slot
    time.sleep(.08)
    frame=request(second,'/api/v1/frame');rgb=bytes.fromhex(frame['rgb'])
    assert len(rgb)==384 and all(rgb[i]>0 and rgb[i+1]==0 and rgb[i+2]==0 for i in range(0,384,3))
    port=open_port()
    try:assert state(port)['s'][key]==fields[key],'USB cannot see HTTP-saved library'
    finally:port.close()
    renamed='Мой закат|0000ff,0000ff,0000ff,0000ff'
    request(second,'/api/set',{key:renamed});assert request(second,'/api/save',{})['saved']
    assert request(first,'/api/state')['s'][key]==renamed
    time.sleep(.08)
    rgb=bytes.fromhex(request(second,'/api/v1/frame')['rgb']);assert all(rgb[i]==0 and rgb[i+1]==0 and rgb[i+2]>0 for i in range(0,384,3))
    # Invalid batch cannot partly change the preset or other settings.
    first.request('POST','/api/set','bright=0&'+key+'=Invalid',{'Content-Type':'application/x-www-form-urlencoded','Connection':'close'})
    response=first.getresponse();response.read();assert response.status==400
    current=request(second,'/api/state');assert current['s']['bright']==100 and current['s'][key]==renamed
    # The actual clock/browser page uses the same inline picker/editor as the APK.
    first.request('GET','/',headers={'Connection':'close'});response=first.getresponse();page=gzip.decompress(response.read()).decode('utf-8')
    assert 'Свой цвет' in page and 'Создать свой градиент' in page
    assert current['t']['ap']==1 and current['t']['apStarts']==starts
    report={'pass':True,'independent_http_clients_share_library':True,'utf8_names':True,'usb_reads_http_saved_preset':True,'live_rgb_after_edit':True,'invalid_batch_atomic':True,'embedded_ui_matches_apk':True,'ap_starts_before':starts,'ap_starts_after':current['t']['apStarts']}
finally:
    if original is not None and restore:
        request(first,'/api/set',restore);assert request(first,'/api/save',{})['saved']
        final=request(second,'/api/state');assert final['s']==original,'Settings not restored'
    first.close();second.close()
report['settings_restored']=True
(ROOT/'build/wifi-gradient-results.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print(json.dumps(report))
