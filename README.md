# EKA2L1
- EKA2L1 is a HLE Symbian OS emulator. Using Symbian 9.4 (S60V5) as a base, this emulator runs a virtual ARM CPU, and calls HLE functions where needed.

- Requirements:
    + 2GB Memory
    + GPU card supports at least OpenGL 3.1

- Dependencies planned:
    + Dynarmic
    + Boost

- Help me
    * Symbian Systemcall Signatures (SSS)
	    * If you have SSS for s60v5 or upper, please send them to me through: fewdspuckrito@gmail.com as json file if i havent imported them yet.
	    * Currently, SSS are extracted from .lib file in Symbian SDK. It's the method that most re use. Library are converted to IDT using IDS tool.
            * After being converted to IDT, there will be a parser that parse the IDT file, to generate correspond HLE header and source for that library.

- Intepreter
    * Need help to write a ARMv6 Intepreter. I intend to use Dynarmic, but some instructions are not done yet, so if one is unavail, the intepreter will take the JIT job from there

- Leave and trap
    * Need help implement them?
    * The emulator will catch the leave and stop the process and report them to the user. When the leave is trapped, if enabled, emulator will log the leave to the Logger, and continue

- Milestone intended
    * Symbian 9.1
