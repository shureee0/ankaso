#include <stdio.h>
#include <stdlib.h>
//#include <string.h>

#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIXS
#include "nob.h"

#include "./include/miniz/miniz.h"

#define defer(x) do{	      \
		result = (x); \
		goto defer;   \
	}while(0)

#define DICT_PATH "Ankaso.ods"
#define DICT_SHEET_NAME "Dictionary"
#define TABLE_STR "table:table" 

#define DICT_SHEET_SV        sv_from_cstr("Dictionary")
#define TABLE_SV             sv_from_cstr("table:table")
#define OFFICE_ANNOTATION_SV sv_from_cstr("office:annotation")
#define COLUMN_SV            sv_from_cstr("table:table-column")
#define ROW_SV               sv_from_cstr("table:table-row")
#define CELL_SV              sv_from_cstr("table:table-cell")
#define TEXTP_SV             sv_from_cstr("text:p")
#define TEXTSPAN_SV          sv_from_cstr("text:span")

#define SV_JS_NULL sv_from_cstr("null")

#define SV_EMPTY (String_View){.data = 0,.count = 0};
#define IS_SV_EMPTY(sv) ((sv).data == 0 && (sv).count == 0)
// :empty switch for your copypasting needs
/*
static_assert(__XmlTabType_count == 0,"update FUNCTION_NAME\n");
switch(tab.tab_type){
	case XM_OPEN:{
		tab.indent = indent++;
		da_append(tabs,tab);
		TODO("OPEN");
		break;
	}
	case XM_CLOSE:{
		tab.indent = --indent;
		da_append(tabs,tab);
		TODO("CLOSE");
		break;
	}
	case XM_CONTENT:{
		tab.indent = indent;
		da_append(tabs,tab);
		TODO("CONTENT");
		break;
	}
	case XM_SELF_CONTAINED:{
		tab.indent = indent;
		da_append(tabs,tab);
		TODO("SELF_CONTAINED");
		break;
	}
	case __XmlTabType_count: UNREACHABLE("__XmlTabType_count in parse_count");
}
*/

// :langs
#define ROOT sv_from_cstr("root")
//TODO: have this a x macro or some kinda fo code gen
#define EN_GENERAL sv_from_cstr("general")
//this name kinda suck 
#define EN_ROOT sv_from_cstr("root meaning")

// :output buffers
String_Builder ankaso_js_buffer = {0};
String_Builder en_js_buffer = {0};

//TODO: put structs and declarations in ods-parser.h
// :structs
/*
 * da is a "type" given in nob.h
 * which allows for dynamic arrays
 * which requires 
 * any *items
 * size_t count
 * size_t capacity
 */
typedef struct{
	String_View *items;
	size_t count;
	size_t capacity;
}String_Views;

typedef struct{
	String_View root;
	String_Views general;
	String_Views root_meaning;
}EN_Word;

typedef struct{
	String_View root;
}Ankaso_Word;

typedef struct{
	String_View name;
	String_View value;
}Attribute;

typedef struct{
	Attribute *items;
	size_t count;
	size_t capacity;
}Attributes;

typedef enum{
	XM_OPEN,
	XM_CLOSE,
	XM_CONTENT,
	XM_SELF_CONTAINED,
	__XmlTabType_count
}XmlTabType;

typedef struct{
	XmlTabType tab_type;
	uint64_t indent;
	union{
		String_View type;
		String_View content;
	};
	Attributes atts;
}XmlTab;

typedef struct{
	XmlTab *items;
	size_t count;
	size_t capacity;
}XmlTabs;

/*
 * quick reason why .ods as the input
 * .ods are just a bunch of .xml files
 * ziped together where all the content
 * of the sheets are stored in content.xml
 */

// :forward decs
static inline bool is_white_space(char c);
static inline bool inc_sv(String_View *sv);
bool parse_textspan(String_View *content,XmlTabs *tabs,String_Builder *out,size_t indent,String_View end);
bool parse_content(String_View *content,XmlTabs *tabs,String_Builder *out,size_t indent,String_View end);

static inline String_Builder sv_to_sb(String_View sv){
	return (String_Builder){.count = sv.count,.capacity = sv.count,.items = (void*)sv.data};
}

