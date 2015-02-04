######################################
#
# Generic makefile
#
# by Coon Xu
# email: coonxu@126.com
#
# Copyright (c) 2005 Coon Xu
# All rights reserved.
# 
# No warranty, no liability;
# you use this at your own risk.
#
# You are free to modify and
# distribute this without giving
# credit to the original author.
#
######################################

#source file
# 源文件，自动找所有 .c 和 .cpp 文件，并将目标定义为同名 .o 文件
SOURCE  := client.c cJSON/cJSON.c
OBJS    := $(patsubst %.c,%.o,$(patsubst %.cpp,%.o,$(SOURCE)))
 
#target you can change test to what you want
# 目标文件名，输入任意你想要的执行文件名
TARGET  := demo
 
#compile and lib parameter
# 编译参数
CC      := gcc
LIBS    := -lpthread -lm -lrt
LDFLAGS:= 
DEFINES:=
INCLUDE:= -I. -I./cJSON
CFLAGS  := -g -Wall -O3 $(DEFINES) $(INCLUDE)
CXXFLAGS:= $(CFLAGS) -DHAVE_CONFIG_H
 
 
#i think you should do anything here
# 下面的基本上不需要做任何改动了
.PHONY : everything objs clean veryclean rebuild
 
everything : $(TARGET)
 
all : $(TARGET)
 
objs : $(OBJS)
 
rebuild: veryclean everything
               
clean :
	rm -fr *.so
	rm -fr *.o
   
veryclean : clean
	rm -fr $(TARGET)
 
$(TARGET) : $(OBJS) 
	$(CC) $(CXXFLAGS) -o $@ $(OBJS) $(LDFLAGS) $(LIBS)