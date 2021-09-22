import os
import subprocess
import json

FILE_NAME = "youtube_parser.py"

URL = "https://www.youtube.com/playlist?list=PLlJZn_WcZ1FJhllZhSUSoZUurIr04i-TL"

URL_PAGE = "https://www.youtube.com/playlist?list=PLlJZn_WcZ1FJhllZhSUSoZUurIr04i-TL"

BAD_URL = "https://www.youtube.com/watch?v=01skyPMeeoc&list=PL8D03A4C0FFBBE36"


def playlist_cmd(url: str) -> str:
    res = subprocess.Popen(["python3", FILE_NAME, "playlist", url], stdout=subprocess.PIPE)

    res.wait()
    return res.communicate()[0], res.returncode

print(playlist_cmd("https://www.youtube.com/watch?v=01skyPMeeoc&list=PL84D03A4C0FFBBE36"))

class TestPlaylistE2E:
    class TestVideoList:
        def test_return_json(self):
            raw, _ = playlist_cmd(URL)

            json.loads(raw)

        def test_error_code(self):
            _, code = playlist_cmd(BAD_URL)

            assert code == 1

        def test_error_output(self):
            raw, _ = playlist_cmd(BAD_URL)

            assert raw == b''

        def test_json_list(self):
            raw, _ = playlist_cmd(URL)

            json_list = json.loads(raw)

            assert len(json_list) > 0    

    
        def test_json_contents(self):
            raw, _ = playlist_cmd(URL)

            json_list = json.loads(raw)

            for vid_json in json_list:
                assert len(vid_json.keys()) == 4

        def test_json_timestamp(self):
            raw, _ = playlist_cmd(URL)

            json_list = json.loads(raw)

            for vid_json in json_list:
                assert vid_json['length'] > 0

    class TestPage:
        def test_json_list_page(self):
            raw, _ = playlist_cmd(URL_PAGE)

            json_list = json.loads(raw)

            assert len(json_list) > 0

        
        def test_json_list(self):
            raw, _ = playlist_cmd(URL_PAGE)

            json_list = json.loads(raw)

            assert len(json_list) > 0    

    
        def test_json_contents(self):
            raw, _ = playlist_cmd(URL_PAGE)

            json_list = json.loads(raw)

            for vid_json in json_list:
                assert len(vid_json.keys()) == 4

        def test_json_timestamp(self):
            raw, _ = playlist_cmd(URL_PAGE)

            json_list = json.loads(raw)

            for vid_json in json_list:
                assert vid_json['length'] > 0