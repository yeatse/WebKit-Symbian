#!/usr/bin/env python3
"""Host-only review of stored public vectors; never starts the fixture or audio."""
import argparse, cmath, hashlib, importlib.util, json, math, sys
from pathlib import Path
sys.dont_write_bytecode = True

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def response(kind, rate, frequency, q, hz):
    w = 2*math.pi*frequency/rate
    alpha = math.sin(w)/(2*q)
    a = [1+alpha, -2*math.cos(w), 1-alpha]
    b = ([(1-math.cos(w))/2, 1-math.cos(w), (1-math.cos(w))/2]
         if kind == 'lowpass' else [1-alpha, -2*math.cos(w), 1+alpha])
    z = cmath.exp(-2j*math.pi*hz/rate)
    return sum(b[n]*z**n for n in range(3))/sum(a[n]*z**n for n in range(3))

def main():
    p = argparse.ArgumentParser()
    p.add_argument('--state', type=Path, required=True)
    p.add_argument('--fixture', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    args = p.parse_args()
    state = json.loads(args.state.read_text())
    checks = []
    def check(name, value):
        checks.append({'name':name, 'pass':bool(value)})
    def load(name, path):
        spec = importlib.util.spec_from_file_location(name, path)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        return module
    old = sys.modules.get('protocol')
    try:
        sys.modules['protocol'] = load('protocol', args.fixture/'protocol.py')
        pure = load('_offline_extra_pure_validator', args.fixture/'validate.py')
        validated = pure.validate(state, state['run'])
    finally:
        if old is None: sys.modules.pop('protocol', None)
        else: sys.modules['protocol'] = old
    check('existing pure schema and vector validator', validated['status']=='OFFLINE_EXTRA_API_COMPLETE')
    a,b,c = state['events']
    check('exact three ordered secure reports', [e['kind'] for e in state['events']]==['swapped-delay-channel','mono-gain-delay','biquad-response'] and all(e['run']==state['run'] and e['sequence']==i and e['secure'] is True for i,e in enumerate(state['events'])))
    check('sample rate and integral delay 128', all(e['sample_rate']==32768 for e in state['events']) and 32768/256==128)
    delayed=[0.0]*512; delayed[128]=1.0; delayed[129]=-0.5
    direct=[0.0]*512; direct[3]=0.25
    check('A complete two by 512 array shape', a['channels']==2 and a['frames']==512 and len(a['samples'])==2 and all(len(x)==512 for x in a['samples']))
    check('A all 1024 samples exact expected delayed and direct channel', a['samples']==[delayed,direct])
    check('B complete one by 512 array shape', b['channels']==1 and b['frames']==512 and len(b['samples'])==1 and len(b['samples'][0])==512)
    check('B all 512 samples exact unity gain delay', b['samples']==[delayed])
    page=(args.fixture/'page.html').read_text()
    check('A fixture swapped delayed channel0 pull order graph', 'source.connect(split);split.connect(delay,0,0);delay.connect(merge,0,0);split.connect(merge,1,1);merge.connect(c.destination)' in page)
    check('B fixture unity gain then delay graph', 'gain.gain.value=1;delay.delayTime.value=1/256;source.connect(gain);gain.connect(delay);delay.connect(c.destination)' in page)
    low,ap = c['lowpass'],c['allpass']; qdb=-10*math.log10(2)
    check('C lowpass parameters', low['type']=='lowpass' and low['frequency']==8192 and abs(low['q']-qdb)<1e-6 and low['detune']==0 and low['frequencies']==[0,8192,16384])
    low_h=[response('lowpass',32768,8192,10**(qdb/20),f) for f in low['frequencies']]
    low_mag=[abs(h) for h in low_h]; low_phase=[cmath.phase(h) for h in low_h[:2]]
    check('C lowpass independent DC cutoff Nyquist formula', max(abs(x-y) for x,y in zip(low_mag,[1,math.sqrt(.5),0]))<=1e-12)
    check('C all three lowpass magnitudes independent formula', len(low['magnitude'])==3 and max(abs(x-y) for x,y in zip(low['magnitude'],low_mag))<=1e-5)
    check('C first two lowpass phases independent formula', len(low['phase'])==2 and max(abs(x-y) for x,y in zip(low['phase'],low_phase))<=1e-5)
    check('C allpass parameters', ap['type']=='allpass' and ap['frequency']==8192 and ap['q']==1 and ap['detune']==0 and ap['frequencies']==[0,4096,8192,12288,16384])
    ap_h=[response('allpass',32768,8192,1,f) for f in ap['frequencies']]
    check('C five independent allpass magnitudes unity', max(abs(abs(h)-1) for h in ap_h)<=1e-12)
    check('C all five reported allpass magnitudes independent formula', len(ap['magnitude'])==5 and max(abs(x-abs(h)) for x,h in zip(ap['magnitude'],ap_h))<=1e-5)
    check('C all five public allpass phases finite', len(ap['phase'])==5 and all(math.isfinite(x) for x in ap['phase']))
    failed=[x['name'] for x in checks if not x['pass']]
    result={'status':'FAIL' if failed else 'PASS_SCOPED_PUBLIC_OFFLINE_EXTRA_NUMERIC_REVIEW', 'run':state['run'], 'checks':checks, 'failures':failed, 'complete_render_samples_compared':1536, 'pure_validator_result':validated, 'sources':{str(path):{'bytes':path.stat().st_size,'sha256':digest(path)} for path in [args.state,args.fixture/'page.html',args.fixture/'protocol.py',args.fixture/'validate.py']}, 'independent_formula':{'lowpass_q_db':qdb,'lowpass_q_linear':10**(qdb/20),'lowpass_magnitude':low_mag,'lowpass_first_two_phases':low_phase,'allpass_magnitude':[abs(h) for h in ap_h], 'formula':'H(z)=(b0+b1*z+b2*z^2)/(a0+a1*z+a2*z^2), z=exp(-j*2*pi*f/rate), alpha=sin(w0)/(2*Qlinear); lowpass Qlinear=10^(QdB/20)', 'comparison_absolute_tolerance':1e-5}, 'limitations':['Stored browser-reported numeric observations only; this host review starts no browser, guest, fixture server, or audio.', 'Not a new PCM PASS, physical-device proof, specific SDK symbol verification, or whole-run restoration claim.', 'No lowpass Nyquist phase comparison because phase at zero magnitude is undefined; allpass phase exactness is not asserted.'], 'private_result_read':False, 'shared_source_modified':False, 'runtime_started':False}
    args.output.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({'status':result['status'],'checks':len(checks),'failures':failed,'output_sha256':digest(args.output),'script_sha256':digest(Path(__file__))}))
    return bool(failed)

if __name__=='__main__':
    raise SystemExit(main())
