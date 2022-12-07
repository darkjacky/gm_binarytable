#include "GarrysMod/Lua/Interface.h"
#include <string.h>
#include "crc32.h"

namespace global {
	uint32_t table[ 256 ];
	void init() {
		crc32::generate_table( table );
	}
}

unsigned int GetStoreSize( long long i ) {
	if ( i == ( signed char )(i & 0xFF) )
		return sizeof( signed char );
	if ( i == ( short )(i & 0xFFFF) )
		return sizeof( short );
	if ( i == ( int )(i & 0xFFFFFFFF) )
		return sizeof( int );
	return sizeof( long long );
}

// define custom types
static const char BOOLFALSE = 1;
static const char BOOLTRUE = 2;
static const char TYPENUMBER = 3;
static const char TYPECHAR = 4;
static const char TYPESHORT = 5;
static const char TYPEINT = 6;
static const char TYPESTRINGCHAR = 7;
static const char TYPESTRINGSHORT = 8;
static const char TYPESTRINGLONG = 9;
static const char TYPEVECTOR = 10;
static const char TYPEANGLE = 11;
static const char TYPETABLE = 12;
static const char TYPETABLEEND = 13;
static const char TYPETABLESEQ = 14;
static const char TYPECRC = 127;

class QuickStrWrite {
public:
	QuickStrWrite() {
		str = new char[ 1048576 ];									// Init 1 MB the lower we set this the slower it will be
	}

	~QuickStrWrite() {												// Destroy class
		delete[] str;												// We need to delete otherwise we will create a memory leak
		str = 0;													// unset for good measure
	}

	void WriteCRC() {
		uint32_t crc = crc32::update( global::table, 0, str, position );			// Calculate the current CRC
		auto inc = sizeof( char ) + sizeof( uint32_t );
		if ( position + inc > length ) {
			unsigned int newlen = position + inc;
			char* replacementstr = new char[ newlen ]; 								// We will end it here so make it fit instead of making it big
			replacementstr[ 0 ] = TYPECRC;
			memmove( replacementstr + sizeof( char ), &crc, sizeof( uint32_t ) );	// Copy CRC to string
			memmove( replacementstr + inc, str, position );							// Copy old string to new string
			position += inc;
			delete[] str;
			length = newlen;
			str = replacementstr;
		} else { // The data will fit so we just move the data and add new to the front
			memmove( str + inc, str, position ); // Move data to the right by 'inc'
			str[ 0 ] = TYPECRC;
			memmove( str + sizeof( char ), &crc, sizeof( uint32_t ) ); // Copy CRC to string
			position += inc;
		}
	}

	unsigned int GetLength() {										// To avoid messing with the length we copy it
		return position;
	}

	void expand( unsigned int amount ) {
		length += amount;											// Increase our length
		char* replacementstr = new char[ length ];					// Create our new char array larger than the one we had before
		memmove( replacementstr, str, position );					// memmove array
		delete[] str;												// Delete our smaller char array
		str = replacementstr;										// Replace the pointer for our array
	}

	void writetype( const unsigned char& type ) {
		if ( position + 1 > length ) expand( 1048576 );				// Looks like we wont be able to fit the new data into the array, lets expand it by 1 MB
		str[ position++ ] = type;									// Write type at position
	}

	template<class T>
	void write( const unsigned char& type, T* in, unsigned int len ) {
		if ( position + len > length ) expand( len + 1048576 );		// Looks like we wont be able to fit the new data into the array, lets expand it by 1 MB
		str[ position++ ] = type;									// Write type at position
		memmove( str + position, in, len );							// Copy memory into char array
		position += len;											// Update position
	}

	void write( const char* in, unsigned int len ) {
		if ( position + len > length ) expand( len + 1048576 );		// Looks like we wont be able to fit the new data into the array, lets expand it by 1 MB
		memmove( str + position, in, len );							// Copy memory into char array
		position += len;											// Update position
	}

	char* GetStr() {
		return str;													// Return pointer of string
	}

private:
	unsigned int position = 0;										// Position we are currently at
	unsigned int length = 0;										// String length
	char* str = 0;													// String pointer
};

