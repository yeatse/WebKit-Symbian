#!/usr/bin/env python3
"""Invented PCM/JSON only; never connects to Pulse or a guest."""
import copy
import hashlib
import unittest
import numpy as np
from analyze import analyze, classify, segment_metrics, RATE

def invented():
    t = np.arange(12 * RATE) / RATE
    pcm = np.zeros((len(t), 2), dtype='<i2')
    active = ((t >= .7) & (t < 3.3)) | ((t >= 5.7) & (t < 8.3))
    tone = np.rint(4000 * np.sin(2 * np.pi * 997 * t)).astype('<i2')
    pcm[active] = tone[active, None]
    raw = pcm.tobytes()
    capture = dict(suite='classic-webaudio-pcm', version=1, status='CAPTURE_STOPPED_REQUIRES_ANALYSIS',
        execute_requested=True, stop_reason='root_signal', forced_cleanup=False, deadline_expired=False,
        pacat_returncode=0, received_stop_signal=15, server='unix:/workspace/symbian-media-work/runtime/native',
        source='symbian_media.monitor', format='s16le', sample_rate=RATE, channels=2, bytes=len(raw),
        sha256=hashlib.sha256(raw).hexdigest(), estimated_origin_range_monotonic=[1000, 1000.02], stopped_monotonic=1011)
    labels=['click-start','native-running','click-suspend','native-suspended','click-resume','native-resumed','click-close','quiet']
    marks=dict(suite='classic-webaudio-pcm', version=1, run='wa-'+'a'*32,
        marks=[dict(label=k,observed_monotonic=1000+t)for k,t in zip(labels,[.5,2,3.3,4.3,5.6,7,8.3,9.6])])
    return raw,capture,marks

class Guards(unittest.TestCase):
    def test_good(self):
        report=analyze(*invented())
        self.assertEqual(report['status'],'PCM_OBSERVATIONS_COMPLETE')
        self.assertFalse(report['whole_run_verified'])
        self.assertFalse(report['timing_alignment_verified'])
    def test_capture_rejects(self):
        raw,capture,marks=invented()
        bad=[('status','PREPARED_NOT_CAPTURED'),('execute_requested',False),('stop_reason','capture_deadline'),
             ('forced_cleanup',True),('deadline_expired',True),('pacat_returncode',-9),('received_stop_signal',None),
             ('server','default'),('source','microphone'),('format','float32le'),('sample_rate',44100),('channels',1),
             ('bytes',len(raw)-4),('sha256','0'*64),('estimated_origin_range_monotonic',[1000,1000.2]),
             ('estimated_origin_range_monotonic',[1000.02,1000]),('estimated_origin_range_monotonic',[float('nan'),1000]),
             ('stopped_monotonic',1010),('stopped_monotonic',float('inf'))]
        for key,value in bad:
            with self.subTest(key=key,value=value):
                changed=copy.deepcopy(capture);changed[key]=value
                with self.assertRaises(ValueError):analyze(raw,changed,marks)
    def test_marks_rejects(self):
        raw,capture,marks=invented()
        bad=[]
        value=copy.deepcopy(marks);value['run']='unknown';bad.append(value)
        value=copy.deepcopy(marks);value['marks'].pop();bad.append(value)
        value=copy.deepcopy(marks);value['marks'][1]['label']='quiet';bad.append(value)
        for index,time in [(1,float('nan')),(2,1002.5),(1,999),(7,1012)]:
            value=copy.deepcopy(marks);value['marks'][index]['observed_monotonic']=time;bad.append(value)
        for value in bad:
            with self.subTest(value=value):
                with self.assertRaises(ValueError):analyze(raw,capture,value)
    def test_pcm_identity_rejects(self):
        raw,capture,marks=invented()
        for changed in [b'',raw[:-1],raw[:-4],b'\x01'+raw[1:]]:
            with self.subTest(length=len(changed)):
                with self.assertRaises(ValueError):analyze(changed,capture,marks)
    def test_spectrum(self):
        t=np.arange(RATE//2)/RATE
        def tone(f,a):
            mono=np.rint(a*np.sin(2*np.pi*f*t));return np.column_stack((mono,mono))
        self.assertTrue(classify(tone(997,4000),True)['observation_passed'])
        bad=[tone(440,4000),tone(997,50),tone(997,32767),np.zeros((RATE//2,2))]
        x=tone(997,4000);x[:,1]=0;bad.append(x)
        x=tone(997,4000);x[1000:1100,0]=0;bad.append(x)
        bad.append(tone(997,4000)+tone(1603,4000))
        for x in bad:self.assertFalse(classify(x,True)['observation_passed'])
    def test_quiet_nonfinite(self):
        zero=np.zeros((RATE//2,2))
        self.assertTrue(classify(zero,False)['observation_passed'])
        self.assertTrue(classify(zero+2,False)['observation_passed'])
        self.assertFalse(classify(zero+3,False)['observation_passed'])
        for invalid in [float('nan'),float('inf'),-float('inf')]:
            x=zero.copy();x[0,0]=invalid
            with self.assertRaises(ValueError):segment_metrics(x)

if __name__=='__main__':unittest.main(verbosity=2)
