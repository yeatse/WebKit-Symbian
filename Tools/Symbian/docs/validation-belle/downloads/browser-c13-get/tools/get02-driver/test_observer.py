#!/usr/bin/env python3
"""No display, guest or browser launch; verify real guards and file restoration."""
import importlib.util, tempfile, unittest, json, subprocess, argparse
from pathlib import Path
from unittest import mock

ROOT=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('download_observer',ROOT/'run-case.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)

class ObserverTest(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory(prefix='host-',dir=ROOT)
        self.addCleanup(self.temp.cleanup)
        self.base=Path(self.temp.name)
        self.args=argparse.Namespace(case='get',output=self.base/'evidence',timeout=60)
        self.o=m.Observer(self.args,{})
        self.o.out.mkdir()
        self.o.live_log=self.base/'log.txt'
        self.o.browser=mock.Mock()
        self.o.browser.poll.return_value=None
        self.o.current_capture=self.base/'capture.json'
        self.o.current_capture.write_text('{}')
        self.o.native=mock.Mock()

    def test_rejects_stale_or_unreviewed_capture_before_input(self):
        good={'capture_sha256':m.sha(self.o.current_capture),'screenshots_reviewed':True,
              'actions':[{'role':'start','target':'main','point':[100,100]}]}
        for override in ({'capture_sha256':'0'*64},{'screenshots_reviewed':False}):
            with self.assertRaises(RuntimeError):self.o.action({**good,**override})
        self.o.native.assert_not_called()

    def test_final_review_requires_terminal_and_exact_native_path(self):
        self.o.live_log.write_text('loading\n')
        value={'run':self.o.token,'sequence':1,'operation':'review-result',
               'native_ui_reviewed':True,'selected_guest_path':self.o.guest_directory+'payload.bin'}
        with self.assertRaises(RuntimeError):self.o.control(value)
        self.o.live_log.write_text('download 1: terminal=0 error=0 cleanup=0\n')
        with self.assertRaises(RuntimeError):self.o.control({**value,'selected_guest_path':'C:\\other\\payload.bin'})
        self.o.control(value)
        self.assertTrue(self.o.ui_verified)
        self.assertEqual(self.o.state['command_sequence'],1)

    def test_release_rejects_no_cancel_no_progress_or_other_id(self):
        self.o.a.case='cancel-transfer'
        self.o.ready={'host_control_url':'http://127.0.0.1:1'}
        for text in ('download 1: 8192/-1 bytes\n',
                     'download 1: terminal=1 error=0 cleanup=0\n',
                     'download 2: 8192/-1 bytes\ndownload 1: terminal=1 error=0 cleanup=0\n',
                     'download 1: terminal=1 error=0 cleanup=0\ndownload 1: 8192/-1 bytes\n'):
            self.o.live_log.write_text(text)
            with mock.patch('urllib.request.build_opener') as opener:
                with self.assertRaises(RuntimeError):self.o.release()
                opener.assert_not_called()

    def test_real_savedfiles_and_config_restore_original_inode(self):
        data=self.base/'fake-data';(data/'drives/c/Data/WebKit').mkdir(parents=True)
        original=data/'drives/c/Data/WebKit/original.txt';original.write_text('host-test-only\n')
        old=original.stat().st_ino
        support=m.module(m.SUPPORT,'host_preservation_module')
        self.o.saved=support.SavedFiles(data,'host-test',self.o.state.setdefault('restoration',{}))
        self.o.saved.reserve(data/'drives/c/Data/WebKit',directory=True)
        (data/'drives/c/Data/WebKit/test-created.txt').write_text('temporary\n')
        config=self.base/'config.yml';config.write_text('test-config-only\n')
        inode=config.stat().st_ino;before=m.sha(config)
        private=self.base/'config.private.yml';config.rename(private);config.write_text('changed\n')
        self.o.config_backup=(config,private,inode,before)
        self.o.browser.poll.return_value=0
        with mock.patch.object(m.subprocess,'run',return_value=argparse.Namespace(returncode=1)):
            self.o.restore()
        self.assertEqual(original.read_text(),'host-test-only\n')
        self.assertEqual(original.stat().st_ino,old)
        self.assertEqual(config.stat().st_ino,inode)
        self.assertEqual(m.sha(config),before)
        self.assertFalse((data/'drives/c/Data/WebKit/test-created.txt').exists())
        self.assertTrue(self.o.state['restoration']['restored'])

    def test_restore_refuses_to_replace_files_while_guest_active(self):
        self.o.browser.poll.return_value=0
        with mock.patch.object(m.subprocess,'run',return_value=argparse.Namespace(returncode=0)):
            with self.assertRaises(RuntimeError):self.o.restore()
        self.assertIn('guest still active',self.o.state['restoration_errors'][0])

    def test_invalid_browser_hash_readonly_and_no_launch(self):
        args=argparse.Namespace(output=self.base/'fresh',timeout=300,browser=self.base/'not-browser',
                               browser_sha256='0'*64,host=self.base/'not-host',host_sha256='0'*64)
        with mock.patch.object(m.subprocess,'Popen') as launch:
            with self.assertRaises(RuntimeError):m.validate_inputs(args)
            launch.assert_not_called()
        self.assertFalse(args.output.exists())

    def test_control_requires_review_before_publishing(self):
        out=self.base/'control';out.mkdir();(out/'commands').mkdir()
        (out/'state.json').write_text(json.dumps({'run':'host-only','command_sequence':0,'capture_sha256':'0'*64}))
        r=subprocess.run(['python3',str(ROOT/'control.py'),'--output',str(out),'--operation','act',
                          '--role','start','--point','100','100'],capture_output=True,text=True)
        self.assertEqual(r.returncode,2)
        self.assertEqual(list((out/'commands').iterdir()),[])

if __name__=='__main__':unittest.main(verbosity=2)
