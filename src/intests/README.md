An auto-test suite. Use this to
- Test EKA2L1's servers and kernels code, which can't be tested using the gtest framework.
- Find out obscure fact about this system!

How to run:
- Generate the expected output *(requires hacked phone)*
    + Build the SIS/SISX with GEN_TESTS macro set to 1
    + Run the tests directly with executable eintests.exe.
    + In the **Private** workspace *(?:\\Private\\e6f75ec0\\)*, there should be a folder called 
    expected. Copy that folder to the test source folder *(/src/intests)*

- Running the tests on the emulator
    + Build the SIS/SISX with GEN_TESTS macro set to 0
    + Install the SIS/SISX on EKA2L1 with the *-install* switch
    + Running the **EKA2L1 Hardware Tests** app
    + The *DebugPrint* should tells you which test fail and what to be expected.
    + If the emulator crash but the hardware is not, there should be some wrong implementation in
    the emulator. Takes that as an bug.