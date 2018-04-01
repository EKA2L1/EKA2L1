# EKA2L1
- EKA2L1 is a HLE Symbian OS emulator. Using Symbian 9.4 (S60V5) as a base. The current thing it can do now is load rom. Progress are slowly being made. Screenshot provides below.

![Screenshot](https://raw.githubusercontent.com/bentokun/EKA2L1/master/screenshots/prototype.png)

- Requirements:
    + 2GB Memory
    + GPU card supports at least OpenGL 3.1

- Dependencies:
    + Dynarmic (Future)
    + ImGui
    + GLFW
    + Miniz
    + Glad

- Help me
    * Symbian Systemcall Signatures (SSS)
	    * Currently, SSS are extracted from library file in Symbian SDK. It's the method that most re use. Library are converted to IDT using IDS tool.
            * After being converted to IDT, there will be a parser that parse the IDT file, to generate correspond HLE header and source for that library.

- Intepreter
    * Need help to write a ARMv6 Intepreter. I intend to use Dynarmic along.

- Leave and trap
    * Need help implement them?
    * The emulator will catch the leave and stop the process and report them to the user. When the leave is trapped, if enabled, emulator will log the leave to the Logger, and continue

- Milestone intended
    * Symbian 9.1
    
- GUI
    * Currently a gui is supported. By compile and run the executable in src/emu/imgui, and change the SIS package path to your path, you can get the metadata and other logging information. A ARM debugger is planned, other nothing much
