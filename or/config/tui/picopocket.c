#include "dflat.h"

char DFlatApplication[] = "PicoPocket-sw configurator";

extern DBOX Log;


static int MainProc(WINDOW wnd,MESSAGE msg,PARAM p1,PARAM p2)
{
	int rtn;
    return DefaultWndProc(wnd, msg, p1, p2);

}

void main(int argc, char *argv[])
{
    WINDOW wnd;
    if (!init_messages())
		return;
    Argv = argv;
    wnd = CreateWindow(APPLICATION,
                        DFlatApplication,
                        0, 0, -1, -1,
						&MainMenu,
                        NULL,
                        MainProc,
                        HASSTATUSBAR
                        );
    SendMessage(wnd, SETFOCUS, TRUE, 0);
    while (dispatch_message());
}
