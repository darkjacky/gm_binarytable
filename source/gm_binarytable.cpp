#include "GarrysMod/Lua/Interface.h"
#include <string.h>
#include <stdlib.h>

unsigned int GetStoreSize(long long i) {
	if (i == (signed char)(i & 0xFF))
		return sizeof(signed char);
	if (i == (short)(i & 0xFFFF))
		return sizeof(short);
	if (i == (int)(i & 0xFFFFFFFF))
		return sizeof(int);
	return sizeof(long long);
}

// define custom types
const char BOOLFALSE = 1;
const char BOOLTRUE = 2;
const char TYPENUMBER = 3;
const char TYPECHAR = 4;
const char TYPESHORT = 5;
const char TYPEINT = 6;
const char TYPESTRINGCHAR = 7;
const char TYPESTRINGSHORT = 8;
const char TYPESTRINGLONG = 9;
const char TYPEVECTOR = 10;
const char TYPEANGLE = 11;
const char TYPETABLE = 12;
const char TYPETABLEEND = 13;
const char TYPETABLESEQ = 14;

class QuickStrWrite {
public:
	QuickStrWrite() {
		str = (char*)malloc(1048576);
		length = 1048576;
	}

	~QuickStrWrite() {																// Destroy class
		free(str);
		str = 0;																// unset for good measure
	}

	void expand(unsigned int amount) {
		length += amount;															// Increase amount
		str = (char*)realloc(str, length);													// If this fails you are screwed anyway
	}

	void writetype(const unsigned char& type) {
		if (position + 1 > length) expand(1048576);												// Looks like we wont be able to fit the new data into the array, lets expand it by 1 MB
		str[position++] = type;															// Write type at position
	}

	template<class T>
	void write(const unsigned char& type, const T& in) {
		if (position + sizeof(T) > length) expand(sizeof(T) + 1048576);										// Looks like we wont be able to fit the new data into the array, lets expand it by len + 1 MB in case its larger than 1 MB
		str[position++] = type;															// Write type at position
		*(T*)(str + position) = in;														// Huge hack replacing memcpy. cast str* + position to T* then get the reference and push the T in to that reference
		position += sizeof(T);															// Update position
	}

	void write(const char* in, unsigned int len) {
		if (position + len > length) expand(len + 1048576);											// Looks like we wont be able to fit the new data into the array, lets expand it by 1 MB
		memcpy(str + position, in, len);													// Copy memory into char array
		position += len;															// Update position
	}

	char* GetStr() {
		return str;																// Return pointer of string
	}

	unsigned int GetLength() {															// To avoid messing with the length we copy it
		return position;
	}

private:
	unsigned int position = 0;															// Position we are currently at
	unsigned int length = 0;															// String length
	char* str = 0;																	// String pointer
};

