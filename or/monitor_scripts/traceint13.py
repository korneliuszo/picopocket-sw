#!/usr/bin/env python3

import monitor

if __name__ == "__main__":
    import sys
    import argparse
    parser = argparse.ArgumentParser(description='Display image')
    parser.add_argument("--ip",'-i', help='ip')

    args = parser.parse_args()
    if (not args.ip):
        print("Provide ip")
        sys.exit(1)
        
    m=monitor.monitor(args.ip)
    chain = m.install_13h()
    m.return_boot()
    while True:
        print(m.get_called_params())
        m.chain(chain)
        m.return_boot()
        
        
        