int ToStringFN = 0;
void BinaryToStrLoop( GarrysMod::Lua::ILuaBase* LUA, QuickStrWrite& stream ) {
	LUA->PushNil();																			// Push nil for Next, This will be our key variable
	while ( LUA->Next( -2 ) ) {																// Get Next variable
		for ( int it = -2; it < 0; it++ ) {
			int type = LUA->GetType( it );													// Loop 2 then 1. Variable 1 is the key, variable 2 is value.
			switch ( type ) {
				case (GarrysMod::Lua::Type::BOOL):
					{
						if ( LUA->GetBool( it ) ) {											// Check if bool is true or false and write it so we don't have to write 2 byes
							stream.writetype( BOOLTRUE );									// Write true to stream
						} else {
							stream.writetype( BOOLFALSE );									// Write true to stream
						}
						break;
					}
				case (GarrysMod::Lua::Type::NUMBER):
					{
						double in = LUA->GetNumber( it );									// Get number
						if ( (in - ( long long )in) == 0 ) {								// Check if number is an integer
							auto size = GetStoreSize( in );									// Get minimum bytes required to store number
							switch ( size ) {
								case (sizeof( signed char )):
									{														// Can be stored as char
										char out = in;										// Turn our variable into a char
										stream.write( TYPECHAR, &out, sizeof( signed char ) );// Write number to stream
										break;
									}
								case (sizeof( short )):
									{														// Can be stored as short
										short out = in;										// Turn our variable into a short
										stream.write( TYPESHORT, &out, sizeof( short ) );	// Write number to stream
										break;
									}
								case (sizeof( int )):
									{														// Can be stored as int
										int out = in;										// Turn our variable into a int
										stream.write( TYPEINT, &out, sizeof( int ) );		// Write number to stream
										break;
									}
								default:
									stream.write( TYPENUMBER, &in, sizeof( double ) );		// Write number to stream
									break;
							}
						} else {															// Number is not an integer
							stream.write( TYPENUMBER, &in, sizeof( double ) );				// Write number to stream 
						}
						break;
					}
				case (GarrysMod::Lua::Type::STRING):
					{
						unsigned int len = 0;												// Assign a length variable
						const char* in = LUA->GetString( it, &len );						// Get string and length
						if ( len < 0xFF ) {
							stream.write( TYPESTRINGCHAR, &len, sizeof( unsigned char ) );	// Write string length
						} else if ( len < 0xFFFF ) {
							stream.write( TYPESTRINGSHORT, &len, sizeof( unsigned short ) );// Write string length
						} else {
							stream.write( TYPESTRINGLONG, &len, sizeof( unsigned int ) );	// Write string length
						}
						stream.write( in, len );											// Write string to stream
						break;
					}
				case (GarrysMod::Lua::Type::VECTOR):
					{
						stream.write( TYPEVECTOR, &LUA->GetVector( it ), sizeof( Vector ) );// Write Vector to stream
						break;
					}
				case (GarrysMod::Lua::Type::ANGLE):
					{
						stream.write( TYPEANGLE, &LUA->GetAngle( it ), sizeof( QAngle ) );	// Write Angle to stream
						break;
					}
				case (GarrysMod::Lua::Type::TABLE):
					{
						LUA->Push( it );													// Push table to -1
						stream.writetype( TYPETABLE );										// Write table start to stream
						BinaryToStrLoop( LUA, stream );										// Start this function to extract everything from the table
						stream.writetype( TYPETABLEEND );									// Write table end to stream
						LUA->Pop();															// Pop table so we can continue with Next()
						break;
					}
				default:
					{
						// We do not know this type so we will use build in functions to convert it into a string
						LUA->ReferencePush( ToStringFN );									// Push reference function tostring
						LUA->Push( it - 1 );												// Push data we want to have the string for
						LUA->Call( 1, 1 );													// Call function with 1 variable and 1 return

						unsigned int len = 0;												// Assign a length variable
						const char* in = LUA->GetString( it, &len );						// Get string and length
						if ( len < 0xFF ) {
							unsigned char convlen = len;
							stream.write( TYPESTRINGCHAR, &len, sizeof( unsigned char ) );	// Write string length
						} else if ( len < 0xFFFF ) {
							stream.write( TYPESTRINGSHORT, &len, sizeof( unsigned short ) );// Write string length
						} else {
							stream.write( TYPESTRINGLONG, &len, sizeof( unsigned int ) );	// Write string length
						}
						stream.write( in, len );											// Write string to stream
						LUA->Pop();															// Pop string
						break;
					}
			}
		}
		LUA->Pop();																			// Pop value from stack
	}
}

