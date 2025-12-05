do
	print("\nStarting binarytable test")
	require("binarytable")

	-- Make everything local so Lua wont make it slower
	local util_TableToJSON = util.TableToJSON
	local util_JSONToTable = util.JSONToTable
	local TableToBinary = TableToBinary
	local BinaryToTable = BinaryToTable
	local SysTime = SysTime
	local print = print
	local math = math
	local string = string

	local function Rand()
		return math.random(-32768, 32768) + math.random() -- returns a "random" number with some decimals
	end

	local function Rand2()
		return math.random(-180, 180) + math.random() -- returns a "random" number with some decimals
	end

	math.randomseed(0) -- Make this test repeatable
	local vec = Vector( Rand(), Rand(), Rand() )
	local ang = Angle( Rand2(), Rand2(), Rand2() )
	local tab = {}
	tab[ 1 ] = { 0, 256, 65536, 9.95, vec, ang, "12345678", true, Color( 128, 129, 130, 0 ) } -- every supported variable except longer strings
	for i = 2, 10000 do
		tab[ i ] = {}
		for k = 1, #tab[ 1 ] do
			tab[ i ][ k ] = tab[ 1 ][ k ]
		end
	end

	-- Run util.TableToJSON, util.JSONToTable, TableToBinary, BinaryToTable


	local JSONOUT, JSONTABLE, BINOUT, BINTABLE
	local loopcount = 10 -- Loop 10 times for accuracy

	local t1 = SysTime()
	for i = 1, loopcount do
		JSONOUT = util_TableToJSON(tab)
	end
	local t2 = SysTime()
	for i = 1, loopcount do
		JSONTABLE = util_JSONToTable(JSONOUT)
	end
	local t3 = SysTime()
	for i = 1, loopcount do
		BINOUT = TableToBinary(tab, nil, false)
	end
	local t4 = SysTime()
	for i = 1, loopcount do
		BINTABLE = BinaryToTable(BINOUT, false)
	end
	local t5 = SysTime()
	for i = 1, loopcount do
		BINOUT = TableToBinary(tab)
	end
	local t6 = SysTime()
	for i = 1, loopcount do
		BINTABLE = BinaryToTable(BINOUT)
	end
	local t7 = SysTime()

	-- print results
	print(string.format("It took util.TableToJSON an average of %.8f", (t2 - t1) / loopcount))
	print(string.format("It took util.JSONToTable an average of %.8f", (t3 - t2) / loopcount))
	print(string.format("It took TableToBinary    an average of %.8f", (t6 - t5) / loopcount))
	print(string.format("It took BinaryToTable    an average of %.8f", (t7 - t6) / loopcount))
	print(string.format("TableToBinary without CRC is %.2f faster than util.TableToJSON", (t2 - t1) / (t4 - t3)))
	print(string.format("BinaryToTable without CRC is %.2f faster than util.JSONToTable", (t3 - t2) / (t5 - t4)))
	print(string.format("TableToBinary with    CRC is %.2f faster than util.TableToJSON", (t2 - t1) / (t6 - t5)))
	print(string.format("BinaryToTable with    CRC is %.2f faster than util.JSONToTable", (t3 - t2) / (t7 - t6)))
	print("TableToBinary    produced an output of", #BINOUT, "Byte")
	print("util.TableToJSON produced an output of", #JSONOUT, "Byte")

	for i = 1, #tab[1] do
		if tab[1][i] ~= BINTABLE[1][i] then
			print("WARNING TEST FAILED:", i, "is not the same as the input!")
			print( tab[1][i], BINTABLE[1][i] )
		end
	end

	print("End of binarytable test\n")
end