int ToStringFN = 0;
void BinaryToStrLoop(GarrysMod::Lua::ILuaBase* LUA, QuickStrWrite& stream) {
	LUA->PushNil();																	// Push nil for Next, This will be our key variable
	while (LUA->Next(-2)) {																// Get Next variable
		for (int it = 2; it > 0; it--) {
			int type = LUA->GetType(-it);													// Loop 2 then 1. Variable 1 is the key, variable 2 is value.
			switch (type) {
				case (GarrysMod::Lua::Type::BOOL): {
					if (LUA->GetBool(-it)) {											// Check if bool is true or false and write it so we don't have to write 2 byes
						stream.writetype(BOOLTRUE);										// Write true to stream
					}
					else {
						stream.writetype(BOOLFALSE);										// Write true to stream
					}
					break;
				}
				case (GarrysMod::Lua::Type::NUMBER): {
					double in = LUA->GetNumber(-it);										// Get number
					if ((in - (long long)in) == 0) {										// Check if number is an integer
						switch (GetStoreSize(in)) {											// Get minimum bytes required to store number
						case (sizeof(signed char)): {										// Can be stored as char
							stream.write(TYPECHAR, (signed char)in);							// Write number to stream
							break;
						}
						case (sizeof(short)): {											// Can be stored as short
							stream.write(TYPESHORT, (short)in);								// Write number to stream
							break;
						}
						case (sizeof(int)): {											// Can be stored as int
							stream.write(TYPEINT, (int)in);									// Write number to stream
							break;
						}
						default:
							stream.write(TYPENUMBER, in);									// Write number to stream
							break;
						}
					}
					else {														// Number is not an integer
						stream.write(TYPENUMBER, in);										// Write number to stream 
					}
					break;
				}
				case (GarrysMod::Lua::Type::STRING): {
					unsigned int len = 0;												// Assign a length variable
					const char* in = LUA->GetString(-it, &len);									// Get string and length
					if (len < 0xFF) {
						stream.write(TYPESTRINGCHAR, (unsigned char)len);							// Write string length
					}
					else if (len < 0xFFFF) {
						stream.write(TYPESTRINGSHORT, (unsigned short)len);							// Write string length
					}
					else {
						stream.write(TYPESTRINGLONG, (unsigned int)len);							// Write string length
					}
					stream.write(in, len);												// Write string to stream
					break;
				}
				case (GarrysMod::Lua::Type::VECTOR): {
					stream.write(TYPEVECTOR, LUA->GetVector(-it));									// Write Vector to stream
					break;
				}
				case (GarrysMod::Lua::Type::ANGLE): {
					stream.write(TYPEANGLE, LUA->GetAngle(-it));									// Write Angle to stream
					break;
				}
				case (GarrysMod::Lua::Type::TABLE): {
					LUA->Push(-it);													// Push table to -1
					stream.writetype(TYPETABLE);											// Write table start to stream
					BinaryToStrLoop(LUA, stream);											// Start this function to extract everything from the table
					stream.writetype(TYPETABLEEND);											// Write table end to stream
					LUA->Pop();													// Pop table so we can continue with Next()
					break;
				}
				default: {
					// We do not know this type so we will use build in functions to convert it into a string
					LUA->ReferencePush(ToStringFN);											// Push reference function tostring
					LUA->Push(-it - 1);												// Push data we want to have the string for
					LUA->Call(1, 1);												// Call function with 1 variable and 1 return

					unsigned int len = 0;												// Assign a length variable
					const char* in = LUA->GetString(-it, &len);									// Get string and length
					if (len < 0xFF) {
						stream.write(TYPESTRINGCHAR, (unsigned char)len);							// Write string length
					}
					else if (len < 0xFFFF) {
						stream.write(TYPESTRINGSHORT, (unsigned short)len);							// Write string length
					}
					else {
						stream.write(TYPESTRINGLONG, (unsigned int)len);							// Write string length
					}
					stream.write(in, len);												// Write string to stream
					LUA->Pop();													// Pop string
					break;
				}
			}
		}
		LUA->Pop();																// Pop value from stack
	}
}

