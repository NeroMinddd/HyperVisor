# HyperVisor
Intel VT-x Hypervisor (Windows x64)

Just a scratch-built thin hypervisor for Intel x86-64 (VT-x), written as a Windows kernel driver (WDM). Built this from the ground up to get hands-on experience with hardware virtualization, EPT (Extended Page Tables), and trapping low-level system events.

Build & Test Setup

• Target OS: Windows 10 / 11 x64
• Toolchain: Visual Studio + WDK
• Testing: VMware Workstation (Nested Virtualization) + WinDbg over KDNET

Note

Purely a personal research project to dig deeper into Intel CPU internals and Windows kernel architecture.
