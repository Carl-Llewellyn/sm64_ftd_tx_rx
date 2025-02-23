package main

/*
#cgo LDFLAGS: -lftdi1 -lusb-1.0
#include <ftdi.h>
#include <stdlib.h>
*/
import "C"
import (
	"fmt"
	"unsafe"
)

func main() {
	// Create a new FTDI context
	ftdi := C.ftdi_new()
	if ftdi == nil {
		fmt.Println("ftdi_new failed")
		return
	}
	defer C.ftdi_free(ftdi)

	// Find all FTDI devices (0, 0 matches any vendor/product)
	var devList *C.struct_ftdi_device_list
	ret := C.ftdi_usb_find_all(ftdi, &devList, 0, 0)
	if ret < 0 {
		fmt.Printf("ftdi_usb_find_all failed: %d (%s)\n", int(ret), C.GoString(C.ftdi_get_error_string(ftdi)))
		return
	}
	deviceCount := int(ret)
	fmt.Printf("Devices: %d\n", deviceCount)

	// Iterate over the device list (it's a linked list)
	i := 0
	for cur := devList; cur != nil; cur = cur.next {
		// Allocate buffers for the strings (128 bytes each is usually enough)
		manufacturer := make([]C.char, 128)
		description := make([]C.char, 128)
		serial := make([]C.char, 128)

		ret = C.ftdi_usb_get_strings(ftdi, cur.dev,
			(*C.char)(unsafe.Pointer(&manufacturer[0])), C.int(len(manufacturer)),
			(*C.char)(unsafe.Pointer(&description[0])), C.int(len(description)),
			(*C.char)(unsafe.Pointer(&serial[0])), C.int(len(serial)))
		if ret < 0 {
			fmt.Printf("ftdi_usb_get_strings failed: %d (%s)\n", int(ret), C.GoString(C.ftdi_get_error_string(ftdi)))
		} else {
			fmt.Printf("Device %d:\n", i)
			fmt.Printf("  Manufacturer: %s\n", C.GoString(&manufacturer[0]))
			fmt.Printf("  Description:  %s\n", C.GoString(&description[0]))
			fmt.Printf("  Serial:       %s\n", C.GoString(&serial[0]))
		}
		i++
	}

	// Free the device list
	C.ftdi_list_free(&devList)
}
