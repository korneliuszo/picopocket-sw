
all : pocket.exe .symbolic
	echo all done

MODEL = l
#------------------------------------------------
COMPILE = wcl -os -c -dWATCOM -dBUILD_FULL_DFLAT -j -c -w4 -s -m$(MODEL) -Idflat20/
#------------------------------------------------

.c.o:
    $(COMPILE) $*.c
 
dflat20/dflat.lib: .symbolic
	cd dflat20
	wmake -f makefile.wat dflat.lib
	cd ..

pocket.exe : picopocket.o dialogs.o menus.o config_iface.o dflat20/dflat.lib
     wcl picopocket.o dialogs.o menus.o config_iface.o dflat20/dflat.lib -k8192 -fe=pocket.exe

