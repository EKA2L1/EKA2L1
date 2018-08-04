- Symbols extract requirements:
	* A Symbian phone (patched Open4All using ROMPatcher)
	* Symbian IDT from IDA

- Steps:
	* Dump all libraries from Symbian Z: drive to the working directory of **exportlister**
	* Run the exportlister with no argument and it will procedure nsd files of those libraries
	* Copy those to **exportyml**
        * Dump all IDT files from IDA files to the same **exportyml**
		+ In case there aren't any IDT files associated with the library, you can travel through the metadata :)
		+ Then export them through the Python file provided.
        * Run the exportyml with no argument. The program will check all the libraries name and find the correspond IDT file.
	* Grab the db.yml

- Using: C++ 17
