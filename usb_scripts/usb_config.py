#!/usr/bin/env python3

import usb.core
import usb.util
import array

class USB_Config:
    def __init__(self):
        dev = usb.core.find(idVendor=0xcafe, idProduct=0x4000)
        if dev is None:
            raise ValueError('Device not found')
        dev.set_configuration()
        cfg = dev.get_active_configuration()
        itf=[item for item in cfg.interfaces() if item.bInterfaceClass==255 and item.bInterfaceSubClass==67 and item.bInterfaceProtocol==0][0]
        self.dev = dev
        self.bInterfaceNumber = itf.bInterfaceNumber
                
    def get_name(self,uid):
        return self.dev.ctrl_transfer(usb.util.CTRL_TYPE_CLASS|usb.util.CTRL_RECIPIENT_INTERFACE|usb.util.CTRL_IN,2,uid,self.bInterfaceNumber,1024).tobytes().decode("ascii")
    def get_help(self,uid):
        return self.dev.ctrl_transfer(usb.util.CTRL_TYPE_CLASS|usb.util.CTRL_RECIPIENT_INTERFACE|usb.util.CTRL_IN,3,uid,self.bInterfaceNumber,1024).tobytes().decode("ascii")
    def get_type(self,uid):
        b=self.dev.ctrl_transfer(usb.util.CTRL_TYPE_CLASS|usb.util.CTRL_RECIPIENT_INTERFACE|usb.util.CTRL_IN,4,uid,self.bInterfaceNumber,3)
        return {
            "to_flash":bool(b[0]),
            "coldboot_required":bool(b[1]),
            "is_directory":bool(b[2]),
            }
    def get_val(self,uid):
        return self.dev.ctrl_transfer(usb.util.CTRL_TYPE_CLASS|usb.util.CTRL_RECIPIENT_INTERFACE|usb.util.CTRL_IN,5,uid,self.bInterfaceNumber,1024).tobytes()
    def set_val(self,uid,val):
        v=val+bytes([0])
        self.dev.ctrl_transfer(usb.util.CTRL_TYPE_CLASS|usb.util.CTRL_RECIPIENT_INTERFACE|usb.util.CTRL_OUT,6,uid,self.bInterfaceNumber,v)
    def reset_val(self,uid):
        return self.dev.ctrl_transfer(usb.util.CTRL_TYPE_CLASS|usb.util.CTRL_RECIPIENT_INTERFACE|usb.util.CTRL_OUT,7,uid,self.bInterfaceNumber,None)
    def load_flash(self):
        return self.dev.ctrl_transfer(usb.util.CTRL_TYPE_CLASS|usb.util.CTRL_RECIPIENT_INTERFACE|usb.util.CTRL_OUT,8,0,self.bInterfaceNumber,None)
    def save_flash(self):
        return self.dev.ctrl_transfer(usb.util.CTRL_TYPE_CLASS|usb.util.CTRL_RECIPIENT_INTERFACE|usb.util.CTRL_OUT,8,1,self.bInterfaceNumber,None)
               
    def Reset(self):
        self.dev.ctrl_transfer(usb.util.CTRL_TYPE_CLASS|usb.util.CTRL_RECIPIENT_INTERFACE|usb.util.CTRL_IN,0,0,self.bInterfaceNumber,0)
if __name__ == "__main__":

    import sys
    from PySide6.QtUiTools import QUiLoader
    from PySide6.QtWidgets import QApplication, QTreeWidgetItem
    from PySide6.QtCore import QFile, QIODevice

    def tomodel(self,uid):
        #assume uid is dir
        
        parent1 = QTreeWidgetItem()
        parent1.setText(0,("0x%04X "%(uid,)) + self.get_name(uid))
        parent1._uid = uid
        if self.get_type(uid)['is_directory']:    
            chlds = array.array("H") #happy le world
            chlds.frombytes(self.get_val(uid))
            for chld in chlds:
                parent1.addChild(tomodel(self,chld))
        return parent1        

    app = QApplication(sys.argv)

    config = USB_Config()

    ui_file_name = "config.ui"
    ui_file = QFile(ui_file_name)
    if not ui_file.open(QIODevice.ReadOnly):
        print(f"Cannot open {ui_file_name}: {ui_file.errorString()}")
        sys.exit(-1)
    loader = QUiLoader()
    window = loader.load(ui_file)
    ui_file.close()
    if not window:
        print(loader.errorString())
        sys.exit(-1)
    window.Reset.clicked.connect(config.Reset)
          
    window.treeView.insertTopLevelItem(0,tomodel(config,0))
    window.treeView.expandAll()
    
    def read_val():
        sel_uid = window.treeView.selectedItems()[0]._uid
        if config.get_type(sel_uid)['is_directory']:
            window.value.setText("!!DIR!!")
            window.value.setReadOnly(True)
            window.setValue.setEnabled(False)
            window.resetValue.setEnabled(False)
        else:
            window.value.setText(config.get_val(sel_uid).decode("ascii"))
            window.value.setReadOnly(False)
            window.setValue.setEnabled(True)
            window.resetValue.setEnabled(True)
                    
    def read_all():
        sel_uid = window.treeView.selectedItems()[0]._uid
        read_val()
        conf = config.get_type(sel_uid)
        window.is_directory.setChecked(conf['is_directory'])
        window.to_flash.setChecked(conf['to_flash'])
        window.coldboot_required.setChecked(conf['coldboot_required'])
        window.textBrowser.setText(config.get_help(sel_uid))
             
    window.treeView.itemSelectionChanged.connect(read_all)

    def set_val():
        sel_uid = window.treeView.selectedItems()[0]._uid        
        config.set_val(sel_uid,window.value.text().encode("ascii"))
        read_val()
    
    window.setValue.clicked.connect(set_val)
  
    def load_flash():
        config.load_flash()
        read_val()
  
    window.LoadFlash.clicked.connect(load_flash)
    window.SaveFlash.clicked.connect(config.save_flash)
 
    def reset_value():
        sel_uid = window.treeView.selectedItems()[0]._uid        
        config.reset_val(sel_uid)
        read_val()
        
    window.resetValue.clicked.connect(reset_value)
        
    window.show()

    sys.exit(app.exec())