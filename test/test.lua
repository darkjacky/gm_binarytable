require("binarytable")

-- Make everything local so Lua wont make it slower
local util_TableToJSON = util.TableToJSON
local util_JSONToTable = util.JSONToTable
local TableToBinary = TableToBinary
local BinaryToTable = BinaryToTable
local SysTime = SysTime


print("\nStarting binarytable test")
local tab = {}
for i = 1, 10000 do
	tab[i] = { 0, 256, 65536, 9.95, VectorRand(), AngleRand(), "String testing...", true } -- every supported variable
end

-- Run util.TableToJSON, util.JSONToTable, TableToBinary, BinaryToTable
-- Loop 10 times for accuracy

local JSONOUT, JSONTABLE, BINOUT, BINTABLE

local t1 = SysTime()
for i = 1, 10 do
	JSONOUT = util_TableToJSON(tab)
end
local t2 = SysTime()
for i = 1, 10 do
	JSONTABLE = util_JSONToTable(JSONOUT)
end
local t3 = SysTime()
for i = 1, 10 do
	BINOUT = TableToBinary(tab)
end
local t4 = SysTime()
for i = 1, 10 do
	BINTABLE = BinaryToTable(BINOUT)
end
local t5 = SysTime()

-- print results
print("It took util.TableToJSON an average of ", (t2 - t1) / 10)
print("It took util.JSONToTable an average of ", (t3 - t2) / 10)
print("It took TableToBinary an average of ", (t4 - t3) / 10)
print("It took BinaryToTable an average of ", (t5 - t4) / 10)
print(string.format("TableToBinary is %.2f faster than util.TableToJSON", (t2 - t1) / (t4 - t3)))
print(string.format("BinaryToTable is %.2f faster than util.JSONToTable", (t3 - t2) / (t5 - t4)))
print("TableToBinary produced an output of ", #BINOUT)
print("util.TableToJSON produced an output of ", #JSONOUT)
print("End of binarytable test\n")
