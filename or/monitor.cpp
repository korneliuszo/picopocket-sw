#include "or_internal.hpp"
#include "config.hpp"
#include "fifo.hpp"
#include <cassert>
#ifdef PICOPOCKET_SIM
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/fcntl.h>
#include <netinet/in.h>
#include <unistd.h>
#endif

extern const
OROMHandler monitor_handler;

#ifdef PICOPOCKET_SIM
using PCB = int;
struct Packet
{
	Packet * next;
	uint8_t * payload;
	size_t tot_len;
	size_t len;
};
#endif

struct Recv_state {
	Fifo<uint8_t,2048> recvbuff;
	Fifo<uint8_t,2048> sendbuff;
};

static volatile PCB lsock;
static Recv_state crstate;



#ifdef PICOPOCKET_SIM
static void* recv_thread(void* arg)
{
	while(1)
	{
		PCB connfd = accept4(lsock, nullptr, nullptr, 0);
		assert(connfd>=0);

		while (1)
		{
			{
				auto buff = crstate.recvbuff.get_wrbuff();
				int len = recv(connfd, (void*) buff.start,
						buff.stop - buff.start,
						MSG_DONTWAIT);
				if (len == -1 && (errno != EAGAIN && errno != EWOULDBLOCK))
					break;
				if (len > 0)
					crstate.recvbuff.commit_wrbuff(len);
			}
			while(crstate.sendbuff.fifo_check()){
				auto buff = crstate.sendbuff.get_rdbuff();
				int len = send(connfd, (void*) buff.start,
						buff.stop - buff.start,
						0);
				if (len < 0)
					break;
				if (len > 0)
					crstate.sendbuff.commit_rdbuff(len);
			}
		}
		close(connfd);
	}
}
#endif

void monitor_install(Thread * main)
{
#ifdef PICOPOCKET_SIM
	lsock = socket(AF_INET, SOCK_STREAM, 0);
	if (lsock < 0)
		exit(1);

	int reuse = 1;
	if(setsockopt(lsock,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse))!=0)
		exit(1);
	struct sockaddr_in servaddr;
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(5555);
    if ((bind(lsock, (sockaddr*)&servaddr, sizeof(servaddr))) != 0)
		exit(1);
    if ((listen(lsock, 2)) != 0) {
		exit(1);
    }

    pthread_t ptid;

    // Creating a new thread
    pthread_create(&ptid, NULL, &recv_thread, NULL);
#endif
}



static bool monitor_decide(const ENTRY_STATE & state)
{
	if(state.entry == 0) //boot
		return false;
	return true;
}

static void monitor_entry (Thread_SHM * thread)
{
	bool autoboot = Config::MONITOR_AUTOBOOT::val.ival;
	thread->putstr("PicoPocket monitor: press M to ");
	if(autoboot)
		thread->putstr("stop ");
	thread->putstr("entry\r\n");
	if((thread->getch() != 'm') ^ autoboot)
		thread->callback_end(); //nonreturn
	thread->putstr("Monitor entry\r\n");

	while(1)
	{
		while(crstate.recvbuff.fifo_check()<9)
			thread->yield();
		volatile uint32_t slen,rlen;
		volatile uint8_t cmd;
		{
			auto lenbuff = crstate.recvbuff.get_rdbuff();
			slen = *lenbuff[0] | (*lenbuff[1]<<8) | (*lenbuff[2]<<16) | (*lenbuff[3]<<24);
			rlen = *lenbuff[4] | (*lenbuff[5]<<8) | (*lenbuff[6]<<16) | (*lenbuff[7]<<24);
			cmd = *lenbuff[8];
		}
		crstate.recvbuff.commit_rdbuff(9);
		while(crstate.recvbuff.fifo_check()<slen)
			thread->yield();
		while(crstate.sendbuff.fifo_free()<rlen)
			thread->yield();
		switch(cmd)
		{
		case 0xff: //entry
		 {
			auto wbuff = crstate.sendbuff.get_wrbuff();

			wbuff.put_bytes(&thread->entry.entry,1,0);
			wbuff.put_bytes(&thread->entry.irq_no,1,1);
			wbuff.put_bytes(thread->entry.regs.data,sizeof(thread->entry.regs.data),2);
		 }
		 break;
		default:
		 {
			auto rbuff = crstate.recvbuff.get_rdbuff();
			rbuff.set_len(slen);
			auto wbuff = crstate.sendbuff.get_wrbuff();
			wbuff.set_len(rlen);
			BuffList req[4] = {
					{
						const_cast<uint8_t*>(rbuff.start),
						rbuff.stop - rbuff.start,
						&req[1]
					},
					{
						const_cast<uint8_t*>(rbuff.start2),
						rbuff.stop2 - rbuff.start2,
						nullptr
					},
					{
						const_cast<uint8_t*>(wbuff.start),
						wbuff.stop - wbuff.start,
						&req[3]
					},
					{
						const_cast<uint8_t*>(wbuff.start2),
						wbuff.stop2 - wbuff.start2,
						nullptr
					},
			};
			if(!req[1].len)
				req[0].next = nullptr;
			if(!req[3].len)
				req[2].next = nullptr;

			{
				thread->cmd.send = req[0].len ? &req[0] : nullptr;
				thread->cmd.recv = req[2].len ? &req[2] : nullptr;
				thread->cmd.command = cmd;
				thread->wait_for_exec();
			}
		  }
		 break;
		}
		crstate.recvbuff.commit_rdbuff(slen);
		crstate.sendbuff.commit_wrbuff(rlen);
	}

	thread->callback_end();

}

OROMHandler_type_section
OROMHandler monitor_handler = {
		monitor_decide,
		monitor_entry
};
