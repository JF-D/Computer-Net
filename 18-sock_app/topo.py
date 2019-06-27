#!/usr/bin/python

from mininet.node import OVSBridge
from mininet.net import Mininet
from mininet.topo import Topo
from mininet.link import TCLink
from mininet.cli import CLI

nworkers = 2

class MyTopo(Topo):
    def build(self):
        s1 = self.addSwitch('s1')

        hosts = list()
        for i in range(1, nworkers+2):
            h = self.addHost('h' + str(i))
            hosts.append(h)

        for h in hosts:
            self.addLink(h, s1)

if __name__ == '__main__':
    topo = MyTopo()
    net = Mininet(topo = topo, switch = OVSBridge, controller = None)

    h1, h2, h3 = net.get('h1', 'h2', 'h3')
    for h in (h1, h2, h3):
        h.cmd('scripts/disable_offloading.sh')
        h.cmd('scripts/disable_tcp_rst.sh')
        h.cmd('scripts/disable_ipv6.sh')
    
    net.start()
    CLI(net)
    net.stop()
