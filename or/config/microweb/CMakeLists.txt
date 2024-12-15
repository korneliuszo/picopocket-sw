cmake_minimum_required(VERSION 3.29)

include(FetchContent)

FetchContent_Declare(
  microweb_boot_disk
  URL      https://github.com/jhhoward/MicroWeb/raw/refs/heads/master/floppy/freedos.boot.disk.720K.img
  URL_HASH MD5=33bdbac4d149baea7a44d4263a2fa1e1
  DOWNLOAD_NO_EXTRACT true
)

FetchContent_MakeAvailable(microweb_boot_disk)

add_custom_command(
	OUTPUT ${microweb_boot_disk_BINARY_DIR}/MICROWEB
	WORKING_DIRECTORY ${microweb_boot_disk_BINARY_DIR}
	COMMAND
		mcopy -i ${microweb_boot_disk_SOURCE_DIR}/freedos.boot.disk.720K.img :: MICROWEB
	#places in data
	DEPENDS 
		${microweb_boot_disk_SOURCE_DIR}/freedos.boot.disk.720K.img
)