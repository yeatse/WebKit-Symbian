#!/usr/bin/env python3
"""Read-only, host-only public archive verification; no browser/server/input/audio."""
from pathlib import Path
import hashlib,json,math,subprocess,sys,tempfile
ROOT=Path(__file__).resolve().parent
def read(rel):return json.loads((ROOT/rel).read_text())
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def verify():
    checks={}
    def check(name,value):
        checks[name]=bool(value)
        if not value:raise ValueError(name)
    if (ROOT/'SHA256SUMS').exists():
        for line in (ROOT/'SHA256SUMS').read_text().splitlines():
            expected,rel=line.split('  ',1);check('sha256:'+rel,digest(ROOT/rel)==expected)
    source=read('copy-provenance.json')
    for rel,pin in source.items():
        p=ROOT/rel;check('copied:'+rel,p.stat().st_size==pin['bytes'] and digest(p)==pin['sha256'])
    state=read('public/state.json');run=state['run'];accept=read('acceptance.json')
    check('combined_acceptance_scope',accept['status']=='PASS' and accept['run']==run and accept['timing_alignment_verified'] is False)
    validations={}
    for rel,validator,expected in [('public/state.json','harness/fixture/validate.py','PUBLIC_API_COMPLETE'),
      ('prior-failures/c26/state.json','harness/fixture/validate.py','FAIL'),
      ('prior-failures/c27/state.json','harness/fixture/validate.py','FAIL'),
      ('offline-extra/public/state.json','offline-extra/harness/fixture/validate.py','OFFLINE_EXTRA_API_COMPLETE')]:
        token=read(rel)['run']
        p=subprocess.run([sys.executable,'-B',str(ROOT/validator),'--state',str(ROOT/rel),'--run',token],capture_output=True,text=True,check=False)
        v=json.loads(p.stdout);check('strict_validator:'+rel,v['status']==expected and v['native_audio_verified'] is False)
        validations[rel]=v
    summary=read('run-summary.json')['booleans']
    check('outer_run_gates',all(summary[k] is True for k in ['host_exit_zero','wm_close_sent','wm_close_after_functional','owned_host_stopped_before_restore','inputs_unchanged','guest_profile_and_resources_restored','configuration_bindings_Qt_devices_restored','restoration_errors_empty']) and all(summary[k] is False for k in ['forced_cleanup','deadline_expired','private_data_archived','native_audio_verified']))
    extra_summary=read('offline-extra/run-summary.json')
    extra_gates=extra_summary['booleans']
    check('extra_outer_run_gates',extra_summary['status']=='OFFLINE_EXTRA_RUN_COMPLETE' and all(extra_gates[k] is True for k in ['host_exit_zero','wm_close_sent','wm_close_after_functional','owned_host_stopped_before_restore','inputs_unchanged','guest_profile_and_resources_restored','configuration_bindings_Qt_devices_restored','restoration_errors_empty']) and all(extra_gates[k] is False for k in ['forced_cleanup','deadline_expired','private_data_archived','native_audio_verified']))
    if (ROOT/'offline-extra/independent-review.json').exists():
        extra=read('offline-extra/independent-review.json')
        check('extra_independent_formula_record',extra['status']=='PASS_SCOPED_PUBLIC_OFFLINE_EXTRA_NUMERIC_REVIEW' and extra['failures']==[] and len(extra['checks'])==17 and all(c['pass'] is True for c in extra['checks']) and extra['complete_render_samples_compared']==1536)
        with tempfile.TemporaryDirectory(prefix='webaudio-public-math-') as temp:
            output=Path(temp)/'review.json'
            p=subprocess.run([sys.executable,'-B',str(ROOT/'offline-extra/verify_offline_extra.py'),'--state',str(ROOT/'offline-extra/public/state.json'),'--fixture',str(ROOT/'offline-extra/harness/fixture'),'--output',str(output)],capture_output=True,text=True,check=True)
            repeated=json.loads(output.read_text())
        check('extra_independent_formula_repeated',repeated['status']==extra['status'] and repeated['failures']==[] and all(c['pass'] is True for c in repeated['checks']))
    for case in [26,27]:
        s=read(f'prior-failures/c{case}/summary.json')
        check(f'c{case}_remains_fail',s['status']=='FAIL' and s['booleans']['forced_cleanup'] is True)
    actions=[json.loads(line) for line in (ROOT/'public/trusted-input.jsonl').read_text().splitlines()]
    cal=read('public/calibration.json');marks={m['label']:m['observed_monotonic'] for m in read('pcm/marks.json')['marks']}
    check('four_real_actions',len(actions)==4 and [x['step'] for x in actions]==['start','suspend','resume','close'])
    check('visual_calibration',cal['visually_reviewed'] is True and cal['run']==run and cal['css_viewport']==[360,532] and cal['image']=='native-ready.png')
    for a,event in zip(actions,[state['events'][i] for i in [9,11,12,13]]):
        rect=cal['content_rectangle'];button=a['layout']['button']
        point=[round(rect[0]+button['x']+button['width']/2),round(rect[1]+button['y']+button['height']/2)]
        check('trusted_input:'+a['step'],a['run']==run and a['pid']==actions[0]['pid'] and a['window']==actions[0]['window'] and a['geometry']==cal['window_geometry'] and a['point']==point==[450,223] and a['method']=='real XTest pointer press/release' and a['before_monotonic']<a['after_monotonic']==marks['click-'+a['step']] and event['trusted'] is True)
    check('single_public_png',len(list(ROOT.rglob('*.png')))==1 and (ROOT/'public/native-ready.png').is_file())
    analysis=read('pcm/analysis.json');review=read('pcm/independent-review.json')
    check('pcm_scope_and_timing',analysis['status']=='PCM_OBSERVATIONS_COMPLETE' and analysis['whole_run_verified'] is False and analysis['timing_alignment_verified'] is False and len(review['checks'])==52 and all(review['checks'].values()))
    raw=read('pcm/raw-reference.json')
    check('raw_reference_only',raw['copied'] is False and raw['bytes']==15087076 and raw['sha256']=='4943624d59da7983669041fe8b1546e6045c2ab1b29b246551bf63c611292d99' and not list(ROOT.rglob('*.raw')))
    check('no_private_or_binary_payloads',not any(p.name in ['result.json','config.yml','devices.yml','items.json','inventory.json'] or p.suffix in ['.exe','.elf','.dll','.log','.pyc'] or '__pycache__' in p.parts for p in ROOT.rglob('*') if p.is_file()))
    return {'status':'PASS_SCOPED_PUBLIC_ARCHIVE_REVIEW','scope':'Host-only identities, strict stored public reports, input/calibration coherence, sanitized gates and PCM metadata review; no runtime or independent spectral recomputation.','checks':checks,'strict_validations':validations}
if __name__=='__main__':
    print(json.dumps(verify(),indent=2))