bool get_content(String_View* out){
	/*
	 * miniz is only used with this function
	 * and it's a rather big thing for what we
	 * so TODO: replace miniz with a smaller lib
	 *
	 * this is the unziper of the zip in the .ods file
	 * which also grabs the content.xml
	 */
	bool result = true;//defer
	mz_zip_archive zip_archive;
	memset(&zip_archive, 0, sizeof(zip_archive));

	nob_log(NOB_INFO,"unziping %s",DICT_PATH);
	mz_bool status = mz_zip_reader_init_file(&zip_archive, DICT_PATH, 0);
	if (!status) {
		nob_log(NOB_ERROR,"Could not open %s as zip file",DICT_PATH);
		defer(false);
	}
	
	nob_log(NOB_INFO,"grabing content.xml");
	int file_i = mz_zip_reader_locate_file(&zip_archive,"content.xml",NULL,0);
	if(file_i < 0){
		nob_log(NOB_ERROR,"Could find content.xml in zip");
		defer(false);
	}

	size_t size;
	//TODO: maybe this needs to use custom alocator
	//or memcpy
	nob_log(NOB_INFO,"extracting content.xml from zip to heap");
	char* pBuf = mz_zip_reader_extract_to_heap(&zip_archive, file_i, &size, 0);	
	if(!pBuf){
		nob_log(NOB_ERROR,"Could not extract content.xml to heap");
		defer(false);
	}
	String_View sv = {.count = size,.data = pBuf};
	*out = sv;
defer:
	mz_zip_reader_end(&zip_archive);
	return result;
}

String_Views get_entrys_from_sv(String_View sv){
	/*
	 * the list in the spreed sheeet
	 * are seperated by a comma and a space but not
	 * when there is a newline so to make parsing
	 * then it is only a newline
	 * example
	 * 	a, ai\n
	 * 	ayo
	 * therefore we split by whitespace and chop commas
	 */
	String_Views result = {0};
	String_View comp = sv;
	for(;;){
		if(!inc_sv(&sv)) break;	
		if(is_white_space(sv.data[0])){
			comp.count = comp.count - sv.count;
			comp = sv_trim_right(comp);

			sv_chop_prefix(&comp,sv_from_cstr(","));
			da_append(&result,comp);
			sv = sv_trim_left(sv);//get rid of white spaces
			comp = sv;
		}
	}
	if(result.count == 0) da_append(&result,comp);//case for one entry
	return result;
}

// :xml parsing

String_View sv_cpy(String_View sv){
	char *temp = malloc(sv.count);
	if(temp == NULL) return (String_View){.data = NULL};
	memcpy(temp,sv.data,sv.count);
	return (String_View){.count = sv.count,.data = temp};
	
}

static inline bool is_white_space(char c){
	return c == ' ' || c == '\n' || c == '\t' || c == '\r';
}

//#define INC_SV(sv) (((sv)->data)++ || (sv)->count >= 0)
static inline bool inc_sv(String_View *sv){
	/*
	 * this function is used instead of nob_shift_right
	 * because we need to get know if it's empty
	*/
	bool result = sv->count != 0;//so we can check the last char
	++(sv->data);
	--(sv->count);
	return result;
}
/*
 * this function takes a buffer shifts it
 * grabs the next tab
 */
