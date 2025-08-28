# Simple Makefile with profile correction
all:
	gcc -fprofile-use -fprofile-correction -frename-registers -finline-functions -fpredictive-commoning -fno-math-errno -foptimize-sibling-calls -fpredictive-commoning -fbranch-probabilities -fstrict-aliasing -O2 -o um um.c

clean:
	rm -f um


##############################################################################
##############################################################################

###############################################################################
# Makefile for Comp 40 assignment
###############################################################################

###################### Compiler and Flags #####################################

# CC      = gcc
# IFLAGS  = -I/comp/40/build/include -I/usr/sup/cii40/include/cii
# # Compile flags tuned for maximum run‑time speed on *this* machine
# # ---------- compile flags ----------
# CFLAGS = -g -std=gnu99 \
#          -O2 \
#          -Wall -Wextra -Werror -Wfatal-errors -pedantic $(IFLAGS)

# # ---------- link flags ----------
# LDFLAGS = -g -L/comp/40/build/lib -L/usr/sup/cii40/lib64

# # Libraries:
# #  -lcii40 is Hanson’s CII library
# #  -lm for math
# #  -lrt for real-time/timing (if needed)
# LDLIBS  = -l40locality -lcii40 -lm -lrt

# # Gather local .h files so we never forget to update dependencies.
# INCLUDES = $(shell echo *.h)

# all: um

# # Compile all .c into .o
# %.o: %.c $(INCLUDES)
# 	$(CC) $(CFLAGS) -c $< -o $@

# um: um.o
# 	$(CC) $(LDFLAGS) $^ -o $@ $(LDLIBS)

# clean: 
# 	rm -f um *.o
