all:
	g++ -g -o stream stream.cpp -lavformat -lavcodec -lavfilter -lavdevice -lswscale -lavutil -lglog -pthread
	g++ -g -o test test.cpp -lavformat -lavcodec -lavfilter -lavdevice -lswscale -lavutil -lglog -pthread