bool get_next_tab(String_View *sv,XmlTab* out){
	//TODO: add better error messages
	bool result = true; //defer
	*out = (XmlTab){0};//TODO:

	String_Builder sb = {0};
	bool first = false;
	*sv = sv_trim_left(*sv);
	if(sv->data[0] != '<'){
		out->tab_type = XM_CONTENT;

		for(;;){
			//TODO: parse quotes
			if(sv->data[0] == '<') break;
			sb_append(&sb,sv->data[0]);
			if(!inc_sv(sv)) defer(false);
		}
		out->content = sv_cpy(sb_to_sv(sb));
		if(out->content.data == NULL){
			printf("OUT OF RAM OH NO!!!!");
			defer(false);
		}	
		defer(true);
	}
	if(!inc_sv(sv)) defer(false);
	if(sv->data[0] == '?'){
		out->tab_type = XM_SELF_CONTAINED;
		first = true;
		if(!inc_sv(sv)) defer(false);
	}
	else if(sv->data[0] == '/'){
		out->tab_type = XM_CLOSE;
		if(!inc_sv(sv)) defer(false);
	}
	else out->tab_type = XM_OPEN;
	*sv = sv_trim_left(*sv);
	// :first
	if(first) for(;;){
		//TODO: this should only get used as the first tab is 
		//TODO: complint with xml 1.0
		if(sv->data[0] == '=' || sv->data[0] == '/' || sv->data[0] == '>'){
			printf("unexepected \'%c\' for tab\n",sv->data[0]);
			defer(false);
		}
		if(is_white_space(sv->data[0]) || sv->data[0] == '?'){
			//TODO: bit of a memory leak 
			//sv_cpy should be replaced with 
			//a better memory system
			//this is not that bad right now since the content.xml
			//is small but is could be a problum later
			out->type = sv_cpy(sb_to_sv(sb));
			if(out->type.data == NULL){
				printf("OUT OF RAM OH NO!!!!");
				defer(false);
			}
			if(sv->data[0] != '?') if(!inc_sv(sv)) defer(false);
			sb.count = 0;//clean sb
			break;
		}
		sb_append(&sb,sv->data[0]);
		if(!inc_sv(sv)) defer(false);
	}
	else for(;;){
		if(sv->data[0] == '=' || sv->data[0] == '?'){
			printf("unexepected \'%c\' for tab\n",sv->data[0]);
			defer(false);
		}
		if(is_white_space(sv->data[0]) || sv->data[0] == '/' || sv->data[0] == '>'){
			//TODO: bit of a memory leak 
			//sv_cpy should be replaced with 
			//a better memory system
			//this is not that bad right now since the content.xml
			//is small but is could be a problum later
			out->type = sv_cpy(sb_to_sv(sb));
			if(out->type.data == NULL){
				printf("OUT OF RAM OH NO!!!!");
				defer(false);
			}
			if(sv->data[0] != '/' && sv->data[0] != '>') if(!inc_sv(sv)) defer(false);
			sb.count = 0;//clean sb
			break;
		}
		sb_append(&sb,sv->data[0]);
		if(!inc_sv(sv)) defer(false);
	}
	// :attributes parsing
	for(;;){
		*sv = sv_trim_left(*sv);
		if(first && sv->data[0] == '?'){
			if(!inc_sv(sv)) defer(false);
			*sv = sv_trim_left(*sv);
			if(sv->data[0] != '>'){
				printf("exepected \'>\' for tab\n");
				defer(false);
			}
			if(!inc_sv(sv)) defer(false);
			break;
		}
		else if(!first && sv->data[0] == '/'){
			if(out->tab_type == XM_OPEN) out->tab_type = XM_SELF_CONTAINED;
			else{
				printf("only tabtype open can be self closed\n");
				defer(false);
			}
			//defer(false);
			if(inc_sv(sv)){//this is end of file we are done
				*sv = sv_trim_left(*sv);
			}
			if(sv->data[0] != '>'){
				printf("exepected \'>\' for tab\n");
				defer(false);
			}
			if(!inc_sv(sv)){/*defer(false);*/}
			break;
		}
		//this covers the 'first' case
		else if(sv->data[0] == '>'){
			if(!inc_sv(sv)){ /*defer(false);*/ }//EOF
			break;
		}
	//att_end end
		String_View key;
		String_View value;
		// :key
		for(;;){
			if(sv->data[0] == '>' || sv->data[0] == '/' || sv->data[0] == '?'){
				printf("unexpected char \'%c\' for attribute name\n",sv->data[0]);
				defer(false);
			}
			if(is_white_space(sv->data[0]) || sv->data[0] == '='){
				//TODO: bit of a memory leak 
				//sv_cpy should be replaced with 
				//a better memory system
				//this is not that bad right now since the content.xml
				//is small but is could be a problum later
				key = sv_cpy(sb_to_sv(sb));
				if(key.data == NULL){
					printf("OUT OF RAM OH NO!!!!");
					defer(false);
				}	
				sb.count = 0;//clean sb
				if(sv->data[0] == '=') break;//break early so we can check for it
				if(!inc_sv(sv)) defer(false); 
				break;
			}
			sb_append(&sb,sv->data[0]);
			if(!inc_sv(sv)) defer(false); 
		}
		*sv = sv_trim_left(*sv);
		if(sv->data[0] != '=') defer(false);
		if(!inc_sv(sv)) defer(false); 
		// :value
		*sv = sv_trim_left(*sv);
		char quote = sv->data[0];
		if(quote != '\"' && quote != '\'') defer(1);
		if(!inc_sv(sv)) defer(false); 
		*sv = sv_trim_left(*sv);
		for(;;){
			if(sv->data[0] == quote){
				//printf("values\n");
				//TODO: bit of a memory leak 
				//sv_cpy should be replaced with 
				//a better memory system
				//this is not that bad right now since the content.xml
				//is small but is could be a problum later
				value = sv_cpy(sb_to_sv(sb));
				if(value.data == NULL){
					printf("OUT OF RAM OH NO!!!!");
					defer(false);
				}	
				if(!inc_sv(sv)) defer(false); 
				sb.count = 0;//clean sb
				break;
			}
			sb_append(&sb,sv->data[0]);
			if(!inc_sv(sv)) defer(false);
		}
		//printf(SV_Fmt":\'"SV_Fmt"\'\n",SV_Arg(key),SV_Arg(value));
		if(key.count == 0){
			printf("ERROR: attribute name is empty\n");
			defer(false);
		}
		if(value.count == 0) value = SV_JS_NULL;
		da_append(&out->atts,((Attribute){.name = key,.value = value}));
	}
defer:
	sb_free(sb);//could avoid this with static
	return result;
}

