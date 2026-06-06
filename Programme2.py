# -*- coding: utf-8 -*-
"""
Created on Wed Jun  3 02:35:00 2026

@author: nervo
"""

# gonna preface this by saying this code GENUINLEY consumes 30 gigs of ram doing literally nothing its VERY bad im sorry 
# this was written with the assumption it had 64 gb of ram and 140gb swap file and a ryzen 9 9950x3d to work with
# im so sorry

import sys
import os 
import numpy as np
import pandas as pd
import scipy as scipy
import matplotlib.pyplot as plt

timeLeft = []
voltLeft = []
timeRight = []
voltRight = []
fullMergedArrays = {}
monoAvg = {} #for use when maaking sure we have stuff that we can analyise that starts at the same point 


def loadMergeCsvToArray(abc123, saveall, savenone): #abc123 is a placeholder for number of datasets  #function to merge the two CSV files from each dataset into one single array and or csv. unforunatley due to having to use lists converted to arrays there will be some small floating point error introduced
    if saveall == "n" and savenone == "n":
        if abc123 == 0:  #there isnt really a reason for this if statement to exist but i dont wanna get rid of it  now
            print("")
        elif type(abc123) == int:
            for i in range(abc123):
                folder = str(input(f"input folder name of dataset {i+1} : \n"))
                print("")
                

                #tempTime = []
                #tempVL =[] #have to use lists and then convert 
                #tempVR =[]
                
                TL, VL = np.loadtxt(f"{folder}/RTB2004_CHAN2.csv", delimiter=",", skiprows=1, unpack=True)
                TR, VR = np.loadtxt(f"{folder}/RTB2004_CHAN4.csv", delimiter=",", skiprows=1, unpack=True)
                
                print("files loaded")
                print("")
                
                
                #for j in range(len(TL)):
                #    tempTime.append(TL[j])
                #    tempVL.append(VL[j])
                #    tempVR.append(VR[j])
                    
                #print("temp lists created")
                #print("")
                    
                print("Saving as seperate csv files will NOT return anything. You will have to np.loadtxt the new files. This is done for sake of testing/programme efficiency")
                save = str(input("save as seperate CSV? (y/n):  \n"))
                
                
                while True: #this is just so we cant input nonsense and the code continue anyways
                    
                    if save in ("y", "yes", "Yes", "YES", "yea", "yeah"):
                        DataMerged = np.column_stack((TL, VL, VR))
                        filenameMergedData = str(input(f"filename? (will be appended with {folder}.csv): \n"))
                        np.savetxt(f"{folder}/{filenameMergedData}.csv", DataMerged, delimiter=",")
                        fullMergedArrays[f"{folder}_time"] = TL
                        fullMergedArrays[f"{folder}_VL"] = VL
                        fullMergedArrays[f"{folder}_VR"] = VR
                        print(f"filesize(bytes):{os.stat(f"{folder}/{filenameMergedData}.csv")}")
                        break
                        
                        #want to save as dictionary, too
                            
                    elif save in ("n", "no", "No", "NO", "nah"):
                        fullMergedArrays[f"{folder}_time"] = TL
                        fullMergedArrays[f"{folder}_VL"] = VL
                        fullMergedArrays[f"{folder}_VR"] = VR
                        #want to save as dictionary
                        break
                    else:
                        save = str(input("please give y/n answer: \n"))
            print("")
            #print(f"total filesize of all files created: {filesize}")
            #print(f"ram used by dictionary:  {sys.getsizeof(fullMergedArrays)*1e-9} gigabytes")
    elif saveall == "n" and savenone == "y":
        if abc123 == 0:  #there isnt really a reason for this if statement to exist but i dont wanna get rid of it  now
            print("")
        elif type(abc123) == int:
            for ia in range(abc123):
                folder = str(input(f"input folder name of dataset {ia+1} : \n"))
                print("")
                
                
                
                TL, VL = np.loadtxt(f"{folder}/RTB2004_CHAN2.csv", delimiter=",", skiprows=1, unpack=True)
                TR, VR = np.loadtxt(f"{folder}/RTB2004_CHAN4.csv", delimiter=",", skiprows=1, unpack=True)
                
                print("files loaded")
                print("")
                
                
                fullMergedArrays[f"{folder}_time"] = TL
                fullMergedArrays[f"{folder}_VL"] = VL
                fullMergedArrays[f"{folder}_VR"] = VR
                
                print("arrays placed in dictionary")
    elif saveall == "y" and savenone == "y":
        for ib in range(abc123):
            folder = str(input(f"input folder name of dataset {ib+1} : \n"))
            print("")
            
            #tempTime = []
            #tempVL =[] #have to use lists and then convert 
            #tempVR =[]
            
            TL, VL = np.loadtxt(f"{folder}/RTB2004_CHAN2.csv", delimiter=",", skiprows=1, unpack=True)
            TR, VR = np.loadtxt(f"{folder}/RTB2004_CHAN4.csv", delimiter=",", skiprows=1, unpack=True)
            
            print("files loaded")
            print("")
            DataMerged = np.column_stack((TL, VL, VR))
            filenameMergedData = str(input(f"filename? (will be appended with {folder}.csv): \n"))
            np.savetxt(f"{folder}/{filenameMergedData}.csv", DataMerged, delimiter=",")
            fullMergedArrays[f"{folder}_time"] = TL
            fullMergedArrays[f"{folder}_VL"] = VL
            fullMergedArrays[f"{folder}_VR"] = VR
            print(f"filesize(bytes):{os.stat(f"{folder}/{filenameMergedData}.csv")}")
            
            
        
        
            
            
