#!/usr/bin/env python3

import monitor
import inmememu

if __name__ == "__main__":
    import sys
    import argparse
    parser = argparse.ArgumentParser(description='Display image')
    parser.add_argument("--ip",'-i', help='ip')
    parser.add_argument("--image",'-I', help='image')
    parser.add_argument("--kernel",'-k', help='kernel')

    args = parser.parse_args()
    if (not args.ip):
        print("Provide ip")
        sys.exit(1)
    if (not args.image):
        print("Provide image")
        sys.exit(1)

    m = monitor.monitor(args.ip)
    i = open(args.kernel,"rb").read()
    
    m.putmem(0x60,0,i)
    m.chain(0x00600000)
    
    params = m.get_called_params()
    params["bx"] = 0x00
    m.set_called_params(params)
    fdd = inmememu.inmememu(m,0x00,args.image)

    chain=m.install_13h()
    m.return_boot()

    while True:
        m.chain(chain)
        fdd.process()
        m.return_boot()    
    