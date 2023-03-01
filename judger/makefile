MKDIR := mkdir
.PHONY : clean
FILTEROUT := main.cpp
COLOR_TAR := \e[34;01m
COLOR_END := \e[0m
CC_SRC := $(patsubst ./%, %, $(wildcard ./*.cpp))
CC_SRC := $(filter-out $(FILTEROUT), $(CC_SRC))
CC_OBJ := $(patsubst %.cpp,%.o,$(CC_SRC))
CC = g++ -std=c++14

DIRS = obj/
DIR_OBJ = $(addprefix $(DIRS), $(CC_OBJ))
DIR_OBJ_SO = $(patsubst %.o, %_so.o, $(DIR_OBJ))
RM_DIRS = bin obj/x64

INCLUDE_PATH = -I ./thirdlib -I.
LIB_PATH = -L /usr/lib64/
LIBS = -lcgroup -lseccomp 

SO = -fPIC -shared -DSO
DEF = -DTEST -DWAIT4_ON -DSYNC_CG_ON # -DSCMP_OFF 禁用scmp
SUFFIX = -Wall -pthread
OPT ?= -O2

all: $(DIRS) runner compiler so
print:
	@echo $(DIR_OBJ)
$(DIRS):
	@echo "mkdirs"
	$(MKDIR) $@

runner: $(DIR_OBJ)
	@printf "compile $(COLOR_TAR) $@ $(COLOR_END)\n"
	@$(CC) $(DIR_OBJ) main.cpp -o runner $(INCLUDE_PATH) $(LIB_PATH) $(LIBS) $(SUFFIX) $(DEF) $(OPT) -DRUN_CMD
compiler: $(DIR_OBJ)
	@printf "compile $(COLOR_TAR) $@ $(COLOR_END)\n"
	@$(CC) $(DIR_OBJ) main.cpp -o compiler $(INCLUDE_PATH) $(LIB_PATH) $(LIBS) $(SUFFIX) $(DEF) $(OPT) -DCOMPILE_CMD

so: $(DIR_OBJ_SO)
	@printf "compile $(COLOR_TAR) $@ $(COLOR_END)\n"
	@$(CC) $(DIR_OBJ_SO) main.cpp -o libjudger.so $(INCLUDE_PATH) $(LIB_PATH) $(LIBS) $(SUFFIX) $(SO) $(DEF) $(OPT)

$(DIR_OBJ): obj/%.o:%.cpp
	@printf "compile $(COLOR_TAR) $@ $(COLOR_END)\n"
	@$(CC) $^ -o $@ $(INCLUDE_PATH) $(SUFFIX) $(DEF) -c $(OPT)

$(DIR_OBJ_SO): obj/%_so.o:%.cpp
	@printf "compile $(COLOR_TAR) $@ $(COLOR_END)\n"
	@$(CC) $^ -o $@ $(INCLUDE_PATH) $(SUFFIX) $(DEF) -c $(OPT) $(SO)

clean: clean_exec clean_so
	@echo clean finish
	@$(RM) -rf $(RM_DIRS)
clean_exec:
	@$(RM) runner compiler $(DIR_OBJ)
clean_so:
	@$(RM) libjudger.so $(DIR_OBJ_SO)