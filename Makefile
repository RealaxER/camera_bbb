all:
	g++ -o stream stream.cpp -lavformat -lavcodec -lavfilter -lavdevice -lswscale -lavutil -lglog -pthread