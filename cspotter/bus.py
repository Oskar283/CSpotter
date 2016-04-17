# -*- coding: utf-8 -*-
import socket, os
import ast
import matplotlib.pyplot as plt
import cPickle

def show(img):
    plt.imshow(img, cmap=plt.cm.gray)
    plt.show()

s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
try:
    os.remove("/tmp/socketname")
except OSError:
    pass
s.bind("/tmp/socketname")
while 1:
    s.listen(1)
    conn, addr = s.accept()
    data = conn.recv(1024)
    print 'received data'
    #data = ast.literal_eval(data)
    unser_img = cPickle.loads(data)
    print unser_img
    #show(data)
    conn.send("bus salutes you")
conn.close()
