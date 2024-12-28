

.PHONY: clean all default

THIS_DIR := $(dir $(abspath $(firstword $(MAKEFILE_LIST))))

default: optionrom.bin

all: default

HEADERS = 
HEADERS_TARGETS = $(addprefix $(THIS_DIR),$(HEADERS))

%.obj : ${THIS_DIR}%.c $(HEADERS_TARGETS)
	cd ${THIS_DIR}; \
	wcc -3 -wx -s -oatx -hd -d1 -ms -zu -za99 -fo$(CURDIR)/$@ $<

%.obj : ${THIS_DIR}%.S 
	cd ${THIS_DIR}; \
	wasm -3 -wx -ms -fo$(CURDIR)/$@ $<


optionrom.bin: main.obj init.obj ${THIS_DIR}link.lnk
	wlink name $@ @${THIS_DIR}link.lnk
	${THIS_DIR}checksum.py $@
