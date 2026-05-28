#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIXS
#include "nob.h"

//#define cc "cc"
#define cc "cc"
#define mingw "x86_64-w64-mingw32-gcc"
#define mingw_build_dir "build-x86_64-meson-mingw"
#define Olevel "-o0"
#define debug_flags "-g","-fsanitize=address"
#define warnings "-Wextra","-Wall"// debug_flags

#define miniz_dir "include/miniz"

bool compile_with_mingw = false;

bool compile_miniz(void){
	Cmd cmd = {0};
	if(compile_with_mingw){
		//cmd_append(&cmd,"CC="mingw"
		cmd_append(&cmd,"meson");
		cmd_append(&cmd,"setup");
		cmd_append(&cmd,"--wipe");//TODO:maybe cashe this
		cmd_append(&cmd,miniz_dir"/"mingw_build_dir);
		cmd_append(&cmd,miniz_dir);//source
		//set cc and cppc
		cmd_append(&cmd,"--native-file");
		cmd_append(&cmd,"build/x86_64-mingw.ini");
		if(!cmd_run(&cmd)) return false;

		cmd_append(&cmd,"meson");
		cmd_append(&cmd,"compile");
		cmd_append(&cmd,"-C");
		cmd_append(&cmd,miniz_dir"/"mingw_build_dir);
		if(!cmd_run(&cmd)) return false;
		return true;
	}
	cmd_append(&cmd,"cmake");
	cmd_append(&cmd,"-S",miniz_dir);
	cmd_append(&cmd,"-B",miniz_dir);
	if(!cmd_run(&cmd)) return false;
	cmd_append(&cmd,"make","-C",miniz_dir);
	if(!cmd_run(&cmd)) return false;
	return true;
}

int main(int argc, char **argv){
	GO_REBUILD_URSELF(argc,argv);
	(void)shift(argv,argc);
	while(argc > 0){
		char *arg = shift(argv,argc);
		if(strcmp(arg,"--mingw") == 0) compile_with_mingw = true;
		else{
			//TODO: usage??
			nob_log(NOB_ERROR,"unknown option %s",arg);
			return 1;
		}
	}

	if(!compile_miniz()) return 1;

	Cmd cmd = {0};
	if(compile_with_mingw) cmd_append(&cmd,mingw);
	else{
		cmd_append(&cmd,cc);
		//TODO: make warning work for mingw
		cmd_append(&cmd,warnings);
		cmd_append(&cmd,Olevel);
		cmd_append(&cmd,debug_flags);
	}
	char *build_folder;
	if(compile_with_mingw) build_folder = "./"miniz_dir"/"mingw_build_dir;
	else build_folder = "./"miniz_dir;//TODO: attact it to build folder

	//TODO: make cmake use build folder
	if(compile_with_mingw) cmd_append(&cmd,"-o","build/ods-parser.exe");
	else cmd_append(&cmd,"-o","ods-parser");

	cmd_append(&cmd,"ods-parser.c");
	cmd_append(&cmd,"-I",build_folder);

	if(compile_with_mingw) cmd_append(&cmd,"-L./"miniz_dir"/"mingw_build_dir,"-lminiz");
	else cmd_append(&cmd,"-L./"miniz_dir,"-lminiz");//TODO: atttact to build folder

	if(!cmd_run(&cmd)) return 1;

	if(!compile_with_mingw){
		cmd_append(&cmd,"./ods-parser");
		if(!cmd_run(&cmd)) return 1;
	}
}
