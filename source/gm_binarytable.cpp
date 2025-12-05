#include "GarrysMod/Lua/Interface.h"
#include <string.h>
#include <nmmintrin.h>

namespace global {
	int ToStringFN = 0;
}

// AI code
uint32_t crc32_sse42( const void* data, size_t length ) {
	const uint8_t* p = reinterpret_cast< const uint8_t* >(data);

	// Standard IEEE CRC32 initial seed
	uint32_t crc = 0xFFFFFFFFu;

#if INTPTR_MAX == INT64_MAX
	// 64-bit target — align to 8 bytes
	while ( length && (reinterpret_cast< uintptr_t >(p) & 7) ) {
		crc = _mm_crc32_u8( crc, *p++ );
		--length;
	}

	const uint64_t* p64 = reinterpret_cast< const uint64_t* >(p);
	while ( length >= 8 ) {
		crc = _mm_crc32_u64( crc, *p64++ );
		length -= 8;
	}
	p = reinterpret_cast< const uint8_t* >(p64);

#else
	// 32-bit target — align to 4 bytes
	while ( length && (reinterpret_cast< uintptr_t >(p) & 3) ) {
		crc = _mm_crc32_u8( crc, *p++ );
		--length;
	}

	const uint32_t* p32 = reinterpret_cast< const uint32_t* >(p);
	while ( length >= 4 ) {
		crc = _mm_crc32_u32( crc, *p32++ );
		length -= 4;
	}
	p = reinterpret_cast< const uint8_t* >(p32);
#endif

	// Tail bytes
	while ( length-- ) {
		crc = _mm_crc32_u8( crc, *p++ );
	}

	return crc ^ 0xFFFFFFFFu;
}


struct Color { // Would make it unsigned short but the engine seems to limit it to 255
	unsigned char r;
	unsigned char g;
	unsigned char b;
	unsigned char a;
};

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
static const char TYPECOLOR = 15;
static const char TYPECRC = 127;

class QuickStrWrite {
public:
	QuickStrWrite() : length( 1048576 ) {
		str = new char[ length ];													// Init 1 MB the lower we set this the slower it will be
	}

	~QuickStrWrite() {																// Destroy class
		delete[] str;																// We need to delete otherwise we will create a memory leak
		str = 0;																	// unset for good measure
	}

