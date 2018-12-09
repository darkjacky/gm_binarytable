#include "GarrysMod/Lua/Interface.h"

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

class QuickStrWrite {
public:
	QuickStrWrite() {
		str = new char[1048576];												// Init 1 MB the lower we set this the slower it will be
	}

	~QuickStrWrite() {															// Destroy class
		delete[] str;															// We need to delete otherwise we will create a memory leak
		str = 0;																// unset for good measure
	}

	unsigned int GetLength() {													// To avoid messing with the length we copy it
		return position;
	}

	void expand(unsigned int amount) {											// Looks like we ran out of space we need to increase this
		char *replacementstr = new char[length + amount];						// Create our new char array larger than the one we had before
		unsigned int pos = 0;													// The position we copy from
		while (pos < position) replacementstr[pos] = str[pos++];				// Copy until pos reaches position
		delete[] str;															// Delete our smaller char array
		str = replacementstr;													// Replace the pointer for our array
		length += amount;														// Increase our length
	}

	void write(char *in, unsigned int len) {
		if (position + len > length) expand(len + 1048576);						// Looks like we wont be able to fit the new data into the array, lets expand it by 1 MB
		unsigned int pos = 0;													// The position we copy from
		while (len > pos) str[position++] = in[pos++];							// Copy until pos reaches position
	}

	void write(const char *in, unsigned int len) {
		if (position + len > length) expand(len + 1048576);						// Looks like we wont be able to fit the new data into the array, lets expand it by 1 MB
		unsigned int pos = 0;													// The position we copy from
		while (len > pos) str[position++] = in[pos++];							// Copy until pos reaches position
	}

	char *GetStr() {
		return str;																// Return pointer of string
	}

private:
	unsigned int position = 0;													// Position we are currently at
	unsigned int length = 0;													// String length
	char *str = 0;																// String pointer
};

int ToStringFN = 0;
void BinaryToStrLoop(GarrysMod::Lua::ILuaBase *LUA, QuickStrWrite &stream) {
	LUA->PushNil();																// Push nil for Next, This will be our key variable
	while (LUA->Next(-2)) {														// Get Next variable
		for (int it = 2; it > 0; it--) {	
			int type = LUA->GetType(-it);										// Loop 2 then 1. Variable 1 is the key, variable 2 is value.
			switch (type) {
			case (GarrysMod::Lua::Type::BOOL): {
					if (LUA->GetBool(-it)) {									// Check if bool is true or false and write it so we don't have to write 2 byes
						stream.write((char *)&BOOLTRUE, sizeof(char));			// Write true to stream
					}
					else {
						stream.write((char *)&BOOLFALSE, sizeof(char));			// Write true to stream
					}
					break;
				}
			case (GarrysMod::Lua::Type::NUMBER): {
					double in = LUA->GetNumber(-it);							// Get number
					if ((in - (long long)in) == 0) {							// Check if number is an integer
						auto size = GetStoreSize(in);							// Get minimum bytes required to store number
						switch (size) {
						case (sizeof(signed char)): {							// Can be stored as char
							stream.write((char *)&TYPECHAR, sizeof(char));		// Write type to stream
							char out = in;										// Turn our variable into a char
							stream.write((char *)&out, sizeof(signed char));	// Write number to stream
							break;
						}
						case (sizeof(short)): {									// Can be stored as short
							stream.write((char *)&TYPESHORT, sizeof(char));		// Write type to stream
							short out = in;										// Turn our variable into a short
							stream.write((char *)&out, sizeof(short));			// Write number to stream
							break;
						}
						case (sizeof(int)): {									// Can be stored as int
							stream.write((char *)&TYPEINT, sizeof(char));		// Write type to stream
							int out = in;										// Turn our variable into a int
							stream.write((char*)&out, sizeof(int));				// Write number to stream
							break;
						}
						default:
							stream.write((char *)&TYPENUMBER, sizeof(char));	// Write type to stream
							stream.write((char *)&in, sizeof(double));			// Write number to stream
							break;
						}
					} else {													// Number is not an integer
						stream.write((char *)&TYPENUMBER, sizeof(char));		// Write type to stream
						stream.write((char *)&in, sizeof(double));				// Write number to stream 
					}
					break;
				}
			case (GarrysMod::Lua::Type::STRING): {
					unsigned int len = 0;										// Assign a length variable
					const char * in = LUA->GetString(-it, &len);				// Get string and length
					if (len < 0xFF) {
						stream.write((char *)&TYPESTRINGCHAR, sizeof(char));	// Write type to stream
						unsigned char convlen = len;
						stream.write((char *)&convlen, sizeof(unsigned char));	// Write string length
					} else if (len < 0xFFFF) {
						stream.write((char *)&TYPESTRINGSHORT, sizeof(char));	// Write type to stream
						unsigned short convlen = len;
						stream.write((char *)&convlen, sizeof(unsigned short));	// Write string length
					} else {
						stream.write((char *)&TYPESTRINGLONG, sizeof(char));	// Write type to stream
						stream.write((char *)&len, sizeof(unsigned int));		// Write string length
					}
					stream.write(in, len);										// Write string to stream
					break;
				}
			case (GarrysMod::Lua::Type::VECTOR): {
					Vector vec = LUA->GetVector(-it);							// Get Vector from Lua
					stream.write((char *)&TYPEVECTOR, sizeof(char));			// Write type to stream
					stream.write((char *)&vec, sizeof(vec));					// Write Vector.x to stream
					break;
				}
			case (GarrysMod::Lua::Type::ANGLE): {
					QAngle vec = LUA->GetAngle(-it);							// Get Angle from Lua
					stream.write((char *)&TYPEANGLE, sizeof(char));				// Write type to stream
					stream.write((char *)&vec, sizeof(vec));					// Write Angle to stream
					break;
				}
			case (GarrysMod::Lua::Type::TABLE): {
					LUA->Push(-it);												// Push table to -1
					stream.write((char *)&TYPETABLE, sizeof(char));				// Write type to stream
					BinaryToStrLoop(LUA, stream);								// Start this function to extract everything from the table
					stream.write((char *)&TYPETABLEEND, sizeof(char));			// Write type to stream
					LUA->Pop();													// Pop table so we can continue with Next()
					break;
				}
			default: {
				// We do not know this type so we will use build in functions to convert it into a string
				LUA->ReferencePush(ToStringFN);									// Push reference function tostring
				LUA->Push(-it - 1);												// Push data we want to have the string for
				LUA->Call(1, 1);												// Call function with 1 variable and 1 return

				unsigned int len = 0;											// Assign a length variable
				const char * in = LUA->GetString(-it, &len);					// Get string and length
				if (len < 0xFF) {
					stream.write((char *)&TYPESTRINGCHAR, sizeof(char));		// Write type to stream
					unsigned char convlen = len;
					stream.write((char *)&convlen, sizeof(unsigned char));		// Write string length
				}
				else if (len < 0xFFFF) {
					stream.write((char *)&TYPESTRINGSHORT, sizeof(char));		// Write type to stream
					unsigned short convlen = len;
					stream.write((char *)&convlen, sizeof(unsigned short));		// Write string length
				}
				else {
					stream.write((char *)&TYPESTRINGLONG, sizeof(char));		// Write type to stream
					stream.write((char *)&len, sizeof(unsigned int));			// Write string length
				}
				stream.write(in, len);											// Write string to stream
				LUA->Pop();														// Pop string
				break;
			}
			}
		}
		LUA->Pop();																// Pop value from stack
	}
}

