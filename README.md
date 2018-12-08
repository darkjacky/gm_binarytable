# gm_binarytable
Another way to store a table to a file

This module takes a Lua table and turns it into binary.
Number are stored lossless from the engine.

Output from the test.lua:
TableToBinary is 6.09X faster than util.TableToJSON
BinaryToTable is 3.81X faster than util.JSONToTable

To compile download https://github.com/Facepunch/gmod-module-base/tree/development and premake5 https://premake.github.io/
Put the include folder from gmod-module-base in the folder above gm_binarytable, put premake5.exe in the gm_binarytable folder and run BuildProjects.bat.
You should know from there.

Please let me know if you encounter any problems.
I would also like to know your application for this. Please let me know.

Thank you for using this.