	void WriteCRC() {
		uint32_t crc = crc32_sse42( str, position );								// Calculate the current CRC32 using SSE
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
	void write( const unsigned char& type, T in ) {
		if ( position + sizeof( T ) > length ) expand( sizeof( T ) + 1048576 );// Looks like we wont be able to fit the new data into the array, lets expand it by 1 MB
		str[ position++ ] = type;									// Write type at position
		*( T* )(str + position) = in;
		position += sizeof(T);										// Update position
	}

	void write( const char* in, unsigned int len ) {
		if ( position + len > length ) expand( len + 1048576 );		// Looks like we wont be able to fit the new data into the array, lets expand it by 1 MB
		memmove( str + position, in, len );							// Copy memory into char array
		position += len;											// Update position
	}

	char* GetStr() {
		return str;													// Return pointer of string
	}

	void reset() {
		position = 0;
	}

private:
	unsigned int position = 0;										// Position we are currently at
	unsigned int length = 0;										// String length
	char* str = 0;													// String pointer
};

void BinaryToStrLoop( GarrysMod::Lua::ILuaBase* LUA, QuickStrWrite& stream ) {
	LUA->PushNil();																			// Push nil for Next, This will be our key variable
	while ( LUA->Next( -2 ) ) {																// Get Next variable
		for ( int it = -2; it < 0; it++ ) {
			int type = LUA->GetType( it );													// Loop 2 then 1. Variable 1 is the key, variable 2 is value.
			switch ( type ) {
				case (GarrysMod::Lua::Type::Bool):
					{
						if ( LUA->GetBool( it ) ) {											// Check if bool is true or false and write it so we don't have to write 2 byes
							stream.writetype( BOOLTRUE );									// Write true to stream
						} else {
							stream.writetype( BOOLFALSE );									// Write true to stream
						}
						break;
					}
				case (GarrysMod::Lua::Type::Number):
					{
						double in = LUA->GetNumber( it );									// Get number
						if ( (in - ( long long )in) == 0 ) {								// Check if number is an integer
							auto size = GetStoreSize( in );									// Get minimum bytes required to store number
							switch ( size ) {
								case (sizeof( signed char )):
									{														// Can be stored as char
										char out = in;										// Turn our variable into a char
										stream.write( TYPECHAR, out );						// Write number to stream
										break;
									}
								case (sizeof( short )):
									{														// Can be stored as short
										short out = in;										// Turn our variable into a short
										stream.write( TYPESHORT, out );						// Write number to stream
										break;
									}
								case (sizeof( int )):
									{														// Can be stored as int
										int out = in;										// Turn our variable into a int
										stream.write( TYPEINT, out );						// Write number to stream
										break;
									}
								default:
									stream.write( TYPENUMBER, in );							// Write number to stream
									break;
							}
						} else {															// Number is not an integer
							stream.write( TYPENUMBER, in );									// Write number to stream 
						}
						break;
					}
				case (GarrysMod::Lua::Type::String):
					{
						unsigned int len = 0;												// Assign a length variable
						const char* in = LUA->GetString( it, &len );						// Get string and length
						if ( len < 0xFF ) {
							unsigned char out = len;
							stream.write( TYPESTRINGCHAR, out );							// Write string length
						} else if ( len < 0xFFFF ) {
							unsigned short out = len;
							stream.write( TYPESTRINGSHORT, out );							// Write string length
						} else {
							stream.write( TYPESTRINGLONG, len );							// Write string length
						}
						stream.write( in, len );											// Write string to stream
						break;
					}
				case (GarrysMod::Lua::Type::Vector):
					{
						stream.write( TYPEVECTOR, LUA->GetVector( it ));// Write Vector to stream
						break;
					}
				case (GarrysMod::Lua::Type::Angle):
					{
						stream.write( TYPEANGLE, LUA->GetAngle( it ) );	// Write Angle to stream
						break;
					}
				case (GarrysMod::Lua::Type::Table):
					{
						if ( LUA->GetMetaTable( it ) ) {										// Check if we have a metatable
							LUA->GetField( -1, "MetaName" );									// get MetaName and compare it
							const char* metaname = LUA->GetString( -1 );
							LUA->Pop( 2 );														// Pop metatable and metaname
							if ( strcmp( metaname, "Color" ) == 0 ) {
								LUA->Push( it );												// Push table to -1
								{
									Color col;
									LUA->GetField( -1, "r" );									// Get r
									col.r = LUA->GetNumber( -1 );
									LUA->Pop();
									LUA->GetField( -1, "g" );									// Get g
									col.g = LUA->GetNumber( -1 );
									LUA->Pop();
									LUA->GetField( -1, "b" );									// Get b
									col.b = LUA->GetNumber( -1 );
									LUA->Pop();
									LUA->GetField( -1, "a" );									// Get a
									col.a = LUA->GetNumber( -1 );
									LUA->Pop();
									stream.write( TYPECOLOR, col );								// Write to our buffer
								}
								LUA->Pop();
								break;
							}
						}

						LUA->Push( it );
						stream.writetype( TYPETABLE );										// Write table start to stream
						BinaryToStrLoop( LUA, stream );										// Start this function to extract everything from the table
						stream.writetype( TYPETABLEEND );									// Write table end to stream
						LUA->Pop();															// Pop table so we can continue with Next()
						
						break;
					}
				default:
					{
						// We do not know this type so we will use build in functions to convert it into a string
						LUA->ReferencePush( global::ToStringFN );							// Push reference function tostring
						LUA->Push( it - 1 );												// Push data we want to have the string for
						LUA->Call( 1, 1 );													// Call function with 1 variable and 1 return

						unsigned int len = 0;												// Assign a length variable
						const char* in = LUA->GetString( it, &len );						// Get string and length
						if ( len < 0xFF ) {
							unsigned char out = len;
							stream.write( TYPESTRINGCHAR, out );	// Write string length
						} else if ( len < 0xFFFF ) {
							unsigned short out = len;
							stream.write( TYPESTRINGSHORT, out );// Write string length
						} else {
							stream.write( TYPESTRINGLONG, len );	// Write string length
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
			case (GarrysMod::Lua::Type::Bool):
				{
					if ( LUA->GetBool( -1 ) ) {												// Check if bool is true or false and write it so we don't have to write 2 byes
						stream.writetype( BOOLTRUE );										// Write true to stream
					} else {
						stream.writetype( BOOLFALSE );										// Write true to stream
					}
					break;
				}
			case (GarrysMod::Lua::Type::Number):
				{
					double in = LUA->GetNumber( -1 );										// Get number
					if ( (in - ( long long )in) == 0 ) {									// Check if number is an integer
						auto size = GetStoreSize( in );										// Get minimum bytes required to store number
						switch ( size ) {
							case (sizeof( signed char )):
								{															// Can be stored as char
									char out = in;											// Turn our variable into a char
									stream.write( TYPECHAR, out );							// Write number to stream
									break;
								}
							case (sizeof( short )):
								{															// Can be stored as short
									short out = in;											// Turn our variable into a short
									stream.write( TYPESHORT, out );							// Write number to stream
									break;
								}
							case (sizeof( int )):
								{															// Can be stored as int
									int out = in;											// Turn our variable into a int
									stream.write( TYPEINT, out );							// Write number to stream
									break;
								}
							default:
								stream.write( TYPENUMBER, in );								// Write number to stream
								break;
						}
					} else {																// Number is not an integer
						stream.write( TYPENUMBER, in );										// Write number to stream 
					}
					break;
				}
			case (GarrysMod::Lua::Type::String):
				{
					unsigned int len = 0;													// Assign a length variable
					const char* in = LUA->GetString( -1, &len );							// Get string and length
					if ( len < 0xFF ) {
						unsigned char out = len;
						stream.write( TYPESTRINGCHAR, out );								// Write string length
					} else if ( len < 0xFFFF ) {
						unsigned short out = len;
						stream.write( TYPESTRINGSHORT, out );								// Write string length
					} else {
						stream.write( TYPESTRINGLONG, len );								// Write string length
					}
					stream.write( in, len );												// Write string to stream
					break;
				}
			case (GarrysMod::Lua::Type::Vector):
				{
					stream.write( TYPEVECTOR, LUA->GetVector( -1 ) );						// Write Vector to stream
					break;
				}
			case (GarrysMod::Lua::Type::Angle):
				{
					stream.write( TYPEANGLE, LUA->GetAngle( -1 ) );							// Write Angle to stream
					break;
				}
			case (GarrysMod::Lua::Type::Table):
				{
					if ( LUA->GetMetaTable( -1 ) ) {											// check if we have a metatable
						LUA->GetField( -1, "MetaName" );										// get MetaName and compare it
						const char* metaname = LUA->GetString( -1 );
						LUA->Pop( 2 );															// Pop metatable and MetaName
						if ( strcmp( metaname, "Color" ) == 0 ) {
							LUA->Push( -1 );													// Push table to -1
							{
								Color col;
								LUA->GetField( -1, "r" );										// Get r
								col.r = LUA->GetNumber( -1 );
								LUA->Pop();
								LUA->GetField( -1, "g" );										// Get g
								col.g = LUA->GetNumber( -1 );
								LUA->Pop();
								LUA->GetField( -1, "b" );										// Get b
								col.b = LUA->GetNumber( -1 );
								LUA->Pop();
								LUA->GetField( -1, "a" );										// Get a
								col.a = LUA->GetNumber( -1 );
								LUA->Pop();
								stream.write( TYPECOLOR, col );
							}
							LUA->Pop();
							break;
						}
					}

					LUA->Push( -1 );
					stream.writetype( TYPETABLE );										// Write table start to stream
					BinaryToStrLoopSeq( LUA, stream );									// Start this function to extract everything from the table
					stream.writetype( TYPETABLEEND );									// Write table end to stream
					LUA->Pop();															// Pop table so we can continue with Next()
					
					break;
				}
			default:
				{
					// We do not know this type so we will use build in functions to convert it into a string
					LUA->ReferencePush( global::ToStringFN );										// Push reference function tostring
					LUA->Push( -1 - 1 );													// Push data we want to have the string for
					LUA->Call( 1, 1 );														// Call function with 1 variable and 1 return

					unsigned int len = 0;													// Assign a length variable
					const char* in = LUA->GetString( -1, &len );							// Get string and length
					if ( len < 0xFF ) {
						unsigned char out = len;
						stream.write( TYPESTRINGCHAR, out );		// Write string length
					} else if ( len < 0xFFFF ) {
						unsigned short out = len;
						stream.write( TYPESTRINGSHORT, out );	// Write string length
					} else {
						stream.write( TYPESTRINGLONG, len);		// Write string length
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
	LUA->CheckType( 1, GarrysMod::Lua::Type::Table );							// Check if our input is a table
	bool CheckCRC = true;														// auto enabled
	if ( LUA->GetType( 3 ) == GarrysMod::Lua::Type::Bool ) { // For crc check
		CheckCRC = LUA->GetBool( 3 );
	}
	static QuickStrWrite stream;												// Custom stringstream replacement for this specific thing
	stream.reset();
	if ( LUA->GetType( 2 ) == GarrysMod::Lua::Type::Bool && LUA->GetBool( 2 ) ) {// Check if we have a second variable if this is true we assume the table is sequential
		LUA->Push( 1 );															// Push table to -1
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
		end = str + len;														// The end variable is the str offset + len
	}

	unsigned char GetType() {
		return *str++;															// Probably faster than calling memmove
	}

	template<class T>
	T *read() {
		T *to = ( T* )(str);													// Make a fake copy by setting it to an offset of the pointer of our input
		str += sizeof( T );														// Offset pointer
		return to;
	}

	const char *GetStr( unsigned int len ) {
		const char * out = str;													// Reference current str offset so we can return it.
		str += len;																// Just offset the pointer we don't need to delete it as its a reference to a Lua object
		return out;
	}

	bool atend() {																// Check if we are at the end of string
		return str == end;
	}

private:
	const char* end = 0;
	const char* str = 0;														// Input string pointer
};

void BinaryToTableLoop( GarrysMod::Lua::ILuaBase* LUA, QuickStrRead& stream ) {
	char top = LUA->Top();														// Store Top variable to check if we added something to our stack at the end
	while ( !stream.atend() ) {													// Loop as long as we have something in our stream or until we reach our endpos
		for ( int i = 0; i < 2; i++ ) {											// Loop twice for key and variable
			switch ( stream.GetType() ) {										// Get type and run corresponding code
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
						signed char *out = stream.read<signed char>();										// Read variable from stream
						LUA->PushNumber( *out );								// Push variable to Lua
						break;
					}
				case (TYPESHORT):
					{
						short *out = stream.read<short>();										// Read variable from stream
						LUA->PushNumber( *out );									// Push variable to Lua
						break;
					}
				case (TYPEINT):
					{
						int *out = stream.read<int>();										// Read variable from stream
						LUA->PushNumber( *out );									// Push variable to Lua
						break;
					}
				case (TYPENUMBER):
					{
						double *out = stream.read<double>();										// Read variable from stream
						LUA->PushNumber( *out );									// Push variable to Lua
						break;
					}
				case (TYPESTRINGCHAR):
					{
						unsigned char *len = stream.read<unsigned char>();
						const char* str = stream.GetStr( *len ); 				// Push len, pointer, header length
						LUA->PushString( str, *len );							// Push string to Lua with length
						break;
					}
				case (TYPESTRINGSHORT):
					{
						unsigned short *len = stream.read<unsigned short>();
						const char* str = stream.GetStr( *len ); 					// Push len, pointer, header length
						LUA->PushString( str, *len );							// Push string to Lua with length
						break;
					}
				case (TYPESTRINGLONG):
					{
						unsigned int *len = stream.read<unsigned int>();
						const char* str = stream.GetStr( *len ); 				// Push len, pointer, header length
						LUA->PushString( str, *len );							// Push string to Lua with length
						break;
					}
				case (TYPEVECTOR):
					{
						Vector *vec = stream.read<Vector>();										// Copy Vector from stream
						LUA->PushVector( *vec );									// Push Vector to Lua
						break;
					}
				case (TYPEANGLE):
					{
						QAngle *ang = stream.read<QAngle>();										// Copy Angle from stream
						LUA->PushAngle( *ang );									// Push Angle to Lua
						break;
					}
				case (TYPECOLOR):
					{
						Color* col = stream.read<Color>();

						LUA->CreateTable();
						LUA->PushNumber( col->r );
						LUA->SetField(-2, "r");
						LUA->PushNumber( col->g );
						LUA->SetField( -2, "g" );
						LUA->PushNumber( col->b );
						LUA->SetField( -2, "b" );
						LUA->PushNumber( col->a );
						LUA->SetField( -2, "a" );
						LUA->CreateMetaTable("Color");
						LUA->SetMetaTable( -2 );

						break;
					}
				case (TYPETABLE):
					{
						LUA->CreateTable();										// Create table
						BinaryToTableLoop( LUA, stream );						// Call this function
						break;
					}
				case (0):
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
					signed char* out = stream.read<signed char>();						// Read variable from stream
					LUA->PushNumber( *out );									// Push variable to Lua
					break;
				}
			case (TYPESHORT):
				{
					short* out = stream.read<short>();						// Read variable from stream
					LUA->PushNumber( *out );									// Push variable to Lua
					break;
				}
			case (TYPEINT):
				{
					int* out = stream.read<int>();						// Read variable from stream
					LUA->PushNumber( *out );									// Push variable to Lua
					break;
				}
			case (TYPENUMBER):
				{
					double* out = stream.read<double>();						// Read variable from stream
					LUA->PushNumber( *out );									// Push variable to Lua
					break;
				}
			case (TYPESTRINGCHAR):
				{
					unsigned char *len = stream.read<unsigned char>();
					const char* str = stream.GetStr( *len ); 					// Push len, pointer, header length
					LUA->PushString( str, *len );								// Push string to Lua with length
					break;
				}
			case (TYPESTRINGSHORT):
				{
					unsigned short *len = stream.read<unsigned short>();
					const char* str = stream.GetStr( *len ); 					// Push len, pointer, header length
					LUA->PushString( str, *len );								// Push string to Lua with length
					break;
				}
			case (TYPESTRINGLONG):
				{
					unsigned int *len = stream.read<unsigned int>();
					const char* str = stream.GetStr( *len ); 					// Push len, pointer, header length
					LUA->PushString( str, *len );								// Push string to Lua with length
					break;
				}
			case (TYPEVECTOR):
				{
					Vector *vec = stream.read<Vector>();										// Copy Vector from stream
					LUA->PushVector( *vec );									// Push Vector to Lua
					break;
				}
			case (TYPEANGLE):
				{
					QAngle *ang = stream.read<QAngle>();
					LUA->PushAngle( *ang );									// Push Angle to Lua
					break;
				}
			case (TYPECOLOR):
				{
					Color *col = stream.read<Color>();

					LUA->CreateTable();
					LUA->PushNumber( col->r );
					LUA->SetField( -2, "r" );
					LUA->PushNumber( col->g );
					LUA->SetField( -2, "g" );
					LUA->PushNumber( col->b );
					LUA->SetField( -2, "b" );
					LUA->PushNumber( col->a );
					LUA->SetField( -2, "a" );
					LUA->CreateMetaTable( "Color" );
					LUA->SetMetaTable( -2 );

					break;
				}
			case (TYPETABLE):
				{
					LUA->CreateTable();										// Create table
					BinaryToTableLoopSeq( LUA, stream );					// Call this function
					break;
				}
			default:														// should never happen
				break;
		}

		if ( top + 2 == LUA->Top() )
			LUA->RawSet( -3 );												// Push new variable to table
		else if ( LUA->Top() > top )
			LUA->Pop( LUA->Top() - top );									// Try to pop as much variables off stack as are probably broken this should never be run but lets be prepared for everything
	}
}

LUA_FUNCTION( BinaryToTable ) {
	LUA->CheckType( 1, GarrysMod::Lua::Type::String );		// Check type is string
	bool CheckCRC = true;									// auto enabled
	if ( LUA->GetType( 2 ) == GarrysMod::Lua::Type::Bool ) {// For crc check
		CheckCRC = LUA->GetBool( 2 );
	}
	unsigned int len = 0;									// This will be our string length
	const char* str = LUA->GetString( 1, &len );			// Get string and string length from Lua
	if ( str[ 0 ] == TYPECRC ) {							// check CRC
		auto offset = sizeof( char ) + sizeof( uint32_t );
		if ( CheckCRC ) {
			uint32_t knownCRC = 0;
			memmove( &knownCRC, str + sizeof( char ), sizeof( uint32_t ) );
			// We need to calculate the CRC from an offset
			uint32_t crc = crc32_sse42( str + offset, len - offset );
			if ( knownCRC != crc ) {
				LUA->ThrowError("CRC does not match");
				return 0;
			}
		} else if ( str[ offset ] < 0 || str[ offset ] > 15 ) {
			LUA->ThrowError( "Invalid input" );
			return 0;
		}
		str += offset;										// Since we have our crc we need to offset our str and len
		len -= offset;
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
	global::ToStringFN = LUA->ReferenceCreate();			// Create reference to function tostring
	LUA->Pop();												// Pop _G

	return 0;
}


GMOD_MODULE_CLOSE() {
	LUA->ReferenceFree( global::ToStringFN );

	return 0;
}