void BinaryToStrLoopSeq(GarrysMod::Lua::ILuaBase* LUA, QuickStrWrite& stream) {
	LUA->PushNil();																	// Push nil for Next, This will be our key variable
	while (LUA->Next(-2)) {																// Get Next variable
		int type = LUA->GetType(-1);														// Loop 2 then 1. Variable 1 is the key, variable 2 is value.
		switch (type) {
			case (GarrysMod::Lua::Type::BOOL): {
				if (LUA->GetBool(-1)) {													// Check if bool is true or false and write it so we don't have to write 2 byes
					stream.writetype(BOOLTRUE);											// Write true to stream
				}
				else {
					stream.writetype(BOOLFALSE);											// Write true to stream
				}
				break;
			}
			case (GarrysMod::Lua::Type::NUMBER): {
				double in = LUA->GetNumber(-1);												// Get number
				if ((in - (long long)in) == 0) {											// Check if number is an integer
					auto size = GetStoreSize(in);											// Get minimum bytes required to store number
					switch (size) {
					case (sizeof(signed char)): {											// Can be stored as char
						stream.write(TYPECHAR, (signed char)in);								// Write number to stream
						break;
					}
					case (sizeof(short)): {												// Can be stored as short
						stream.write(TYPESHORT, (short)in);									// Write number to stream
						break;
					}
					case (sizeof(int)): {												// Can be stored as int
						stream.write(TYPEINT, (int)in);										// Write number to stream
						break;
					}
					default:
						stream.write(TYPENUMBER, in);										// Write number to stream
						break;
					}
				}
				else {															// Number is not an integer
					stream.write(TYPENUMBER, in);											// Write number to stream 
				}
				break;
			}
			case (GarrysMod::Lua::Type::STRING): {
				unsigned int len = 0;													// Assign a length variable
				const char* in = LUA->GetString(-1, &len);										// Get string and length
				if (len < 0xFF) {
					stream.write(TYPESTRINGCHAR, (unsigned char)len);								// Write string length
				}
				else if (len < 0xFFFF) {
					stream.write(TYPESTRINGSHORT, (unsigned short)len);								// Write string length
				}
				else {
					stream.write(TYPESTRINGLONG, (unsigned int)len);								// Write string length
				}
				stream.write(in, len);													// Write string to stream
				break;
			}
			case (GarrysMod::Lua::Type::VECTOR): {
				stream.write(TYPEVECTOR, LUA->GetVector(-1));										// Write Vector to stream
				break;
			}
			case (GarrysMod::Lua::Type::ANGLE): {
				stream.write(TYPEANGLE, LUA->GetAngle(-1));										// Write Angle to stream
				break;
			}
			case (GarrysMod::Lua::Type::TABLE): {
				LUA->Push(-1);														// Push table to -1
				stream.writetype(TYPETABLE);												// Write table start to stream
				BinaryToStrLoopSeq(LUA, stream);											// Start this function to extract everything from the table
				stream.writetype(TYPETABLEEND);												// Write table end to stream
				LUA->Pop();														// Pop table so we can continue with Next()
				break;
			}
			default: {
				// We do not know this type so we will use build in functions to convert it into a string
				LUA->ReferencePush(ToStringFN);												// Push reference function tostring
				LUA->Push(-1 - 1);													// Push data we want to have the string for
				LUA->Call(1, 1);													// Call function with 1 variable and 1 return

				unsigned int len = 0;													// Assign a length variable
				const char* in = LUA->GetString(-1, &len);										// Get string and length
				if (len < 0xFF) {
					stream.write(TYPESTRINGCHAR, (unsigned char)len);								// Write string length
				}
				else if (len < 0xFFFF) {
					stream.write(TYPESTRINGSHORT, (unsigned short)len);								// Write string length
				}
				else {
					stream.write(TYPESTRINGLONG, (unsigned int)len);								// Write string length
				}
				stream.write(in, len);													// Write string to stream
				LUA->Pop();														// Pop string
				break;
			}
		}
		LUA->Pop();																// Pop value from stack
	}
}

LUA_FUNCTION(TableToBinary) {
	LUA->CheckType(1, GarrysMod::Lua::Type::TABLE);													// Check if our input is a table
	if (LUA->GetType(2) == GarrysMod::Lua::Type::BOOL) {												// Check if we have a second variable
		if (LUA->GetBool(2)) {															// If this is true we assume the table is sequential
			LUA->Push(1);															// Push table to -1
			QuickStrWrite stream;														// Custom stringstream replacement for this specific thing
			stream.writetype(TYPETABLESEQ);													// Write Sequential table indicator
			BinaryToStrLoopSeq(LUA, stream);												// Compute table into string
			LUA->Pop();															// Pop table
			LUA->PushString(stream.GetStr(), stream.GetLength());										// Get string and length of string then push as return value
			return 1;
		}
	}
	LUA->Push(1);																	// Push table to -1
	QuickStrWrite stream;																// Custom stringstream replacement for this specific thing
	BinaryToStrLoop(LUA, stream);															// Compute table into string
	LUA->Pop();																	// Pop table
	LUA->PushString(stream.GetStr(), stream.GetLength());												// Get string and length of string then push as return value
	return 1;																	// return 1 variable to Lua
}

class QuickStrRead {
public:
	QuickStrRead(const char* in, unsigned int& len) : str(in), length(len) {}									// init variables

	unsigned char GetType() {
		return str[position++];															// Probably faster than calling memcpy
	}

