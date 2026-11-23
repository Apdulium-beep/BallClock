"""Explicit COM5 hardware check: clock ticks with the controller disconnected.

Does not change visual settings. Sets correct current time using stored UTC offset.
"""
import time
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
from pc_to_clock_test import open_port, command, state


def seconds(text):
    hour, minute, second = map(int, text.split(':'))
    return hour * 3600 + minute * 60 + second


def main():
    port = open_port()
    try:
        initial = state(port)
        settings = initial['s']
        command(port, f"time {int(time.time())} {settings['gmt']}", b'ok')
        command(port, 'stop', b'ok')
        before = state(port)
    finally:
        port.close()
    # This dwell measures autonomous ticking, not completion of a background job.
    time.sleep(6.2)
    port = open_port()
    try:
        after = state(port)
    finally:
        port.close()
    elapsed = (seconds(after['t']['time']) - seconds(before['t']['time'])) % 86400
    assert 4 <= elapsed <= 12, f'Clock did not advance normally: {elapsed}s'
    assert after['s'] == settings, 'Autonomous settings changed'
    assert after['t']['synced'] == 1 and after['t']['remote'] == 0
    assert after['t']['saved'] == 1, 'Settings not confirmed saved'
    print(f'PASS: clock advanced {elapsed}s with serial controller disconnected; autonomous mode, settings unchanged and saved.')


if __name__ == '__main__':
    main()
