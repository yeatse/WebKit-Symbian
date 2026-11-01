"""Private rename/clone/restore for emulator configuration, never evidence copies."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import stat


def require(value, message):
    if not value:
        raise RuntimeError(message)


def snapshot(path):
    path=Path(path)
    if not path.exists():
        require(not path.is_symlink(), 'symlink configuration rejected')
        return {'exists':False}, []
    require(not path.is_symlink(), 'symlink configuration rejected')
    paths=[path]+sorted(path.rglob('*')) if path.is_dir() else [path]
    rows=[]
    for item in paths:
        require(not item.is_symlink(), 'symlink within configuration rejected')
        s=item.stat();require(item.is_dir() or (item.is_file() and s.st_nlink==1), 'non-regular or hardlinked config rejected')
        row={'relative':str(item.relative_to(path)) if item!=path else '.', 'directory':item.is_dir(),
             'device':s.st_dev,'inode':s.st_ino,'mode':stat.S_IMODE(s.st_mode),'mtime_ns':s.st_mtime_ns,'atime_ns':s.st_atime_ns}
        if item.is_file():row.update(bytes=s.st_size,sha256=hashlib.sha256(item.read_bytes()).hexdigest())
        rows.append(row)
    # Keep names and individual metadata only in the private transaction state.
    root=rows[0]
    return {'exists':True,'directory':path.is_dir(),'device':root['device'],'inode':root['inode'],
            'mode':root['mode'],'mtime_ns':root['mtime_ns'],'items':len(rows)}, rows


class PrivateConfigurations:
    def __init__(self, backup, specifications, report):
        self.backup=Path(backup);self.specifications=[(Path(p),d) for p,d in specifications]
        self.report=report;self.entries=[]
        report.update(restored=False,items=[])

    def reserve_all(self):
        require(not self.backup.exists(), 'fresh private config backup required')
        self.backup.mkdir(mode=0o700)
        for path,directory in self.specifications:
            before,rows=snapshot(path)
            require(not before['exists'] or before['directory']==directory, 'unexpected config kind')
            saved=self.backup/str(len(self.entries))
            item={'path':str(path),'before':before,'restored':False}
            entry={'path':path,'saved':saved,'before':before,'rows':rows,'report':item}
            self.entries.append(entry);self.report['items'].append(item)
            self.journal()
            if before['exists']:
                os.replace(path,saved)
                if directory:shutil.copytree(saved,path,copy_function=shutil.copy2)
                else:shutil.copy2(saved,path)
            # An originally absent path remains absent until EKA creates it.

    def journal(self):
        (self.backup/'restore-journal.json').write_text(json.dumps(self.report,indent=2)+'\n')

    def restore(self):
        failures=[]
        for entry in reversed(self.entries):
            path,saved,before=entry['path'],entry['saved'],entry['before']
            try:
                if before['exists'] and not saved.exists():
                    current,_=snapshot(path)
                    require(current==before, 'original rename failed and identity differs')
                else:
                    if path.is_dir():shutil.rmtree(path)
                    elif path.exists() or path.is_symlink():path.unlink()
                    if before['exists']:os.replace(saved,path)
                if before['exists']:
                    # Verify hashes without exposing file names/content, then
                    # restore read-access times affected by private cloning.
                    for row in reversed(entry['rows']):
                        item=path if row['relative']=='.' else path/row['relative']
                        s=item.stat();require(s.st_dev==row['device'] and s.st_ino==row['inode'] and stat.S_IMODE(s.st_mode)==row['mode'] and s.st_mtime_ns==row['mtime_ns'],'config metadata differs')
                        if not row['directory']:require(item.stat().st_size==row['bytes'] and hashlib.sha256(item.read_bytes()).hexdigest()==row['sha256'],'config bytes differ')
                        os.utime(item,ns=(row['atime_ns'],row['mtime_ns']))
                else:require(not path.exists(),'original absence not restored')
                entry['report']['restored']=True
            except (OSError,RuntimeError) as error:
                entry['report']['error']=str(error);failures.append(str(error))
        self.report['restored']=not failures
        if self.backup.exists():
            if failures:self.journal()
            else:
                (self.backup/'restore-journal.json').unlink(missing_ok=True);self.backup.rmdir()
        require(not failures,'configuration restoration failed; keep private backup')
