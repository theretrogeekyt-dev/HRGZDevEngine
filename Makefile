#==============================================================================
# HRGZDevEngine DOOM - Top-Level Unified Makefile
#==============================================================================

CC ?= gcc
CFLAGS ?= -O2 -Wall -Wno-parentheses -Wno-unused-const-variable -Wno-unused-but-set-variable -Wno-unused-variable -Wno-misleading-indentation -Wno-format-overflow -Wno-maybe-uninitialized -Wno-dangling-pointer -Wno-unknown-warning-option -std=c99 -Isrc/doom -Isrc/hal/common
LDFLAGS ?= -lm

BUILD_DIR = build
TARGET_TEST = $(BUILD_DIR)/doom_test

CORE_SRCS = \
	src/doom/am_map.c \
	src/doom/d_items.c \
	src/doom/d_main.c \
	src/doom/d_net.c \
	src/doom/doomdef.c \
	src/doom/doomstat.c \
	src/doom/dstrings.c \
	src/doom/f_finale.c \
	src/doom/f_wipe.c \
	src/doom/g_game.c \
	src/doom/hu_lib.c \
	src/doom/hu_stuff.c \
	src/doom/info.c \
	src/doom/m_argv.c \
	src/doom/m_bbox.c \
	src/doom/m_cheat.c \
	src/doom/m_fixed.c \
	src/doom/m_menu.c \
	src/doom/m_misc.c \
	src/doom/m_random.c \
	src/doom/m_swap.c \
	src/doom/p_ceilng.c \
	src/doom/p_doors.c \
	src/doom/p_enemy.c \
	src/doom/p_floor.c \
	src/doom/p_inter.c \
	src/doom/p_lights.c \
	src/doom/p_map.c \
	src/doom/p_maputl.c \
	src/doom/p_mobj.c \
	src/doom/p_plats.c \
	src/doom/p_pspr.c \
	src/doom/p_saveg.c \
	src/doom/p_setup.c \
	src/doom/p_sight.c \
	src/doom/p_spec.c \
	src/doom/p_switch.c \
	src/doom/p_telept.c \
	src/doom/p_tick.c \
	src/doom/p_user.c \
	src/doom/r_bsp.c \
	src/doom/r_data.c \
	src/doom/r_draw.c \
	src/doom/r_main.c \
	src/doom/r_plane.c \
	src/doom/r_segs.c \
	src/doom/r_sky.c \
	src/doom/r_things.c \
	src/doom/s_sound.c \
	src/doom/sounds.c \
	src/doom/st_lib.c \
	src/doom/st_stuff.c \
	src/doom/tables.c \
	src/doom/v_video.c \
	src/doom/w_wad.c \
	src/doom/wi_stuff.c \
	src/doom/z_zone.c

COMMON_SRCS = \
	src/hal/common/i_sound_mixer.c \
	src/hal/common/i_mus2midi.c

TEST_SRCS = \
	src/hal/test/i_main_test.c \
	src/hal/test/i_system_test.c \
	src/hal/test/i_video_test.c \
	src/hal/test/i_sound_test.c \
	src/hal/test/i_net_test.c

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
all: all-ports
else
all: test
endif

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

all-ports: $(BUILD_DIR)
	./scripts/build_all.sh

test: $(BUILD_DIR)
	$(CC) $(CFLAGS) $(CORE_SRCS) $(COMMON_SRCS) $(TEST_SRCS) $(LDFLAGS) -o $(TARGET_TEST)
	@echo "Test runner built: $(TARGET_TEST)"

run-test: test
	$(TARGET_TEST) -iwad doom1.wad -warp 1 1 -testframes 70

mac: $(BUILD_DIR)
	./scripts/build_mac.sh

dos: $(BUILD_DIR)
	@mkdir -p build/dos
	$(MAKE) -f Makefile.dos TARGET=build/dos/DOOM.EXE

win: $(BUILD_DIR)
	@mkdir -p build/win
	$(MAKE) -f Makefile.win TARGET=build/win/doom.exe

clean:
	rm -rf $(BUILD_DIR) *.ppm *.png
