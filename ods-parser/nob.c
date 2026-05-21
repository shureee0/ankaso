#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIXS
#include "nob.h"

#define cc "cc"
#define Olevel "-o0"
#define debug_flags "-fsanitize=address","-g"
#define warnings "-Wextra","-Wall"// debug_flags

#define miniz_dir "include/miniz"

bool compile_miniz(void){
	Cmd cmd = {0};
	cmd_append(&cmd,"cmake","-S",miniz_dir,"-B",miniz_dir);
	if(!cmd_run(&cmd)) return 1;
	cmd_append(&cmd,"make","-C",miniz_dir);
	if(!cmd_run(&cmd)) return 1;
	return true;
}

int main(int argc, char **argv){	
	GO_REBUILD_URSELF(argc,argv);
	if(!compile_miniz()) return 1;
	Cmd cmd = {0};
	cmd_append(&cmd,cc,warnings,Olevel,"ods-parser.c","-o","ods-parser","-L./include/miniz/","-lminiz");
	if(!cmd_run(&cmd)) return 1;
	cmd_append(&cmd,"./ods-parser");
	if(!cmd_run(&cmd)) return 1;
}
