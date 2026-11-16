import sys
import unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from data_touch.capture_service import CaptureService, CAPTURE_REQUEST, CAPTURE_DONE, CAPTURE_RELEASE

TOKEN=bytes(range(8))
class Link:
    def __init__(self,release=True):
        self.queue=[(CAPTURE_REQUEST,TOKEN)];self.sent=[];self.now=0;self.release=release
    def poll_frame(self):return self.queue.pop(0) if self.queue else None
    def send_frame(self,kind,payload):
        self.sent.append((kind,payload))
        if self.release and kind==CAPTURE_DONE:self.queue.append((CAPTURE_RELEASE,payload))
    def _ticks_ms(self):return self.now
    def _ticks_diff(self,a,b):return a-b
    def _sleep_ms(self,n):self.now+=n
class Camera:
    def __init__(self,fail=False):self.calls=0;self.fail=fail
    def capture_gray(self):
        self.calls+=1
        if self.fail:raise RuntimeError('camera disconnected')
        return 'copied image'
class CaptureTests(unittest.TestCase):
    def test_once_and_duplicate(self):
        link=Link();camera=Camera();s=CaptureService(link,camera)
        self.assertEqual(s.poll(),(TOKEN,'copied image'))
        link.queue.append((CAPTURE_REQUEST,TOKEN));self.assertIsNone(s.poll())
        self.assertEqual(camera.calls,1);self.assertEqual(link.sent[-1],(CAPTURE_DONE,TOKEN+b'\0'))
    def test_failure_consumes_request(self):
        link=Link();camera=Camera(True);s=CaptureService(link,camera)
        self.assertIsNone(s.poll());self.assertEqual(link.sent[-1][1][-1],1)
        link.queue.append((CAPTURE_REQUEST,TOKEN));self.assertIsNone(s.poll());self.assertEqual(camera.calls,1)
    def test_lost_release_retries_only_notification(self):
        link=Link(False);camera=Camera();s=CaptureService(link,camera)
        self.assertIsNone(s.poll());self.assertEqual(camera.calls,1)
        self.assertEqual(len(link.sent),15);self.assertTrue(all(k==CAPTURE_DONE for k,p in link.sent))
    def test_wrong_token_and_duplicate_while_waiting(self):
        link=Link(False);camera=Camera();s=CaptureService(link,camera)
        link.queue.extend([(CAPTURE_RELEASE,b'x'*8+b'\0'),(CAPTURE_REQUEST,TOKEN),
                           (CAPTURE_RELEASE,TOKEN+b'\0')])
        self.assertEqual(s.poll(),(TOKEN,'copied image'));self.assertEqual(camera.calls,1)
    def test_rejected_release_does_not_evaluate(self):
        link=Link(False);camera=Camera();s=CaptureService(link,camera)
        link.queue.append((CAPTURE_RELEASE,TOKEN+b'\1'))
        self.assertIsNone(s.poll());self.assertEqual(camera.calls,1)
    def test_empty_image_is_failure(self):
        link=Link();camera=Camera();camera.capture_gray=lambda:None
        self.assertIsNone(CaptureService(link,camera).poll());self.assertEqual(link.sent[-1][1][-1],1)
    def test_non_capture_frames_do_not_take_photo(self):
        link=Link();link.queue=[(0x80,b'\0\0'),(CAPTURE_REQUEST,b'bad')]
        camera=Camera();s=CaptureService(link,camera)
        self.assertIsNone(s.poll());self.assertIsNone(s.poll());self.assertEqual(camera.calls,0)
