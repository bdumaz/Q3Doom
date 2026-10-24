# Q3Doom: Quake III Arena engine (id Software GPL source) + Doom gameplay.
cmake_minimum_required(VERSION 3.20)
project(Q3Doom C)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "Q3Doom is 64-bit only")
endif()

set(SRC ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(RUN_DIR ${CMAKE_CURRENT_SOURCE_DIR}/run CACHE PATH "Runtime folder (binaries are copied here)")
set(ARCH_STRING x86_64)

# --- SDL2 --------------------------------------------------------------------
if(WIN32)
  set(SDL2_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/SDL2/SDL2-2.32.10/cmake)
endif()
find_package(SDL2 REQUIRED CONFIG)

# --- common compile settings -------------------------------------------------
add_compile_definitions(ARCH_STRING="${ARCH_STRING}")
if(MSVC)
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS _WINSOCK_DEPRECATED_NO_WARNINGS)
  # pointer truncation must never compile silently on 64-bit
  add_compile_options(/W3 /wd4996 /wd4244 /wd4267 /wd4305 /we4311 /we4312 /we4302 /we4022 /we4047 /we4013 /MP)
endif()

function(q3_sources out dir)
  set(list "")
  foreach(f ${ARGN})
    list(APPEND list ${SRC}/${dir}/${f})
  endforeach()
  set(${out} ${list} PARENT_SCOPE)
endfunction()

# Copy a built target into the run folder (optionally a game subfolder)
function(q3_deploy target subdir)
  add_custom_command(TARGET ${target} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${RUN_DIR}/${subdir}
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:${target}> ${RUN_DIR}/${subdir}
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_PDB_FILE:${target}> ${RUN_DIR}/${subdir}
    VERBATIM)
endfunction()

# --- libdoom: WAD, Doom asset decoding, map model (no engine dependencies) ---
q3_sources(LIBDOOM_SRC libdoom wad.c graphics.c sound.c map.c builder.c specials.c doom_info.c progress.c)
add_library(libdoom STATIC ${LIBDOOM_SRC})
target_include_directories(libdoom PUBLIC ${SRC}/libdoom)
set_target_properties(libdoom PROPERTIES OUTPUT_NAME doom)

add_executable(doomtool ${CMAKE_CURRENT_SOURCE_DIR}/tools/doomtool/doomtool.c)
target_link_libraries(doomtool PRIVATE libdoom)

enable_testing()
set(DOOM_IWAD ${CMAKE_CURRENT_SOURCE_DIR}/GameData/DOOM.WAD CACHE FILEPATH "IWAD used by the tests")
add_executable(test_libdoom ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_libdoom.c)
target_link_libraries(test_libdoom PRIVATE libdoom)
add_test(NAME libdoom COMMAND test_libdoom ${DOOM_IWAD})
set_tests_properties(libdoom PROPERTIES SKIP_RETURN_CODE 77)

# --- botlib ------------------------------------------------------------------
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
q3_sources(SDLGL_SRC sdl sdl_glimp.c sdl_qgl.c)
add_library(renderer STATIC ${REND_SRC} ${JPEG_SRC} ${SDLGL_SRC})
target_link_libraries(renderer PRIVATE SDL2::SDL2)

# --- engine executable ---------------------------------------------------------
q3_sources(CLIENT_SRC client
  cl_cgame.c cl_cin.c cl_console.c cl_doom.c cl_input.c cl_keys.c cl_main.c cl_net_chan.c
  cl_parse.c cl_scrn.c cl_ui.c snd_adpcm.c snd_dma.c snd_mem.c snd_mix.c snd_wavelet.c)
q3_sources(QCOMMON_SRC qcommon
  cm_load.c cm_patch.c cm_polylib.c cm_test.c cm_trace.c cmd.c common.c cvar.c
  doom_assets.c files.c huffman.c md4.c msg.c net_chan.c unzip.c vm.c)
q3_sources(SERVER_SRC server
  sv_bot.c sv_ccmds.c sv_client.c sv_game.c sv_init.c sv_main.c sv_net_chan.c
  sv_snapshot.c sv_world.c)
q3_sources(SDL_SRC sdl sdl_input.c sdl_snd.c)
q3_sources(SHARED_SRC game q_math.c q_shared.c)
if(WIN32)
  q3_sources(SYS_SRC win32 win_main.c win_net.c win_shared.c win_syscon.c win_crash.c winquake.rc)
  set(SYS_LIBS winmm ws2_32 wsock32 dbghelp)
endif()

