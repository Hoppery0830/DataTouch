import sys
import unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from data_touch.capture_service import CaptureService

TOKEN=b"12345678"
class Link:
    def __init__(self, respond=2, reject=0):
        self.now=0; self.sent=[]; self.frames=[]; self.respond=respond; self.reject=reject
    def _ticks_ms(self): return self.now
    def _ticks_diff(self,a,b): return a-b
    def _sleep_ms(self,n): self.now+=n
    def send_frame(self,kind,payload):
        self.sent.append((kind,payload))
        # An unrelated/stale ACK cannot confirm the current result.
        self.frames.append((0x34,b"oldtoken"+b"\0"))
        if len(self.sent)==self.respond:
            self.frames.append((0x34,TOKEN+bytes([self.reject])))
    def poll_frame(self): return self.frames.pop(0) if self.frames else None
class ResultTests(unittest.TestCase):
    def test_lost_ack_repeats_same_payload_without_recapture(self):
        link=Link();s=CaptureService(link,object())
        self.assertTrue(s.send_result(TOKEN,0.5))
        self.assertEqual(len(link.sent),2)
        self.assertEqual(link.sent[0],link.sent[1])
        self.assertEqual(link.sent[0][0],0x33)
    def test_bounded_timeout(self):
        link=Link(respond=99)
        self.assertFalse(CaptureService(link,object()).send_result(TOKEN,0.5))
        self.assertEqual(len(link.sent),5)
        self.assertEqual(link.now,2500)
    def test_rejection_is_terminal(self):
        link=Link(respond=1,reject=1)
        self.assertFalse(CaptureService(link,object()).send_result(TOKEN,0.5))
        self.assertEqual(len(link.sent),1)
    def test_busy_retries(self):
        link=Link(respond=1,reject=2)
        self.assertFalse(CaptureService(link,object()).send_result(TOKEN,0.5))
        self.assertEqual(len(link.sent),5)

    def test_new_capture_request_survives_delayed_result_ack(self):
        link=Link(respond=2)
        next_token=b"nexttest"
        link.frames.append((0x30,next_token))
        service=CaptureService(link,object())
        self.assertTrue(service.send_result(TOKEN,0.5))
        self.assertEqual(service.pending_request,(0x30,next_token))
