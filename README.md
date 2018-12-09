# gm_binarytable
Another way to store a table to a file

This module takes a Lua table and turns it into binary.
Number are stored lossless from the engine.

Output from the test.lua:
TableToBinary is roughly 11X faster than util.TableToJSON

BinaryToTable is roughlt 6X faster than util.JSONToTable

To compile download https://github.com/Facepunch/gmod-module-base/tree/development and premake5 https://premake.github.io/
Put the include folder from gmod-module-base in the folder above gm_binarytable, put premake5.exe in the gm_binarytable folder and run BuildProjects.bat.
You should know from there.

Please let me know if you encounter any problems.
I would also like to know your application for this. Please let me know.

Thank you for using this.



# How does this work?
C and C++ use memory to store temporary values. These values are stored in the memory as bits. 8 bits per byte.

You push a table to this module and it does the following:

Get the table

Check what type the first variable is, number, string, Vector, Angle, table.

For numbers it converts it into the smallest possible storage space. Strings get written raw with a length in front of it. Vectors and angles get written raw. With a table we start from the begin and loop it.

Before adding our variable into our output we add a header byte, this contains the type we will write.

We write the variable and we start from the begin until we are done.

This storage method should be lossless as it stores everything. Meaning { Vector() } will be stored like this:

1 byte for our table index type indicator, 1 byte for our table index index, 1 byte for our table key indicator 12 bytes for the Vector and done. 15 bytes in total completely lossless.

for { { Vector() } } it is slightly different but it is about the same.

1 byte key index indicator, 1 byte key index number, 1 byte table start, 1 byte key index indicator, 1 byte key index number, 1 byte values index indicator, 12 bytes value, 1 byte table close. A total of 19 bytes.

At [128] = { Vector() } all that changes is the initial key index number can no longer be stored in a byte but needs 2 bytes changing the size from 19 to 20.

At [32768] = { Vector() } this will be done again but this time it goes from 2 to 4 bytes chaning the size from 20 to 22.

Floating points from Lua that are not in a Vector or Angle will be saved with 8 bytes. This ensures lossless storing.

util.TableToJSON converts floating points into a string meaning 9.99999 will be turned into 10 this modules stores it as its value meaning after you store it and get it back it is still 9.99999. If you need this precision this is the way to go.

#
By not having to convert anything we can save a lot of time. util.TableToJSON takes 0.4170118 seconds to convert 100000 time { VectorRand() } in a table to a string where TableToBinary takes 0.0635785 seconds. This leaves more time for the server to do other things. Better yet converting it back to a table takes 0.0651309 seconds in this case compared to 0.1256353 with util.JSONToTable.

# Known Issues
Trying to convert the Global table crashes the program

# Possible improvements
It might be possible to change the indicator bit into a 2 in 1 thing. We currently use 1 byte per key and 1 per values maybe these can be combined into 1.
There might be some way to increase the speed, for example writing my own string library.
