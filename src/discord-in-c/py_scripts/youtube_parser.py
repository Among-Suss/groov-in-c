from typing import Any
import requests
import json
import sys
import os
import time
import traceback
from datetime import datetime
import argparse

RETRIES = 10

class EndRetry(Exception): pass
class AttemptRetry(Exception): pass

def get_description(text):
    """Retrieves the video description

    Args:
        text (str): Raw HTML

    Returns:
        str: description
    """

    return get_between(text, '"description":{"simpleText":"', '"},').replace("\\n", "\n")

def get_playlist_data(text, do_save=False):
    """Retrieves the video description

    Args:
        text (str): Raw HTML

    Returns:
        dict
    """

    json_string = get_between(text, 'ytInitialData = ', ';</script>')
    parsed_json: dict = json.loads(json_string)

    if do_save:
        with open('__pycache__/test.json', 'w') as fp:
            fp.write(json_string)

    playlist_mode = '/playlist' in url

    if playlist_mode:
        content_list = parsed_json['contents']['twoColumnBrowseResultsRenderer']['tabs'][0]['tabRenderer']['content']['sectionListRenderer']['contents'][0]['itemSectionRenderer']['contents'][0]['playlistVideoListRenderer']['contents']
    else:
        try:
            content_list = parsed_json['contents']['twoColumnWatchNextResults']['playlist']['playlist']['contents']
        except KeyError as e:
            raise EndRetry("Playlist key error") from e

    # Filter out missing videos
    content_list = [x for x in content_list if "unplayableText" not in x]

    dict_list = []
    
    for data in content_list:
        if 'continuationItemRenderer' in data.keys():
            continue
        video = data['playlistVideoRenderer' if playlist_mode else 'playlistPanelVideoRenderer']

        try:
            duration = video["lengthText"]["simpleText"]
        except KeyError as e:
            raise AttemptRetry("Length key error") from e
        length_vals = duration.split(":")

        if len(length_vals) > 2:
            length = int(length_vals[0])*3600 + int(length_vals[1])*60 + int(length_vals[2])
        else:
            length = int(length_vals[0])*60 + int(length_vals[1])

        dict_list.append({
            "id": video['videoId'],
            "title": video["title"]["runs"][0]['text'] if playlist_mode else video["title"]["simpleText"],
            "duration": duration,
            "length": length
        }) 

    return dict_list
    
    
def log(e: str):
     with open(os.path.join(os.path.dirname(__file__), "parser.log"), "a+") as fp:
        fp.write(str(datetime.now()) + '\n')            
        fp.write(str(e))
        fp.write('\n')

def retry(retries, func, *args) -> Any:
    backoff = 0
    for _ in range(retries + 1):
        try:
            return func(*args)
        except AttemptRetry:
            log(traceback.format_exc())
            time.sleep(backoff)
            backoff += 1
        except EndRetry:
            log(traceback.format_exc())
            break
            
    sys.exit(1)

# Utils


def get_between(text: str, start: str, end: str) -> str:
    return text.split(start)[1].split(end)[0]

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Parses youtube url.')

    # Global arguments
    parent_parser = argparse.ArgumentParser(add_help=False)

    parent_parser.add_argument('-r', '--retries',type=int, default=RETRIES, help='fetch retries count')
    parent_parser.add_argument('-d', '--debug', dest='debug', action='store_true', help='debug flag')
    parent_parser.set_defaults(debug=False)

    # Commands
    subparser = parser.add_subparsers(dest='command')

    description = subparser.add_parser('description', parents=[parent_parser])
    playlist = subparser.add_parser('playlist', parents=[parent_parser])
    
    # Parse arguments
    parser.add_argument('url', help='url to parse')
    args = parser.parse_args()

    command = args.command
    url = args.url
    debug = args.debug
    retries = args.retries

    if debug:
        print("====DEBUG MODE====")

    def fetch_and_parse():
        try:
            text = requests.get(url).text
        except Exception as e:
            raise AttemptRetry("Fetch error") from e

        if debug:
            with open('__pycache__/test.html', 'w') as fp:
                fp.write(text)
        
        if command == 'description':
            print(get_description(text))
        elif command == 'playlist':
            print(json.dumps(get_playlist_data(
                text, debug), indent=2 if debug else None))


    retry(retries, fetch_and_parse)