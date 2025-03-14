#!/usr/bin/env python3

import monitor
from PIL import Image
import threading
from timebudget import timebudget

class cga_06():
    def __init__(self,ip):
        self.m= monitor.monitor(ip)
        r=self.m.emptyregs()
        r["ax"] = 0x0006
        self.m.irq(0x10,r)
    def resolution(self):
        return (640,200) 
    def display(self,i):
        re=bytearray(100*80)
        ro=bytearray(100*80)
        im = i.tobytes()
        for y in range(200):
            for x in range(0,640,8):
                    if(y%2):
                        ro[(y//2)*80+x//8]=(im[y*(640//8)+x//8])
                    else:
                        re[(y//2)*80+x//8]=(im[y*(640//8)+x//8])
            
        with timebudget("io"):
            self.m.putmem(0xB800,0,re)
            self.m.putmem(0xBA00,0,ro)
    
if __name__ == "__main__":
    import sys
    import argparse
    timebudget.set_quiet()  # don't show measurements as they happen
    timebudget.report_at_exit()  # Generate report when the program exits
    parser = argparse.ArgumentParser(description='Display image')
    parser.add_argument("--ip",'-i', help='ip')
    parser.add_argument("--image",'-I', help='image')

    args = parser.parse_args()
    if (not args.ip):
        print("Provide ip")
        sys.exit(1)
    if (not args.image):
        print("Provide image")
        sys.exit(1)

    i = Image.open(args.image)
    cga = cga_06(args.ip)
    
    i=i.resize(cga.resolution())
    i=i.convert("1")
    cga.display(i)        
    cga.m.sync()
    
