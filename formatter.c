#include "H.h"

#define formatter_version AS_BYTES( 0.1 )

start
{
	group( line_type )
	{
		line_unknown,

		line_comment,
		line_include,
		line_define,
		line_define_multi,
		line_if,
		line_elif_else,
		line_endif,
		line_preprocessor
	};

	group( token_type )
	{
		token_unknown,

		token_newline,
		token_newline_gap,
		token_define_newline,
		token_word,
		token_value,
		token_symbol,
		token_open_parenthesis,
		token_closed_parenthesis,
		token_semicolon
	};

	group( assignment_type )
	{
		assignment_unknown,

		assignment_start,
		assignment_single_line,
		assignment_multi_line
	};

	temp n1 input_file_index = 1;

	if( input_count <= 1 )
	{
		print( "usage: formatter [text-to-format] [optional-out]\n   or: formatter version\n" );
		out success;
	}
	else
	{
		if( bytes_compare( input_bytes[ 1 ], "version", 7 ) is 0 )
		{
			print( "formatter version " formatter_version " (" OS_NAME ")\n" );
			out success;
		}
	}

	//

	temp const n2 input_path_size = bytes_measure( input_bytes[ input_file_index ] );
	file input_file = map_file( input_bytes[ input_file_index ], input_path_size );

	if_nothing( input_file.mapped_bytes )
	{
		print( "file-mapping failed: cannot find file \"" );
		print( input_bytes[ 1 ] );
		print( "\"" );
		out failure;
	}

	temp byte ref input_ref = input_file.mapped_bytes;

	perm declare_bytes( output, MB( 1 ) );
	temp n4 line_size = 0;
	temp line_type current_line_type = line_unknown;
	temp token_type previous_token_type = token_unknown;
	temp assignment_type current_assignment_type = assignment_unknown;

	temp i2 parenthesis_scope = 0;
	temp i2 brace_scope = 0;

	//

	#define output_set( BYTE ) val_of( output_ref ) = BYTE

	#define _output_add( BYTE )\
		output_set( BYTE );\
		++output_ref

	#define output_add( BYTE )\
		START_DEF\
		{\
			_output_add( BYTE );\
			++line_size;\
		}\
		END_DEF

	#define output_add_input() output_add( val_of( input_ref++ ) )

	#define output_add_newline()\
		START_DEF\
		{\
			if( current_line_type is line_define_multi )\
			{\
				_output_add( '\\' );\
				previous_token_type = token_define_newline;\
			}\
			else\
			{\
				previous_token_type = token_newline;\
			}\
			_output_add( '\n' );\
			line_size = 0;\
		}\
		END_DEF

	#define output_add_space()\
		START_DEF\
		{\
			skip_if( line_size is 0 );\
			output_add( ' ' );\
		}\
		END_DEF

	#define output_add_indent()\
		START_DEF\
		{\
			skip_if( line_size isnt 0 );\
			repeat( brace_scope )\
			{\
				output_set( '\t' );\
				++output_ref;\
			}\
		}\
		END_DEF

	//

	check_input:
	{
		temp byte input_ref_val = val_of( input_ref );

		with( input_ref_val )
		{
			when( '\0' )
			{
				jump input_eof;
			}

			when( ' ', '\t' )
			{
				++input_ref;
				jump check_input;
			}

			when( '\r' )
			{
				if( val_of( input_ref + 1 ) is '\n' )
				{
					++input_ref;
				}
			} // fall through
			when( '\n' )
			{
				if( current_assignment_type is assignment_start )
				{
					current_assignment_type = assignment_multi_line;
					++brace_scope;
				}
				else if( current_assignment_type isnt assignment_multi_line )
				{
					current_assignment_type = assignment_unknown;
				}

				with( current_line_type )
				{
					when( line_define, line_define_multi )
					{
						if( previous_token_type is token_define_newline )
						{
							output_ref -= 2;
						}
						current_line_type = line_unknown;
					} // fall through
					when( line_endif )
					{
						--brace_scope;
					} // fall through
					when( line_include, line_if, line_elif_else, line_preprocessor )
					{
						output_add_newline();
					}
				}

				if( previous_token_type is token_newline_gap and parenthesis_scope is 0 )
				{
					output_add_newline();
					previous_token_type = token_unknown;
				}
				else if( previous_token_type isnt token_unknown )
				{
					previous_token_type = token_newline_gap;
				}

				current_line_type = line_unknown;
				++input_ref;
				jump check_input;
			}

			other skip;
		}

		if( current_assignment_type is assignment_start )
		{
			current_assignment_type = assignment_single_line;
		}

		with( input_ref_val )
		{
			when( '#' )
			{
				previous_token_type = token_symbol;

				if( line_size is 0 )
				{
					++input_ref;

					preprocessor_skip_gap:
					{
						with( val_of( input_ref ) )
						{
							when( ' ', '\t' )
							{
								++input_ref;
								jump preprocessor_skip_gap;
							}

							other skip;
						}
					}

					if( val_of( input_ref ) is 'e' )
					{ // #else, #elif, and #endif are back 1 scope
						--brace_scope;
					}
					output_add_indent();
					output_add( '#' );

					with( val_of( input_ref ) )
					{
						when( 'i' )
						{
							if( val_of( input_ref + 1 ) is 'n' )
							{
								current_line_type = line_include;
							}
							else
							{
								current_line_type = line_if;
								++brace_scope;
							}
							skip;
						}

						when( 'd' )
						{
							current_line_type = line_define;
							++brace_scope;
							skip;
						}

						when( 'e' )
						{
							++brace_scope;
							if( val_of( input_ref + 1 ) is 'l' )
							{
								current_line_type = line_elif_else;
							}
							else
							{
								current_line_type = line_endif;
							}
							skip;
						}

						other
						{
							current_line_type = line_preprocessor;
							skip;
						};
					}

					jump check_input;
				}

				if( val_of( input_ref + 1 ) is '#' )
				{
					output_add_input();
				}
				else
				{
					output_add_space();
				}

				output_add_input();
				jump check_input;
			}

			when( '\\' )
			{
				++input_ref;
				process_define_newline:
				{
					with( val_of( input_ref ) )
					{
						when( '\r' )
						{
							if( val_of( input_ref + 1 ) is '\n' )
							{
								++input_ref;
							}
						} // fall through
						when( '\n' )
						{
							++input_ref;
							skip;
						}

						when( ' ', '\t' )
						{
							++input_ref;
							jump process_define_newline;
						}

						other skip;
					}
				}

				if( current_line_type is line_define )
				{
					current_line_type = line_define_multi;
					output_add_newline();
				}

				jump check_input;
			}

			when( '(' )
			{
				if( ( previous_token_type isnt token_word and val_of( input_ref - 1 ) isnt '-' ) or ( parenthesis_scope isnt 0 and val_of( input_ref - 1 ) is ' ' ) )
				{
					output_add_space();
				}
				previous_token_type = token_open_parenthesis;
				output_add_input();
				++parenthesis_scope;
				jump check_input;
			}

			when( ')' )
			{
				if( previous_token_type isnt token_open_parenthesis )
				{
					output_add_space();
				}
				previous_token_type = token_closed_parenthesis;
				output_add_input();
				--parenthesis_scope;
				jump check_input;
			}

			when( '{', '}' )
			{
				if( parenthesis_scope isnt 0 or current_assignment_type is assignment_single_line )
				{
					jump add_input;
				}

				if( current_line_type is line_define )
				{
					if( val_of( output_ref - 1 ) isnt '{' )
					{
						output_add_space();
					}
				}
				else if( line_size isnt 0 )
				{
					output_add_newline();
				}

				if( input_ref_val is '{' )
				{
					output_add_indent();
					++brace_scope;
				}
				else
				{
					--brace_scope;
					output_add_indent();
				}
				output_add_input();

				if( current_line_type isnt line_define )
				{
					output_add_newline();
				}
				jump check_input;
			}

			when( '[' )
			{
				previous_token_type = token_symbol;
				output_add_input();
				jump check_input;
			}

			when( ']' )
			{
				if( val_of( output_ref - 1 ) isnt '[' )
				{
					output_add_space();
				}
				previous_token_type = token_symbol;
				output_add_input();
				jump check_input;
			}

			when( '<' )
			{
				if( current_line_type isnt line_include )
				{
					jump add_input;
				}

				output_add( ' ' );
				while( val_of( input_ref ) isnt '>' )
				{
					output_add_input();
				}
				output_add_input();
				jump check_input;
			}

			when( ',' )
			{
				previous_token_type = token_symbol;
				if( parenthesis_scope is 0 and current_line_type is line_define )
				{
					output_add( ' ' );
				}
				else if( val_of( output_ref - 1 ) is '\n' )
				{
					--output_ref;
				}

				output_add_input();
				if( parenthesis_scope is 0 and current_assignment_type isnt assignment_single_line and current_line_type isnt line_define )
				{
					output_add_newline();
				}
				jump check_input;
			}

			when( '=' )
			{
				if( val_of( input_ref + 1 ) is '=' )
				{
					jump add_input;
				}

				with( val_of( output_ref - 1 ) )
				{
					when( '=', '<', '>', '!', '+', '-', '*', '/', '%', '&', '|', '^' )
					{
						skip;
					}

					other
					{
						output_add_space();
						if( current_assignment_type is assignment_unknown and parenthesis_scope is 0 )
						{
							current_assignment_type = assignment_start;
						}
					}
				}

				previous_token_type = token_symbol;
				output_add_input();
				jump check_input;
			}

			when( ':' )
			{
				if( val_of( input_ref + 1 ) is ':' )
				{
					previous_token_type = token_symbol;
					output_add_input();
					output_add_input();
					jump check_input;
				}
				else if_any( current_assignment_type is assignment_single_line, parenthesis_scope isnt 0, previous_token_type is token_value, previous_token_type is token_symbol, previous_token_type is token_closed_parenthesis )
				{
					jump add_input;
				}
			} // fall through
			when( ';' )
			{
				if( current_assignment_type is assignment_multi_line )
				{
					--brace_scope;
				}

				current_assignment_type = assignment_unknown;

				if( parenthesis_scope isnt 0 )
				{
					if( previous_token_type is token_open_parenthesis )
					{
						output_add_space();
					}
					output_add_input();
					previous_token_type = token_semicolon;
					jump check_input;
				}

				if( previous_token_type is token_newline )
				{
					--output_ref;
				}
				previous_token_type = token_semicolon;

				output_add_input();

				skip_semicolon_whitespace:
				{
					with( val_of( input_ref ) )
					{
						when( ' ', '\t' )
						{
							++input_ref;
							jump skip_semicolon_whitespace;
						}

						other skip;
					}
				}

				if( current_line_type isnt line_define )
				{
					output_add_newline();
				}
				jump check_input;
			}

			when( '/' )
			{
				if( val_of( input_ref + 1 ) is '/' )
				{
					current_line_type = line_comment;
					previous_token_type = token_newline;

					if( ( val_of( output_ref - 1 ) is '\n' or val_of( output_ref - 1 ) is '\t' ) and val_of( input_ref - 1 ) isnt '\n' and val_of( input_ref - 1 ) isnt '\t' )
					{
						--output_ref;
						line_size = n4_max;
					}
					else
					{
						output_add_indent();
					}

					output_add_space();
					output_add_input();
					output_add_input();

					process_comment:
					{
						with( val_of( input_ref ) )
						{
							when( '\0' )
							{
								jump input_eof;
							}

							when( '\r' )
							{
								if( val_of( input_ref + 1 ) is '\n' )
								{
									++input_ref;
								}
							} // fall through
							when( '\n' )
							{
								output_add_newline();
								jump check_input;
							}

							other
							{
								output_add_input();
								jump process_comment;
							}
						}
					}
				}
				else if( val_of( input_ref + 1 ) is '*' )
				{
					output_add_indent();
					output_add_space();
					output_add_input();
					output_add_input();

					process_multi_comment:
					{
						with( val_of( input_ref ) )
						{
							when( '\0' )
							{
								jump input_eof;
							}

							when( '*' )
							{
								if( val_of( input_ref + 1 ) is '/' )
								{
									output_add_input();
									output_add_input();
									jump check_input;
								}
								// else fall through
							}

							other
							{
								output_add_input();
								jump process_multi_comment;
							}
						}
					}
				}
				else
				{
					jump add_input;
				}
			}

			when( '\'' )
			{
				previous_token_type = token_symbol;
				output_add_indent();
				output_add_space();
				output_add_input();

				if( val_of( input_ref ) is '\\' )
				{
					output_add_input();
				}
				output_add_input();
				output_add_input();
				jump check_input;
			}

			when( '"' )
			{
				previous_token_type = token_symbol;
				output_add_indent();
				output_add_space();
				output_add_input();

				process_bytes:
				{
					with( val_of( input_ref ) )
					{
						when( '"' )
						{
							output_add_input();
							skip;
						}

						when( '\\' )
						{
							output_add_input();
						}

						other
						{
							output_add_input();
							jump process_bytes;
						}
					}
				}
				jump check_input;
			}

			when( '+' )
			{
				if( val_of( input_ref + 1 ) is '+' )
				{
					output_add_indent();
					if( previous_token_type isnt token_word )
					{
						output_add_space();
					}
					output_add_input();
					output_add_input();
					jump check_input;
				}

				jump add_input;
			}

			when( '-' )
			{
				with( val_of( input_ref + 1 ) )
				{
					when( '-' )
					{
						output_add_indent();
						if( previous_token_type isnt token_word )
						{
							output_add_space();
						}
					} // fall through
					when( '>' )
					{
						output_add_input();
						output_add_input();
						jump check_input;
					}
				}

				jump add_input;
			}

			when( '.' )
			{
				output_add_indent();

				if( val_of( input_ref + 1 ) is '.' )
				{
					if( previous_token_type isnt token_word )
					{
						output_add_space();
					}
					output_add_input();
					output_add_input();
					jump check_input;
				}

				previous_token_type = token_symbol;
				output_add_input();
				jump check_input;
			}

			//

			other
			{
				add_input:
				{
					output_add_indent();

					if( is_letter( input_ref_val ) or input_ref_val is '_' )
					{
						if_all( val_of( input_ref - 1 ) isnt '-', val_of( output_ref - 1 ) isnt '.', val_of( output_ref - 1 ) isnt '#', ( val_of( output_ref - 1 ) isnt '*' or val_of( input_ref - 1 ) is ' ' ), val_of( output_ref - 1 ) isnt '&', val_of( output_ref - 1 ) isnt '!', ( val_of( output_ref - 1 ) isnt '+' or val_of( output_ref - 2 ) isnt '+' ), ( val_of( output_ref - 1 ) isnt '-' or val_of( output_ref - 2 ) isnt '-' ), ( val_of( output_ref - 1 ) isnt '>' or val_of( output_ref - 2 ) isnt '-' ) )
						{
							output_add_space();
						}

						previous_token_type = token_word;

						do
						{
							output_add_input();
							input_ref_val = val_of( input_ref );
						}
						while_any( is_letter( input_ref_val ), is_number( input_ref_val ), input_ref_val is '_' );
					}
					else if( is_number( input_ref_val ) )
					{
						if( val_of( input_ref - 1 ) isnt '-' )
						{
							output_add_space();
						}

						previous_token_type = token_value;

						do
						{
							output_add_input();
							input_ref_val = val_of( input_ref );
						}
						while_any( is_letter( input_ref_val ), is_number( input_ref_val ), input_ref_val is '.' );
					}
					else
					{
						// is symbol
						if( val_of( input_ref - 1 ) isnt input_ref_val )
						{
							output_add_space();
						}

						previous_token_type = token_symbol;
						output_add_input();
					}

					jump check_input;
				}
			}
		}
	}

	//

	input_eof:
	{
		if( val_of( output_ref - 1 ) isnt '\n' )
		{
			output_add_newline();
		}

		print( "formatting: \"" );
		print( input_bytes[ input_file_index ] );
		print( "\"" );

		file_unmap( ref_of( input_file ) );

		//

		file output_file;
		if( input_count > 2 )
		{
			output_file = new_file( input_bytes[ input_file_index + 1 ] );

			print( "\noutput: \"" );
			print( output_file.path );
			print( "\"" );
		}
		else
		{
			output_file = new_file( input_bytes[ input_file_index ], input_path_size );
		}

		file_save( ref_of( output_file ), output, output_ref - output );
		file_close( ref_of( output_file ) );
		print_newline();
		out success;
	}
}
