#----------------------------------------
# Makefile for test_fonts (inside test/fonts/)
#----------------------------------------

src_dir     = ../../
picasso     = $(HOME)/personalC/Picasso
canopy	    = $(HOME)/personalC/Canopy

cc          = clang
cc_flags    = -framework Cocoa \
              -I$(picasso) \
	      -I$(picasso)/icc_profiles \
	      -I$(canopy) \
	      -I$(canopy)/src \
	      -I$(canopy)/lib

cc_flags   += -Wall -Wextra

src_common  = $(picasso)/bmp.c \
              $(picasso)/picasso.c \
              $(picasso)/icc_profiles/picasso_icc_profiles.c \
              $(picasso)/logger.c \
              $(canopy)/src/canopy.m\
              $(canopy)/src/canopy_time.c\
	      $(canopy)/src/canopy_event.c \
              $(canopy)/src/common.c\

target      = test_fonts

all: $(target)

$(target): test_fonts.c $(src_common)
	$(cc) $^ $(cc_flags) -o $@

clean:
	rm -rf test_fonts
