

#ifndef POTREEWRITER_H
#define POTREEWRITER_H


class Point;



class PotreeWriter{


public:
	virtual void add(Point &p) = 0;
	virtual void flush() = 0;
	virtual void close() = 0;
	virtual long long numPointsWritten() = 0;


};





#endif