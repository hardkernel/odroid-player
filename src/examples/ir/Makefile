# SPDX-FileCopyrightText: 2025 Phillip Choi for Hardkernel
# SPDX-License-Identifier: Apache-2.0

CC = gcc
CFLAGS = -Wall -Wextra
LDFLAGS = -ldl

NAME = odroid-player-client-ir
SRCS = client_ir.c lib_loader.c
OBJS = $(SRCS:.c=.o)
DEPS = $(SRCS:.c=.d)

%.o: %.c
	$(COMPILE.c) $(OUTPUT_OPTION) $<

$(NAME): $(OBJS)
	$(LINK.c) $^ $(LOADLIBS) $(LDLIBS) $(OUTPUT_OPTION)

-include $(DEPS)

.PHONY: all clean fclean re

all: $(NAME)

clean:
	@rm -rf $(OBJS) $(DEPS)

fclean: clean
	@rm -rf $(NAME)

re: fclean
	$(MAKE) all

