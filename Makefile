all:
	g++ -g -o audio audio.cpp -lavformat -lavcodec -lavfilter -lavdevice -lswscale -lavutil -lglog -pthread -lswresample -lm
	./audio