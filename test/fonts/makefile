#----------------------------------------
# Makefile for test_fonts (inside test/fonts/)
#----------------------------------------

src_dir     = ../../
canopy      = $(HOME)/personalC/Canopy
picasso     = $(HOME)/personalC/Picasso

cc          = clang
cc_flags    = -framework Cocoa \
              -I$(canopy)/lib \
              -I$(canopy) \
              -I$(picasso) \
              -I$(src_dir)

cc_flags   += -Wall -Wextra

src_common  = $(canopy)/src/canopy.m \
              $(canopy)/src/canopy_event.c \
              $(canopy)/src/canopy_time.c \
              $(canopy)/src/common.c \
              $(picasso)/bmp.c \
              $(picasso)/picasso.c \
              $(picasso)/icc_profiles/picasso_icc_profiles.c \
              $(picasso)/logger.c

target      = test_fonts

all: $(target)

$(target): test_fonts.c $(src_common)
	$(cc) $^ $(cc_flags) -o $@

clean:
	rm -rf test_fonts
