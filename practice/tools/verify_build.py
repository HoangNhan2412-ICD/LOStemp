#!/usr/bin/env python3
"""Verify make/clean in disposable copies, preserving tested submission files."""
from pathlib import Path
import json
import shutil
import subprocess
import time

P = Path(__file__).resolve().parents[1]
S = P/'submission'
run = P/'output'/('clean-check-'+time.strftime('%Y%m%d-%H%M%S'))
run.mkdir()
with (S/'logs/build-clean-verification.txt').open('w') as log:
    for dirname, module, test in [('task1_ioctl','asgn1_driver','asgn1_test'),
                                  ('task2_misc','counter_driver','counter_test')]:
        dst = run/dirname
        dst.mkdir()
        for f in (S/dirname).iterdir():
            if f.suffix in ['.c','.h','.sh'] and not f.name.endswith('.mod.c') or f.name=='Makefile':
                shutil.copy2(f,dst/f.name)
        for target in ['all','clean','all']:
            cmd = ['make','-C',str(dst),'KDIR='+str(P/'environment/build-dyk'),target]
            log.write('$ '+' '.join(cmd)+'\n'); log.flush()
            subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT,check=True)
            if target=='clean':
                assert not (dst/(module+'.ko')).exists()
                assert not (dst/test).exists()
                assert (dst/(module+'.c')).exists()
                log.write('PASS clean removes generated products and preserves source\n')
            else:
                assert (dst/(module+'.ko')).is_file() and (dst/test).is_file()
                log.write('PASS build produces module and static ARM test executable\n')
    log.write('BUILD_CLEAN_VERIFIED\n')
with (S/'logs/module-metadata.txt').open('w') as log:
    for dirname, module, test in [('task1_ioctl','asgn1_driver','asgn1_test'),('task2_misc','counter_driver','counter_test')]:
        subprocess.run(['file',str(S/dirname/(module+'.ko')),str(S/dirname/test)],stdout=log,check=True)
        subprocess.run(['modinfo',str(S/dirname/(module+'.ko'))],stdout=log,check=True)
print('BUILD_CLEAN_VERIFIED',run)
