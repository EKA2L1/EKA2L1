# EKA2L1 contributing rules and references

# Contributing
Please note that any contribution you make to EKA2L1 will be licensed under the GNU GPL version 3 (or any later version)

## Coding style and formats
### General
- Try to follow the Clang format rules. Turn on the clang formatter for convenice.
- Try to limit lines of code to maximum of 100 characters. 
- Indentation style of EKA2L1 is 4 spaces per level. No tabs allowed.
- Opening brace for namespaces, classes, functions, structs must be on the current line.
- Comments should be deleted, unless it's a note.
- References and pointers have the ampersand or asterisk against the variable name. So it should be ```char &a``` instead of 
```char& a```.
- Don't collapse single line conditional or loop bodies onto the same line as its header. Put it on the next line. Example (from Dolphin
contributing and coding rules):
    - Yes:
    ```cpp

    if (condition)
      return 0;

    while (var != 0)
      var--;
     ```

    - No:
    ```cpp 
    if (condition) return 0;
    while (var != 0) var--;
    ```

### Naming
- Follow the underscore notation for all variables, methods and classes, except for Symbian method and class legacy code style, if it's really neccesary. For example:
   - Yes:
   ```cpp
   int me_do;
   ```
   - No:
   ```cpp
   int meDo;
   ```

- Compile time constant should be a *constexpr*. For example
   ```cpp
   constexpr std::int32_t domain_id = 0x10152026;
   constexpr std::array<std::int32_t, 7U> array_id = {15, 16, 17, 19, 21, 11, 15};
   ```

### Conditionals
Do not leave else or else if conditions dangling unless the if condition lacks braces.
   - Yes:

   ```cpp
   if (condition) {
     // code
   }
   else {
      // code
   }
   ```

   - Acceptable:
   ```cpp
   if (condition)
      // code line
   else
     // code line
   ```

   - No:
   ```cpp
    if (condition) {
      // code
    }
    else
     // code line
   ```

# Knowledges and references
- EKA2L1 is a HLE emulator, it emulates kernel and neccessary servers and system calls, in order to get apps and games booting, not the entire OS.
- Despite that, you can still get to know about the codebase more, by just traveling and read the code. If there is any problems, please ask in the Discord server provided in README.md.
- For references, search up for Symbian OS internal and Symbian Source code on Github.