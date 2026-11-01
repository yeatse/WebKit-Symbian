"""Only revise panic/probe predicates; original files and verdict stay intact.

public_probe is mask_review.validate_probe_evidence's independently checked
summary. No raw log, interaction, or screenshot flag is rewritten.
"""
from pathlib import Path
import importlib.util,json,re
from parent_probe import validate_closure
ROOT=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('unchanged_auth_validator',ROOT/'upstream/validate.py');original=importlib.util.module_from_spec(spec);spec.loader.exec_module(original)
PANIC_FAILURE='guest panic or host crash marker is present'
SHOT_FAILURE='interaction must explicitly avoid secret screenshots'
HARD=re.compile(r'KERN-EXEC|USER-EXEC|access violation|SIGSEGV|segmentation fault',re.I)
PANIC=re.compile(r'\bPANIC\b|panicked',re.I)
DOMINO=re.compile(r'\bThread\s+(?P<thread>\S+)\s+panicked\s+with\s+category:\s*Domino\s+and\s+exit\s+code:\s*13\s*$',re.I)
DESTRUCTOR=re.compile(r'^[0-2][0-9]:[0-5][0-9]:[0-5][0-9]\.[0-9]{3} WebKit Browser exiting$',re.M)
def expected_probe_count(case):return 3 if case=='retry' else 1 if case=='success' or case.endswith('password') else 0

def probe_contract(drive,case,summary):
    failed=[];incomplete=[];expected=expected_probe_count(case);shots=drive.get('screenshots_taken');proofs=drive.get('password_mask_reviews',[]);headings=drive.get('heading_reviews',[])
    if type(shots)is not bool:failed.append('actual screenshots_taken boolean missing')
    if drive.get('credential_stage_screenshots')is not False:failed.append('credential-stage screenshots not explicitly prohibited')
    if not isinstance(proofs,list):failed.append('password mask proof list invalid');proofs=[]
    if shots is True and drive.get('public_probe_only_screenshots')is not True:failed.append('screenshots are not confined to public probes')
    if shots is False and (proofs or headings):failed.append('public screenshots hidden by screenshots_taken=false')
    if shots is True and not (proofs or headings):failed.append('public screenshots lack probe evidence')
    if expected==0 and proofs:failed.append('username-only case must not capture password probes')
    if not isinstance(headings,list) or len(headings)!=1:incomplete.append('current native heading review missing')
    if len(proofs)!=expected:incomplete.append('expected public password probes incomplete')
    if summary is None:
        if expected or shots is True:incomplete.append('public probe evidence and review not validated')
        summary={'status':'INCOMPLETE' if incomplete else 'PASS','failures':[],'incomplete':[],'expected':expected,'approved':0}
    elif not isinstance(summary,dict) or set(summary)!={'status','failures','incomplete','expected','approved'}:
        failed.append('public probe result has unknown or missing fields');summary={'approved':0}
    else:
        valid=(summary['status'] in ('PASS','FAIL','INCOMPLETE') and type(summary['expected'])is int and summary['expected']==expected and type(summary['approved'])is int and 0<=summary['approved']<=expected and isinstance(summary['failures'],list) and all(isinstance(x,str) for x in summary['failures']) and isinstance(summary['incomplete'],list) and all(isinstance(x,str) for x in summary['incomplete']))
        if not valid:failed.append('public probe result typed fields invalid')
        else:
            failed+=summary['failures'];incomplete+=summary['incomplete']
            if summary['status']=='FAIL':failed.append('public probe validation failed')
            elif summary['status']=='INCOMPLETE':incomplete.append('public probe review incomplete')
            elif summary['failures'] or summary['incomplete'] or summary['approved']!=expected:failed.append('public probe PASS does not cover all reviews')
    return {'status':'FAIL' if failed else 'INCOMPLETE' if incomplete else 'PASS','failures':failed,'incomplete':incomplete,'expected':expected,'approved':summary.get('approved',0)}

