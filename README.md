# HyperVisor
Intel VT-x Hypervisor (Windows x64)

Just a scratch-built thin hypervisor for Intel x86-64 (VT-x), written as a Windows kernel driver (WDM). Built this from the ground up to get hands-on experience with hardware virtualization, EPT (Extended Page Tables), and trapping low-level system events.

Build & Test Setup

• Target OS: Windows 10 / 11 x64
• Toolchain: Visual Studio + WDK
• Testing: VMware Workstation (Nested Virtualization) + WinDbg over KDNET

Note

Purely a personal research project to dig deeper into Intel CPU internals and Windows kernel architecture.


Гипервизор Intel VT-x (Windows x64)

Небольшой тонкий гипервизор под Intel x86-64 (VT-x), написанный с нуля в виде драйвера ядра Windows (WDM). Делал проект для себя, чтобы на практике разобраться в аппаратной виртуализации, EPT (Extended Page Tables) и перехвате низкоуровневых системных событий.

Сборка и тестирование

• Целевая ОС: Windows 10 / 11 x64
• Инструменты: Visual Studio + WDK
• Тестирование: VMware Workstation (с вложенной виртуализацией) + WinDbg по KDNET

Примечание

Чисто личный исследовательский проект для глубокого изучения архитектуры процессоров Intel и внутреннего устройства ядра Windows.
