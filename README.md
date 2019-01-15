# stm32audio

## Atollic TrueStudio installation

Import the project in TrueStudio, then:

1. Mark the following folders as header sources (right click => "Add/remove include paths" => OK): `inc`, `lib/inc`, `lib/inc/core`, `lib/inc/peripherals`.
2. Configure the C compiler:
	1. Right click on project root
	2. Open Properties => C/C++ Build => Settings => Target settings.
	3. Select STM32F4 then the Discovery board under "Boards", then click OK/Finish as many times as required.
3. Build the project.
4. Run "Debug", which should open the "Edit configuration" window.
5. Under the "Debugger" tab, select the "ST-LINK" debug probe, "Apply" then "OK".
