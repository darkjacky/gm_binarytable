# gm_binarytable
Another way to store a table to a file

This module takes a Lua table and turns it into binary.
Number are stored lossless from the engine.

With version 1.03 there is a secondary arg if you pass true it will read the table as sequential (EVEN WHEN IT IS NOT) and write variables without the key. If you have a sequential table the output will be correct, if you don't have a sequential table the output will be sequential but it will not match your input.

Output from the test.lua:
```
Ryzen 9 3900x 64 bit
It took util.TableToJSON an average of  0.091015573333334
It took util.JSONToTable an average of  0.076666526666666
It took TableToBinary an average of     0.0040803233333335
It took BinaryToTable an average of     0.0067394266666668
TableToBinary is 22.31 faster than util.TableToJSON
BinaryToTable is 7.09 faster than util.JSONToTable
TableToBinary produced an output of     859873
util.TableToJSON produced an output of  1060010

Ryzen 9 3900x 32 bit
It took util.TableToJSON an average of  0.096380183333333
It took util.JSONToTable an average of  0.078502193333333
It took TableToBinary an average of     0.0054306633333333
It took BinaryToTable an average of     0.00623564
TableToBinary is 17.75 faster than util.TableToJSON
BinaryToTable is 6.73 faster than util.JSONToTable
TableToBinary produced an output of     859873
util.TableToJSON produced an output of  1059975

Intel i7 3770k 64 bit
It took util.TableToJSON an average of  0.10309486666667
It took util.JSONToTable an average of  0.085004993333333
It took TableToBinary an average of     0.0045930166666666
It took BinaryToTable an average of     0.0070141933333333
TableToBinary is 22.45 faster than util.TableToJSON
BinaryToTable is 7.32 faster than util.JSONToTable
TableToBinary produced an output of     859873
util.TableToJSON produced an output of  1060043

Intel i7 3770k 32 bit
It took util.TableToJSON an average of  0.10758651
It took util.JSONToTable an average of  0.086499176666667
It took TableToBinary an average of     0.0057781733333333
It took BinaryToTable an average of     0.0077053466666667
TableToBinary is 18.62 faster than util.TableToJSON
BinaryToTable is 6.42 faster than util.JSONToTable
TableToBinary produced an output of     859873
util.TableToJSON produced an output of  1060030
```


BinaryToTable is roughly 7X faster than util.JSONToTable

To compile, download https://github.com/Facepunch/gmod-module-base/tree/development and premake5 https://premake.github.io/
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