void print_tab(XmlTab tab,uint64_t indent){
	/* TODO:
	 * this function was writen for debuging
	 * but broke early on a new version
	 * should be written for easier debuging
	 */
	(void)tab;	
	(void)indent;
	TODO("print tab");
}

void sb_json_close(String_Builder *sb,char end){
	char c = da_pop(sb);//da_pop asserts 
	if(c != ',') sb_append(sb,c);
	sb_append(sb,end);
}
	
void sb_append_json_sv(String_Builder *sb,String_View sv){
	/*
	 * used to put key and value pairs for json
	 */
	sv = sv_trim(sv);
	sb_append(sb,'\"');
	do{
		char c = sv.data[0];
		if(c == '\"') sb_append(sb,'\\');
		sb_append(sb,c);
	}while(inc_sv(&sv));
	(void)da_pop(sb);//pop the null terminator
	sb_append(sb,'\"');
}

void sb_append_json_svs(String_Builder *sb,String_Views svs){
	sb_append(sb,'[');
	for(size_t i = 0; i < svs.count;++i){
		String_View sv = svs.items[i];
		sb_append_json_sv(sb,sv);
		sb_append(sb,',');
	}
	sb_json_close(sb,']');
}

int get_attribute(XmlTab tab,String_View neddle){
	/* TODO:
	 * negative indecates a failure so
	 * att.count is size_t which is grater than
	 * positive int
	 */
	for(int i = 0; i < (int)tab.atts.count;++i) if(sv_eq(neddle, tab.atts.items[i].name)) return i;
	return -1;
}


bool parse_first_row(String_View *content,XmlTabs *tabs,uint64_t indent){
	/* TODO: 
	 * this is where we can unhard code the order of the columns
	 */
	bool result = true;
	++indent;
	XmlTab tab = {0};
	for(;;){
		if(content->count == 0) defer(false); //means we ended premucutly
		if(!get_next_tab(content,&tab)){
			defer(false);
		}
		static_assert(__XmlTabType_count == 4,"update parse_first_row\n");
		switch(tab.tab_type){
			case XM_OPEN:{
				tab.indent = indent++;
				da_append(tabs,tab);
				if(sv_eq(tab.type,ROW_SV)) defer(false);
				break;
			}
			case XM_CLOSE:{
				tab.indent = --indent;
				da_append(tabs,tab);
				if(sv_eq(tab.type,ROW_SV)) defer(true);
				break;
			}
			case XM_CONTENT:{
				tab.indent = indent;
				da_append(tabs,tab);
				break;
			}
			case XM_SELF_CONTAINED:{
				tab.indent = indent;
				da_append(tabs,tab);
				break;
			}
			case __XmlTabType_count:UNREACHABLE("XmlTabType_count found in parse column in parse_first_row");
		}
	}
defer:
	return result;
}

