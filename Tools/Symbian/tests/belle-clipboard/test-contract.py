#!/usr/bin/env python3
"""Host-only negative report and actual HTTP preservation checks."""
import copy
import http.client
import json
from pathlib import Path
import tempfile
import threading
import unittest
from contract import CASES, validate, validate_shape
from server import Handler, ThreadingHTTPServer


def report(origin='http://localhost:8802'):
    rows=[]
    for index,name in enumerate(CASES):
        events=[] if index==3 else [
            {'type':'paste','trusted':True,'plainTextMatches':True,'files':0,'types':['text/plain']},
            {'type':'input','trusted':True,'inputType':'insertFromPaste'},
            {'type':'keydown','trusted':True,'key':'v'}]
        rows.append({'name':name,'status':'PASS','elapsed_ms':100,'trustedClick':True,
                     'events':events,'observations':{'domPasteDenied':True} if index==3 else {}})
    return {'suite':'belle-clipboard','run':'host_case','mode':'keyboard','origin':origin,
            'status':'PASS','passed':9,'failed':0,'total':9,'elapsed_ms':1000,'cases':rows}


class Contract(unittest.TestCase):
    def test_complete_contract(self):
        validate(report(),'host_case','keyboard','http://localhost:8802')

    def test_identity_and_case_completeness(self):
        for transform in (
            lambda r:r.update(run='../bad'), lambda r:r.update(mode='script'),
            lambda r:r['cases'].pop(), lambda r:r['cases'].__setitem__(1,copy.deepcopy(r['cases'][0])),
            lambda r:r.update(passed=True),lambda r:r.update(elapsed_ms=float('nan'))):
            row=report();transform(row)
            with self.assertRaises(ValueError):validate_shape(row)
        with self.assertRaises(ValueError):validate(report(),'old_run','keyboard','http://localhost:8802')

    def test_positive_needs_trusted_native_paste_and_input(self):
        for transform in (
            lambda r:r['cases'][0].update(trustedClick=False),
            lambda r:r['cases'][0]['events'][0].update(trusted=False),
            lambda r:r['cases'][0]['events'][0].update(plainTextMatches=False),
            lambda r:r['cases'][0]['events'][0].update(types=['text/html']),
            lambda r:r['cases'][0]['events'][0].update(types=['text/plain-fake']),
            lambda r:r['cases'][0]['events'][1].update(inputType='insertText'),
            lambda r:r['cases'][0]['events'].pop()):
            row=report();transform(row)
            with self.assertRaises(ValueError):validate(row,'host_case','keyboard','http://localhost:8802')

    def test_dom_paste_denial_is_separate(self):
        row=report();row['cases'][3]['events']=[{'type':'paste','trusted':True}]
        with self.assertRaises(ValueError):validate(row,'host_case','keyboard','http://localhost:8802')
        row=report();row['cases'][3]['observations']={}
        with self.assertRaises(ValueError):validate(row,'host_case','keyboard','http://localhost:8802')

    def test_failure_can_be_preserved_but_not_accepted(self):
        row=report();row['cases'][0]['status']='FAIL';row.update(status='FAIL',passed=8,failed=1)
        validate_shape(row)
        with self.assertRaises(ValueError):validate(row,'host_case','keyboard','http://localhost:8802')


class HTTP(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.root=Path(self.temp.name)
        self.server=ThreadingHTTPServer(('127.0.0.1',0),Handler);self.server.reports=self.root
        self.thread=threading.Thread(target=self.server.serve_forever,daemon=True);self.thread.start()
        self.origin='http://127.0.0.1:'+str(self.server.server_port)

    def tearDown(self):
        self.server.shutdown();self.server.server_close();self.thread.join();self.temp.cleanup()

    def request(self,method,path,body=None,origin=None):
        connection=http.client.HTTPConnection('127.0.0.1',self.server.server_port,timeout=2)
        headers={'Origin':origin or self.origin,'Content-Type':'application/json'}
        connection.request(method,path,body=body,headers=headers);response=connection.getresponse()
        result=response.status,response.read();connection.close();return result

    def test_report_is_immutable_and_preserves_failure(self):
        row=report(self.origin);body=json.dumps(row).encode()
        self.assertEqual(self.request('POST','/report',body)[0],201)
        stored=self.root/'host_case-keyboard.json';self.assertEqual(stored.read_bytes(),body+b'\n')
        row['cases'][0]['status']='FAIL';row.update(status='FAIL',passed=8,failed=1)
        self.assertEqual(self.request('POST','/report',json.dumps(row).encode())[0],409)
        self.assertEqual(stored.read_bytes(),body+b'\n')
        row['run']='failed_case'
        self.assertEqual(self.request('POST','/report',json.dumps(row).encode())[0],201)
        self.assertEqual(json.loads((self.root/'failed_case-keyboard.json').read_text())['status'],'FAIL')

    def test_origin_and_invalid_report_do_not_write(self):
        self.assertEqual(self.request('POST','/report',json.dumps(report(self.origin)).encode(),'http://other.test')[0],400)
        self.assertEqual(self.request('POST','/report',b'{broken')[0],400)
        row=report(self.origin);row['run']='../outside'
        self.assertEqual(self.request('POST','/report',json.dumps(row).encode())[0],400)
        self.assertEqual(list(self.root.iterdir()),[])

    def test_only_fixture_files_are_served(self):
        for path in ['/','/index.html','/suite.js']:
            self.assertEqual(self.request('GET',path)[0],200)
        for path in ['/../contract.py','/contract.py','/reports/host_case-keyboard.json']:
            self.assertEqual(self.request('GET',path)[0],404)


if __name__=='__main__':unittest.main()
