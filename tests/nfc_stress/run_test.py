#!/usr/bin/env python3
"""Build and run a host-only stress test against the repository's current NFC parser."""
from pathlib import Path
import os, subprocess
root = Path(__file__).resolve().parents[2]
src = root / 'fw/application/src/ntag/ntag_emu_v2.c'
testdir = Path(__file__).resolve().parent
text = src.read_text(encoding='utf-8')
marker = 'typedef enum {'
if marker not in text or 'static void nfc_received_process(' not in text:
    raise SystemExit('Cannot find expected parser structure; review harness extraction.')
(testdir / 'parser_under_test.inc').write_text(text[text.index(marker):], encoding='utf-8')
cmd = ['gcc', '-std=gnu11', '-O1', '-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-o', str(testdir / 'nfc_stress'), str(testdir / 'runner.c')]
print('Compiling current repository parser with sanitizers...',flush=True)
subprocess.run(cmd, check=True)
print('Running 1,000,000 fuzzed command cases...',flush=True)
env = os.environ.copy()
env['ASAN_OPTIONS'] = 'detect_leaks=1:halt_on_error=1'
subprocess.run([str(testdir / 'nfc_stress'), '1000000'], check=True, env=env)