bool close_tab(XmlTabs *tabs,XmlTab tab){
	/*
	 * TODO: when this cleans memory
	 * we need to make sure we don't have use after frees
	 * when we stop leaking memory
	 * like we are using js
	 */
	assert(tab.tab_type == XM_CLOSE);
	bool result = true;
	for(;;){
		XmlTab comp = da_pop(tabs);//TODO:this will assert if there are no items
		//TODO: free the memory
		if(comp.indent < tab.indent){
			nob_log(NOB_ERROR,"indent in close_tab is below passed tab");
			defer(false);
		}
		if(comp.indent == tab.indent
		&& sv_eq(comp.type,tab.type) 
		&& comp.tab_type == XM_OPEN) defer(true); 
	}
defer:
	return result;
}

bool parse_content(String_View *content,XmlTabs *tabs,String_Builder *out,size_t indent,String_View end){
	bool result = true;
	String_Builder sb = {0};
	XmlTab tab = {0};
	for(;;){
		if(content->count == 0) defer(false); //means we ended premucutly
		if(!get_next_tab(content,&tab))	defer(false);
		static_assert(__XmlTabType_count == 4,"update parse_content\n");
		switch(tab.tab_type){
			case XM_OPEN:{
				tab.indent = indent++;
				da_append(tabs,tab);
				if(sv_eq(tab.type,end)) defer(false);
				if(sv_eq(tab.type,TEXTSPAN_SV)){ 
					if(!parse_content(content,tabs,&sb,indent,TEXTSPAN_SV)) defer(false);
					--indent;
					break;
				}
				else UNREACHABLE("unknown type");
				break;
			}
			case XM_CLOSE:{
				tab.indent = --indent;
				da_append(tabs,tab);
				if(!close_tab(tabs,tab)) defer(false);
				if(sv_eq(tab.type,end)) defer(true);
				TODO("close");
				break;
			}
			case XM_CONTENT:{
				tab.indent = indent;
				da_append(tabs,tab);
				sb_append_sv(&sb,tab.content);
				sb_append(&sb,' ');
				break;
			}
			case XM_SELF_CONTAINED:{
				tab.indent = indent;
				da_append(tabs,tab);
				TODO("SELF_CONTAINED");
				break;
			}
			case __XmlTabType_count: UNREACHABLE("__XmlTabType_count in parse_count");
		}
	}
defer:
	sb_append_sv(out,sb_to_sv(sb));//a bit hacky
	sb_free(sb);
	return result;
}

bool parse_cell(String_View *content,XmlTabs *tabs,String_View *out,size_t indent){
	bool result = true;

	XmlTab tab = {0};
	String_Builder sb_out = {0};

	if(content->count == 0) TODO("EOF"); //means we ended premucutly
	if(!get_next_tab(content,&tab))	TODO("FAILED TO GET TAB");
	switch(tab.tab_type){
		case XM_OPEN:{
			tab.indent = indent++;
			da_append(tabs,tab);
			if(!sv_eq(tab.type,CELL_SV)) TODO("not a cell");

			size_t saved_indent = indent;
			for(;;){
				if(content->count == 0) defer(false); //means we ended premucutly
				if(!get_next_tab(content,&tab))	defer(false);
		
				static_assert(__XmlTabType_count == 4,"update parse_cell\n");
				switch(tab.tab_type){
					case XM_OPEN:{
						//NOTE: this just keeps apending content idk if this is a good idea
						tab.indent = indent++;
						da_append(tabs,tab);
						//this + 1 kinda sucks but oh well
						if(indent == saved_indent + 1 && sv_eq(tab.type,TEXTP_SV)){
							if(!parse_content(content,tabs,&sb_out,indent,TEXTP_SV)) 
								TODO("parse content failed");
							--indent;//go back becauce it auto closes
						}
						break;
					}
					case XM_CLOSE:{
						tab.indent = --indent;
						da_append(tabs,tab);
						if(!close_tab(tabs,tab)) defer(false);
						if(sv_eq(tab.type,CELL_SV)) defer(true);
						break;
					}
					case XM_CONTENT:{
						tab.indent = indent;
						da_append(tabs,tab);
						if(indent == saved_indent) UNREACHABLE("content not expected in cell");
						//printf("|"SV_Fmt"|\n",SV_Arg(tab.content));
						//TODO("content");
						break;
					}
					case XM_SELF_CONTAINED:{
						tab.indent = indent;
						da_append(tabs,tab);
						//printf("|"SV_Fmt"|\n",SV_Arg(tab.type));
						//TODO("sc");
						break;
					}
					case __XmlTabType_count:UNREACHABLE("xmltabtype_count in parse_cell");
				}
			}
			break;
		}
		case XM_CLOSE:{
			tab.indent = --indent;
			da_append(tabs,tab);
			UNREACHABLE("expected cell but found a close type tab");
			break;
		}
		case XM_SELF_CONTAINED:{
			tab.indent = indent;
			da_append(tabs,tab);
			if(!sv_eq(tab.type,CELL_SV)) TODO("not a cell");

			if(!sv_eq(tab.type,CELL_SV)) defer(false);
			//*out = SV_JS_NULL;
			break;
		}
		case XM_CONTENT:{
			tab.indent = indent;
			da_append(tabs,tab);
			if(!sv_eq(tab.type,CELL_SV)) TODO("not a cell");
			TODO("content");
			break;
		}
		case __XmlTabType_count:UNREACHABLE("xmltabtype_count in parse_cell");
	}
defer:
	*out = sb_to_sv(sb_out);
	return result; 
}

