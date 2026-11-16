"""One photo per F4 request. Retries repeat DONE, never camera acquisition."""

CAPTURE_REQUEST = 0x30
CAPTURE_DONE = 0x31
CAPTURE_RELEASE = 0x32


## @brief 执行 F4 请求的单次采图，用令牌缓存结果以避免重复拍照。
class CaptureService:
    def __init__(self, link, camera):
        self.link = link
        self.camera = camera
        self.completed = []
        self.pending_request = None

    def _notify(self, token, status):
        self.link.send_frame(CAPTURE_DONE, token + bytes((status,)))

    ## @brief 响应一条采图请求，并等待 F4 确认关灯。
    # @return 放行时返回 (令牌, 独立的灰度图)；无请求、采图失败或未放行时返回 None。
    def poll(self):
        """Return (token, copied_image) only after F4 confirms lamp OFF.

        F4 owns stationary checks, lighting and exposure delay. A missing or failed
        request is not retried by F4; notifications may be retransmitted.
        """
        frame = self.pending_request or self.link.poll_frame()
        self.pending_request = None
        if frame is None:
            return None
        kind, token = frame
        if kind != CAPTURE_REQUEST or len(token) != 8:
            return None
        token = bytes(token)
        print("K230_CAPTURE_REQUEST token=%s" % token.hex())
        for previous, status in self.completed:
            if previous == token:
                self._notify(token, status)
                return None
        # Reserve before entering camera code. Failures also consume the request.
        self.completed.append((token, 1))
        self.completed = self.completed[-16:]
        gray = None
        status = 1
        try:
            print("K230_CAPTURE_BEGIN")
            gray = self.camera.capture_gray()
            if gray is None:
                raise RuntimeError("empty acquisition")
            status = 0
            print("K230_CAPTURE_DONE status=0")
        except Exception as error:
            if "IDE interrupt" in str(error):
                raise
            print("K230_CAPTURE_FAILED error=%s" % error)
        self.completed[-1] = (token, status)
        started = self.link._ticks_ms()
        last_send = None
        while self.link._ticks_diff(self.link._ticks_ms(), started) < 3000:
            now = self.link._ticks_ms()
            if last_send is None or self.link._ticks_diff(now, last_send) >= 200:
                self._notify(token, status)
                last_send = now
            reply = self.link.poll_frame()
            if reply is not None:
                kind, payload = reply
                if kind == CAPTURE_RELEASE and len(payload) == 9 and payload[:8] == token:
                    print("K230_CAPTURE_RELEASE status=%d" % payload[8])
                    if status == 0 and payload[8] == 0:
                        return token, gray
                    return None
                # Duplicate requests never invoke capture_gray a second time.
            self.link._sleep_ms(1)
        print("K230_CAPTURE_RELEASE_TIMEOUT")
        return None


    ## @brief 同一实验指标最多发送五次，收到匹配 F4 确认才成功。
    def send_result(self, token, value):
        from stm32_link import pack_u_curve
        payload = bytes(token) + pack_u_curve(value)
        if len(token) != 8:
            raise ValueError("invalid experiment token")
        for attempt in range(5):
            self.link.send_frame(0x33, payload)
            started = self.link._ticks_ms()
            while self.link._ticks_diff(self.link._ticks_ms(), started) < 500:
                frame = self.link.poll_frame()
                if frame is not None:
                    kind, data = frame
                    if kind == 0x34 and len(data) == 9 and data[:8] == token:
                        if data[8] == 0:
                            return True
                        if data[8] != 2:
                            return False
                        break
                    # Preserve a new request while a previous result ACK is delayed.
                    if kind == CAPTURE_REQUEST and len(data) == 8 and data != token:
                        if self.pending_request is None:
                            self.pending_request = (kind, bytes(data))
                    # Replayed acquisition requests never cause another exposure.
                self.link._sleep_ms(1)
        return False
