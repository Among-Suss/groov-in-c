import requests
import json
import sys


def get_description(url):
    """Retrieves the video description

    Args:
        url (str): Video url

    Returns:
        str: description
    """
    text = requests.get(url).text

    return get_between(text, '"description":{"simpleText":"', '"},').replace("\\n", "\n")

def get_playlist_id(url):
    """Retrieves the video description

    Args:
        url (str): Video url

    Returns:
        list: List of ids
    """
    text = requests.get(url).text

    json_object = get_between(text, 'ytInitialData = ', ';</script>')
    parsed_json: dict = json.loads(json_object)
    content_list = parsed_json['contents']['twoColumnWatchNextResults']['playlist']['playlist']['contents']

    return list(
        map(lambda c: c['playlistPanelVideoRenderer']['videoId'], content_list))
    
    

# String utils
def get_between(text: str, start: str, end: str) -> str:
    return text.split(start)[1].split(end)[0]

if __name__ == "__main__":
    command = sys.argv[1]
    url = sys.argv[2]

    if command == 'description':
        print(get_description(url))
    elif command == 'playlist':
        print(','.join(get_playlist_id(url)))
    else:
        sys.exit(1)
