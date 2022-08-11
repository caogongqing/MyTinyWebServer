OBJS=pub.o wrap.o EpollWebServer.o
TARGET=server

$(TARGET):$(OBJS)
	gcc $^ -o $@

%.o:%.c
	gcc -c $< -o $@

.PHONY:clean
clean:
	rm -rf $(OBJS)

