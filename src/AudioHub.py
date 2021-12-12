# This script takes input data from Firebase and outputs the datastream to ShazamAPI

from shazamio import Shazam
from pydub import AudioSegment
import sys
import threading


async def getShazam(filename):
    shazam = Shazam()
    out = await shazam.recognize_song(filename) # 'D:\\OneDrive\\Music\\Google Play\\Tracks\\'+
    

    songData = {}
    if len(out["matches"]) > 0:
        songData['title'] = out['track']['title']
        # Album title
        songData['album'] = out['track']['sections'][0]['metadata'][0]['text']
        # Artist
        songData['artist'] = out['track']['subtitle']
    else:
        return "No matches found"

    # Note: Sometimes, Lyrics are not found. Check to see if sections[1]['type'] == 'LYRICS', 
    # if it does, then artist is under 3, if not then artist is under 2

    # if out['track']['sections'][1]['type'] == 'LYRICS':
    #     art2 = print(out['track']['sections'][3]['name'])
    # else:
    #     art2 = print(out['track']['sections'][2]['name'])
    # if len(art1) > len(art2):

    return str(songData)



def decodeAudio(encodedAudio, append = False, convert = False):
    # decode our hex encoded audio bytes
    rawdata = []
    for a in encodedAudio.split(';')[:-1]:
        try:
            rawdata.append((int(a, base=16)*3).to_bytes(2, sys.byteorder))
        except ValueError:
            pass

    # output data to raw file
    writeMode = 'ab' if append else 'wb'

    # Lock thread
    with open('audio.raw', writeMode) as f:
        for i in rawdata:
            f.write(i)

    # use audio segment to convert raw to mp3
    if convert:
        rawfile = AudioSegment.from_raw('audio.raw', sample_width=2, frame_rate=44100, channels=1)
        rawfile.export('audio.mp3', format='mp3')

    return 'audio.mp3'
    #Shazam it!
    #songData = loop.run_until_complete(getShazam('audio.mp3'))