LUA_FUNCTION(TableToBinary) {
	LUA->CheckType(1, GarrysMod::Lua::Type::TABLE);								// Check if our input is a table
	LUA->Push(1);																// Push table to -1
	QuickStrWrite stream;														// Custom stringstream replacement for this specific thing
	BinaryToStrLoop(LUA, stream);												// Compute table into string
	LUA->Pop();																	// Pop array away
	LUA->PushString(stream.GetStr(), stream.GetLength());						// Get string and length of string then push as return value
    return 1;																	// return 1 variable to Lua
}

class QuickStrRead {
public:
	QuickStrRead(const char * in, unsigned int len) {
		str = in;																// Copy string pointer
		length = len;															// Copy length
	}

	void read(char *to, unsigned int len) {
		unsigned int pos = 0;
		if (len + position > length) return;									// Should not happen but if it did, this prevents crashing
		while (len > pos)
			to[pos++] = str[position++];										// Copy from string to buffer until we reach length
	}

	bool atend() {																// Check if we are at the end of string
		return position == length;
	}

private:
	unsigned int position = 0;													// Position we are reading the string from
	unsigned int length = 0;													// Length of string
	const char *str = 0;														// String pointer
};

void BinaryToTableLoop(GarrysMod::Lua::ILuaBase *LUA, QuickStrRead &stream) {
	while (!stream.atend()) {										// Loop as long as we have something in our stream or until we reach our endpos
		char top = LUA->Top();										// Store Top variable to check if we added something to our stack at the end
		for (int i = 0; i < 2; i++) {								// Loop twice for key and variable
			char type = 0;											// Create type variable
			stream.read((char *)&type, sizeof(char));				// Read type variable from stream
			if (type == 0) return;									// Should not happen

			switch (type) {
			case (TYPETABLEEND): {
				LUA->RawSet(-3);									// We are at a virtual end of a table RawSet and return so we stop looping
				return;
			}
			case (BOOLTRUE): {
				LUA->PushBool(true);								// Push true to Lua
				break;
			}
			case (BOOLFALSE): {
				LUA->PushBool(false);								// Push false to Lua
				break;
			}
			case (TYPECHAR): {
				signed char out = 0;								// Create variable
				stream.read((char *)&out, sizeof(char));			// Read variable from stream
				LUA->PushNumber(out);								// Push variable to Lua
				break;
			}
			case (TYPESHORT): {
				short out = 0;										// Create variable
				stream.read((char *)&out, sizeof(short));			// Read variable from stream
				LUA->PushNumber(out);								// Push variable to Lua
				break;
			}
			case (TYPEINT): {
				int out = 0;										// Create variable
				stream.read((char *)&out, sizeof(int));				// Read variable from stream
				LUA->PushNumber(out);								// Push variable to Lua
				break;
			}
			case (TYPENUMBER): {
				double out = 0;										// Create variable
				stream.read((char *)&out, sizeof(double));			// Read variable from stream
				LUA->PushNumber(out);								// Push variable to Lua
				break;
			}
			case (TYPESTRINGCHAR): {
				unsigned char len = 0;								// Create length variable
				stream.read((char *)&len, sizeof(unsigned char));	// Read length from stream
				if (len < 1) break;									// Check if length is higher than 0;
				char *str = new char[len];							// Create new char array to copy the string
				stream.read(str, len);								// Copy string from stream
				LUA->PushString(str, len);							// Push string to Lua with length
				delete[] str;										// delete new char array
				break;
			}
			case (TYPESTRINGSHORT): {
				unsigned short len = 0;								// Create length variable
				stream.read((char *)&len, sizeof(unsigned short));	// Read length from stream
				if (len < 1) break;									// Check if length is higher than 0
				char *str = new char[len];							// Create new char array to copy the string
				stream.read(str, len);								// Copy string from stream
				LUA->PushString(str, len);							// Push string to Lua with length
				delete[] str;										// delete new char array
				break;
			}
			case (TYPESTRINGLONG): {
				size_t len = 0;										// Create length variable
				stream.read((char *)&len, sizeof(size_t));			// Read length from stream
				if (len < 1) break;									// Check if length is higher than 0
				char *str = new char[len];							// Create new char array to copy the string
				stream.read(str, len);								// Copy string from stream
				LUA->PushString(str, len);							// Push string to Lua with length
				delete[] str;										// delete new char array
				break;
			}
			case (TYPEVECTOR): {
				Vector vec;											// Create Vector variable
				stream.read((char *)&vec, sizeof(vec));				// Copy Vector from stream
				LUA->PushVector(vec);								// Push Vector to Lua
				break;
			}
			case (TYPEANGLE): {
				QAngle ang;											// Create Angle variable
				stream.read((char *)&ang, sizeof(ang));				// Copy Angle from stream
				LUA->PushAngle(ang);								// Push Angle to Lua
				break;
			}
			case (TYPETABLE): {
				LUA->CreateTable();									// Create table
				BinaryToTableLoop(LUA, stream);						// Call this function
				break;
			}
			default:												// should never happen
				break;
			}
		}

		if (top + 2 == LUA->Top())
			LUA->RawSet(-3);										// Push new variable to table
		else if (LUA->Top() > top)
			LUA->Pop(LUA->Top() - top);								// Try to pop as much variables off stack as are probably broken this should never be run but lets be prepared for everything
	}
}

LUA_FUNCTION(BinaryToTable) {
	LUA->CheckString(1);
	size_t outlen = 0;												// This will be our string length
	auto str = LUA->GetString(1, &outlen);							// Get string and string length from Lua
	QuickStrRead stream(str, outlen);								// Push string to custom read class
	LUA->CreateTable();												// Create Table that will be pushed to Lua when we are done
	BinaryToTableLoop(LUA, stream);									// Turn our string into actual variables
	return 1;														// return 1 variable to Lua
}

GMOD_MODULE_OPEN() {
    LUA->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);					// Get _G
	LUA->PushString("TableToBinary");								// Push name of function
	LUA->PushCFunction(TableToBinary);								// Push function
	LUA->SetTable(-3);												// Set variables to _G
	LUA->PushString("BinaryToTable");								// Push name of function
	LUA->PushCFunction(BinaryToTable);								// Push function
	LUA->SetTable(-3);												// Set variables to _G
	LUA->GetField(-1, "tostring");									// Get function tostring
	ToStringFN = LUA->ReferenceCreate();							// Create reference to function tostring
	LUA->Pop();														// Pop _G
	return 0;
}


GMOD_MODULE_CLOSE() {
	LUA->ReferenceFree(ToStringFN);
	return 0;
}
