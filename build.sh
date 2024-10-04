set -xe

for arch in x86_64 i686
#for arch in i686
do
	OUT_DIR=dist/${arch}
	rm -rf $OUT_DIR
	mkdir -p $OUT_DIR

	min_hook_lib="MinHook.x86"
	if [ ${arch} == x86_64 ]
	then
		min_hook_lib="MinHook.x64"
	fi
	cp minhook_1.3.3/bin/${min_hook_lib}.dll $OUT_DIR

	asi_loader_path=ultimate_asi_loader/x86/dinput8.dll
	if [ ${arch} == x86_64 ]
	then
		asi_loader_path=ultimate_asi_loader/x64/dinput8.dll
	fi

	CC=${arch}-w64-mingw32-gcc
	CPPC=${arch}-w64-mingw32-g++
	$CPPC -g -fPIC -c asi_mode.cpp -std=c++20 -o $OUT_DIR/asi_mode.o
	$CPPC -g -fPIC -c wrapper_mode.cpp -std=c++20 -o $OUT_DIR/wrapper_mode.o
	$CC -g -fPIC -c logging.c -o $OUT_DIR/logging.o
	$CC -g -fPIC -c hooking.c -Iminhook_1.3.3/include -o $OUT_DIR/hooking.o -O0
	$CPPC -g -fPIC -c config.cpp -Ijson_hpp -o $OUT_DIR/config.o
	$CPPC -g -fPIC -c modify_effects.cpp -o $OUT_DIR/modify_effects.o

	obj_list="$OUT_DIR/logging.o $OUT_DIR/hooking.o $OUT_DIR/config.o $OUT_DIR/modify_effects.o"
	$CPPC -g -shared -o $OUT_DIR/dinput8_ffb_tweaks_${arch}.asi $obj_list $OUT_DIR/asi_mode.o -Lminhook_1.3.3/bin -lntdll -lkernel32 -ldxguid -Wl,-Bstatic -lpthread -l${min_hook_lib} -static-libgcc
	$CPPC -g -shared -o $OUT_DIR/dinput8_ffb_tweaks_${arch}.dll $obj_list -Lminhook_1.3.3/bin -lntdll -lkernel32 -ldxguid -Wl,-Bstatic -lpthread -l${min_hook_lib} -static-libgcc
	#$CPPC -g -shared -o $OUT_DIR/dinput8.dll $obj_list $OUT_DIR/wrapper_mode.o -Lminhook_1.3.3/bin -lntdll -lkernel32 -ldxguid -Wl,-Bstatic -lpthread -l${min_hook_lib} -static-libgcc

	rm $OUT_DIR/*.o

	cp dinput8_ffb_tweaks_config.json $OUT_DIR/
	cp $asi_loader_path $OUT_DIR/d3d9.dll
done

