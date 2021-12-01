# This script takes input data from Firebase and outputs the datastream to ShazamAPI

import asyncio
from shazamio import Shazam
import firebase_admin as fb
from firebase_admin import db
from pydub import AudioSegment
import sys


async def getShazam(filename):
    shazam = Shazam()
    out = await shazam.recognize_song(filename) # 'D:\\OneDrive\\Music\\Google Play\\Tracks\\'+
    

    songData = {}

    songData['title'] = out['track']['title']
    # Album title
    songData['album'] = out['track']['sections'][0]['metadata'][0]['text']
    # Artist
    songData['artist'] = out['track']['subtitle']

    # Note: Sometimes, Lyrics are not found. Check to see if sections[1]['type'] == 'LYRICS', 
    # if it does, then artist is under 3, if not then artist is under 2

    # if out['track']['sections'][1]['type'] == 'LYRICS':
    #     art2 = print(out['track']['sections'][3]['name'])
    # else:
    #     art2 = print(out['track']['sections'][2]['name'])
    # if len(art1) > len(art2):

    return songData


def decodeAudio(event: db.Event):
    # Get data from Firebase
    audio = event.data
    
    # use audio segment to convert raw to mp3
    rawdata = [int(a, base=16).to_bytes(4, sys.byteorder) for a in audio[:-1].split(',')]
    # output data to raw file
    with open('audio.raw', 'wb') as f:
        for i in rawdata:
            f.write(i)

    # use audio segment to convert raw to mp3
    rawfile = AudioSegment.from_raw('audio.raw', sample_width=1, frame_rate=44100, channels=2)
    rawfile.export('audio.mp3', format='mp3')

    #Shazam it!
    songData = loop.run_until_complete(getShazam('audio.mp3'))

    # Send data to Firebase
    root.set(songData)


loop = asyncio.get_event_loop()
#print(loop.run_until_complete(getShazam('Howard Shore - Music from The Lord of the (001).mp3')))

# Get firebase credentials object
cred = fb.credentials.Certificate('fir-test-803bb-firebase-adminsdk-p6buy-bea9c8dc15.json')
# Initialize firebase app
app = fb.initialize_app(cred, {
	'databaseURL': 'https://fir-test-803bb-default-rtdb.firebaseio.com/'
	})


root = db.reference('/')
incoming_audio = root.child('AUDIO_ENCODED_DATA')

fb_listener = incoming_audio.listen(decodeAudio)

audio = incoming_audio.get()