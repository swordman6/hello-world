CC = gcc
CFLAGS = -Wall -c

TARGET = test
OBJS = $(patsubst %.c,%.o, $(wildcard *.c))

FFMPEG_DIR = /home/sword/Public/ffmpeg
INC_PATH = -I$(FFMPEG_DIR)
LIB_PATH = $(FFMPEG_DIR)/libs1

STA_LIBS += $(LIB_PATH)/libavfilter.a
STA_LIBS += $(LIB_PATH)/libavformat.a
STA_LIBS += $(LIB_PATH)/libavdevice.a 
STA_LIBS += $(LIB_PATH)/libavcodec.a 
STA_LIBS += $(LIB_PATH)/libavutil.a
STA_LIBS += $(LIB_PATH)/libswscale.a
STA_LIBS += $(LIB_PATH)/libswresample.a
DYN_LIBS += -L/usr/local/lib/
DYN_LIBS += -lfdk-aac -lx264
DYN_LIBS += -lm -lz -lpthread -ldl 

$(TARGET) : $(OBJS)
	$(CC) -o $@ $^ $(STA_LIBS) $(DYN_LIBS)

%.o : %.c
	$(CC) $(INC_PATH) $(CFLAGS) $< -o $@

.PHONY: clean
clean:
	rm *.o test

