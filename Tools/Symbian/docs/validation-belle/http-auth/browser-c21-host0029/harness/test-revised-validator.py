#!/usr/bin/env python3
"""Only synthetic temporary evidence; no real runs, credentials or display."""
from pathlib import Path
import importlib.util,json,tempfile,hashlib
ROOT=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('revised',ROOT/'revised_validator.py');v=importlib.util.module_from_spec(spec);spec.loader.exec_module(v)
DOMINO='T source/thread.cpp:542 [Kernel]: Thread 1234abcd panicked with category: Domino and exit code: 13 \n'
checks=0
original_hash=hashlib.sha256((ROOT/'upstream/validate.py').read_bytes()).hexdigest()
def write(path,value):path.write_text(json.dumps(value)+'\n')
def setup(root,case,scheme):
    run='public-revised-test';ready={'run':run,'scheme':scheme,'case':case,'url':'http://localhost:8794/?run='+run};f=root/'fixture';f.mkdir();write(f/'ready.json',ready)
    write(f/'credentials.private.json',{'username':'publicuser','password':'syntheticpublicsecret','wrong_password':'syntheticpublicwrong'})
    events=[]
    def event(name,**more):events.append(dict(ready,sequence=len(events)+1,event=name,**more))
    event('page-ready');event('page-started');event('auth-request',authorization_present=False,accepted=False);records=[]
    if case=='success':
        event('auth-request',authorization_present=True,accepted=True);event('page-loaded');event('page-responsive');records.append({'event':'credential-submitted','attempt':1})
    elif case=='retry':
        for n in (1,2,3):event('auth-request',authorization_present=True,accepted=False);records.append({'event':'credential-submitted','attempt':n})
        event('page-exhausted');event('page-responsive')
    else:
        records.append({'event':'native-query-pending'})
        if case.startswith('cancel'):records.append({'event':'native-cancel'});event('page-responsive')
        elif case.startswith('navigate'):records.append({'event':'navigation-requested'});event('page-navigated')
        else:records.append({'event':'normal-exit-helper-launch'})
    if not case.startswith('exit'):records.append({'event':'quiet-observation','seconds':5})
    (f/'events.jsonl').write_text(''.join(json.dumps(e)+'\n' for e in events))
    count=v.expected_probe_count(case)
    drive=dict(ready,status='EXIT_REQUEST_SENT_PENDING_VALIDATION' if case.startswith('exit') else 'INTERACTION_COMPLETE_PENDING_VALIDATION',records=records,pid=123,screenshots_taken=True,credential_stage_screenshots=False,public_probe_only_screenshots=True,heading_reviews=[{'synthetic_prevalidated_heading':1}],password_mask_reviews=[{'synthetic_prevalidated_hook':n} for n in range(count)])
    
    if not case.startswith('exit'):
        proof=dict(kind='trusted-parent-button',run=run,pid=123,window='0x17',geometry=[1,25,900,700],fep_absent=True,trusted=True,source='top-parent',page_path='/landed' if case.startswith('navigate') else '/',recorded_after_interaction=True)
        drive['native_query_closure']=proof;records.append(dict(event='trusted-parent-probe-clicked'));records.append(dict(event='trusted-parent-probe',**proof))
    write(root/'interaction.json',drive);(root/'guest.log').write_text(run+'\n'+('12:00:01.000 WebKit Browser exiting\n' if case.startswith('exit') else 'console: http://localhost:8794/:1: AUTH-PARENT-PROBE-CLICK '+json.dumps(dict(run=run,source='top-parent',page_path=proof['page_path'],x=180,y=500,width=360,height=532,trusted=True))+'\n'));(root/'host.log').write_text(DOMINO)
    report=dict(run=run,url=ready['url'],browser_sha256='a'*64,emulator_sha256='b'*64,host_exit_code=0,normal_close=True,forced_cleanup=False,wm_close_sent=not case.startswith('exit'),finished_utc='2026-11-01T00:00:00Z',curl_trace_disabled=True,native_query_reviewed=True,native_query_dismissed_reviewed=True,masked_password_reviewed=True);write(root/'report.json',report)
    helper=root/'helper.log';helper.write_text('BEGIN: browser-exit-request v1\nTARGET: uid=e1a5c002 pid=123 name=C:\\sys\\bin\\browser.exe\nREQUEST: TApaTask::EndTask; no KillTask, Terminate, or host WM close\nREQUEST_SENT: official application shutdown event\nEXIT: type=0 reason=0 logon=0\n')
    observer={'host_pid':123,'host_exit_code':0,'forced_cleanup':False,'input_exit':0,'wm_close_sent':not case.startswith('exit'),'wm_close_record':{'pid':123,'after_driver_complete':True,'sent':True}}
    probe={'status':'PASS','failures':[],'incomplete':[],'expected':count,'approved':count}
    return observer,probe,report,drive,helper

def run(case='success',scheme='basic',mutation=None,expected='PASS'):
    global checks
    with tempfile.TemporaryDirectory(prefix='public-auth-validator-') as directory:
        root=Path(directory);observer,probe,report,drive,helper=setup(root,case,scheme)
        options={'observer':observer,'public_probe':probe,'helper':helper if case.startswith('exit') else None}
        if mutation:mutation(root,options,report,drive)
        write(root/'report.json',report);write(root/'interaction.json',drive)
        raw={p:p.read_bytes() for p in [root/'interaction.json',root/'host.log',root/'guest.log']}
        result=v.validate(root/'fixture',root/'interaction.json',root/'guest.log',root/'host.log',root/'report.json',**options)
        assert result['status']==expected,(case,result)
        assert result['original_validator_status']=='FAIL'
        assert all(p.read_bytes()==data for p,data in raw.items())
        checks+=1
