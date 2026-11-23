"""Read-only USB AP trace while a real Android user reproduces a disconnect."""
import argparse
import json
from pathlib import Path
import sys
import time
sys.path.insert(0, str(Path(__file__).resolve().parent))
from pc_to_clock_test import open_port, state

parser=argparse.ArgumentParser()
parser.add_argument('--output', required=True)
parser.add_argument('--stop-file', required=True)
parser.add_argument('--seconds', type=int, default=240)
args=parser.parse_args()
port=open_port()
count=0
try:
    deadline=time.monotonic()+args.seconds
    with open(args.output,'w',encoding='utf-8') as log:
        while time.monotonic()<deadline and not Path(args.stop_file).exists():
            try:
                current=state(port)
                record={'pc_time':time.time(),'night_setting':current['s'].get('night'),
                        'apOnly':current['s'].get('apOnly'),'t':current['t']}
            except Exception as error:
                record={'pc_time':time.time(),'error':type(error).__name__}
            log.write(json.dumps(record,ensure_ascii=True)+'\n');log.flush();count+=1
            # Measurement cadence, not waiting for another background tool.
            time.sleep(0.4)
finally:
    port.close()
print(f'AP trace finished: {count} samples; {args.output}')