bool pull_cell(String_View *content,XmlTabs *tabs,uint64_t indent,String_View *out){
	/*
	 * return true means to check the data
	 * SV_EMPTY is error
	 * return false means end of the row
	 * mostly empty rows
	 */
	String_View sv_saved = *content;
	XmlTab tab = {0};
	if(content->count == 0){
		nob_log(NOB_ERROR,"Premature EOF");//TODO: put location of __line__ __file__
		*out = SV_EMPTY;
		return true;
	}
	if(!get_next_tab(content,&tab)){
		*out = SV_EMPTY;
		return true;
	}

	static_assert(__XmlTabType_count == 4,"update pull_cell\n");
	if(tab.tab_type == XM_CLOSE){
		tab.indent = --indent;
		da_append(tabs,tab);
		if(!close_tab(tabs,tab)){
			*out = SV_EMPTY;
			return true;
		}
		if(sv_eq(tab.type,ROW_SV)) return false; 
		else UNREACHABLE("unexpected in pull_cell");
	}

	*content = sv_saved;
	if(!parse_cell(content,tabs,out,indent)){
		*out = SV_EMPTY;
		return true;
	}
	//parse_cell can leave out empty so we cast that as "null" so as not to 
	//be iterpreted as an error
	if(IS_SV_EMPTY(*out)) *out = SV_JS_NULL;
	return true;
}