for scheme in ('basic','digest'):
    for case in ('success','retry','cancel-user','cancel-password','navigate-user','navigate-password','exit-user','exit-password'):run(case,scheme)
run(mutation=lambda r,o,p,d:(r/'host.log').write_text('Thread WebKit panicked with category: EIKCTL and exit code: 13\n'),expected='FAIL')
run(mutation=lambda r,o,p,d:(r/'host.log').write_text('Thread WebKit panicked with category: Domino and exit code: 13\n'),expected='FAIL')
run(mutation=lambda r,o,p,d:(r/'host.log').write_text('Thread somebody panicked with category: OTHER and exit code: 13\n'),expected='FAIL')
run(mutation=lambda r,o,p,d:(r/'host.log').write_text('Thread somebody panicked with category: Domino and exit code: 12\n'),expected='FAIL')
run(mutation=lambda r,o,p,d:(r/'host.log').write_text('PANIC unknown source\n'),expected='FAIL')
run(mutation=lambda r,o,p,d:(r/'host.log').write_text('SIGSEGV\n'+DOMINO),expected='FAIL')
run(mutation=lambda r,o,p,d:(r/'host.log').write_text('PANIC unrelated '+DOMINO),expected='FAIL')
run(mutation=lambda r,o,p,d:(r/'guest.log').write_text('public-revised-test\n'+DOMINO),expected='FAIL')
run(mutation=lambda r,o,p,d:o['observer'].update(wm_close_sent=False),expected='FAIL')
run(mutation=lambda r,o,p,d:o['observer']['wm_close_record'].update(after_driver_complete=False),expected='FAIL')
run(mutation=lambda r,o,p,d:o['observer']['wm_close_record'].update(pid=456),expected='FAIL')
run(mutation=lambda r,o,p,d:o['observer']['wm_close_record'].update(unknown=True),expected='FAIL')
run(mutation=lambda r,o,p,d:p.update(host_exit_code=-11),expected='FAIL')
run(mutation=lambda r,o,p,d:o['observer'].update(host_exit_code=-11),expected='FAIL')
run(mutation=lambda r,o,p,d:p.update(forced_cleanup=True),expected='FAIL')
run(mutation=lambda r,o,p,d:d.update(status='RUNNING'),expected='FAIL')
run('exit-password',mutation=lambda r,o,p,d:p.update(wm_close_sent=True),expected='FAIL')
run('exit-password',mutation=lambda r,o,p,d:o.update(helper=None),expected='FAIL')
run('exit-password',mutation=lambda r,o,p,d:(r/'helper.log').write_text('missing official request\n'),expected='FAIL')
run('exit-password',mutation=lambda r,o,p,d:(r/'guest.log').write_text('public-revised-test\n'),expected='FAIL')
run(mutation=lambda r,o,p,d:o['public_probe'].update(status='INCOMPLETE',approved=0,incomplete=['root review missing']),expected='INCOMPLETE')
run(mutation=lambda r,o,p,d:o.update(public_probe=None),expected='INCOMPLETE')
run(mutation=lambda r,o,p,d:o['public_probe'].update(unknown=True),expected='FAIL')
run(mutation=lambda r,o,p,d:o['public_probe'].update(approved=True),expected='FAIL')
run(mutation=lambda r,o,p,d:d.update(credential_stage_screenshots=True),expected='FAIL')
run(mutation=lambda r,o,p,d:d.update(screenshots_taken=False),expected='FAIL')
run(mutation=lambda r,o,p,d:p.update(native_query_reviewed=False),expected='INCOMPLETE')
run(mutation=lambda r,o,p,d:(r/'fixture/events.jsonl').write_text((r/'fixture/events.jsonl').read_text().replace('"accepted": true','"accepted": false')),expected='FAIL')
run(mutation=lambda r,o,p,d:(r/'guest.log').write_text('public-revised-test\nAuthorization: Basic redacted\n'),expected='FAIL')
run(mutation=lambda r,o,p,d:(r/'guest.log').write_text('public-revised-test\nsyntheticpublicsecret\n'),expected='FAIL')
run(mutation=lambda r,o,p,d:d.pop('native_query_closure'),expected='FAIL')
run(mutation=lambda r,o,p,d:d['native_query_closure'].update(trusted=False),expected='FAIL')
run(mutation=lambda r,o,p,d:d['native_query_closure'].update(pid=456),expected='FAIL')
run(mutation=lambda r,o,p,d:d['native_query_closure'].update(fep_absent=False),expected='FAIL')
run(mutation=lambda r,o,p,d:(r/'guest.log').write_text('public-revised-test\n'),expected='FAIL')
run(mutation=lambda r,o,p,d:d.update(heading_reviews=[]),expected='INCOMPLETE')
assert hashlib.sha256((ROOT/'upstream/validate.py').read_bytes()).hexdigest()==original_hash
print(json.dumps({'status':'PASS','checks':checks,'scope':'temporary synthetic evidence only; no existing credentials, guest, server or display'},indent=2))