void BinaryToStrLoopSeq( GarrysMod::Lua::ILuaBase* LUA, QuickStrWrite& stream ) {
	LUA->PushNil();																			// Push nil for Next, This will be our key variable
	while ( LUA->Next( -2 ) ) {																// Get Next variable
		int type = LUA->GetType( -1 );														// Loop 2 then 1. Variable 1 is the key, variable 2 is value.
		switch ( type ) {
			case (GarrysMod::Lua::Type::BOOL):
				{
					if ( LUA->GetBool( -1 ) ) {												// Check if bool is true or false and write it so we don't have to write 2 byes
						stream.writetype( BOOLTRUE );										// Write true to stream
					} else {
						stream.writetype( BOOLFALSE );										// Write true to stream
					}
					break;
				}
			case (GarrysMod::Lua::Type::NUMBER):
				{
					double in = LUA->GetNumber( -1 );										// Get number
					if ( (in - ( long long )in) == 0 ) {									// Check if number is an integer
						auto size = GetStoreSize( in );										// Get minimum bytes required to store number
						switch ( size ) {
							case (sizeof( signed char )):
								{															// Can be stored as char
									char out = in;											// Turn our variable into a char
									stream.write( TYPECHAR, &out, sizeof( signed char ) );	// Write number to stream
									break;
								}
							case (sizeof( short )):
								{															// Can be stored as short
									short out = in;											// Turn our variable into a short
									stream.write( TYPESHORT, &out, sizeof( short ) );		// Write number to stream
									break;
								}
							case (sizeof( int )):
								{															// Can be stored as int
									int out = in;											// Turn our variable into a int
									stream.write( TYPEINT, &out, sizeof( int ) );			// Write number to stream
									break;
								}
							default:
								stream.write( TYPENUMBER, &in, sizeof( double ) );			// Write number to stream
								break;
						}
					} else {																// Number is not an integer
						stream.write( TYPENUMBER, &in, sizeof( double ) );					// Write number to stream 
					}
					break;
				}
			case (GarrysMod::Lua::Type::STRING):
				{
					unsigned int len = 0;													// Assign a length variable
					const char* in = LUA->GetString( -1, &len );							// Get string and length
					if ( len < 0xFF ) {
						stream.write( TYPESTRINGCHAR, &len, sizeof( unsigned char ) );		// Write string length
					} else if ( len < 0xFFFF ) {
						stream.write( TYPESTRINGSHORT, &len, sizeof( unsigned short ) );	// Write string length
					} else {
						stream.write( TYPESTRINGLONG, &len, sizeof( unsigned int ) );		// Write string length
					}
					stream.write( in, len );												// Write string to stream
					break;
				}
			case (GarrysMod::Lua::Type::VECTOR):
				{
					stream.write( TYPEVECTOR, &LUA->GetVector( -1 ), sizeof( Vector ) );	// Write Vector to stream
					break;
				}
			case (GarrysMod::Lua::Type::ANGLE):
				{
					stream.write( TYPEANGLE, &LUA->GetAngle( -1 ), sizeof( QAngle ) );		// Write Angle to stream
					break;
				}
			case (GarrysMod::Lua::Type::TABLE):
				{
					LUA->Push( -1 );														// Push table to -1
					stream.writetype( TYPETABLE );											// Write table start to stream
					BinaryToStrLoopSeq( LUA, stream );										// Start this function to extract everything from the table
					stream.writetype( TYPETABLEEND );										// Write table end to stream
					LUA->Pop();																// Pop table so we can continue with Next()
					break;
				}
			default:
				{
					// We do not know this type so we will use build in functions to convert it into a string
					LUA->ReferencePush( ToStringFN );										// Push reference function tostring
					LUA->Push( -1 - 1 );													// Push data we want to have the string for
					LUA->Call( 1, 1 );														// Call function with 1 variable and 1 return

					unsigned int len = 0;													// Assign a length variable
					const char* in = LUA->GetString( -1, &len );							// Get string and length
					if ( len < 0xFF ) {
						unsigned char convlen = len;
						stream.write( TYPESTRINGCHAR, &len, sizeof( unsigned char ) );		// Write string length
					} else if ( len < 0xFFFF ) {
						stream.write( TYPESTRINGSHORT, &len, sizeof( unsigned short ) );	// Write string length
					} else {
						stream.write( TYPESTRINGLONG, &len, sizeof( unsigned int ) );		// Write string length
					}
					stream.write( in, len );												// Write string to stream
					LUA->Pop();																// Pop string
					break;
				}
		}
		LUA->Pop();																			// Pop value from stack
	}
}

