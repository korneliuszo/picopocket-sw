#include "dflat.h"
#include "config_iface.h"
#include <assert.h>

char DFlatApplication[] = "PicoPocket-sw configurator";

WINDOW pico_list;
WINDOW main_wnd;

extern DBOX DeviceConfig;

void PrintDeviceLastID(char name_buff[8*2+1])
{
	sprintf(name_buff,"%02X%02X%02X%02X%02X%02X%02X%02X - ",
			LastID[0],
			LastID[1],
			LastID[2],
			LastID[3],
			LastID[4],
			LastID[5],
			LastID[6],
			LastID[7]
			);
}

typedef struct {
	uint16_t uid;
} Options;

typedef struct __packed__ {
	uint8_t to_flash;
	uint8_t coldboot_required;
	uint8_t is_directory;
} Type;

Options * options;
uint16_t options_len;

static unsigned RecurseTree(unsigned len, uint16_t uid)
{
	Type * type;
	options[len].uid = uid;
	len +=1;

	SendCmd(4);
	SendCommandUID(uid);
	type = (Type *)RecvString();
	assert(type);

	if(type->is_directory)
	{
		uint16_t ulen;
		uint16_t * uids;
		unsigned i;

		SendCmd(5);
		SendCommandUID(uid);

		ulen = RecvU16();
		uids = (uint16_t *)RecvData((char*)malloc(ulen),ulen);
		ulen = ulen / sizeof(uint16_t);
		for(i=0;i<ulen;i++)
		{
			len=RecurseTree(len,uids[i]);
		}
		free(uids);
	}
	free(type);
	return len;
}

static void FillOptions()
{
	uint16_t recurse_len;

	SendCmd(1);
	options_len = RecvU16();

	options = (Options*)calloc(options_len,sizeof(Options));
	assert(options);

	recurse_len = RecurseTree(0,0);
	assert(recurse_len==options_len);
}

void RedaoutDevice(WINDOW wnd)
{
	int i;
	CTLWINDOW * ct;
	Type * type;
    ct = FindCommand(wnd->extension, 101, LISTBOX);
    i = SendMessage(ct->wnd, LB_CURRENTSELECTION, 0, 0);

    Select(LastID);

    SendCmd(4);
    SendCommandUID(options[i].uid);
    type = (Type *)RecvString();
    assert(type);

    if(type->coldboot_required)
		PutItemText(wnd, 106, "Runtime: [ ]");
    else
		PutItemText(wnd, 106, "Runtime: [X]");

    if(type->to_flash)
		PutItemText(wnd, 105, "Saved:   [X]");
    else
		PutItemText(wnd, 105, "Saved:   [ ]");

    if(type->is_directory)
    {
		PutItemText(wnd, 102, "<DIR>");
		PutItemText(wnd, 107, "DIR:     [X]");
    }
    else
    {
    	char * val;
		SendCmd(5);
		SendCommandUID(options[i].uid);
		val = RecvString();
		PutItemText(wnd, 102, val);
		PutItemText(wnd, 107, "DIR:     [ ]");
		free(val);
    }
    Release();
    free(type);
}

void SetDevice(WINDOW wnd)
{
	int i;
	CTLWINDOW * ct;
    ct = FindCommand(wnd->extension, 101, LISTBOX);
    i = SendMessage(ct->wnd, LB_CURRENTSELECTION, 0, 0);

    Select(LastID);
    {
		SendCmd(6);
		SendCommandUID(options[i].uid);
		{
			char buff[100];
			GetItemText(wnd,102,buff,sizeof(buff));
			buff[sizeof(buff)-1]=0;
			SendString(buff);
		}
    }
    Release();
}


static int DeviceProc(WINDOW wnd,MESSAGE msg,PARAM p1,PARAM p2)
{
	int rtn;
    switch (msg)    {
		case CREATE_WINDOW:
		{
			char name_buff[8*2+3+31];
			char * name;
			unsigned i;
		    rtn = DefaultWndProc(wnd, msg, p1, p2);
	    	PrintDeviceLastID(name_buff);

		    Select(LastID);

		    SendCmd(0);
	    	name = RecvString();
	    	strcat(name_buff,name);
	    	free(name);

            AddAttribute(wnd, HASTITLEBAR);
		    InsertTitle(wnd,name_buff);

		    FillOptions();

		    for(i=0;i<options_len;i++)
		    {
		    	char * attrname;
				SendCmd(3);
				SendCommandUID(options[i].uid);
				attrname = RecvString();
				PutItemText(wnd, 101, attrname);
				free(attrname);
		    }

		    Release();

		    RedaoutDevice(wnd);

			return rtn;
		}

		case COMMAND:
			if ((int) p1 == ID_CANCEL && (int) p2 == 0)	{
			    free(options);
			}
			if ((int) p1 == ID_OK && (int) p2 == 0)	{
			    return TRUE;
			}
			if (p1 == 101 && p2 == LB_SELECTION)
			{
				RedaoutDevice(wnd);
			}
			if ((int) p1 == 104 && (int) p2 == 0)	{
				RedaoutDevice(wnd);
			}
			if ((int) p1 == 103 && (int) p2 == 0)	{
				SetDevice(wnd);
			    RedaoutDevice(wnd);
			}
			if ((int) p1 == 108 && (int) p2 == 0)	{
				Select(LastID);
				SendCmd(7);
				Release();
			}
			if ((int) p1 == 109 && (int) p2 == 0)	{
				Select(LastID);
				SendCmd(8);
				Release();
			}
			break;
        default:
            break;
	}
    return DefaultWndProc(wnd, msg, p1, p2);

}

static int MainProc(WINDOW wnd,MESSAGE msg,PARAM p1,PARAM p2)
{
	if(msg == LB_CHOOSE)
	{
		int i;
	    ResetSelect();
		SelectNext();
	    for(i=1;i<p1;i++)
	    {
	    	Release();
			SelectNext();
	    }
    	Release();

		DialogBox(wnd,&DeviceConfig,TRUE,DeviceProc);
		return TRUE;
	}
	return DefaultWndProc(wnd, msg, p1, p2);
}

void main(int argc, char *argv[])
{
    if (!init_messages())
		return;
    Argv = argv;
    main_wnd = CreateWindow(APPLICATION,
                        DFlatApplication,
                        0, 0, -1, -1,
						&MainMenu,
                        NULL,
                        MainProc,
                        HASSTATUSBAR
                        );
    pico_list = CreateWindow(LISTBOX,
            "Detected pico",
            0, 0, -1, -1,
			NULL,
            main_wnd,
			ListBoxProc,
            0
            );
    ResetSelect();
    while(SelectNext())
    {
    	char buff[8*2+3+31];
    	char *name;
    	PrintDeviceLastID(buff);
    	SendCmd(0);
    	name = RecvString();
    	strcat(buff,name);
    	free(name);
    	Release();
        SendMessage(pico_list, ADDTEXT, (long)buff, 0);
    }
    SendMessage(pico_list, SETFOCUS, TRUE, 0);
    while (dispatch_message());
}