numOfDatasets = int(input("how many datasets would you like to look at?:  \n"))   
a = str(input("saveall? (y/n) \n"))
b = str(input("savenone? (y/n) \n"))
print("For reference, standard index order of these should be: \n vinyl \n uncompressed \n mp3big \n mp3small \n test_compressed")
print("")
loadMergeCsvToArray(numOfDatasets, a, b) 

# code to merge into mono, for plotting and getting the time lined up right

#%%

import matplotlib.pyplot as plt
for name in ["vinyl", "uncompressed", "mp3big", "mp3small", "test_compressed"]:
    monotime = fullMergedArrays[f"{name}_time"]
    monoleft = fullMergedArrays[f"{name}_VL"]
    monoright = fullMergedArrays[f"{name}_VR"]

    mono = (monoleft + monoright) / 2

    plt.title(f"{name}")
    plt.plot(monotime, mono)
    plt.show()
    plt.savefig(f"{name}")
    
    
#%%
import numpy as np
from scipy.io.wavfile import write as wav_write

_audio_dataset_name = "test_compressed"
_audio_sample_rate_hz = round(83.333333333333 * 1000)

_audio_left_channel = fullMergedArrays[f"{_audio_dataset_name}_VL"]
_audio_right_channel = fullMergedArrays[f"{_audio_dataset_name}_VR"]

_audio_stereo_data = np.column_stack((_audio_left_channel, _audio_right_channel))


_audio_peak_value = np.max(np.abs(_audio_stereo_data))
_audio_stereo_normalized = _audio_stereo_data / _audio_peak_value

_audio_wav_int16 = np.int16(_audio_stereo_normalized * 32767)

wav_write(f"{_audio_dataset_name}.wav", _audio_sample_rate_hz, _audio_wav_int16)
        
        
    
    

# okay so what im doing is im finding the start and end points in audacity, i am using the "selection" thing at the bottom
#which can tell me, time in seconds to 3dp, it can ALSO tell me the exact sample.
# given that i have the original files for all of the digital recordings
# i am going to use the uncompressed, original file as a reference. 
# here, i have found a point which i could quite easily use as a reference for all others

# as you can see in the plots, there is a solo section around the middle-end of the song that is a lot quieter than the rest of the song
# on the original uncompressed wav file, which i ripped form the CD, there is a single sample:
# this single sample is easily identifiable as like, a maximum point around when the solo ends and it gets much louder
# the exact sample of this point in audacity (screenshot) is sample 5,478,569
# THis doesnt give us an integer later, so we are choosing 1 sample after this point (about 2 samples in the recorded data)
# however, our equipment recorded at ~88.33333333333333333333.... kHz, but the CD is 44.1kHz
# HOWEVER, we also know the original wav file had a total of 8,561,280 samples
# we also know that we recorded for (time[-1] - time[0]) seconds long, and for 20*10^6 samples, for every recording
# the time of EVERY recording is 240 seconds
# the "distance" from the identifiable sample to the start and end of the song will be used to get a
# near exact start point for each digital recording. 
# our_identifiable_point_original_data * recording_samplerate/original_samplerate is NOT an integer, however
# this should be fine for our purposes, and it is only ~0.3 off an integer.
# in the case of the original wav file and the data from the original wav file being played, we get 
# (5,478,569) * 83,333/44,100 ~= 10352508
# so we find this point on the recorded data, and subtract 11135258. that is where the song starts
# to find where it should end, again, we look at this point on the recorded data:
# (8,561,280 - 5,478,569) * 83,333/44,100 = 5825205
# so, it should end 6,265,648 samples after the point in OUR recording 
# in OUR uncompressed recording, the point is at 11,928,788

