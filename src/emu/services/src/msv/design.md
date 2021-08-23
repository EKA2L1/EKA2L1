The MSV holds message and mails data. While the message data stored on the disk, its generic info is stored differently between OS versions.

The devlib already provides some important info, so I'm gonna copy some of them here. This is just a more details on how the internal works, to understand the code I wrote here more easily.

## Why need to rewrite this server

We need to rewrite some servers that do real message sending/receiving, because LLE interactions on the emulator is unreliable and hard. Beside, full manipulation is fun. The most popular example is SendAs server.

## Index

*"The index contains state and generic message header information for an entry. Each index entry (and so each item) is uniquely identified by an ID field and uses TMsvEntry to provide sufficient information for UI applications to display list views."* - Types of Storage for a Message Entry, Symbian Developer Library.

### Pre-Symbian^3

Before Symbian^3, index is store in a file located in C:\Private\1000484b\Mail2\Index. The file is a permanent store, and its structure (reference from cmsvversion0version1converter.cpp) follows:


