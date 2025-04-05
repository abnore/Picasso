CC      := clang
CFLAGS  := -Wall -Wextra
INCLUDE := -I. -I../ -I../icc_profiles

SRC := \
     test.c \
    ../picasso.c \
    ../logger.c \
    ../bmp.c \
    ../icc_profiles/picasso_icc_profiles.c \
    ../icc_profiles/picasso_icc_enum_to_string.c

TARGET := test_bmp

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(INCLUDE) $(SRC) -o $(TARGET)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
