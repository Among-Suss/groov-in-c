import os
import subprocess
import json
import time

FILE_NAME = os.path.join(os.path.dirname(__file__), "youtube_parser.py")

URL = "https://www.youtube.com/playlist?list=PLlJZn_WcZ1FJhllZhSUSoZUurIr04i-TL"
URL_PAGE = "https://www.youtube.com/playlist?list=PLlJZn_WcZ1FJhllZhSUSoZUurIr04i-TL"
BAD_URL = "https://www.youtube.com/watch?v=01skyPMeeoc&list=PL8D03A4C0FFBBE36"

def playlist_cmd(url: str, retries: int=10) -> str:
    res = subprocess.Popen(["python3", FILE_NAME, "playlist", "--retries", str(retries), url], stdout=subprocess.PIPE)

    res.wait()
    text = res.communicate()[0]

    return text, res.returncode



class TestPlaylistE2E:
    class TestVideoList:
        def test_return_json(self):
            raw, code = playlist_cmd(URL)

            if code == 1:
                raise Exception("Exit code 1")

            json.loads(raw)

        def test_error_code(self):
            _, code = playlist_cmd(BAD_URL, 1)

            assert code == 1

        def test_error_output(self):
            raw, _ = playlist_cmd(BAD_URL, 1)

            assert raw == b''

        def test_json_list(self):
            raw, code = playlist_cmd(URL)

            if code == 1:
                raise Exception("Exit code 1")

            json_list = json.loads(raw)

            assert len(json_list) > 0    

    
        def test_json_contents(self):
            raw, code = playlist_cmd(URL)

            if code == 1:
                raise Exception("Exit code 1")

            json_list = json.loads(raw)

            for vid_json in json_list:
                assert len(vid_json.keys()) == 4

        def test_json_timestamp(self):
            raw, code = playlist_cmd(URL)

            if code == 1:
                raise Exception("Exit code 1")

            json_list = json.loads(raw)

            for vid_json in json_list:
                assert vid_json['length'] > 0

    class TestPage:
        def test_json_list_page(self):
            raw, code = playlist_cmd(URL_PAGE)

            if code == 1:
                raise Exception("Exit code 1")

            json_list = json.loads(raw)

            assert len(json_list) > 0

        
        def test_json_list(self):
            raw, code = playlist_cmd(URL_PAGE)

            if code == 1:
                raise Exception("Exit code 1")

            json_list = json.loads(raw)

            assert len(json_list) > 0    

    
        def test_json_contents(self):
            raw, code = playlist_cmd(URL_PAGE)

            if code == 1:
                raise Exception("Exit code 1")

            json_list = json.loads(raw)

            for vid_json in json_list:
                assert len(vid_json.keys()) == 4

        def test_json_timestamp(self):
            raw, code = playlist_cmd(URL_PAGE)

            if code == 1:
                raise Exception("Exit code 1")

            json_list = json.loads(raw)

            for vid_json in json_list:
                assert vid_json['length'] > 0
