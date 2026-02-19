TEST_IMAGE := test02.png
NAME := video-pixelize
INSTALL_DIR := $(HOME)/.local/share/gegl-0.4/plug-ins
TEST_IN := test-images
TEST_OUT := out

.PHONY: all test clean view run

CFLAGS := -shared -Werror

all: $(NAME).so

$(NAME).so: Makefile $(NAME).c
	gcc $(CFLAGS) $(NAME).c `pkg-config --cflags --libs gegl-0.4` -I. -fpic -o $(NAME).so
	cp -pv $(NAME).so $(INSTALL_DIR)

test: all
	for f in $(TEST_IN)/* ; do gegl $$f -o $(TEST_OUT)/`basename $$f` -- kruthers:$(NAME) ; done

clean:
	rm -vf *.so out/*

view:
	qimgv $(TEST_OUT) >> /dev/null 2>&1 &

run: all
	gimp $(TEST_IN)/$(TEST_IMAGE)
