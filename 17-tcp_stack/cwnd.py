# -*- coding: utf-8 -*-
"""
Created on Wed Jun 26 23:56:39 2019

@author: DJF
"""

import matplotlib.pyplot as plt
line = open('cwnd.dat').readlines()
p = []
for i in range(len(line)):
    if int(line[i].split()[0]) != 1460:
        p.append(line[i])
x = list(range(len(p)))
for i in range(len(p)):
    x[i]    = int(p[i].split()[1])
    p[i] = int(p[i].split()[0])
plt.plot(x, p)