	template<class T>
	T* read() {
		position += sizeof(T);															// Increase position by len
		return (T*)(str + (position - sizeof(T)));												// Big hacker time. return the reference of a pointer from the string + position - length (A lot quicker than creating a variable then memcpy)
	}

	const char* GetStr(unsigned int length) {
		position += length;															// Increase position
		return str + (position - length);													// Return str ptr + position - length to get the right data
	}

	bool atend() {																	// Check if we are at the end of string
		return position >= length;
	}

private:
	unsigned int position = 0;															// Position we are reading the string from
	unsigned int length = 0;															// Length of string
	const char* str = 0;																// String pointer
};

void BinaryToTableLoop(GarrysMod::Lua::ILuaBase* LUA, QuickStrRead& stream) {
	char top = LUA->Top();																// Store Top variable to check if we added something to our stack at the end
	while (!stream.atend()) {															// Loop as long as we have something in our stream or until we reach our endpos
		for (int i = 0; i < 2; i++) {														// Loop twice for key and variable
			switch (stream.GetType()) {													// Get type and run corresponding code
			case (0): {
				return;															// This should not happen
			}
			case (TYPETABLEEND): {
				return;															// We are at the end of our table there is nothing left
			}
			case (BOOLTRUE): {
				LUA->PushBool(true);													// Push true to Lua
				break;
			}
			case (BOOLFALSE): {
				LUA->PushBool(false);													// Push false to Lua
				break;
			}
			case (TYPECHAR): {
				LUA->PushNumber(*stream.read<char>());											// Push variable to Lua
				break;
			}
			case (TYPESHORT): {
				LUA->PushNumber(*stream.read<short>());											// Push variable to Lua
				break;
			}
			case (TYPEINT): {
				LUA->PushNumber(*stream.read<int>());											// Push variable to Lua
				break;
			}
			case (TYPENUMBER): {
				LUA->PushNumber(*stream.read<double>());										// Push variable to Lua
				break;
			}
			case (TYPESTRINGCHAR): {
				unsigned char len = *stream.read<unsigned char>();									// Read length from stream
				LUA->PushString(stream.GetStr(len), len);										// Push string to Lua with length should push from offset until len
				break;
			}
			case (TYPESTRINGSHORT): {
				unsigned short len = *stream.read<unsigned short>();									// Read length from stream
				LUA->PushString(stream.GetStr(len), len);										// Push string to Lua with length should push from offset until len
				break;
			}
			case (TYPESTRINGLONG): {
				unsigned int len = *stream.read<unsigned int>();									// Read length from stream
				LUA->PushString(stream.GetStr(len), len);										// Push string to Lua with length should push from offset until len
				break;
			}
			case (TYPEVECTOR): {
				LUA->PushVector(*stream.read<Vector>());										// Push Vector to Lua
				break;
			}
			case (TYPEANGLE): {
				LUA->PushAngle(*stream.read<QAngle>());											// Push Angle to Lua
				break;
			}
			case (TYPETABLE): {
				LUA->CreateTable();													// Create table
				BinaryToTableLoop(LUA, stream);												// Call this function
				break;
			}
			default:															// should never happen
				break;
			}
		}

		if (top + 2 == LUA->Top())
			LUA->RawSet(-3);														// Push new variable to table
		else if (LUA->Top() > top)
			LUA->Pop(LUA->Top() - top);													// Try to pop as much variables off stack as are probably broken this should never be run but lets be prepared for everything
	}
}

