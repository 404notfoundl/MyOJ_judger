.PHONY: clean
MKDIR := mkdir
FILTEROUT := main.cpp
COLOR_TAR := \e[34;01m
COLOR_END := \e[0m
CC_SRC := $(patsubst ./%, %, $(wildcard ./*.cpp))
CC_SRC := $(filter-out $(FILTEROUT), $(CC_SRC))
CC_OBJ := $(patsubst %.cpp,%.o,$(CC_SRC))
CC = g++ -std=c++14
RM_DIRS = bin obj/x64

DIRS = obj/
DIR_OBJ = $(addprefix $(DIRS), $(CC_OBJ))

INCLUDE_PATH = -I ./ThirdLib -I. -I ./lock
LIB_PATH = -L /usr/lib64/
LIBS = -lcgroup -lseccomp -lrabbitmq -lhiredis -llimiter -ljudger

SUFFIX = -Wall -pthread
OPT ?= -O2

all: $(DIRS) main

$(DIRS):
	@echo "mkdirs"
	$(MKDIR) $@
main: $(DIR_OBJ)
	@printf "compile $(COLOR_TAR) $@ $(COLOR_END)\n"
	@$(CC) $(DIR_OBJ) main.cpp -o main $(INCLUDE_PATH) $(LIB_PATH) $(LIBS) $(SUFFIX) $(OPT)
$(DIR_OBJ):obj/%.o:%.cpp
	@printf "compile $(COLOR_TAR) $@ $(COLOR_END)\n"
	@$(CC) $^ -o $@ $(INCLUDE_PATH) $(SUFFIX) -c $(OPT)

clean:
	@$(RM) -rf main main_dbg $(DIR_OBJ) $(RM_DIRS)