def helper_shutdown(helper,guest):
    if not helper or not Path(helper).is_file():return False
    text=Path(helper).read_text(errors='replace')
    targets=re.findall(r'^TARGET: uid=e1a5c002 pid=[1-9][0-9]* name=[A-Za-z]:\\sys\\bin\\browser\.exe$',text,re.M|re.I)
    exits=re.findall(r'^EXIT: type=(-?\d+) reason=(-?\d+) logon=(-?\d+)$',text,re.M)
    return (text.count('BEGIN: browser-exit-request v1')==1 and len(targets)==1 and text.count('REQUEST: TApaTask::EndTask; no KillTask, Terminate, or host WM close')==1 and text.count('REQUEST_SENT: official application shutdown event')==1 and 'FAIL:' not in text and (not exits or exits==[('0','0','0')]) and len(DESTRUCTOR.findall(guest))==1)

def shutdown_gate(case,report,observer,helper,guest,functional_complete):
    if not functional_complete or not isinstance(observer,dict):return False
    if (type(report.get('host_exit_code'))is not int or report['host_exit_code']!=0 or report.get('forced_cleanup')is not False or report.get('normal_close')is not True or type(observer.get('host_exit_code'))is not int or observer['host_exit_code']!=0 or observer.get('forced_cleanup')is not False or observer.get('input_exit')!=0):return False
    if case.startswith('exit'):
        return report.get('wm_close_sent')is False and observer.get('wm_close_sent')is False and helper_shutdown(helper,guest)
    record=observer.get('wm_close_record')
    return (report.get('wm_close_sent')is True and observer.get('wm_close_sent')is True and type(observer.get('host_pid'))is int and observer['host_pid']>0 and isinstance(record,dict) and set(record)=={'pid','after_driver_complete','sent'} and type(record['pid'])is int and record['pid']==observer['host_pid'] and record['after_driver_complete']is True and record['sent']is True)

def classify_panics(guest,host,*,shutdown_qualified=False):
    failed=[];permitted=[]
    for source,text in [('guest',guest),('host',host)]:
        for number,line in enumerate(text.splitlines(),1):
            if HARD.search(line):failed.append({'source':source,'line':number,'reason':'host crash or USER/KERN marker'});continue
            if not PANIC.search(line):continue
            match=DOMINO.search(line) if source=='host' else None
            allowed=(match and not match.group('thread').casefold().startswith('webkit') and not PANIC.search(line[:match.start()]) and shutdown_qualified)
            if allowed:permitted.append({'source':source,'line':number,'thread':match.group('thread'),'category':'Domino','exit_code':13})
            else:failed.append({'source':source,'line':number,'reason':'target, unknown, or unqualified panic'})
    return {'status':'FAIL' if failed else 'PASS','failures':failed,'permitted_background_shutdown':permitted,'shutdown_qualified':shutdown_qualified}

def validate(fixture,interaction,guest=None,host=None,report=None,helper=None,*,observer=None,public_probe=None):
    fixture=Path(fixture);interaction=Path(interaction)
    old=original.validate(fixture,interaction,guest,host,report,helper)
    drive=json.loads(interaction.read_text());observation=json.loads(Path(report).read_text()) if report else {}
    guest_text=Path(guest).read_text(errors='replace') if guest else '';host_text=Path(host).read_text(errors='replace') if host else ''
    probe=probe_contract(drive,old['case'],public_probe)
    closure_failures=validate_closure(drive,Path(guest) if guest else None)
    # Only these two exact original predicates are replaced. Every other
    # HTTP, privacy, retry, lifecycle, identity and UI condition is retained.
    failed=[x for x in old['failures'] if x!=PANIC_FAILURE and not(x==SHOT_FAILURE and probe['status']!='FAIL')]
    failed+=closure_failures
    qualified=shutdown_gate(old['case'],observation,observer,helper,guest_text,not failed and not probe['failures'])
    panic=classify_panics(guest_text,host_text,shutdown_qualified=qualified)
    failed+=probe['failures']
    if panic['status']=='FAIL':failed.append('target/unknown panic or unqualified background shutdown in original logs')
    incomplete=list(old['incomplete'])+probe['incomplete']
    result=dict(old,status='FAIL' if failed else 'INCOMPLETE' if incomplete else 'PASS',failures=failed,incomplete=incomplete,functional_checks='FAIL' if failed else 'PASS')
    result.update(validator_revision='precise-panic-public-heading-mask-v3',original_validator_status=old['status'],original_failures=list(old['failures']),panic_classification=panic,public_probe_validation=probe)
    return result
