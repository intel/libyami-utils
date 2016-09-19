#!/usr/bin/env python3

#This script will compare ssim value between libyami and ffmpeg.
#Firstly, it will generate yuv using yamidecode, then it will compare the ssim with ffmpeg. Mini version for ffmpeg is 2.8
#Example commands:
#    ssim_test.py directory
#    ssim_test.py file

import subprocess
import re
import glob, os
import sys
from os.path import join, getsize

def isCandidate(f):
    filename, ext = os.path.splitext(f)
    ext = ext.lower()
    supported = [
        ".jpeg"
        , ".jpg"
        , ".vc1"
        , ".rcv"
        , ".mpeg"
        , ".mpg"
        , ".mpeg2"
        , ".mjpg"
        , ".mjpeg"
    ]
    return ext in supported

def testYami(f, log):
    output = None if log else subprocess.DEVNULL
    yamidecode = os.path.dirname(os.path.realpath(__file__)) + "/../tests/yamidecode"
    r = subprocess.call(yamidecode + r" -i "+ f + " -f i420 -m 0 ", stdout=output,  stderr=output, shell=True)
    if r != 0:
        print("yami return "+str(r));
    return r == 0

def getFmt(f):
    filename, ext = os.path.splitext(f)
    if ".I420" == ext:
        return "yuvj420p"
    if ".422H" == ext:
        return "yuvj422p"
    if ".422V" == ext:
        return "yuvj440p"
    if ".444P" == ext:
        return "yuvj444p"
    return ""

def getYuvFile(src):
    dirname, basename = os.path.split(src)
    files = glob.glob(basename+"_*")
    if len(files) == 0:
        print("no yuv for "+src)
        return None
    return files[0]

def getFileInfo(file):
    m=re.search("(?P<width>\d+)x(?P<height>\d+)", file);
    w = int(m.group("width"))
    h = int(m.group("height"))
    if w == 0 or h == 0:
        return 0, 0, ""
    return w, h, getFmt(file)

def info(cmd, log):
    a = " All:"
    for line in cmd.stdout:
        l = line.decode("utf8");
        if (log):
            print(l)
        if "error" in l.lower():
            print(l)
            return False
        if  a in l:
            t =  l.split(a)
            t = t[1].split(" ")
            if (float(t[0]) > 0.99):
                return True;
    return False;

def verify(yuv, f, log):

    w, h, fmt = getFileInfo(yuv)
    cmd = subprocess.Popen(r"ffmpeg -f rawvideo -video_size " + str(w) + "x" + str(h) +" -pix_fmt "+ fmt + " -i " + yuv + " -i " + f + ' -lavfi "ssim;[0:v][1:v]psnr" -f null -', shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    return info(cmd, log)

def test(f, log = False):
    if not testYami(f, log):
        return False

    yuv = getYuvFile(f);
    if yuv is None:
        return False
    ret = verify(yuv, f, log)
    os.remove(yuv)
    print(f, end =  " ")
    print("pass" if ret else "failed")
    return ret

if len(sys.argv)!=2:
    print(sys.argv[0] + " directory")

dir = sys.argv[1]
#test file
if os.path.isfile(dir):
    test(dir, True)
    sys.exit(0)

#test directory
total = 0
failed = 0
for root, dirs, files in os.walk(dir):
    for f in files:
        if isCandidate(f):
            total += 1
            pss = test(join(root, f))
            failed +=  not pss
print("total = "+ str(total) +", failed = "+ str(failed))
