#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIXS
#include "nob.h"

//#define cc "cc"
#define cc "clang"
#define Olevel "-o0"
#define debug_flags "-g","-fsanitize=address"
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
	cmd_append(&cmd,cc);
	cmd_append(&cmd,warnings);
	cmd_append(&cmd,Olevel);
	cmd_append(&cmd,debug_flags);
	cmd_append(&cmd,"ods-parser.c");
	cmd_append(&cmd,"-o","ods-parser");
	cmd_append(&cmd,"-L./include/miniz/","-lminiz");
	if(!cmd_run(&cmd)) return 1;
	cmd_append(&cmd,"./ods-parser");
	if(!cmd_run(&cmd)) return 1;
}
