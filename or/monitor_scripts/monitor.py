#!/usr/bin/env python3

import socket
import struct
import copy

class monitor():
    def __init__(self,ip):
        self.s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.s.connect((ip,5555))
        
    def msg(self,cmd,sdata,rlen):
        self.s.send(struct.pack("<IIB",len(sdata),rlen,cmd))
        self.s.send(sdata)
        if(rlen>0):
            return self.s.recv(rlen,socket.MSG_WAITALL)
        return
        
    def sync(self):
        return self.msg(0x09,b'',1)
    def outb(self,port,val):
        self.msg(0x04,struct.pack(">HB",port,val),0)
        return
    def inb(self,port):
        r=self.msg(0x05,struct.pack(">H",port),1)
        return r[0]
    def putmem_page(self,seg,addr,data):
        self.msg(0x06,struct.pack(">HHH",seg,addr,len(data))+data,0)
        return
    def putmem(self,seg,addr,data):
        [self.putmem_page(seg,addr+i,data[i:i+1024]) for i in range(0, len(data), 1024)]
        return
    def getmem_page(self,seg,addr,l):
        return self.msg(0x07,struct.pack(">HHH",seg,addr,l),l)
    def getmem(self,seg,addr,l):
        return b"".join([self.getmem_page(seg,addr+i,min(1024,l-i)) for i in range(0, l, 1024)])
    def emptyregs(self):
        return copy.copy({"ax":0,"bx":0,"cx":0,"dx":0,
                "si":0,"di":0,"fl":0,
                "ds":0,"es":0})
    def stackcode8(self,regs,code):
        assert(len(code)<=8)
        code += bytes([0xa5])*(8-len(code))
        req = struct.pack("<HHHHHHHHH",
                regs["es"],
                regs["ds"],
                regs["di"],
                regs["si"],
                regs["bx"],
                regs["dx"],
                regs["cx"],
                regs["ax"],
                regs["fl"])
        ret = self.msg(0x01,req+code,9*2)   
        keys = ("es","ds","di","si","bx","dx","cx","ax","fl")
        values = struct.unpack("<HHHHHHHHH",ret)
        d = dict(zip(keys,values))
        return d
        
    def irq(self,irq,regs):
        code = bytes([0xCD, irq, 0xCB])
        return self.stackcode8(regs,code)
    def get_called_params(self):
        ret=self.msg(0xff,b'',1+1+15*2+1)
        keys = ("entry","irq","es","ds","di","si","bp","bx","dx","cx","ax", "fl", "chain", "ip", "cs","rf","retcode")
        values = struct.unpack("<BBHHHHHHHHHHIHHHB",ret)
        d = dict(zip(keys,values))
        return d
    def wait_for_isr(self):
        return self.get_called_params()      
    def set_called_params(self,regs):
        cmd = struct.pack("<HHHHHHHHHHIHHHB",
                regs["es"],
                regs["ds"],
                regs["di"],
                regs["si"],
                regs["bp"],
                regs["bx"],
                regs["dx"],
                regs["cx"],
                regs["ax"],
                regs["fl"],
                regs["chain"],
                regs["ip"],
                regs["cs"],
                regs["rf"],
                regs["retcode"]
                )
        self.msg(0x02,cmd,0)

    def install_13h(self):
        return struct.unpack("<I",self.msg(0x08,struct.pack("<B",0x13),4))[0]
    def return_boot(self):
        self.msg(0x03,b'',0)
    def chain(self,entry):
        params = self.get_called_params()
        params["chain"] = entry
        params["retcode"] = 0
        self.set_called_params(params)

    def jump_to_ram(self,orig_cs,target_cs):
        bios = self.getmem(orig_cs,0,2048)
        self.putmem(target_cs,0,bios)
        irqentry =  bios[0x16] | (bios[0x17]<<8)
        self.chain((target_cs<<16) | (irqentry+3*0x19)<<0)
        self.return_boot()
