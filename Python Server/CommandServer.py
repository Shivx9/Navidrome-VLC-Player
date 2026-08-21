from dataclasses import dataclass
from typing import List
from flask import Flask, request, send_file
import  requests, os, vlc, time, json, threading, queue
from PIL import Image
from io import BytesIO

app = Flask(__name__)

@dataclass
class Song:
    id: str
    path: str
    title: str
    album: str
    artist: str
    albumArt: str


NAVIDROME_URL = "http://100.69.111.63:4533"
baseDir = "./"
baseMusicDir = "/datapool/Audio/Music Archive"
musicMedia = None
vlc_instance = vlc.Instance()
player = vlc_instance.media_player_new()
event_manager = player.event_manager()
vlc_action_queue = queue.Queue() # Thread-safe queue to pass actions from VLC events to our worker thread

playState = False # Updated when toggling play/pause
playMode = "Albums" # Albums | Songs
seeking = False # Updated when rewinding/Fast-forwarding
rewindBuffer: float = 0.0
rewindMarker: float = 0.0
rewindSample = vlc_instance.media_new(os.path.join(baseDir, "deandre_aaron-dj-turntable-rewind-429858.mp3"))
playlist:List[Song] = []
playIndex=-1
lastPlayIndex=-1
lastVolume=0
fastMultiplier:float=10.0



#initial
player.audio_set_volume(50)





with open('RFID_Mapping.json', 'r') as f:  # Load mapped song list
    rfidMap = json.load(f)


@app.route('/add/<mode>/<mediaID>', methods=['GET'])
def add_music(mode, mediaID):

    global playIndex

    try:
        response = 0

        match mode:
            case 'song':
                response = requests.get(f"{NAVIDROME_URL}/rest/getSong?u=blanc&p=pass&v=1.16.1&c=myapp&f=json&id={rfidMap[mediaID]}", timeout=5)
                data = response.json()

                ### Trimming additional indexing added by Navidrome from path - Shift to Album logic later
                tempPath = data["subsonic-response"]["song"]["path"]
                tempIndex = tempPath.rfind('/')
                songPath = tempPath[:tempIndex + 1] + tempPath[tempIndex + 6:]

                songTitle = data["subsonic-response"]["song"]["title"]
                songAlbum = data["subsonic-response"]["song"]["album"]
                songArtist = data["subsonic-response"]["song"]["artist"]
                coverArt = data["subsonic-response"]["song"]["coverArt"]
                playlist.append(
                    Song(
                        id=rfidMap[mediaID],
                        path=songPath,
                        title=songTitle,
                        album=songAlbum,
                        artist=songArtist,
                        albumArt=coverArt
                    )
                )

            case 'album':
                response = requests.get(f"{NAVIDROME_URL}/rest/getAlbum?u=blanc&p=pass&v=1.16.1&c=myapp&f=json&id={rfidMap[mediaID]}", timeout=5)
                data = response.json()

                albumTitle = data["subsonic-response"]["album"]["name"]
                albumArtist = data["subsonic-response"]["album"]["displayArtist"]
                albumCoverArt = data["subsonic-response"]["album"]["coverArt"]
                albumSongs = data["subsonic-response"]["album"]["song"]

                for i in range(int(data["subsonic-response"]["album"]["songCount"])):
                    tempPath = albumSongs[i]["path"]
                    tempIndex = tempPath.rfind('/')
                    songPath = tempPath[:tempIndex + 1] + tempPath[tempIndex + 6:]

                    songTitle = albumSongs[i]["title"]

                    playlist.append(
                        Song(
                            id= albumSongs[i]["id"],
                            path=songPath,
                            title=songTitle,
                            album=albumTitle,
                            artist=albumArtist,
                            albumArt=albumCoverArt
                        )
                    )

        return f"Success from secondary service: {playlist}", 200




    except requests.exceptions.RequestException as e:
        return f"Could not connect to blanc Navidrome: {e}", 500


@app.route('/play')
def play_music():
    global playIndex
    global lastPlayIndex
    global playState
    global seeking
    global musicMedia

    try:
        # Check valid play index within playlist
        if 0<=playIndex<len(playlist):
            print(f"playing at index {playIndex} with last index {lastPlayIndex}")
            if seeking:
                player.set_rate(1.0)
                seeking = False

            # Check if moving to different song or if the last song has ended
            if lastPlayIndex!=playIndex or is_stopped():
                lastPlayIndex = playIndex
                # Check both directories for presence
                if os.path.isfile(os.path.join(baseMusicDir, "Albums", playlist[playIndex].path)):
                    print("playing from album")
                    musicMedia = vlc_instance.media_new(os.path.join(baseMusicDir, "Albums", playlist[playIndex].path))
                else:
                    musicMedia = vlc_instance.media_new(os.path.join(baseMusicDir, "Singles", playlist[playIndex].path))
                    print("playing from song")
                player.set_media(musicMedia)
                player.play()
                playState = True
                return "Playing", 200

            # Same song - resume (toggle play/pause)
            else:
                player.pause()
                playState = False
                return f"Play/Pause toggle", 200
        else:
            return "Playlist index out of range", 500




    except requests.exceptions.RequestException as e:
        return f"Could not connect to blanc Navidrome: {e}", 500