LUA_FUNCTION( TableToBinary ) {
	LUA->CheckType( 1, GarrysMod::Lua::Type::TABLE );							// Check if our input is a table
	bool CheckCRC = true;														// auto enabled
	if ( LUA->GetType( 3 ) == GarrysMod::Lua::Type::BOOL ) { // For crc check
		CheckCRC = LUA->GetBool( 3 );
	}
	if ( LUA->GetType( 2 ) == GarrysMod::Lua::Type::BOOL && LUA->GetBool( 2 ) ) {// Check if we have a second variable if this is true we assume the table is sequential
		LUA->Push( 1 );															// Push table to -1
		QuickStrWrite stream;													// Custom stringstream replacement for this specific thing
		stream.writetype( TYPETABLESEQ );										// Write Sequential table indicator
		BinaryToStrLoopSeq( LUA, stream );										// Compute table into string
		LUA->Pop();																// Pop table
		if ( CheckCRC ) {
			stream.WriteCRC();
		}
		LUA->PushString( stream.GetStr(), stream.GetLength() );					// Get string and length of string then push as return value
		return 1;
	}
	LUA->Push( 1 );																// Push table to -1
	QuickStrWrite stream;														// Custom stringstream replacement for this specific thing
	BinaryToStrLoop( LUA, stream );												// Compute table into string
	LUA->Pop();																	// Pop table
	if ( CheckCRC ) {															// Check for CRC
		stream.WriteCRC();
	}
	LUA->PushString( stream.GetStr(), stream.GetLength() );						// Get string and length of string then push as return value
	return 1;																	// return 1 variable to Lua
}

class QuickStrRead {
public:
	QuickStrRead( const char* in, unsigned int& len ) {
		str = in;																// Copy string pointer
		length = len;															// Copy length
	}

	~QuickStrRead() {
		if ( Tstr )
			delete[] Tstr;														// Delete the buffer we created
	}

	unsigned char GetType() {
		return str[ position++ ];												// probably faster than calling memmove
	}

	template<class T>
	void read( T* to, unsigned int len ) {
		if ( len + position > length ) return;									// Should not happen but if it did, this prevents crashing
		memmove( to, str + position, len );										// Copy from string to buffer until we reach length
		position += len;
	}

	void ReadString( unsigned int& len, char*& outstr, unsigned int readsize ) {
		read( &len, readsize );
		if ( Tstrlen > 0 && Tstrlen < len ) {									// Can not reuse buffer, it is too small
			delete[] Tstr;														// Delete old char array
			Tstr = 0;															// Reset pointer
		}
		if ( !Tstr ) {															// We don't have a buffer we need to create one
			Tstr = new char[ len ];												// Create a new char array
			Tstrlen = len;														// We have created a new Temporary string save its length in our variable
		}
		read( Tstr, len );														// Read to buffer
		outstr = Tstr;															// Set external pointer
	}

	bool atend() {																// Check if we are at the end of string
		return position == length;
	}

private:
	unsigned int position = 0;													// Position we are reading the string from
	unsigned int length = 0;													// Length of string
	const char* str = 0;														// String pointer
	char* Tstr = 0;																// Temporary storage char array
	unsigned int Tstrlen = 0;													// Length of temporary char array
};

