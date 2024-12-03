#include <ioiface.hpp>
#include <isa_worker.hpp>
#ifndef PICOPOCKET_SIM
#include "pico/platform.h"
#endif

extern "C" IoIface::Handler __start_ioiface_handlers;
extern "C" IoIface::Handler __stop_ioiface_handlers;


//core1 only variables!!!
IoIface::OHandler * curr_handler[4];
int nop_stack_of_unrecognized[4];

static uint32_t read_fn(void* obj, uint32_t faddr)
{
	uint8_t pipe = faddr>>1;
	bool ctrl = faddr&0x01;
	if(ctrl)
		return ISA_FAKE_READ;
	else
	{
		if (curr_handler[pipe])
			return curr_handler[pipe]->rd_byte();
		else
			return ISA_FAKE_READ;
	}
}

static void write_fn(void* obj, uint32_t faddr, uint8_t data)
{
	uint8_t pipe = faddr>>1;
	bool ctrl = faddr&0x01;
	if(ctrl)
	{

		uint8_t find_ec = data;
		IoIface::OHandler * hndlr = curr_handler[pipe];

		if(data == 0x00)
		{
			if(!hndlr)
			{
				if (nop_stack_of_unrecognized[pipe])
				{
					nop_stack_of_unrecognized[pipe]--;
					return;
				}
#ifndef PICOPOCKET_SIM
				__breakpoint();
#endif
				// if your debugger stops here
				// pc code behaves wrong as it pops from empty list
				return;
			}
			if(hndlr->stack_of_unrecognized)
				hndlr->stack_of_unrecognized--;
			else
			{
				IoIface::OHandler * prev = hndlr->prev;
				hndlr->ec().release_object(hndlr);
				curr_handler[pipe] = prev;
			}
		}
		else
		{
			IoIface::Handler *itr=&__start_ioiface_handlers;
			while(itr < &__stop_ioiface_handlers)
			{
				if(itr->ec == data)
				{
					IoIface::OHandler * nwhndlr = itr->acquire_object(pipe);
					nwhndlr->stack_of_unrecognized = 0;
					nwhndlr->prev = hndlr;
					curr_handler[pipe] = nwhndlr;
					return;
				}
				itr++;
			}
			if(hndlr)
			{
				hndlr->stack_of_unrecognized++;
			}
			else
			{
				nop_stack_of_unrecognized[pipe]++;
			}
		}


	}
	else
	{
		if (curr_handler[pipe])
			curr_handler[pipe]->wr_byte(data);
	}
}

IoIface::Handler_return_t IoIface::response_byte(uint8_t byte)
{
	return byte;
}
IoIface::Handler_return_t IoIface::response_tristate()
{
	return ISA_FAKE_READ;
}

void IoIface::ioiface_install()
{
	add_device({
					.start = 0x88,
					.size = (uint32_t)8,
					.type = Device::Type::IO,
					.rdfn = read_fn,
					.wrfn = write_fn,
					.obj = nullptr,
	});
}

