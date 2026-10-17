#!/usr/bin/env python3
"""Package only final deliverables and validate exact archive bytes/paths."""
from pathlib import Path
import hashlib
import json
import shutil
import subprocess
import tarfile

P=Path(__file__).resolve().parents[1]
ROOT=P.parent
S=P/'submission'
name='SE200778_PhamVoHoangNhan_Asgn01'
out=P/'output'
logs=S/'logs'
latest=ROOT/(logs/'latest-run.txt').read_text().strip()
result=json.loads((logs/'result.json').read_text())
assert result['task1'] and result['task2'] and result['dyk_kernel'] and result['no_kernel_oops']
shutil.copy2(latest/'serial.log', logs/'qemu_serial.txt')
shutil.copy2(latest/'originals-before.json',logs/'originals-sha256.json')
shutil.copy2(P/'CHECKLIST.md',S/'CHECKLIST.md')
for run in sorted((out/'runs').iterdir()):
    if (run/'result.json').exists():
        r=json.loads((run/'result.json').read_text())
        if not r['task1'] or not r['task2']:
            shutil.copy2(run/'serial.log',logs/'debug-first-run-serial.txt')
            break
baseline=json.loads((logs/'originals-sha256.json').read_text())
assert all(hashlib.sha256((ROOT/f).read_bytes()).hexdigest()==digest for f,digest in baseline.items())
for args in [['git','diff','--exit-code'],['git','diff','--cached','--exit-code']]:
    subprocess.run(args,cwd=ROOT,check=True)
files=[S/'README.md',S/'CHECKLIST.md']
for folder,expected in [
    ('task1_ioctl',['asgn1_driver.c','asgn1_ioctl.h','Makefile','asgn1_driver.ko','asgn1_test.c','asgn1_test','test_asgn1.sh']),
    ('task2_misc',['counter_driver.c','Makefile','counter_driver.ko','counter_test.c','counter_test','test_counter.sh'])]:
    files += [S/folder/f for f in expected]
files += sorted(p for p in logs.iterdir() if p.is_file())
files += sorted(p for p in (S/'bao_cao').iterdir() if p.is_file())
assert all(p.is_file() for p in files)
manifest={str(p.relative_to(S)):hashlib.sha256(p.read_bytes()).hexdigest() for p in files}
(S/'MANIFEST.sha256.json').write_text(json.dumps(manifest,indent=2,ensure_ascii=False))
files.append(S/'MANIFEST.sha256.json')
archive=out/(name+'.tar.gz')
with tarfile.open(archive,'w:gz') as tar:
    for p in files:
        tar.add(p,arcname=name+'/'+str(p.relative_to(S)),recursive=False)
with tarfile.open(archive,'r:gz') as tar:
    for member in tar.getmembers():
        assert member.isfile() and member.name.startswith(name+'/') and '..' not in Path(member.name).parts
        relative=member.name[len(name)+1:]
        data=tar.extractfile(member).read()
        assert data==(S/relative).read_bytes()
        if relative in manifest: assert hashlib.sha256(data).hexdigest()==manifest[relative]
pdf=out/'SE200778_PhamVoHoangNhan_Asgn01_BaoCao.pdf'
shutil.copy2(S/'bao_cao/SE200778_Asgn01_BaoCao.pdf',pdf)
validation={'archive':archive.name,'pdf':pdf.name,'files':len(files),
            'original_files_unchanged':len(baseline),'archive_members_verified':True,
            'task1_pass':result['task1'],'task2_pass':result['task2'],
            'run':str(latest.relative_to(ROOT)),
            'archive_sha256':hashlib.sha256(archive.read_bytes()).hexdigest(),
            'pdf_sha256':hashlib.sha256(pdf.read_bytes()).hexdigest(),
            'course_submission_rules':'to be reviewed by student against assignment sections IV-V',
            'lms_submission':'not performed'}
(out/'final-validation.json').write_text(json.dumps(validation,indent=2))
print(json.dumps(validation,indent=2))
