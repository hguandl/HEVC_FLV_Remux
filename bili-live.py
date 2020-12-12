#!/usr/bin/env python3

import json
import os
import time
from datetime import datetime

import requests

import remux

user_agent = (
    "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_6)"
    " AppleWebKit/605.1.15 (KHTML, like Gecko)"
    " Version/14.0.1 Safari/605.1.15"
) 

headers = {
    "User-Agent": user_agent,
    "Accept-Encoding": "gzip, deflate, br",
    "Accept": "text/html,application/xhtml+xml,application/xml;q=0.9",
    "Accept-Language": "zh-cn",
    "Connection": "keep-alive"
}

class BiliLiveRoom(object):
    def __init__(self, room_id: str) -> None:
        super().__init__()
        self.room_id = room_id
        self.playurl_info = None
        self.session = requests.Session()
        self.session.headers.update(headers)

    
    def _get_api_url(self, qn) -> str:
        return (
            "https://api.live.bilibili.com/xlive/web-room/v2/index"
            "/getRoomPlayInfo?"
            f"room_id={self.room_id}"
            "&protocol=0,1"
            "&format=0,2"
            "&codec=0,1"
            f"&qn={qn}"
            "&platform=web"
            "&ptype=16"
        )

    
    def _fetch_api(self, qn: int = 0) -> str:
        r = self.session.get(self._get_api_url(qn))
        api_data = json.loads(r.text)
        return api_data["data"]["playurl_info"]


    @staticmethod
    def _get_codecs(playurl_info):
        stream = playurl_info["playurl"]["stream"][0]
        format = stream["format"][0]
        return format["codec"]


    def _get_codec_qn(self) -> dict:
        codec_qn = dict()
        for codec in BiliLiveRoom._get_codecs(self.playurl_info):
            codec_name = codec["codec_name"]
            best_qn = codec["accept_qn"][0]
            codec_qn[codec_name] = best_qn

        return codec_qn

    
    def _get_stream_url(self, codec_name: str, qn: int) -> str:
        playurl_info = self._fetch_api(qn=qn)
        for codec in BiliLiveRoom._get_codecs(playurl_info):
            if codec["codec_name"] == codec_name:
                return (
                    f'{codec["url_info"][0]["host"]}'
                    f'{codec["base_url"]}'
                    f'{codec["url_info"][0]["extra"]}'
                )

    
    def update(self) -> None:
        self.playurl_info = self._fetch_api()


    def download(self, prefer_hevc: bool = True) -> int:
        codec_qn = self._get_codec_qn()
        codec_name = "avc"
        if prefer_hevc and "hevc" in codec_qn.keys():
            codec_name = "hevc"
        qn = codec_qn[codec_name]
        ret = remux.remux(
            self._get_stream_url(codec_name, qn),
            f'{datetime.now().strftime("%Y-%m-%d_%H%M%S")}'
            f'-{self.room_id}.mp4',
            f'User-Agent: {user_agent}\r\n'
        )
        self.playurl_info = None
        return ret


    def wait_for_live(self) -> None:
        self.update()
        while self.playurl_info is None:
            print(f'{datetime.now().strftime("%Y-%m-%d %H:%M:%S")}: Waiting...')
            time.sleep(60)
            self.update()

    
    def watch_forever(self) -> None:
        while True:
            self.wait_for_live()
            ret = self.download()
            if ret == -1:
                break
        

if __name__ == "__main__":
    room = BiliLiveRoom("744393") # 4588774
    room.watch_forever()
