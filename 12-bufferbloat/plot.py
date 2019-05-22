# -*- coding: utf-8 -*-
"""
Created on Wed May 22 08:37:06 2019

@author: DJF
"""
import matplotlib.pyplot as plt

path = 'qlen-100/'
###################################
f = open('qlen-50/cwnd.txt')
line = f.readline()
x, y = [], []
while line:
    if line.split()[1].split(':')[0] == '10.0.1.11':
        y.append(int(line.split()[6]))
        x.append(float(line.split()[0]))
    line = f.readline()
f.close()
plt.plot(x, y, label='qlen=50')

f = open(path+'cwnd.txt')
line = f.readline()
x, y = [], []
while line:
    if line.split()[1].split(':')[0] == '10.0.1.11':
        y.append(int(line.split()[6]))
        x.append(float(line.split()[0]))
    line = f.readline()
f.close()
plt.plot(x, y, label='qlen=100')

f = open('qlen-150/cwnd.txt')
line = f.readline()
x, y = [], []
while line:
    if line.split()[1].split(':')[0] == '10.0.1.11':
        y.append(int(line.split()[6]))
        x.append(float(line.split()[0]))
    line = f.readline()
f.close()
plt.plot(x, y, label='qlen=150')

plt.ylabel('CWND(KB)')
plt.legend()
plt.savefig('qlen-crowd.jpg')
plt.show()

########################################
f = open('qlen-50/qlen.txt')
line = f.readline()
x, y = [], []
while line:
    if len(line.split()) > 1:
        x.append(float(line.split()[0].split(',')[0]))
        y.append(int(line.split()[1]))
    line = f.readline()
f.close()

for i in range(1, len(x)):
    x[i] = x[i] - x[0]
x[0] = 0.0
plt.plot(x, y, label='qlen=50')

f = open(path+'qlen.txt')
line = f.readline()
x, y = [], []
while line:
    if len(line.split()) > 1:
        x.append(float(line.split()[0].split(',')[0]))
        y.append(int(line.split()[1]))
    line = f.readline()
f.close()

for i in range(1, len(x)):
    x[i] = x[i] - x[0]
x[0] = 0.0
plt.plot(x, y, label='qlen=100')

f = open('qlen-150/qlen.txt')
line = f.readline()
x, y = [], []
while line:
    if len(line.split()) > 1:
        x.append(float(line.split()[0].split(',')[0]))
        y.append(int(line.split()[1]))
    line = f.readline()
f.close()

for i in range(1, len(x)):
    x[i] = x[i] - x[0]
x[0] = 0.0
plt.plot(x, y, label='qlen=150')

plt.ylabel('QLen: #(Packages)')
plt.legend()
plt.savefig('qlen-QLen.jpg')
plt.show()

########################################
f = open('qlen-50/ping.txt')
line = f.readline()
line = f.readline()
x, y = [], []
while line:
    y.append(float(line.split()[6].split('=')[1]))
    line = f.readline()
f.close()

x = list(range(len(y)))
plt.plot(x, y, label='qlen=50')

f = open(path+'ping.txt')
line = f.readline()
line = f.readline()
x, y = [], []
while line:
    y.append(float(line.split()[6].split('=')[1]))
    line = f.readline()
f.close()

x = list(range(len(y)))
plt.plot(x, y, label='qlen=100')

f = open('qlen-150/ping.txt')
line = f.readline()
line = f.readline()
x, y = [], []
while line:
    y.append(float(line.split()[6].split('=')[1]))
    line = f.readline()
f.close()

x = list(range(len(y)))
plt.plot(x, y, label='qlen=150')

plt.ylabel('RTT(ms)')
plt.legend()
plt.savefig('qlen-RTT.jpg')
plt.show()

########################################
f = open('taildrop/ping.txt')
line = f.readline()
line = f.readline()
x, y = [], []
while line:
    y.append(float(line.split()[6].split('=')[1]))
    line = f.readline()
f.close()

x = list(range(len(y)))
plt.plot(x, y, label='taildrop')

f = open('red/ping.txt')
line = f.readline()
line = f.readline()
x, y = [], []
while line:
    y.append(float(line.split()[6].split('=')[1]))
    line = f.readline()
f.close()

x = list(range(len(y)))
plt.plot(x, y, label='red')

f = open('codel/ping.txt')
line = f.readline()
line = f.readline()
x, y = [], []
while line:
    y.append(float(line.split()[6].split('=')[1]))
    line = f.readline()
f.close()

x = list(range(len(y)))
plt.plot(x, y, label='codel')

plt.ylabel('RTT(ms)')
plt.legend()
plt.savefig('solve.jpg')
plt.show()