CC = clang
CFLAGS = -Wall -Wextra -g -I. -I../../ -I.. -I../../icc_profiles
SRC = test_stb_load_bmp.c ../../bmp.c ../../picasso.c ../../logger.c ../../icc_profiles/picasso_icc_profiles.c ../../icc_profiles/picasso_icc_enum_to_string.c
OUT = test_stb_bmp

all: $(OUT)

$(OUT): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(OUT)

run: $(OUT)
	./$(OUT)

clean:
	rm -f $(OUT)
