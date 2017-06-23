#include "ATParser.h"
#include <string.h>
//#include "assert_param.h"
//#include "vcom.h"


#ifndef NULL
#define NULL 0
#endif

#define assert_param( x )


typedef enum {
	ATParserCommandTypeInvalid,
	ATParserCommandTypeGet,
	ATParserCommandTypeSet,
	ATParserCommandTypeTest,
	ATParserCommandTypeExecute
} ATParserCommandType_t;

#define NOT_FOUND -1



void ATParser_init( ATParser_t * parser, uint8_t * marker, uint8_t markerLen, ATParserCommand_t * commands, uint8_t commandCount, ATParserExecute markerOnlyCallback )
{
	assert_param( parser != NULL );
	assert_param( marker != NULL && markerLen > 0 );
	assert_param( commands != NULL & commandCount > 0 );

	memcpy( parser->Marker, marker, markerLen );
	parser->MarkerLen = markerLen;
	parser->Commands = commands;
	parser->CommandCount = commandCount;
	parser->MarkerOnlyCallback = markerOnlyCallback;
}


ATParserResult_t ATParser_parseString( ATParser_t * parser, uint8_t * input, uint8_t inputLen )
{
    uint16_t start = 0;
    uint16_t end = inputLen - 1;
    uint16_t len = inputLen;

    // determine actual command start
    for (bool found = false; !found && parser->MarkerLen < len;)
    {
    	if (memcmp( &input[start], parser->Marker, parser->MarkerLen) == 0)
    	{
    		found = true;
    	}
    	else
    	{
    		start++;
    		len--;
    	}
    }

    // if the length is shorter than the marker, there was no marker found
    if (len < parser->MarkerLen)
    {
    	return ATParserResultError;
    }

    // determine actual command ending (well, we trim)
    for (bool found = false; !found && start < end; )
    {
    	switch(input[end])
    	{
    		// for convenience skip all of these characters
    		case '\0':
    		case '\n':
    		case '\r':
    		case ' ':
    		case '\t':
    			end--;
    			len--;
    			break;

    		default:
    			found = true;
    			break;
    	}
    }

    // if the length is exactly the marker length, then we're dealing with the markeronly test command
    if (len == parser->MarkerLen)
    {
    	if ( parser->MarkerOnlyCallback != NULL )
    	{
    		return parser->MarkerOnlyCallback();
    	}
    	return ATParserResultError;
    }
    
    // try to find a matching command
    uint8_t * parseStart = &input[start + parser->MarkerLen];
    ATParserCommand_t * command = NULL;
    for (uint8_t i = 0; i < parser->CommandCount; i++)
    {
    	// if the remaining length is shorter than the command, it just can't be that one
    	if (len < parser->Commands[i].Length)
    	{
    		continue;
    	}
    	// if the string does not match, it just can't be it.
    	if (memcmp( parseStart, parser->Commands[i].String, parser->Commands[i].Length ) == 0)
    	{
    		command = &parser->Commands[i];
    	}
    }

    // if not found, return error
    if ( command == NULL )
    {
    	return ATParserResultError;
    }

    uint8_t argStart = start + parser->MarkerLen + command->Length;
    ATParserCommandType_t commandType = ATParserCommandTypeInvalid;
    if ( argStart == end + 1 )
    {
    	if (command->Execute != NULL)
    	{
        	commandType = ATParserCommandTypeExecute;
    	}
    }
    else if ( input[argStart] == '?' )
    {
    	if (command->Get != NULL)
    	{
        	commandType = ATParserCommandTypeGet;
        	argStart++;
    	}
    }
    else if ( argStart + 1 <= end && input[argStart] == '=' && input[argStart+1] == '?' )
    {
    	if (command->Test != NULL)
    	{
        	commandType = ATParserCommandTypeTest;
    	}
    }
    else if ( argStart < end && input[argStart] == '=' )
    {
    	if (command->Set != NULL)
    	{
        	commandType = ATParserCommandTypeSet;
        	argStart++;
    	}
    }

    // so we didn't find a valid command
    if ( commandType == ATParserCommandTypeInvalid )
    {
    	return ATParserResultError;
    }


    uint8_t argc = 0;
    uint8_t * argv[AT_PARSER_ARGUMENTS_MAX_COUNT];
    if ( argStart <= end && ( commandType == ATParserCommandTypeGet || commandType == ATParserCommandTypeSet ) )
    {
    	argv[argc++] = &input[argStart];

    	for (uint8_t i = argStart; i <= end && argc < AT_PARSER_ARGUMENTS_MAX_COUNT; i++)
    	{
    		// if there is a comma, we assume there's a new argument
    		if (input[i] == ',')
    		{
    			// for convenience.. (although here we change the original buffer)
    			input[i] = '\0';
				argv[argc++] = &input[i+1];
    		}
    	}
    	input[end+1] = '\0';

    	if (argc == AT_PARSER_ARGUMENTS_MAX_COUNT)
    	{
    		return ATParserResultError;
    	}
    }
    switch( commandType )
    {
    	case ATParserCommandTypeGet:
    		return command->Get( argc, argv );

    	case ATParserCommandTypeSet:
    		return command->Set( argc, argv );

    	case ATParserCommandTypeExecute:
    		return command->Execute();

    	case ATParserCommandTypeTest:
    		return command->Test();

    	default: // this should never be reached
        	//PRINTF("should not occur\n");
    		return ATParserResultError;
    }

}
