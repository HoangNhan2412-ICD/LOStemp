#!/usr/bin/env python3
"""Build a new initramfs overlay and run real tests on the unmodified dyk image.
All generated data stays under practice/. Existing runs are retained.
"""
import gzip
import hashlib
import json
from pathlib import Path
import re
import shutil
import stat
import subprocess
import time

ROOT = Path(__file__).resolve().parents[2]
P = ROOT / 'practice'
S = P / 'submission'


def entry(name, data=b'', mode=stat.S_IFREG | 0o644, ino=1):
    """Encode one newc entry with relative name; return aligned bytes."""
    name = name.encode() + b'\0'
    fields = [ino, mode, 0, 0, 1, 0, len(data), 0, 0, 0, 0, len(name), 0]
    head = b'070701' + ''.join(f'{n:08x}' for n in fields).encode() + name
    return head + b'\0' * (-len(head) % 4) + data + b'\0' * (-len(data) % 4)


def originals():
    """Return SHA256 of all tracked originals, without modifying Git metadata."""
    names = subprocess.check_output(['git', 'ls-files', '-z'], cwd=ROOT).split(b'\0')
    return {n.decode(): hashlib.sha256((ROOT/n.decode()).read_bytes()).hexdigest()
            for n in names if n and not n.startswith(b'practice/')}


def main():
    """Create unique run, execute QEMU, extract evidence, assert acceptance."""
    run = P / 'output' / 'runs' / time.strftime('%Y%m%d-%H%M%S')
    run.mkdir(parents=True, exist_ok=False)
    before = originals()
    (run/'originals-before.json').write_text(json.dumps(before, indent=2))
    init = '''#!/bin/sh
export PATH=/bin:/sbin:/usr/bin:/usr/sbin
mount -t proc proc /proc
mount -t sysfs sysfs /sys
mount -t devtmpfs devtmpfs /dev
echo ASM1_ENV_BEGIN
uname -a
cat /proc/version
zcat /proc/config.gz | sha256sum
echo ASM1_ENV_END
echo ASM1_TASK1_BEGIN
sh /asm1/task1_ioctl/test_asgn1.sh
a=$?
echo ASM1_TASK1_RC=$a
echo ASM1_TASK1_END
echo ASM1_TASK2_BEGIN
sh /asm1/task2_misc/test_counter.sh
b=$?
echo ASM1_TASK2_RC=$b
echo ASM1_TASK2_END
echo ASM1_DMESG_BEGIN
dmesg
echo ASM1_DMESG_END
echo ASM1_ALL_DONE
poweroff -f
'''
    overlay = entry('asm1', mode=stat.S_IFDIR|0o755)
    overlay += entry('asm1/init', init.encode(), stat.S_IFREG|0o755)
    for dirname, module, test, script in [
        ('task1_ioctl','asgn1_driver.ko','asgn1_test','test_asgn1.sh'),
        ('task2_misc','counter_driver.ko','counter_test','test_counter.sh')]:
        overlay += entry('asm1/'+dirname, mode=stat.S_IFDIR|0o755)
        for filename in [module,test,script]:
            overlay += entry('asm1/'+dirname+'/'+filename,
                             (S/dirname/filename).read_bytes(),stat.S_IFREG|0o755)
    overlay += entry('TRAILER!!!', mode=0)
    base = gzip.decompress((P/'environment/boot/initramfs.cpio.gz').read_bytes())
    image = run/'initramfs-asgn1.cpio.gz'
    image.write_bytes(gzip.compress(base+overlay, mtime=0))
    cmd = [shutil.which('qemu-system-arm'), '-M','vexpress-a9','-cpu','cortex-a9',
           '-m','512M','-smp','2','-display','none','-monitor','none',
           '-serial','stdio','-no-reboot','-nic','none',
           '-kernel',str(P/'environment/boot/zImage'),
           '-dtb',str(P/'environment/boot/vexpress-v2p-ca9.dtb'),
           '-initrd',str(image),'-append',
           'console=ttyAMA0,115200 rdinit=/asm1/init mem=512M log_buf_len=256K']
    (run/'command.json').write_text(json.dumps(cmd, indent=2))
    print('Running actual dyk kernel in QEMU:', run, flush=True)
    with (run/'serial.log').open('wb') as log:
        completed = subprocess.run(cmd, stdin=subprocess.DEVNULL, stdout=log,
                                   stderr=subprocess.STDOUT, timeout=180)
    serial = (run/'serial.log').read_text(errors='replace').replace('\r','')
    for tag, filename in [('ENV','runtime.txt'),('TASK1','test_asgn1_output.txt'),
                          ('TASK2','test_counter_output.txt'),('DMESG','dmesg_full.txt')]:
        match = re.search(r'^ASM1_'+tag+r'_BEGIN\n(.*?)^ASM1_'+tag+r'_END$',serial,re.M|re.S)
        if match:
            (run/filename).write_text(match[1])
    after = originals()
    result = {'qemu_returncode': completed.returncode,
              'task1': 'ASM1_TASK1_RC=0' in serial and 'TASK1_SCRIPT_PASS' in serial,
              'task2': 'ASM1_TASK2_RC=0' in serial and 'TASK2_SCRIPT_PASS' in serial,
              'dyk_kernel': '(dyk@mydklaptop)' in serial,
              'no_kernel_oops': not bool(re.search(r'Kernel panic|Oops:|BUG:|Unable to handle kernel', serial)),
              'originals_unchanged': before == after,
              'completed': 'ASM1_ALL_DONE' in serial}
    (run/'result.json').write_text(json.dumps(result, indent=2))
    print(json.dumps(result,indent=2), flush=True)
    if not all(result[k] for k in result if k!='qemu_returncode') or completed.returncode:
        print(serial[-12000:])
        raise SystemExit(1)
    for name in ['runtime.txt','test_asgn1_output.txt','test_counter_output.txt','dmesg_full.txt','result.json']:
        shutil.copy2(run/name,S/'logs'/name)
    (S/'logs/latest-run.txt').write_text(str(run.relative_to(ROOT))+'\n')


if __name__ == '__main__':
    main()