bool parse_row(String_View *content,XmlTabs *tabs,uint64_t indent){
	/* TODO:
	 * this is a really hard coded function only used for parse_dict
	 * we will need to abstract this to make it work
	 * with parse_names and parse_compounds
	 */
	bool result = true;
	EN_Word en_word = {0};
	Ankaso_Word ankaso_word = {0};

	String_View text = {0};
	if(!pull_cell(content,tabs,indent,&text)){
		//no root
		defer(true);//junk
	}
	if(IS_SV_EMPTY(text)) defer(false);	

	if(!pull_cell(content,tabs,indent,&text)){
		//no root
		defer(true);//general
	}
	if(IS_SV_EMPTY(text)) defer(false);
	en_word.general = get_entrys_from_sv(text);
	
	if(!pull_cell(content,tabs,indent,&text)){
		TODO("root early json");
		defer(true);//root
	}
	if(IS_SV_EMPTY(text)) defer(false);
	en_word.root     = text;
	ankaso_word.root = text;

	if(!pull_cell(content,tabs,indent,&text)){
		TODO("root-meaning early json");
		defer(true);//root-meaning
	}
	if(IS_SV_EMPTY(text)) defer(false);
	en_word.root_meaning = get_entrys_from_sv(text);

	//TODO: factor out this svs
	// :write ankaso_word
	sb_append_json_sv(&ankaso_js_buffer,ankaso_word.root);
	sb_append_sv(&ankaso_js_buffer,sv_from_cstr(": {\n"));
	sb_append_sv(&ankaso_js_buffer,sv_from_cstr("\"root\":"));
	sb_append_json_sv(&ankaso_js_buffer,ankaso_word.root);
	sb_append(&ankaso_js_buffer,'\n');
	sb_append_sv(&ankaso_js_buffer,sv_from_cstr("},"));

	// :write en_word
	sb_append_json_sv(&en_js_buffer,en_word.root);
	sb_append_sv(&en_js_buffer,sv_from_cstr(": {\n"));
	sb_append_sv(&en_js_buffer,sv_from_cstr("\"root\":"));
	sb_append_json_svs(&en_js_buffer,en_word.root_meaning);
	sb_append_sv(&en_js_buffer,sv_from_cstr(",\n"));
	sb_append_sv(&en_js_buffer,sv_from_cstr("\"general\":"));
	sb_append_json_svs(&en_js_buffer,en_word.general);
	sb_append(&en_js_buffer,'\n');
	sb_append_sv(&en_js_buffer,sv_from_cstr("},"));
	
	while(pull_cell(content,tabs,indent,&text)){//junk...
		if(IS_SV_EMPTY(text)) defer(false);
	}
defer:
	return result;
}

bool parse_dict(String_View *content,XmlTabs *tabs,uint64_t indent){
	bool result = true;
	XmlTab tab = {0};
	String_View last_tab = *content;
	size_t start = indent;
	// :skip columns
	//TODO: factor skip columns
	//TODO: unhard code the columns by reading the first row
	nob_log(NOB_INFO,"skiping columns");
	for(;;){	
		if(content->count == 0){
			printf("premuture EOF\n");
			defer(false); //means we ended premucutly
		}
		if(!get_next_tab(content,&tab)) defer(false);
		static_assert(__XmlTabType_count == 4,"update parse_dict\n");
		switch(tab.tab_type){
			case XM_OPEN:{
				tab.indent = indent++;
				da_append(tabs,tab);
				if(sv_eq(tab.type,ROW_SV)){
					if(indent - 1 != start) defer(false);
					--indent;
					goto over_columns;
				}
				break;
			}
			case XM_CLOSE:{
				tab.indent = --indent;
				da_append(tabs,tab);
				TODO("close");
				if(sv_eq(tab.type,ROW_SV)) goto over_columns;
				break;
			}
			case XM_CONTENT:{
				tab.indent = indent;
				da_append(tabs,tab);
				TODO("content");
				if(sv_eq(tab.type,ROW_SV)) goto over_columns;
				break;
			}
			case XM_SELF_CONTAINED:{
				tab.indent = indent;
				da_append(tabs,tab);
				if(sv_eq(tab.type,ROW_SV)){
					if(indent != start) defer(false);
					goto over_columns;
				}
				break;
			}
			case __XmlTabType_count:UNREACHABLE("XmlTabType_count found in parse column in parse_dict");
		}
		last_tab = *content;
	}
	over_columns:
	// :rows
	nob_log(NOB_INFO,"parsing rows and writing json to buffers");
	bool first_row = true;
	*content = last_tab;
	sb_append_sv(&ankaso_js_buffer,sv_from_cstr("{\n"));
	sb_append_sv(&en_js_buffer,sv_from_cstr("{\n"));
	for(;;){
		if(content->count == 0){
			printf("premuture EOF\n");
			defer(false); //means we ended premucutly
		}
		if(!get_next_tab(content,&tab)){
			printf("failed to get_next_tab\n");
			defer(false);
		}
		static_assert(__XmlTabType_count == 4,"update parse_dict\n");
		switch(tab.tab_type){
			case XM_OPEN:{
				tab.indent = indent++;
				da_append(tabs,tab);
				if(sv_eq(tab.type,ROW_SV)){	
					if(first_row){ 
						if(!parse_first_row(content,tabs,indent)) defer(false); 
						first_row = false;
					}
					else if(!parse_row(content,tabs,indent)) defer(false);
					--indent;
				}
				else UNREACHABLE("unexpected in parse_dict");
				break;
			}
			case XM_CLOSE:{
				tab.indent = --indent;
				da_append(tabs,tab);
				if(!close_tab(tabs,tab)) defer(false);
				//this -1 is kinda ugly
				if(tab.indent == start - 1){
					if(sv_eq(tab.type,TABLE_SV)) goto over_rows;
				}
				break;
			}
			case XM_CONTENT:{
				tab.indent = indent;
				da_append(tabs,tab);
				TODO("OPEN row");
				break;
			}
			case XM_SELF_CONTAINED:{
				tab.indent = indent;
				da_append(tabs,tab);

				TODO("sc");
				if(sv_eq(tab.type,ROW_SV)){}
				printf(SV_Fmt"\n",SV_Arg(tab.type));
				break;
			}
			case __XmlTabType_count:UNREACHABLE("XmlTabType_count found in parse column in parse_dict");
		}
	}
	over_rows:
	sb_json_close(&ankaso_js_buffer,'}');
	sb_json_close(&en_js_buffer,'}');
defer:
	return result;
}