void BinaryToTableLoop( GarrysMod::Lua::ILuaBase* LUA, QuickStrRead& stream ) {
	char top = LUA->Top();														// Store Top variable to check if we added something to our stack at the end
	while ( !stream.atend() ) {													// Loop as long as we have something in our stream or until we reach our endpos
		for ( int i = 0; i < 2; i++ ) {											// Loop twice for key and variable
			switch ( stream.GetType() ) {										// Get type and run corresponding code
				case (0):
					{
						return;													// This should not happen
					}
				case (TYPETABLEEND):
					{
						return;													// We are at the end of our table there is nothing left
					}
				case (BOOLTRUE):
					{
						LUA->PushBool( true );									// Push true to Lua
						break;
					}
				case (BOOLFALSE):
					{
						LUA->PushBool( false );									// Push false to Lua
						break;
					}
				case (TYPECHAR):
					{
						signed char out = 0;									// Create variable
						stream.read( &out, sizeof( char ) );					// Read variable from stream
						LUA->PushNumber( out );									// Push variable to Lua
						break;
					}
				case (TYPESHORT):
					{
						short out = 0;											// Create variable
						stream.read( &out, sizeof( short ) );					// Read variable from stream
						LUA->PushNumber( out );									// Push variable to Lua
						break;
					}
				case (TYPEINT):
					{
						int out = 0;											// Create variable
						stream.read( &out, sizeof( int ) );						// Read variable from stream
						LUA->PushNumber( out );									// Push variable to Lua
						break;
					}
				case (TYPENUMBER):
					{
						double out = 0;											// Create variable
						stream.read( &out, sizeof( double ) );					// Read variable from stream
						LUA->PushNumber( out );									// Push variable to Lua
						break;
					}
				case (TYPESTRINGCHAR):
					{
						unsigned int len = 0;									// Create length variable
						char* str;												// Create pointer
						stream.ReadString( len, str, sizeof( unsigned char ) );	// Push len, pointer, header length
						LUA->PushString( str, len );							// Push string to Lua with length
						break;
					}
				case (TYPESTRINGSHORT):
					{
						unsigned int len = 0;									// Create length variable
						char* str;												// Create pointer
						stream.ReadString( len, str, sizeof( unsigned short ) );// Push len, pointer, header length
						LUA->PushString( str, len );							// Push string to Lua with length
						break;
					}
				case (TYPESTRINGLONG):
					{
						unsigned int len = 0;									// Create length variable
						char* str;												// Create pointer
						stream.ReadString( len, str, sizeof( unsigned int ) );	// Push len, pointer, header length
						LUA->PushString( str, len );							// Push string to Lua with length
						break;
					}
				case (TYPEVECTOR):
					{
						Vector vec;												// Create Vector variable
						stream.read( &vec, sizeof( Vector ) );					// Copy Vector from stream
						LUA->PushVector( vec );									// Push Vector to Lua
						break;
					}
				case (TYPEANGLE):
					{
						QAngle ang;												// Create Angle variable
						stream.read( &ang, sizeof( QAngle ) );					// Copy Angle from stream
						LUA->PushAngle( ang );									// Push Angle to Lua
						break;
					}
				case (TYPETABLE):
					{
						LUA->CreateTable();										// Create table
						BinaryToTableLoop( LUA, stream );						// Call this function
						break;
					}
				default:														// should never happen
					break;
			}
		}

		if ( top + 2 == LUA->Top() )
			LUA->RawSet( -3 );													// Push new variable to table
		else if ( LUA->Top() > top )
			LUA->Pop( LUA->Top() - top );										// Try to pop as much variables off stack as are probably broken this should never be run but lets be prepared for everything
	}
}

void BinaryToTableLoopSeq( GarrysMod::Lua::ILuaBase* LUA, QuickStrRead& stream ) {
	unsigned int position = 1;													// Lua starts with a table index of 1
	char top = LUA->Top();														// Store Top variable to check if we added something to our stack at the end
	while ( !stream.atend() ) {													// Loop as long as we have something in our stream or until we reach our endpos
		LUA->PushNumber( position++ );											// Push index and increase its value
		switch ( stream.GetType() ) {											// Get type and run corresponding code
			case (0):
				{
					LUA->Pop();													// Pop number we just pushed
					return;														// This should not happen
				}
			case (TYPETABLEEND):
				{
					LUA->Pop();													// Pop number we just pushed
					return;														// We are at the end of our table there is nothing left
				}
			case (BOOLTRUE):
				{
					LUA->PushBool( true );										// Push true to Lua
					break;
				}
			case (BOOLFALSE):
				{
					LUA->PushBool( false );										// Push false to Lua
					break;
				}
			case (TYPECHAR):
				{
					signed char out = 0;										// Create variable
					stream.read( &out, sizeof( char ) );						// Read variable from stream
					LUA->PushNumber( out );										// Push variable to Lua
					break;
				}
			case (TYPESHORT):
				{
					short out = 0;												// Create variable
					stream.read( &out, sizeof( short ) );						// Read variable from stream
					LUA->PushNumber( out );										// Push variable to Lua
					break;
				}
			case (TYPEINT):
				{
					int out = 0;												// Create variable
					stream.read( &out, sizeof( int ) );							// Read variable from stream
					LUA->PushNumber( out );										// Push variable to Lua
					break;
				}
			case (TYPENUMBER):
				{
					double out = 0;												// Create variable
					stream.read( &out, sizeof( double ) );						// Read variable from stream
					LUA->PushNumber( out );										// Push variable to Lua
					break;
				}
			case (TYPESTRINGCHAR):
				{
					unsigned int len = 0;										// Create length variable
					char* str;													// Create pointer
					stream.ReadString( len, str, sizeof( unsigned char ) );		// Push len, pointer, header length
					LUA->PushString( str, len );								// Push string to Lua with length
					break;
				}
			case (TYPESTRINGSHORT):
				{
					unsigned int len = 0;										// Create length variable
					char* str;													// Create pointer
					stream.ReadString( len, str, sizeof( unsigned short ) );	// Push len, pointer, header length
					LUA->PushString( str, len );								// Push string to Lua with length
					break;
				}
			case (TYPESTRINGLONG):
				{
					unsigned int len = 0;										// Create length variable
					char* str;													// Create pointer
					stream.ReadString( len, str, sizeof( unsigned int ) );		// Push len, pointer, header length
					LUA->PushString( str, len );								// Push string to Lua with length
					break;
				}
			case (TYPEVECTOR):
				{
					Vector vec;													// Create Vector variable
					stream.read( &vec, sizeof( Vector ) );						// Copy Vector from stream
					LUA->PushVector( vec );										// Push Vector to Lua
					break;
				}
			case (TYPEANGLE):
				{
					QAngle ang;													// Create Angle variable
					stream.read( &ang, sizeof( QAngle ) );						// Copy Angle from stream
					LUA->PushAngle( ang );										// Push Angle to Lua
					break;
				}
			case (TYPETABLE):
				{
					LUA->CreateTable();											// Create table
					BinaryToTableLoopSeq( LUA, stream );						// Call this function
					break;
				}
			default:															// should never happen
				break;
		}

		if ( top + 2 == LUA->Top() )
			LUA->RawSet( -3 );													// Push new variable to table
		else if ( LUA->Top() > top )
			LUA->Pop( LUA->Top() - top );										// Try to pop as much variables off stack as are probably broken this should never be run but lets be prepared for everything
	}
}