void BinaryToTableLoopSeq(GarrysMod::Lua::ILuaBase* LUA, QuickStrRead& stream) {
	unsigned int position = 1;															// Lua starts with a table index of 1
	char top = LUA->Top();																// Store Top variable to check if we added something to our stack at the end
	while (!stream.atend()) {															// Loop as long as we have something in our stream or until we reach our endpos
		LUA->PushNumber(position++);														// Push index and increase its value
		switch (stream.GetType()) {														// Get type and run corresponding code
		case (0): {
			LUA->Pop();															// Pop number we just pushed
			return;																// This should not happen
		}
		case (TYPETABLEEND): {
			LUA->Pop();															// Pop number we just pushed
			return;																// We are at the end of our table there is nothing left
		}
		case (BOOLTRUE): {
			LUA->PushBool(true);														// Push true to Lua
			break;
		}
		case (BOOLFALSE): {
			LUA->PushBool(false);														// Push false to Lua
			break;
		}
		case (TYPECHAR): {
			LUA->PushNumber(*stream.read<char>());												// Push variable to Lua
			break;
		}
		case (TYPESHORT): {
			LUA->PushNumber(*stream.read<short>());												// Push variable to Lua
			break;
		}
		case (TYPEINT): {
			LUA->PushNumber(*stream.read<int>());												// Push variable to Lua
			break;
		}
		case (TYPENUMBER): {
			LUA->PushNumber(*stream.read<double>());											// Push variable to Lua
			break;
		}
		case (TYPESTRINGCHAR): {
			unsigned char len = *stream.read<unsigned char>();										// Read length from stream
			LUA->PushString(stream.GetStr(len), len);											// Push string to Lua with length should push from offset until len
			break;
		}
		case (TYPESTRINGSHORT): {
			unsigned short len = *stream.read<unsigned short>();										// Read length from stream
			LUA->PushString(stream.GetStr(len), len);											// Push string to Lua with length should push from offset until len
			break;
		}
		case (TYPESTRINGLONG): {
			unsigned int len = *stream.read<unsigned int>();										// Read length from stream
			LUA->PushString(stream.GetStr(len), len);											// Push string to Lua with length should push from offset until len
			break;
		}
		case (TYPEVECTOR): {
			LUA->PushVector(*stream.read<Vector>());											// Push Vector to Lua
			break;
		}
		case (TYPEANGLE): {
			LUA->PushAngle(*stream.read<QAngle>());												// Push Angle to Lua
			break;
		}
		case (TYPETABLE): {
			LUA->CreateTable();														// Create table
			BinaryToTableLoopSeq(LUA, stream);												// Call this function
			break;
		}
		default:																// should never happen
			break;
		}

		if (top + 2 == LUA->Top())
			LUA->RawSet(-3);														// Push new variable to table
		else if (LUA->Top() > top)
			LUA->Pop(LUA->Top() - top);													// Try to pop as much variables off stack as are probably broken this should never be run but lets be prepared for everything
	}
}

LUA_FUNCTION(BinaryToTable) {
	LUA->CheckType(1, GarrysMod::Lua::Type::STRING);												// Check type is string
	unsigned int len = 0;																// This will be our string length
	const char* str = LUA->GetString(1, &len);													// Get string and string length from Lua
	if (str[0] < 0 || str[0] > 15) {														// We cant process this
		LUA->ThrowError("Invalid input");													// Throw an error
		return 0;																// Return nothing
	}
	QuickStrRead stream(str, len);															// Push string to custom read class
	LUA->CreateTable();																// Create Table that will be pushed to Lua when we are done
	if (str[0] == TYPETABLESEQ) {															// Sequential table
		stream.GetType();															// Type is sequential so we need to remove the first char
		BinaryToTableLoopSeq(LUA, stream);													// Turn our string into actual variables
	}
	else {
		BinaryToTableLoop(LUA, stream);														// Turn our string into actual variables
	}
	return 1;																	// return 1 variable to Lua
}

GMOD_MODULE_OPEN() {
	LUA->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);													// Get _G
	LUA->PushString("TableToBinary");														// Push name of function
	LUA->PushCFunction(TableToBinary);														// Push function
	LUA->SetTable(-3);																// Set variables to _G
	LUA->PushString("BinaryToTable");														// Push name of function
	LUA->PushCFunction(BinaryToTable);														// Push function
	LUA->SetTable(-3);																// Set variables to _G
	LUA->GetField(-1, "tostring");															// Get function tostring
	ToStringFN = LUA->ReferenceCreate();														// Create reference to function tostring
	LUA->Pop();																	// Pop _G
	return 0;
}


GMOD_MODULE_CLOSE() {
	LUA->ReferenceFree(ToStringFN);															// Not required doing it anyway -- Freeing reference
	return 0;
}