# now, i am going to find this point in each file. I am doing it this way because i am only comparing them to the digital uncompressed and the wav file.
# for the vinyl, im going to use a basically the same method.

# point vinyl data 12244899
#start 1892391
#end 18070104
# FOR THE VINYL THIS IS LIKE WAY OFF, i think this is because it doesnt spin at exactly 45 RPM when it should. That is gonna suck for data analysis lol its basicallyuselesss
# tho ig we learned that people who swear by vinyl sounding better or more accurate are just straight up wrong


# point uncompressed data 11928788
#s 1576280
#e 17753993

#actually i just remembered the song has like 1 second extra at the start i dont think its way foff for the vinyl

# point bigmp3 11994981
#s 1642473
#e 18260629

# point smallmp3 11997770
#s 1645262
#e 17822975

# point test_compressed 12068121
#s 1715613
#e 17893326


### note to self psuedocode:
# will want to make something that appends a f"{folder}_merged" to the start of each list    


# i realised i did the wrong CD sample rate. im just going to code this myself

#%%

# ok im gonna like chop the data up now 
# this code gets worse by the minute i am sorry i havent slept and its friday the 5th of june i dont have time 
starts = [1892391, 1576280, 1642473, 1645262, 1715613]
ends =  [18070104, 17753993, 17820186, 17822975, 17893326]

NUMBERRRRR = 0
for name in ["vinyl", "uncompressed", "mp3big", "mp3small", "test_compressed"]: #maybe change it from name to another variable it mighr break
    time1 = fullMergedArrays[f"{name}_time"][starts[NUMBERRRRR]:(ends[NUMBERRRRR])+1]
    left1 = fullMergedArrays[f"{name}_VL"][starts[NUMBERRRRR]:(ends[NUMBERRRRR])+1]
    right1 = fullMergedArrays[f"{name}_VR"][starts[NUMBERRRRR]:(ends[NUMBERRRRR])+1]
    abridgedDataMerged = np.column_stack((time1, left1, right1))
    np.savetxt(f"{name}/{name}_abridged.csv", abridgedDataMerged, delimiter=",")
    NUMBERRRRR = NUMBERRRRR + 1
    
#%%

# calculating ROOT MEAN SQUARE ERROR 
listofnames =  ["mp3big", "mp3small", "test_compressed"]
RMSE_LC_wav = []
RMSE_RC_wav = []
RMSE_LC_vin = []
RMSE_RC_vin = []
NUMBER_2 = 0
T_ref_wav, L_ref_wav, R_ref_wav = np.loadtxt("uncompressed/uncompressed_abridged.csv", delimiter=",", skiprows=0, unpack=True)
T_ref_vin, L_ref_vin, R_ref_vin = np.loadtxt("vinyl/vinyl_abridged.csv", delimiter=",", skiprows=0, unpack=True)

for name1 in listofnames:
    T, L, R = np.loadtxt(f"{name1}/{name1}_abridged.csv", delimiter=",", skiprows=0, unpack=True)
    RMSE_LC_wav.append(np.sqrt(((L_ref_wav-L)**2).mean()))
    RMSE_RC_wav.append(np.sqrt(((R_ref_wav-R)**2).mean()))
    RMSE_LC_vin.append(np.sqrt(((L_ref_vin-L)**2).mean()))
    RMSE_RC_vin.append(np.sqrt(((R_ref_vin-R)**2).mean()))
#np.savetxt("results_RMSE.csv", np.column_stack((RMSE_LC_wav, RMSE_RC_wav, RMSE_LC_vin, RMSE_RC_vin)), delimeter=",")
print("RMSE_LC_wav in order of mp3big mp3small test_compressed")
print(RMSE_LC_wav)
print(RMSE_RC_wav)
print(RMSE_LC_vin)
print(RMSE_RC_vin)
#%%

#everything takes so long to run i am putting this in a seperate cell
# im so glad were not getting graded on how good the code is
# everyone in DoC would kill me if they saw this