bool parse_tabs(String_View content){
	nob_log(NOB_INFO,"parsing content.xml");
	bool result = true;
	//needed for the da
	XmlTabs tabs = {0};//do we need this?
	XmlTab tab = {0};	
	uint64_t indent = 0;
	for(;;){
		if(content.count == 0) defer(true); //TODO: EOF
		if(!get_next_tab(&content,&tab)) defer(false);

		static_assert(__XmlTabType_count == 4,"update parse_tabs\n");
		switch(tab.tab_type){
			case XM_OPEN:{
				tab.indent = indent++;
				da_append(&tabs,tab);
				if(sv_eq(tab.type, sv_from_cstr(TABLE_STR))){
					//TODO: less hardcoding
					int i = get_attribute(tab,sv_from_cstr("table:name"));
					if(i < 0) defer(false);
					//printf(SV_Fmt"\n",SV_Arg(tab.atts.items[i].value));
					if(sv_eq(tab.atts.items[i].value,DICT_SHEET_SV)){
						nob_log(NOB_INFO,"parsing "SV_Fmt,SV_Arg(DICT_SHEET_SV));
						if(!parse_dict(&content,&tabs,indent)) defer(false);
						--indent;
						break;
					}
				}
				break;
			}
			case XM_CLOSE:{
				tab.indent = --indent;
				da_append(&tabs,tab);
				if(!close_tab(&tabs,tab)) defer(false);
				break;
			}
			case XM_CONTENT:{
				tab.indent = indent;
				da_append(&tabs,tab);
				break;
			}
			case XM_SELF_CONTAINED:{
				tab.indent = indent;
				da_append(&tabs,tab);
				if(sv_eq(tab.type, sv_from_cstr(TABLE_STR))) UNREACHABLE("FOUND EMPTY TABLE");
				break;
			}
			case __XmlTabType_count:UNREACHABLE("XmlTabType_count found in parse_tabs");
		}
	}
defer:
	//TODO: make it not complain
	//otherwise MEMORYLEAK is fine tho
	//mz_free((void*)content.data);
	//free(content.data);
	return result;
}

#define OUTPUT_DIR "output"
#define LANG_OUTPUT_DIR OUTPUT_DIR"/lang"

bool dump_json(){
	bool result = true;
	if(!mkdir_if_not_exists(OUTPUT_DIR)) defer(false);
	nob_log(NOB_INFO,"dumping ankaso_json_buffer to dictinary.json");
	if(!write_entire_file(OUTPUT_DIR"/dictinary.json",ankaso_js_buffer.items,ankaso_js_buffer.count)) defer(false);
	// :langs
	if(!mkdir_if_not_exists(LANG_OUTPUT_DIR)) defer(false);
	nob_log(NOB_INFO,"dumping en_json_buffer to en.json");
	if(!write_entire_file(LANG_OUTPUT_DIR"/en.json",en_js_buffer.items,en_js_buffer.count)) defer(false);
defer:
	return result;
}

int main(void){
	int result = 0;
	String_View content;
	if(!get_content(&content)) defer(1);
	if(!parse_tabs(content)) defer(1);
	//TODO: free content
	if(!dump_json()) defer(1);
defer:
	return result;
}
