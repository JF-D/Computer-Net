#!/usr/bin/python

import sys
import string
import socket
import numpy as np
from time import sleep

arr = np.fromfile('client-input.dat', dtype='uint8')
data = ''
for i in range(len(arr)):
    data += chr(arr[i])

def server(port):
    s = socket.socket()
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    
    s.bind(('0.0.0.0', int(port)))
    s.listen(3)
    
    cs, addr = s.accept()
    print addr
    
    data = ''
    while True:
        rcv = cs.recv(1000)
        if rcv:
            data += rcv
        else:
            break
    
    s.close()

    arr = []
    for i in range(len(data)):
        arr.append(data[i])
    arr = np.array(arr, dtype=str)
    arr.tofile('server-rcv.dat')


def client(ip, port):
    s = socket.socket()
    s.connect((ip, int(port)))
    
    s.send(data)
    
    s.close()

if __name__ == '__main__':
    if sys.argv[1] == 'server':
        server(sys.argv[2])
    elif sys.argv[1] == 'client':
        client(sys.argv[2], sys.argv[3])
