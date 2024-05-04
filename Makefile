pong:
	gcc pong.c -o pong -lncurses -lpthread
clean:
	rm -rf pong
install: pong
	cp -rf pong ~/.local/bin/
