librtl2832++ by Balint Seeber (http://spench.net/)

The output DLL enables you to control your RTL2832-based device on Windows.

libusb-1 is required to compile the DLL. Please ensure you have it installed.
You should add the path to the "libusb-1.0/libusb.h" header into your VS global Include directory list.
Then you should update the Linker's "Additional Library Directories" to where "libusb-1.0.lib" can be found. 
Currently it is set to: "C:\Dev\SDK\libusb-pbatard\Win32\Release\dll"
