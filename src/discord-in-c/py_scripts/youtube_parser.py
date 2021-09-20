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

def get_playlist_data(url, do_save=False):
    """Retrieves the video description

    Args:
        url (str): Video url

    Returns:
        dict
    """
    text = requests.get(url).text

    if do_save:
        with open('test.html', 'w') as fp:
            fp.write(text)

    json_string = get_between(text, 'ytInitialData = ', ';</script>')
    parsed_json: dict = json.loads(json_string)

    if do_save:
        with open('test.json', 'w') as fp:
            fp.write(json_string)

    playlist_mode = '/playlist' in url

    if playlist_mode:
        content_list = parsed_json['contents']['twoColumnBrowseResultsRenderer']['tabs'][0]['tabRenderer']['content']['sectionListRenderer']['contents'][0]['itemSectionRenderer']['contents'][0]['playlistVideoListRenderer']['contents']
    else:
        content_list = parsed_json['contents']['twoColumnWatchNextResults']['playlist']['playlist']['contents']

    # Filter out missing videos
    content_list = [x for x in content_list if "unplayableText" not in x]

    dict_list = []
    
    for data in content_list:
        if 'continuationItemRenderer' in data.keys():
            continue
        video = data['playlistVideoRenderer' if playlist_mode else 'playlistPanelVideoRenderer']

        duration = video["lengthText"]["simpleText"]
        length_vals = duration.split(":")

        if len(length_vals) > 2:
            length = int(length_vals[0])*3600 + int(length_vals[1])*60 + int(length_vals[2])
        else:
            length = int(length_vals[0])*60 + int(length_vals[1])

        dict_list.append({
            "id": video['videoId'],
            "title": video["title"]["runs"][0] if playlist_mode else video["title"]["simpleText"],
            "duration": duration,
            "length": length
        }) 

    return dict_list
    
    

# String utils
def get_between(text: str, start: str, end: str) -> str:
    return text.split(start)[1].split(end)[0]

if __name__ == "__main__":
    debug = False

    command = sys.argv[1]
    url = sys.argv[2]

    if len(sys.argv) > 3:
        debug = sys.argv[3] == '-debug'

    if debug:
        print("====DEBUG MODE====")

    if command == 'description':
        print(get_description(url))
    elif command == 'playlist':
        print(json.dumps(get_playlist_data(url, debug)))
    else:
        sys.exit(1)