@app.route('/skip/<direction>')
def skip_music(direction="next"):
    global playIndex

    try:
        print(f"skip try - index {playIndex} - len {len(playlist)}")
        if (playIndex<len(playlist)-1 and direction=="next") or (0<playIndex and direction=="previous"):
            print(direction)
            if direction=="next":
                playIndex+=1
            else:
                playIndex-=1
            play_music()
            return f"Status: Skipped to {direction}", 200
        else:
            return "Error: Out of bounds playlist index", 500

    except requests.exceptions.RequestException as e:
        return f"Could not connect to blanc Navidrome: {e}", 500

@app.route('/vol', methods=['GET'])
def set_volume():
    muteFlag = request.args.get("mute")
    action = request.args.get("action")
    global lastVolume

    try:
        currVol = player.audio_get_volume()
        print(currVol)
        targetVol = currVol

        #mute toggle
        if muteFlag=='true':
            print("in mute toggle")
            #was already set to zero volume manually
            if lastVolume<=0:
                print("1")
                targetVol = 10
            # was muted - now unmute
            elif currVol<=0:
                print(2)
                targetVol = lastVolume
            # mute
            else:
                print(3)
                targetVol = 0

        #vol incr/decr
        else:
            if action=="incr":
                targetVol += 10
            if action=="decr":
                targetVol -= 10

            # clamp to min & max vol limits
            targetVol = max((min(targetVol, 100), 0))
            lastVolume = targetVol

        if currVol!=targetVol:
            player.audio_set_volume(targetVol)

        return f"Volume set to {targetVol}", 200

    except requests.exceptions.RequestException as e:
        return f"Could not adjust volume: {e}", 500


@app.route('/getSong')
def get_song():
    full = request.args.get("full") #if all details are required

    try:
        result = {}
        if len(playlist)>0 and playIndex>=0:
            result["id"]=playlist[playIndex].id
            if full=="true":
                result["name"]=playlist[playIndex].title
                result["artist"]=playlist[playIndex].artist
                result["album"]=playlist[playIndex].album
            return result, 200
        else:
            return "", 400
    except requests.exceptions.RequestException as e:
        return f"Could not connect to get song details: {e}", 500


@app.route('/getArt')
def get_art():
    try:
        response = requests.get(f"{NAVIDROME_URL}/rest/getCoverArt?u=blanc&p=pass&v=1.16.1&c=myapp&f=json&id={playlist[playIndex].id}&size=64", timeout=5)
        img = Image.open(BytesIO(response.content))
        rgb_img = img.convert("RGB")

        # 4. Save to an in-memory bytes buffer
        img_io = BytesIO()
        rgb_img.save(img_io, 'JPEG', quality=85)
        img_io.seek(0)  # Move pointer back to the beginning of the file

        # 5. Stream the file back with the proper MIME type
        return send_file(img_io, mimetype='image/jpeg')

        return response, 200
    except requests.exceptions.RequestException as e:
        return f"Could not connect to blanc Navidrome: {e}", 500

@app.route('/clearq')
def clear_queue():
    global playIndex
    global lastPlayIndex
    try:
        playlist.clear()
        playIndex = -1
        lastPlayIndex = -1
        return f"Cleared queue", 200
    except requests.exceptions.RequestException as e:
        return f"Error clearing queue: {e}", 500

@app.route('/ffwd/<flag>')
def fastforward(flag):
    global playState
    global seeking

    try:
        if is_stopped():
            return "Playback already ended", 500
        if flag=='start':
            seeking = True
            player.play()
            time.sleep(0.1)
            player.set_rate(fastMultiplier)
        elif flag=='stop':
            # if not playState:
            #     play_music()  # Toggle pause if that was state prior to fast-forwarding
            #     time.sleep(0.1)
            player.set_rate(1.0)
            seeking = True
        else:
            return "Invalid flag", 500
        return f"Fast-forward {flag}", 200
    except requests.exceptions.RequestException as e:
        return f"Connection failed on fastforward func: {e}", 500


@app.route('/rwd/<flag>')
def rewind(flag):
    global playState
    global seeking
    global rewindBuffer
    global rewindMarker
    global rewindSample

    try:
        if is_stopped():
            return "Playback already ended", 500
        if flag=='start':
            seeking = True
            rewindBuffer = time.time()
            rewindMarker = player.get_time()
            print(f"rewind start at {rewindMarker}")
            if rewindMarker == 0:
                player.pause()
            else:
                player.set_media(rewindSample)
                player.play()
        elif flag=='stop':
            rewindBuffer = time.time() - rewindBuffer
            player.set_media(musicMedia)

            player.play()
            time.sleep(0.1)

            # if not playState:
            #     play_music()
            #     time.sleep(0.1)

            #     play_music()  # Toggle pause if that was state prior to fast-forwarding
            #     time.sleep(0.1)

            print(f"rewind end at {max(0, int(rewindMarker - rewindBuffer * 1000 * fastMultiplier))}")
            player.set_time(max(0, int(rewindMarker - rewindBuffer * 1000 * fastMultiplier)))
            player.set_rate(1.0)
        else:
            return "Invalid flag", 500
        return f"Rewind {flag}", 200
    except requests.exceptions.RequestException as e:
        return f"Connection failed on fastforward func: {e}", 500



##############################3

def is_stopped():
    return player.get_state() in [vlc.State.Ended, vlc.State.Stopped]

def on_end_reached(event):
    print("Media playback finished.")
    threading.Thread(target=skip_music, daemon=True).start()



if __name__ == '__main__':
    # Run on localhost port 5000
    app.run(host='0.0.0.0', port=5000, debug=True)
    event_manager.event_attach(vlc.EventType.MediaPlayerEndReached, on_end_reached)