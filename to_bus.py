# -*- coding: utf-8 -*-

# kör like so: python to_bus.py bilder/16-25-59.jpeg

import socket, sys

from PIL import Image
import numpy as np
from scipy import misc
import cPickle


def send(data):
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.connect("/tmp/socketname")
    s.send(data)
    received_data = s.recv(1024)
    s.close()
    print 'Received', repr(received_data)

def low_qual(path):
    img = np.array(Image.open(sys.argv[1]).convert('L'))
    img = misc.imresize(img, 0.01) / 255. # 0.10 ger size (28, 35)
    return img

print sys.argv[1]
lq_img = low_qual(sys.argv[1])
ser_img = cPickle.dumps(lq_img)
send(ser_img)

#print ser_img
#unser_img = cPickle.loads(ser_img)
#print unser_img
