<!-- 
    TODO: this markdown suck make it better
-->
ods-parser.c
relies on miniz
miniz needs cmake and make or meson and ninja to build

to build and run ods-parser.c
build nob.c then run nob
it will build and link miniz for you

cc -o nob nob.c
./nob
