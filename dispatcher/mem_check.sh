#!/bin/bash
valgrind -s --tool=memcheck --leak-check=full  --show-leak-kinds=all --log-file=mem_leck.log ./main > output.log