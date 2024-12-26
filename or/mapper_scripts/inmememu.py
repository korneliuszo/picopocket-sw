#!/usr/bin/env python3

if __name__ == "__main__":
    import sys
    import argparse
    import chssize
    import socket
    import struct
    parser = argparse.ArgumentParser(description='floppy image')
    parser.add_argument("--ip",'-i', help='ip')
    parser.add_argument("--image",'-I', help='image')
    parser.add_argument("--drive",'-d', help='drive',type=lambda x: int(x,0),default=0)

    args = parser.parse_args()
    if (not args.ip):
        print("Provide ip")
        sys.exit(1)
    if (not args.image):
        print("Provide image")
        sys.exit(1)

    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    s.connect((args.ip,5557))
    images=list(args.image.split(":"))
    
    class Image():
        def __init__(self,idx,image):
            self.idx = idx
            self.image = bytearray(open(image,"rb").read())
        def process(self,req,s):
            if req[1] == self.idx:
                lba = struct.unpack("<I",req[2:6])[0]
                if req[0] == 0x01: #read
                    s.send(self.image[lba*512:(lba+1)*512])
                if req[0] == 0x02: #write
                    data = s.recv(512)
                    self.image[lba*512:(lba+1)*512] = data
                    s.send(b'\x01')
        
    image=Image(args.drive,images.pop(0))
    
    s.send(bytes([0])) # no resize of disks_no
    c,h,sc = chssize.chssize(len(image.image))
    s.send(struct.pack("<BBHBBI",image.idx,0xff,c,h,sc,len(image.image)))
    for i in range(1,10):
        s.send(struct.pack("<BBHBBI",0xFF,0x00,0,0,0,0))
    
    while True:
        try:
            req=s.recv(6)
        except KeyboardInterrupt:
            image=Image(args.drive,images.pop(0))
            print("Nextimage")
            continue
        image.process(req,s)
