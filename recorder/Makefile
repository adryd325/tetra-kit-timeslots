CC = g++
CFLAGS = -O2 -g -std=c++11 -Wall -Wextra -I. -Iaudio -Iaudio/cdecoder -Iaudio/sdecoder -fmax-errors=5
LDFLAGS = -lncurses -lz

# recorder
SRC = recorder_main.cc window.cc base64.cc json_parser.cc cid.cc call_identifier.cc utils.cc

# codec source files SRC1 to SRC3
# cdecoder
SRC1 =	audio/cdecoder/cdecoder.cc \
	audio/cdecoder/cdecoder_func.cc \
	audio/cdecoder/cdecoder_operators.cc
# sdecoder
SRC2 = 	audio/sdecoder/sdecoder.cc \
	audio/sdecoder/sdecoder_lsp.cc \
	audio/sdecoder/sdecoder_operators.cc \
	audio/sdecoder/sdecoder_codec.cc \
	audio/sdecoder/sdecoder_dsp.cc \
	audio/sdecoder/sdecoder_math.cc
# audio_decoder
SRC3 = 	audio/audio_decoder.cc

OBJ = $(SRC:.cc=.o) $(SRC1:.cc=.o) $(SRC2:.cc=.o) $(SRC3:.cc=.o)

EXE = recorder

.cc.o:
	$(CC) $(CFLAGS) -c $< -o $@

all: $(EXE)

$(EXE): $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $@ $(LDFLAGS)

clean:
	rm -f $(EXE) *.o *~ $(OBJ)
