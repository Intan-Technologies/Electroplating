# Electroplating
Intan Electroplating is free software that controls an RHD Electroplating Board and an Intan 128-channel headstage to electroplate. This software allows for automatic and manual plating of all 128 channels, either individually or in a group. With update 1.03, the Windows release of this software includes an installer that automatically installs prerequisites (Opal Kelly drivers, Microsoft Visual C++ Redistributables) to make use of this software easier.

The most recent binaries are available from the Intan website: https://intantech.com or here on GitHub under Releases:

* Electroplating.exe -> Window 64-bit installer (Wix Burn bootstrapper application that guides the user through installation)

* Electroplating.dmg -> MacOS 64-bit disk image

These binaries were built with Qt 5.12

While developers are free to download the source code or fork their own repositories if they wish to make changes to their own versions, we will generally not integrate any of these changes to a public release. If there are features you'd like to see in an official Intan release or you find a bug, please send us feedback!  Thank  you!

# Steps To Run Software

## All Platforms:

Various files need to be present in the same directory as the binary executable at runtime. These include
* main.bit
* okFrontPanel.dll

### Windows:

The RHX software depends on Opal Kelly USB drivers and Microsoft Redistributables. When running the distributed Windows installer from the Intan website, these are automatically installed, but when building RHX from source, these should still be installed on the system. Opal Kelly USB drivers should be installed so that the Intan hardware can communicate via USB. These are available at: https://intantech.com/files/Intan_controller_USB_drivers.zip. These also rely on the Microsoft Visual C++ Redistributables (x64) from 2010, 2013, and 2015-2019, which are available from Microsoft and should also be installed prior to running IntanRHX. Finally, okFrontPanel.dll (found in the libraries directory) should be in the same directory as the binary executable at runtime.

### Mac:

libokFrontPanel.dylib should be in a directory called "Frameworks" alongside the MacOS directory within the built IntanRHX.app. Running macdeployqt on this application will also populate this directory with required Qt libraries.