add_executable(q3doom WIN32 ${CLIENT_SRC} ${QCOMMON_SRC} ${SERVER_SRC} ${SDL_SRC} ${SYS_SRC} ${SHARED_SRC})
target_link_libraries(q3doom PRIVATE botlib renderer libdoom SDL2::SDL2 ${SYS_LIBS})
q3_deploy(q3doom "")
if(WIN32)
  add_custom_command(TARGET q3doom POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:SDL2::SDL2> ${RUN_DIR}
    VERBATIM)
endif()

# --- game modules (native DLLs) ---------------------------------------------
function(q3_module target name gamedir deffile)
  add_library(${target} SHARED ${ARGN} ${deffile})
  # one output folder per game directory: modules share file names
  set_target_properties(${target} PROPERTIES OUTPUT_NAME ${name}${ARCH_STRING} PREFIX ""
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/modules/${gamedir}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/modules/${gamedir}
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/modules/${gamedir})
  q3_deploy(${target} ${gamedir})
endfunction()

q3_sources(GAME_SRC game
  ai_chat.c ai_cmd.c ai_dmnet.c ai_dmq3.c ai_main.c ai_team.c ai_vcmd.c
  bg_misc.c bg_pmove.c bg_slidemove.c g_active.c g_arenas.c g_bot.c g_client.c
  g_cmds.c g_combat.c g_items.c g_main.c g_mem.c g_misc.c g_missile.c g_mover.c
  g_session.c g_spawn.c g_svcmds.c g_syscalls.c g_target.c g_team.c g_trigger.c
  g_utils.c g_weapon.c q_math.c q_shared.c)
q3_module(stock_qagame qagame baseq3 ${SRC}/game/game.def ${GAME_SRC})

q3_sources(CGAME_SRC cgame
  cg_consolecmds.c cg_draw.c cg_drawtools.c cg_effects.c cg_ents.c cg_event.c
  cg_info.c cg_localents.c cg_main.c cg_marks.c cg_players.c
  cg_playerstate.c cg_predict.c cg_scoreboard.c cg_servercmds.c cg_snapshot.c
  cg_syscalls.c cg_view.c cg_weapons.c)
q3_sources(CGAME_SHARED game bg_misc.c bg_pmove.c bg_slidemove.c q_math.c q_shared.c)
q3_module(stock_cgame cgame baseq3 ${SRC}/cgame/cgame.def ${CGAME_SRC} ${CGAME_SHARED})

q3_sources(UI_SRC q3_ui
  ui_addbots.c ui_atoms.c ui_cdkey.c ui_cinematics.c ui_confirm.c ui_connect.c
  ui_controls2.c ui_credits.c ui_demo2.c ui_display.c ui_gameinfo.c ui_ingame.c
  ui_loadconfig.c ui_main.c ui_menu.c ui_mfield.c ui_mods.c ui_network.c
  ui_options.c ui_playermodel.c ui_players.c ui_playersettings.c ui_preferences.c
  ui_qmenu.c ui_removebots.c ui_saveconfig.c ui_serverinfo.c ui_servers2.c
  ui_setup.c ui_sound.c ui_sparena.c ui_specifyserver.c ui_splevel.c
  ui_sppostgame.c ui_spreset.c ui_spskill.c ui_startserver.c ui_team.c
  ui_teamorders.c ui_video.c)
q3_sources(UI_SHARED game bg_misc.c q_math.c q_shared.c)
q3_module(stock_ui ui baseq3 ${SRC}/q3_ui/ui.def ${UI_SRC} ${UI_SHARED} ${SRC}/ui/ui_syscalls.c)

# --- Q3Doom game modules (run/q3doom, the default fs_game) -------------------
# Forked from the stock modules; headers shared with the engine are forwarded
# to src/game, src/cgame and src/ui. The stock ui is used until it is forked.
set(QD ${SRC}/q3doom)
file(GLOB QD_GAME_SRC ${QD}/game/*.c)
q3_module(q3doom_qagame qagame q3doom ${SRC}/game/game.def ${QD_GAME_SRC} ${SRC}/game/q_math.c ${SRC}/game/q_shared.c)
target_link_libraries(q3doom_qagame PRIVATE libdoom)

file(GLOB QD_CGAME_SRC ${QD}/cgame/*.c)
q3_module(q3doom_cgame cgame q3doom ${SRC}/cgame/cgame.def ${QD_CGAME_SRC}
  ${QD}/game/bg_misc.c ${QD}/game/bg_pmove.c ${QD}/game/bg_slidemove.c ${QD}/game/bg_doom.c ${QD}/game/bg_doom_weapon.c
  ${SRC}/game/q_math.c ${SRC}/game/q_shared.c)
target_link_libraries(q3doom_cgame PRIVATE libdoom)

# data files of the q3doom game directory (configs)
add_custom_target(q3doom_data ALL
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/data/q3doom ${RUN_DIR}/q3doom
  VERBATIM)