LUA_FUNCTION( BinaryToTable ) {
	LUA->CheckType( 1, GarrysMod::Lua::Type::STRING );		// Check type is string
	bool CheckCRC = true;									// auto enabled
	if ( LUA->GetType( 2 ) == GarrysMod::Lua::Type::BOOL ) {// For crc check
		CheckCRC = LUA->GetBool( 2 );
	}
	unsigned int len = 0;									// This will be our string length
	const char* str = LUA->GetString( 1, &len );			// Get string and string length from Lua
	if ( str[ 0 ] == TYPECRC ) {							// check CRC
		if ( CheckCRC ) {
			uint32_t knownCRC = 0;
			memmove( &knownCRC, str + sizeof( char ), sizeof( uint32_t ) );
			// We need to calculate the CRC from an offset
			auto offset = sizeof( char ) + sizeof( uint32_t );
			uint32_t crc = crc32::update( global::table, 0, str + offset, len - offset );
			if ( knownCRC != crc ) {
				LUA->ThrowError("CRC does not match");
				return 0;
			}
		}
	} else if ( str[ 0 ] < 0 || str[ 0 ] > 15 ) {			// We cant process this
		LUA->ThrowError( "Invalid input" );					// Throw an error
		return 0;											// Return nothing
	}

	QuickStrRead stream( str, len );						// Push string to custom read class
	LUA->CreateTable();										// Create Table that will be pushed to Lua when we are done
	if ( str[ 0 ] == TYPETABLESEQ ) {						// Sequential table
		stream.GetType();									// Type is sequential so we need to remove the first char
		BinaryToTableLoopSeq( LUA, stream );				// Turn our string into actual variables
	} else {
		BinaryToTableLoop( LUA, stream );					// Turn our string into actual variables
	}
	return 1;												// return 1 variable to Lua
}

GMOD_MODULE_OPEN() {
	LUA->PushSpecial( GarrysMod::Lua::SPECIAL_GLOB );		// Get _G
	LUA->PushString( "TableToBinary" );						// Push name of function
	LUA->PushCFunction( TableToBinary );					// Push function
	LUA->SetTable( -3 );									// Set variables to _G
	LUA->PushString( "BinaryToTable" );						// Push name of function
	LUA->PushCFunction( BinaryToTable );					// Push function
	LUA->SetTable( -3 );									// Set variables to _G
	LUA->GetField( -1, "tostring" );						// Get function tostring
	ToStringFN = LUA->ReferenceCreate();					// Create reference to function tostring
	LUA->Pop();												// Pop _G

	global::init();

	return 0;
}


GMOD_MODULE_CLOSE() {
	LUA->ReferenceFree( ToStringFN );
	return 0;
}
