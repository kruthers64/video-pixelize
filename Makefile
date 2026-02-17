TEST_IMAGE := test-02.jpg
NAME_VID := video-pixels
NAME_PIX := pixelize-plus
INSTALL_DIR := $(HOME)/.local/share/gegl-0.4/plug-ins
TEST_IN := test-images
TEST_OUT := out

phony: all test clean view

CFLAGS := -shared -Werror

all: $(NAME_VID).so $(NAME_PIX).so

$(NAME_VID).so: Makefile $(NAME_VID).c
	gcc $(CFLAGS) $(NAME_VID).c `pkg-config --cflags --libs gegl-0.4` -I. -fpic -o $(NAME_VID).so
	cp -pv $(NAME_VID).so $(INSTALL_DIR)

$(NAME_PIX).so: Makefile $(NAME_PIX).c
	gcc $(CFLAGS) $(NAME_PIX).c `pkg-config --cflags --libs gegl-0.4` -I. -fpic -o $(NAME_PIX).so
	cp -pv $(NAME_PIX).so $(INSTALL_DIR)

test: all
	for f in $(TEST_IN)/* ; do gegl $$f -o $(TEST_OUT)/`basename $$f` -- kruthers:video-pixels ; done

clean:
	rm -vf *.so out/*

view:
	qimgv $(TEST_OUT) >> /dev/null 2>&1 &

run: all
	gimp $(TEST_IMAGE)
