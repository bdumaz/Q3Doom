# M0 baseline: builds the UNMODIFIED id Software Quake III Arena engine (32-bit Win32)
# straight from external/Quake-III-Arena, so we have a reference to compare against.
# Replaced by the real build in M1.
cmake_minimum_required(VERSION 3.20)
project(Q3Baseline C CXX)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
  message(FATAL_ERROR "The baseline must be built 32-bit: cmake -A Win32")
endif()

set(Q3 ${CMAKE_CURRENT_SOURCE_DIR}/../external/Quake-III-Arena/code)

add_compile_definitions(WIN32 _WINDOWS _CRT_SECURE_NO_WARNINGS _WINSOCK_DEPRECATED_NO_WARNINGS)
# Old code: no SAFESEH (inline asm objects), relaxed warnings
add_compile_options(/W1 /wd4996)
add_link_options(/SAFESEH:NO)

function(q3_sources out dir)
  set(list "")
  foreach(f ${ARGN})
    list(APPEND list ${Q3}/${dir}/${f})
  endforeach()
  set(${out} ${list} PARENT_SCOPE)
endfunction()

# --- botlib -----------------------------------------------------------------
q3_sources(BOTLIB_SRC botlib
  be_aas_bspq3.c be_aas_cluster.c be_aas_debug.c be_aas_entity.c be_aas_file.c
  be_aas_main.c be_aas_move.c be_aas_optimize.c be_aas_reach.c be_aas_route.c
  be_aas_routealt.c be_aas_sample.c be_ai_char.c be_ai_chat.c be_ai_gen.c
  be_ai_goal.c be_ai_move.c be_ai_weap.c be_ai_weight.c be_ea.c be_interface.c
  l_crc.c l_libvar.c l_log.c l_memory.c l_precomp.c l_script.c l_struct.c)
add_library(botlib STATIC ${BOTLIB_SRC})
target_compile_definitions(botlib PRIVATE BOTLIB)

# --- renderer ----------------------------------------------------------------
q3_sources(REND_SRC renderer
  tr_animation.c tr_backend.c tr_bsp.c tr_cmds.c tr_curve.c tr_flares.c tr_font.c
  tr_image.c tr_init.c tr_light.c tr_main.c tr_marks.c tr_mesh.c tr_model.c
  tr_noise.c tr_scene.c tr_shade.c tr_shade_calc.c tr_shader.c tr_shadows.c
  tr_sky.c tr_surface.c tr_world.c)
q3_sources(JPEG_SRC jpeg-6
  jcapimin.c jccoefct.c jccolor.c jcdctmgr.c jchuff.c jcinit.c jcmainct.c
  jcmarker.c jcmaster.c jcomapi.c jcparam.c jcphuff.c jcprepct.c jcsample.c
  jctrans.c jdapimin.c jdapistd.c jdatadst.c jdatasrc.c jdcoefct.c jdcolor.c
  jddctmgr.c jdhuff.c jdinput.c jdmainct.c jdmarker.c jdmaster.c jdpostct.c
  jdsample.c jdtrans.c jerror.c jfdctflt.c jidctflt.c jmemmgr.c jmemnobs.c jutils.c)
q3_sources(WINGL_SRC win32 win_gamma.c win_glimp.c win_qgl.c)
add_library(renderer STATIC ${REND_SRC} ${JPEG_SRC} ${WINGL_SRC})

# --- splines (C++) -----------------------------------------------------------
q3_sources(SPLINES_SRC splines
  math_angles.cpp math_matrix.cpp math_quaternion.cpp math_vector.cpp
  q_parse.cpp q_shared.cpp splines.cpp util_str.cpp)
add_library(splines STATIC ${SPLINES_SRC})

# --- quake3.exe --------------------------------------------------------------
q3_sources(CLIENT_SRC client
  cl_cgame.c cl_cin.c cl_console.c cl_input.c cl_keys.c cl_main.c cl_net_chan.c
  cl_parse.c cl_scrn.c cl_ui.c snd_adpcm.c snd_dma.c snd_mem.c snd_mix.c snd_wavelet.c)
q3_sources(QCOMMON_SRC qcommon
  cm_load.c cm_patch.c cm_polylib.c cm_test.c cm_trace.c cmd.c common.c cvar.c
  files.c huffman.c md4.c msg.c net_chan.c unzip.c vm.c vm_interpreted.c vm_x86.c)
q3_sources(SERVER_SRC server
  sv_bot.c sv_ccmds.c sv_client.c sv_game.c sv_init.c sv_main.c sv_net_chan.c
  sv_snapshot.c sv_world.c)
q3_sources(WIN_SRC win32
  win_input.c win_main.c win_net.c win_shared.c win_snd.c win_syscon.c win_wndproc.c winquake.rc)
q3_sources(SHARED_SRC game q_math.c q_shared.c)

add_executable(quake3 WIN32 ${CLIENT_SRC} ${QCOMMON_SRC} ${SERVER_SRC} ${WIN_SRC} ${SHARED_SRC}
  crashreport.c)
target_link_libraries(quake3 PRIVATE botlib renderer splines
  winmm ws2_32 wsock32 dinput8 dxguid opengl32 user32 gdi32 advapi32 dbghelp)
