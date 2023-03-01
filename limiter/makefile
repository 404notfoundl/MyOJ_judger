MKDIR := mkdir
.PHONY: clean
FILTEROUT := 
COLOR_TAR := \e[34;01m
COLOR_END := \e[0m
CC_SRC := $(patsubst ./%, %, $(wildcard ./*.cpp))
CC_SRC := $(filter-out $(FILTEROUT), $(CC_SRC))
CC_OBJ := $(patsubst %.cpp,%.o,$(CC_SRC))
CC = g++ -std=c++14

INCLUDE_PATH = -I ./thirdlib -I.
LIB_PATH = -L /usr/lib64/
LIBS = -lcgroup -lseccomp -lrabbitmq -lhiredis
RM_DIRS = bin obj/x64

SO = -fPIC -shared -DSO
SUFFIX = -Wall -pthread

all: limiter so

limiter:
	@printf "compile $(COLOR_TAR) $@ $(COLOR_END)\n"
	@$(CC) resource_limit.cpp -o limiter $(INCLUDE_PATH) $(LIB_PATH) $(LIBS) $(SUFFIX) $(OPT)

so: 
	@printf "compile $(COLOR_TAR) $@ $(COLOR_END)\n"
	@$(CC) resource_limit.cpp -o liblimiter.so $(INCLUDE_PATH) $(LIB_PATH) $(LIBS) $(SUFFIX) $(SO) $(OPT)

clean:
	@$(RM) limiter liblimiter.so $(RM_DIRS)